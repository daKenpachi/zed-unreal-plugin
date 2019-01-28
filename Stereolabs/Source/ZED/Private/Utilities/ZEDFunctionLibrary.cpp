//======= Copyright (c) Stereolabs Corporation, All rights reserved. ===============

#include "ZEDPrivatePCH.h"
#include "ZED/Classes/ZEDLocalPlayer.h"
#include "ZED/Public/Utilities/ZEDFunctionLibrary.h"
#include "ZED/Public/Core/ZEDPlayerController.h"
#include "Stereolabs/Public/Core/StereolabsCoreGlobals.h"
#include "Stereolabs/Public/Utilities/StereolabsFunctionLibrary.h"

#include <sl_mr_core/latency.hpp>

#include <sl_mr_core/Rendering.hpp>

DEFINE_LOG_CATEGORY(ZEDFunctionLibrary);

#define PLANE_THRESHOLD 0.75f

AZEDPlayerController* UZEDFunctionLibrary::GetPlayerController(UObject* WorldContextObject)
{
	APlayerController* PlayerController = WorldContextObject->GetWorld()->GetFirstPlayerController();

#if WITH_EDITOR
	if(PlayerController)
	{
		checkf(Cast<AZEDPlayerController>(PlayerController), TEXT("PlayerController must inherit from AZEDPlayerController"));
	}
#endif

	return static_cast<AZEDPlayerController*>(PlayerController);
}

AZEDCamera* UZEDFunctionLibrary::GetCameraActor(UObject* WorldContextObject)
{
	AZEDPlayerController* PlayerController = UZEDFunctionLibrary::GetPlayerController(WorldContextObject);

	if (!PlayerController)
	{
		return nullptr;
	}

	return PlayerController->ZedCamera;
}

FVector UZEDFunctionLibrary::GetCenteredLocationInFrontOfPlayer(float Distance)
{
	FVector4 OpticalCenterOffsets = USlFunctionLibrary::GetOpticalCentersOffsets(GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution, Distance);

	FVector Forward = UKismetMathLibrary::GetForwardVector(GZedViewPointRotation) * Distance;
	FVector Right = UKismetMathLibrary::GetRightVector(GZedViewPointRotation) * OpticalCenterOffsets.X;
	FVector Up = UKismetMathLibrary::GetUpVector(GZedViewPointRotation) * OpticalCenterOffsets.Y;

	return GZedViewPointLocation + Forward + Right - Up;
}

FVector UZEDFunctionLibrary::GetCustomLocationInFrontOfPlayer(const FVector& OriginLocation, const FRotator& OriginRotation, float Distance)
{
	FVector4 OpticalCenterOffsets = USlFunctionLibrary::GetOpticalCentersOffsets(GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution, Distance);

	FVector Forward = UKismetMathLibrary::GetForwardVector(OriginRotation) * Distance;
	FVector Right = UKismetMathLibrary::GetRightVector(OriginRotation) * OpticalCenterOffsets.X;
	FVector Up = UKismetMathLibrary::GetUpVector(OriginRotation) * OpticalCenterOffsets.Y;

	return OriginLocation + Forward + Right - Up;
}

bool UZEDFunctionLibrary::IsLocationInScreenBounds(AZEDPlayerController* PlayerController, const FVector& WorldLocation)
{
	FVector2D ScreenPosition;
	UZEDFunctionLibrary::ProjectWorldToScreen(PlayerController, WorldLocation, ScreenPosition);

	return PlayerController->ViewportHelper.IsInViewport(ScreenPosition.X, ScreenPosition.Y);
}

bool UZEDFunctionLibrary::IsLocationVisible(AZEDPlayerController* PlayerController, const FVector& WorldLocation)
{
	FVector2D ScreenPosition;
	UZEDFunctionLibrary::ProjectWorldToScreen(PlayerController, WorldLocation, ScreenPosition);
	
	return UZEDFunctionLibrary::IsVisible(PlayerController, WorldLocation, ScreenPosition);
}

bool UZEDFunctionLibrary::IsVisible(AZEDPlayerController* PlayerController, const FVector& WorldLocation, const FVector2D& ScreenPosition)
{
	FVector PlayerForward = PlayerController->GetPawn()->GetActorForwardVector();
	FVector ToActor = (WorldLocation - GZedViewPointLocation).GetUnsafeNormal();

	return (PlayerController->ViewportHelper.IsInViewport(ScreenPosition.X, ScreenPosition.Y) && FVector::DotProduct(PlayerForward, ToActor) > 0.0f);
}

ESlRetrieveResult UZEDFunctionLibrary::GetDepthAtScreenPosition(AZEDPlayerController* PlayerController, const FVector2D& ScreenPosition, float& Depth, float& Distance, FVector& WorldDirection)
{
	if (!PlayerController->ViewportHelper.IsInViewport(ScreenPosition.X, ScreenPosition.Y))
	{
		return ESlRetrieveResult::RR_LocationNotValid;
	}

	FVector WorldLocation;
	UZEDFunctionLibrary::DeprojectScreenToWorld(PlayerController, ScreenPosition, WorldLocation, WorldDirection);

	Depth = GSlCameraProxy->GetDepth(PlayerController->ViewportHelper, FIntPoint(ScreenPosition.X, ScreenPosition.Y));

	Distance = USlFunctionLibrary::ConvertDepthToDistance(PlayerController->ViewportHelper, ScreenPosition, Depth);

	return ESlRetrieveResult::RR_RetrieveValid;
}

ESlRetrieveResult UZEDFunctionLibrary::GetDepthAtWorldLocation(AZEDPlayerController* PlayerController, const FVector& Location, float& Depth, float& Distance)
{
	FVector2D ScreenPosition;
	UZEDFunctionLibrary::ProjectWorldToScreen(PlayerController, Location, ScreenPosition);
	if (!UZEDFunctionLibrary::IsVisible(PlayerController, Location, ScreenPosition))
	{
		return ESlRetrieveResult::RR_LocationNotValid;
	}

	Depth = GSlCameraProxy->GetDepth(PlayerController->ViewportHelper, FIntPoint(ScreenPosition.X, ScreenPosition.Y));

	Distance = USlFunctionLibrary::ConvertDepthToDistance(PlayerController->ViewportHelper, ScreenPosition, Depth);

	return ESlRetrieveResult::RR_RetrieveValid;
}

ESlRetrieveResult UZEDFunctionLibrary::GetNormalAtScreenPosition(AZEDPlayerController* PlayerController, const FVector2D& ScreenPosition, FVector& Normal)
{
	if (!PlayerController->ViewportHelper.IsInViewport(ScreenPosition.X, ScreenPosition.Y))
	{
		return ESlRetrieveResult::RR_LocationNotValid;
	}

	Normal = GSlCameraProxy->GetNormal(PlayerController->ViewportHelper, FIntPoint(ScreenPosition.X, ScreenPosition.Y));

	return (Normal == FVector::ZeroVector ? ESlRetrieveResult::RR_NormalNotValid : ESlRetrieveResult::RR_RetrieveValid);
}

ESlRetrieveResult UZEDFunctionLibrary::GetNormalAtWorldLocation(AZEDPlayerController* PlayerController, const FVector& Location, FVector& Normal)
{
	FVector2D ScreenPosition;
	UZEDFunctionLibrary::ProjectWorldToScreen(PlayerController, Location, ScreenPosition);
	if (!UZEDFunctionLibrary::IsVisible(PlayerController, Location, ScreenPosition))
	{
		return ESlRetrieveResult::RR_LocationNotValid;
	}

	Normal = GSlCameraProxy->GetNormal(PlayerController->ViewportHelper, FIntPoint(ScreenPosition.X, ScreenPosition.Y));

	return (Normal == FVector::ZeroVector ? ESlRetrieveResult::RR_NormalNotValid : ESlRetrieveResult::RR_RetrieveValid);
}

ESlRetrieveResult UZEDFunctionLibrary::GetDepthAndNormalAtScreenPosition(AZEDPlayerController* PlayerController, const FVector2D& ScreenPosition, float& Depth, float& Distance, FVector& Normal, FVector& WorldDirection)
{
	if (!PlayerController->ViewportHelper.IsInViewport(ScreenPosition.X, ScreenPosition.Y))
	{
		return ESlRetrieveResult::RR_LocationNotValid;
	}

	FVector WorldLocation;
	UZEDFunctionLibrary::DeprojectScreenToWorld(PlayerController, ScreenPosition, WorldLocation, WorldDirection);

	GSlCameraProxy->GetDepthAndNormal(PlayerController->ViewportHelper, FIntPoint(ScreenPosition.X, ScreenPosition.Y), Depth, Normal);

	Distance = USlFunctionLibrary::ConvertDepthToDistance(PlayerController->ViewportHelper, ScreenPosition, Depth);

	return (Normal == FVector::ZeroVector ? ESlRetrieveResult::RR_NormalNotValid : ESlRetrieveResult::RR_RetrieveValid);
}

ESlRetrieveResult UZEDFunctionLibrary::GetDepthAndNormalAtWorldLocation(AZEDPlayerController* PlayerController, const FVector& Location, float& Depth, float& Distance, FVector& Normal)
{
	FVector2D ScreenPosition;
	UZEDFunctionLibrary::ProjectWorldToScreen(PlayerController, Location, ScreenPosition);
	if (!UZEDFunctionLibrary::IsVisible(PlayerController, Location, ScreenPosition))
	{
		return ESlRetrieveResult::RR_LocationNotValid;
	}

	GSlCameraProxy->GetDepthAndNormal(PlayerController->ViewportHelper, FIntPoint(ScreenPosition.X, ScreenPosition.Y), Depth, Normal);

	Distance = USlFunctionLibrary::ConvertDepthToDistance(PlayerController->ViewportHelper, ScreenPosition, Depth);

	return (Normal == FVector::ZeroVector ? ESlRetrieveResult::RR_NormalNotValid : ESlRetrieveResult::RR_RetrieveValid);
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetDepthsAtScreenPositions(AZEDPlayerController* PlayerController, const TArray<FVector2D>& ScreenPositions, TArray<float>& Depths, TArray<float>& Distances, TArray<FVector>& WorldDirections)
{
	int ScreenPositionsNum = ScreenPositions.Num();

	TArray<ESlRetrieveResult> RetrieveResults;
	RetrieveResults.Reserve(ScreenPositionsNum);

	TArray<FIntPoint> VisibleScreenPositions;
	VisibleScreenPositions.Reserve(ScreenPositionsNum);

	Depths.Reserve(ScreenPositionsNum);
	Distances.Reserve(ScreenPositionsNum);
	WorldDirections.Init(FVector(0.0f, 0.0f, 0.0f), ScreenPositionsNum);

	for (auto ScreenPositionIt = ScreenPositions.CreateConstIterator(); ScreenPositionIt; ++ScreenPositionIt)
	{
		if (!PlayerController->ViewportHelper.IsInViewport(ScreenPositionIt->X, ScreenPositionIt->Y))
		{
			RetrieveResults.Add(ESlRetrieveResult::RR_LocationNotValid);
			continue;
		}

		FVector WorldLocation;
		UZEDFunctionLibrary::DeprojectScreenToWorld(PlayerController, *ScreenPositionIt, WorldLocation, WorldDirections[ScreenPositionIt.GetIndex()]);

		RetrieveResults.Add(ESlRetrieveResult::RR_RetrieveValid);
		VisibleScreenPositions.Add(FIntPoint(ScreenPositionIt->X, ScreenPositionIt->Y));
	}

	TArray<float> OutDepths = GSlCameraProxy->GetDepths(PlayerController->ViewportHelper, VisibleScreenPositions);

	int OutDepthsIndex = 0;
	for (int ScreenPositionIndex = 0; ScreenPositionIndex < ScreenPositionsNum; ++ScreenPositionIndex)
	{
		if (RetrieveResults[ScreenPositionIndex] == ESlRetrieveResult::RR_RetrieveValid)
		{
			Depths.Add(OutDepths[OutDepthsIndex]);
			Distances.Add(USlFunctionLibrary::ConvertDepthToDistance(PlayerController->ViewportHelper, VisibleScreenPositions[OutDepthsIndex], OutDepths[OutDepthsIndex]));

			++OutDepthsIndex;
		}
		else
		{
			Depths.Add(-1.0f);
			Distances.Add(0.0f);
		}
	}

	return RetrieveResults;
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetDepthsAtWorldLocations(AZEDPlayerController* PlayerController, const TArray<FVector>& Locations, TArray<float>& Depths, TArray<float>& Distances)
{
	int LocationsNum = Locations.Num();

	TArray<ESlRetrieveResult> RetrieveResults;
	RetrieveResults.Reserve(LocationsNum);

	TArray<FIntPoint> VisibleScreenPositions;
	VisibleScreenPositions.Reserve(LocationsNum);

	Depths.Reserve(LocationsNum);
	Distances.Reserve(LocationsNum);

	for (auto LocationsIt = Locations.CreateConstIterator(); LocationsIt; ++LocationsIt)
	{
		FVector2D ScreenPosition;
		UZEDFunctionLibrary::ProjectWorldToScreen(PlayerController, *LocationsIt, ScreenPosition);
		if (!UZEDFunctionLibrary::IsVisible(PlayerController, *LocationsIt, ScreenPosition))
		{
			RetrieveResults.Add(ESlRetrieveResult::RR_LocationNotValid);
			continue;
		}

		RetrieveResults.Add(ESlRetrieveResult::RR_RetrieveValid);
		VisibleScreenPositions.Add(FIntPoint(ScreenPosition.X, ScreenPosition.Y));
	}

	TArray<float> OutDepths = GSlCameraProxy->GetDepths(PlayerController->ViewportHelper, VisibleScreenPositions);

	int OutDepthsIndex = 0;
	for (int LocationsIndex = 0; LocationsIndex < LocationsNum; ++LocationsIndex)
	{
		if (RetrieveResults[LocationsIndex] == ESlRetrieveResult::RR_RetrieveValid)
		{
			Depths.Add(OutDepths[OutDepthsIndex]);
			Distances.Add(USlFunctionLibrary::ConvertDepthToDistance(PlayerController->ViewportHelper, VisibleScreenPositions[OutDepthsIndex], OutDepths[OutDepthsIndex]));

			++OutDepthsIndex;
		}
		else
		{
			Depths.Add(-1.0f);
			Distances.Add(0.0f);
		}
	}

	return RetrieveResults;
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetNormalsAtScreenPositions(AZEDPlayerController* PlayerController, const TArray<FVector2D>& ScreenPositions, TArray<FVector>& Normals)
{
	int ScreenPositionsNum = ScreenPositions.Num();

	TArray<ESlRetrieveResult> RetrieveResults;
	RetrieveResults.Reserve(ScreenPositionsNum);

	TArray<FIntPoint> VisibleScreenPositions;
	VisibleScreenPositions.Reserve(ScreenPositionsNum);

	Normals.Reserve(ScreenPositionsNum);

	for (auto ScreenPositionIt = ScreenPositions.CreateConstIterator(); ScreenPositionIt; ++ScreenPositionIt)
	{
		if (!PlayerController->ViewportHelper.IsInViewport(ScreenPositionIt->X, ScreenPositionIt->Y))
		{
			RetrieveResults.Add(ESlRetrieveResult::RR_LocationNotValid);
			continue;
		}

		RetrieveResults.Add(ESlRetrieveResult::RR_RetrieveValid);
		VisibleScreenPositions.Add(FIntPoint(ScreenPositionIt->X, ScreenPositionIt->Y));
	}

	TArray<FVector> OutNormals = GSlCameraProxy->GetNormals(PlayerController->ViewportHelper, VisibleScreenPositions);

	int OutNormalsIndex = 0;
	for (int ScreenPositionIndex = 0; ScreenPositionIndex < ScreenPositionsNum; ++ScreenPositionIndex)
	{
		if (RetrieveResults[ScreenPositionIndex] == ESlRetrieveResult::RR_RetrieveValid)
		{
			Normals.Add(OutNormals[OutNormalsIndex]);
			++OutNormalsIndex;

			if (Normals[ScreenPositionIndex] == FVector::ZeroVector)
			{
				RetrieveResults[ScreenPositionIndex] = ESlRetrieveResult::RR_NormalNotValid;
			}
		}
		else
		{
			Normals.Add(FVector::ZeroVector);
		}
	}

	return RetrieveResults;
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetNormalsAtWorldLocations(AZEDPlayerController* PlayerController, const TArray<FVector>& Locations, TArray<FVector>& Normals)
{
	int LocationsNum = Locations.Num();

	TArray<ESlRetrieveResult> RetrieveResults;
	RetrieveResults.Reserve(LocationsNum);

	TArray<FIntPoint> VisibleScreenPositions;
	VisibleScreenPositions.Reserve(LocationsNum);

	Normals.Reserve(LocationsNum);

	for (auto LocationsIt = Locations.CreateConstIterator(); LocationsIt; ++LocationsIt)
	{
		FVector2D ScreenPosition;
		UZEDFunctionLibrary::ProjectWorldToScreen(PlayerController, *LocationsIt, ScreenPosition);
		if (!UZEDFunctionLibrary::IsVisible(PlayerController, *LocationsIt, ScreenPosition))
		{
			RetrieveResults.Add(ESlRetrieveResult::RR_LocationNotValid);
			continue;
		}

		RetrieveResults.Add(ESlRetrieveResult::RR_RetrieveValid);
		VisibleScreenPositions.Add(FIntPoint(ScreenPosition.X, ScreenPosition.Y));
	}

	TArray<FVector> OutNormals = GSlCameraProxy->GetNormals(PlayerController->ViewportHelper, VisibleScreenPositions);

	int OutNormalsIndex = 0;
	for (int LocationsIndex = 0; LocationsIndex < LocationsNum; ++LocationsIndex)
	{
		if (RetrieveResults[LocationsIndex] == ESlRetrieveResult::RR_RetrieveValid)
		{
			Normals.Add(OutNormals[OutNormalsIndex]);
			++OutNormalsIndex;

			if (Normals[LocationsIndex] == FVector::ZeroVector)
			{
				RetrieveResults[LocationsIndex] = ESlRetrieveResult::RR_NormalNotValid;
			}
		}
		else
		{
			Normals.Add(FVector::ZeroVector);
		}
	}

	return RetrieveResults;
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetDepthsAndNormalsAtScreenPositions(AZEDPlayerController* PlayerController, const TArray<FVector2D>& ScreenPositions, TArray<float>& Depths, TArray<float>& Distances, TArray<FVector>& Normals, TArray<FVector>& WorldDirections)
{
	int ScreenPositionsNum = ScreenPositions.Num();

	TArray<ESlRetrieveResult> RetrieveResults;
	RetrieveResults.Reserve(ScreenPositionsNum);

	TArray<FIntPoint> VisibleScreenPositions;
	VisibleScreenPositions.Reserve(ScreenPositionsNum);

	Depths.Reserve(ScreenPositionsNum);
	Distances.Reserve(ScreenPositionsNum);
	Normals.Reserve(ScreenPositionsNum);

	WorldDirections.Init(FVector(0.0f, 0.0f, 0.0f), ScreenPositionsNum);

	for (auto ScreenPositionIt = ScreenPositions.CreateConstIterator(); ScreenPositionIt; ++ScreenPositionIt)
	{
		if (!PlayerController->ViewportHelper.IsInViewport(ScreenPositionIt->X, ScreenPositionIt->Y))
		{
			RetrieveResults.Add(ESlRetrieveResult::RR_LocationNotValid);
			continue;
		}

		FVector WorldLocation;
		UZEDFunctionLibrary::DeprojectScreenToWorld(PlayerController, *ScreenPositionIt, WorldLocation, WorldDirections[ScreenPositionIt.GetIndex()]);

		RetrieveResults.Add(ESlRetrieveResult::RR_RetrieveValid);
		VisibleScreenPositions.Add(FIntPoint(ScreenPositionIt->X, ScreenPositionIt->Y));
	}

	TArray<float> OutDepths;
	TArray<FVector> OutNormals;
	GSlCameraProxy->GetDepthsAndNormals(PlayerController->ViewportHelper, VisibleScreenPositions, OutDepths, OutNormals);

	int OutIndex = 0;
	for (int ScreenPositionIndex = 0; ScreenPositionIndex < ScreenPositionsNum; ++ScreenPositionIndex)
	{
		if (RetrieveResults[ScreenPositionIndex] == ESlRetrieveResult::RR_RetrieveValid)
		{
			Depths.Add(OutDepths[OutIndex]);
			Normals.Add(OutNormals[OutIndex]);
			Distances.Add(USlFunctionLibrary::ConvertDepthToDistance(PlayerController->ViewportHelper, VisibleScreenPositions[OutIndex], OutDepths[OutIndex]));

			++OutIndex;

			if (Normals[ScreenPositionIndex] == FVector::ZeroVector)
			{
				RetrieveResults[ScreenPositionIndex] = ESlRetrieveResult::RR_NormalNotValid;
			}
		}
		else
		{
			Depths.Add(-1.0f);
			Distances.Add(0.0f);
			Normals.Add(FVector::ZeroVector);
		}
	}

	return RetrieveResults;
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetDepthsAndNormalsAtImagePositions(AZEDPlayerController* PlayerController, const TArray<FVector2D>& ImagePositions,
	TArray<float>& Depths, TArray<float>& Distances, TArray<FVector>& Normals, TArray<FVector>& WorldDirections)
{
	int ScreenPositionsNum = ImagePositions.Num();

	TArray<ESlRetrieveResult> RetrieveResults;
	RetrieveResults.Reserve(ScreenPositionsNum);

	TArray<FIntPoint> VisibleScreenPositions;
	VisibleScreenPositions.Reserve(ScreenPositionsNum);

	Depths.Reserve(ScreenPositionsNum);
	Distances.Reserve(ScreenPositionsNum);
	Normals.Reserve(ScreenPositionsNum);

	WorldDirections.Init(FVector(0.0f, 0.0f, 0.0f), ScreenPositionsNum);

	for (auto ScreenPositionIt = ImagePositions.CreateConstIterator(); ScreenPositionIt; ++ScreenPositionIt)
	{
		if (!PlayerController->ViewportHelper.IsInViewport(ScreenPositionIt->X, ScreenPositionIt->Y))
		{
			RetrieveResults.Add(ESlRetrieveResult::RR_LocationNotValid);
			continue;
		}

		FVector WorldLocation;
		UZEDFunctionLibrary::DeprojectScreenToWorld(PlayerController, *ScreenPositionIt, WorldLocation, WorldDirections[ScreenPositionIt.GetIndex()]);

		RetrieveResults.Add(ESlRetrieveResult::RR_RetrieveValid);
		VisibleScreenPositions.Add(FIntPoint(ScreenPositionIt->X, ScreenPositionIt->Y));
	}

	TArray<float> OutDepths;
	TArray<FVector> OutNormals;
	GSlCameraProxy->GetDepthsAndNormals(PlayerController->ViewportHelper, VisibleScreenPositions, OutDepths, OutNormals);

	int OutIndex = 0;
	for (int ScreenPositionIndex = 0; ScreenPositionIndex < ScreenPositionsNum; ++ScreenPositionIndex)
	{
		if (RetrieveResults[ScreenPositionIndex] == ESlRetrieveResult::RR_RetrieveValid)
		{
			Depths.Add(OutDepths[OutIndex]);
			Normals.Add(OutNormals[OutIndex]);
			Distances.Add(USlFunctionLibrary::ConvertDepthToDistance(PlayerController->ViewportHelper, VisibleScreenPositions[OutIndex], OutDepths[OutIndex]));

			++OutIndex;

			if (Normals[ScreenPositionIndex] == FVector::ZeroVector)
			{
				RetrieveResults[ScreenPositionIndex] = ESlRetrieveResult::RR_NormalNotValid;
			}
		}
		else
		{
			Depths.Add(-1.0f);
			Distances.Add(0.0f);
			Normals.Add(FVector::ZeroVector);
		}
	}

	return RetrieveResults;
}


ESlRetrieveResult UZEDFunctionLibrary::GetPointCloudAtImagePositions(const TArray<FVector2D> ImagePositions, TArray<FVector>& Points, TArray<FColor>& Colors, 
	float MinConfidence, float MaxConfidence)
{
	//Points.Reserve(ImagePositions.Num());
	//Colors.Reserve(ImagePositions.Num());

	sl::Mat Mat;
	sl::Mat confidence;

	if (!GSlCameraProxy->RetrieveMeasure(Mat, ESlMeasure::M_XYZ_RGBA, ESlMemoryType::MT_CPU, 
		GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution))
	{
		return ESlRetrieveResult::RR_LocationNotValid;
	}
	if (!GSlCameraProxy->RetrieveMeasure(confidence, ESlMeasure::M_Confidence, ESlMemoryType::MT_CPU,
		GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution)) {
		UE_LOG(LogTemp, Error, TEXT("No Confidence Map available"));
		MinConfidence = 0; 
		MaxConfidence = 100;
	}

	int ResulstsNum = ImagePositions.Num();
	for (int ResultsIndex = 0; ResultsIndex < ResulstsNum; ++ResultsIndex)
	{
		float conf;
		confidence.getValue(int(ImagePositions[ResultsIndex].X), int(ImagePositions[ResultsIndex].Y), &conf);
		if (conf >= MinConfidence && conf <= MaxConfidence) {

			sl::float4 point;
			Mat.getValue(int(ImagePositions[ResultsIndex].X), int(ImagePositions[ResultsIndex].Y), &point);
			Points.Add(FVector(point.x, point.y, point.z));
			uint32 color;
			memcpy(&color, &point.w, sizeof color);
			Colors.Add(FColor(color));
		}
	}

	return ESlRetrieveResult::RR_RetrieveValid;
}


ESlRetrieveResult UZEDFunctionLibrary::GetPointCloudAndNormalsAtImagePositions(const TArray<FVector2D> ImagePositions, TArray<FVector>& Points, TArray<FVector>& Normals, TArray<FColor>& Colors,
	float MinConfidence, float MaxConfidence)
{
	//Points.Reserve(ImagePositions.Num());
	//Colors.Reserve(ImagePositions.Num());

	sl::Mat Mat;
	sl::Mat myNormals;
	sl::Mat confidence;

	if (!GSlCameraProxy->RetrieveMeasure(Mat, ESlMeasure::M_XYZ_RGBA, ESlMemoryType::MT_CPU,
		GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution))
	{
		return ESlRetrieveResult::RR_LocationNotValid;
	}
	if (!GSlCameraProxy->RetrieveMeasure(myNormals, ESlMeasure::M_Normals, ESlMemoryType::MT_CPU,
		GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution))
	{
		return ESlRetrieveResult::RR_NormalNotValid;
	}
	if (!GSlCameraProxy->RetrieveMeasure(confidence, ESlMeasure::M_Confidence, ESlMemoryType::MT_CPU,
		GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution)) {
		UE_LOG(LogTemp, Error, TEXT("No Confidence Map available"));
		MinConfidence = 0;
		MaxConfidence = 100;
	}

	int ResulstsNum = ImagePositions.Num();
	for (int ResultsIndex = 0; ResultsIndex < ResulstsNum; ++ResultsIndex)
	{
		float conf;
		confidence.getValue(int(ImagePositions[ResultsIndex].X), int(ImagePositions[ResultsIndex].Y), &conf);
		if (conf >= MinConfidence && conf <= MaxConfidence) {

			sl::float4 point;
			sl::float3 normal;
			Mat.getValue(int(ImagePositions[ResultsIndex].X), int(ImagePositions[ResultsIndex].Y), &point);
			myNormals.getValue(int(ImagePositions[ResultsIndex].X), int(ImagePositions[ResultsIndex].Y), &normal);
			Points.Add(FVector(point.x, point.y, point.z));
			uint32 color;
			memcpy(&color, &point.w, sizeof color);
			Colors.Add(FColor(color));
			Normals.Add(FVector(normal.x, normal.y, normal.z));
		}
	}

	return ESlRetrieveResult::RR_RetrieveValid;
}



TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetDepthsAndNormalsAtWorldLocations(AZEDPlayerController* PlayerController, const TArray<FVector>& Locations, TArray<float>& Depths, TArray<float>& Distances, TArray<FVector>& Normals)
{
	int LocationsNum = Locations.Num();

	TArray<ESlRetrieveResult> RetrieveResults;
	RetrieveResults.Reserve(LocationsNum);

	TArray<FIntPoint> VisibleScreenPositions;
	VisibleScreenPositions.Reserve(LocationsNum);

	Depths.Reserve(LocationsNum);
	Distances.Reserve(LocationsNum);
	Normals.Reserve(LocationsNum);

	for (auto LocationsIt = Locations.CreateConstIterator(); LocationsIt; ++LocationsIt)
	{
		FVector2D ScreenPosition;
		UZEDFunctionLibrary::ProjectWorldToScreen(PlayerController, *LocationsIt, ScreenPosition);
		if (!UZEDFunctionLibrary::IsVisible(PlayerController, *LocationsIt, ScreenPosition))
		{
			RetrieveResults.Add(ESlRetrieveResult::RR_LocationNotValid);
			continue;
		}

		RetrieveResults.Add(ESlRetrieveResult::RR_RetrieveValid);
		VisibleScreenPositions.Add(FIntPoint(ScreenPosition.X, ScreenPosition.Y));
	}

	TArray<float> OutDepths;
	TArray<FVector> OutNormals;
	GSlCameraProxy->GetDepthsAndNormals(PlayerController->ViewportHelper, VisibleScreenPositions, OutDepths, OutNormals);

	int OutIndex = 0;
	for (int LocationsIndex = 0; LocationsIndex < LocationsNum; ++LocationsIndex)
	{
		if (RetrieveResults[LocationsIndex] == ESlRetrieveResult::RR_RetrieveValid)
		{
			Depths.Add(OutDepths[OutIndex]);
			Normals.Add(OutNormals[OutIndex]);
			Distances.Add(USlFunctionLibrary::ConvertDepthToDistance(PlayerController->ViewportHelper, VisibleScreenPositions[OutIndex], OutDepths[OutIndex]));

			++OutIndex;

			if (Normals[LocationsIndex] == FVector::ZeroVector)
			{
				RetrieveResults[LocationsIndex] = ESlRetrieveResult::RR_NormalNotValid;
			}
		}
		else
		{
			Depths.Add(-1.0f);
			Distances.Add(0.0f);
			Normals.Add(FVector::ZeroVector);
		}
	}

	return RetrieveResults;
}

ESlRetrieveResult UZEDFunctionLibrary::GetPixelLocationAtScreenPosition(AZEDPlayerController* PlayerController, const FVector2D& ScreenPosition, FVector& PixelWorldLocation, float& Depth, float& Distance)
{
	ESlRetrieveResult Result = UZEDFunctionLibrary::GetDepthAtScreenPosition(PlayerController, ScreenPosition, Depth, Distance, PixelWorldLocation);

	if (Result == ESlRetrieveResult::RR_RetrieveValid)
	{
		PixelWorldLocation = PixelWorldLocation * Distance + GZedViewPointLocation;
	}

	return Result;
}

ESlRetrieveResult UZEDFunctionLibrary::GetPixelLocationAtWorldLocation(AZEDPlayerController* PlayerController, const FVector& Location, FVector& PixelWorldLocation, float& Depth, float& Distance)
{
	ESlRetrieveResult Result = UZEDFunctionLibrary::GetDepthAtWorldLocation(PlayerController, Location, Depth, Distance);

	if (Result == ESlRetrieveResult::RR_RetrieveValid)
	{
		PixelWorldLocation = (Location - GZedViewPointLocation).GetUnsafeNormal() * Distance + GZedViewPointLocation;
	}

	return Result;
}

ESlRetrieveResult UZEDFunctionLibrary::GetPixelLocationAndNormalAtScreenPosition(AZEDPlayerController* PlayerController, const FVector2D& ScreenPosition, FVector& PixelWorldLocation, float& Depth, float& Distance, FVector& Normal)
{
	ESlRetrieveResult Result = UZEDFunctionLibrary::GetDepthAndNormalAtScreenPosition(PlayerController, ScreenPosition, Depth, Distance, Normal, PixelWorldLocation);

	if (Result != ESlRetrieveResult::RR_LocationNotValid)
	{
		PixelWorldLocation = PixelWorldLocation * Distance + GZedViewPointLocation;
	}

	return Result;
}

ESlRetrieveResult UZEDFunctionLibrary::GetPixelLocationAndNormalAtWorldLocation(AZEDPlayerController* PlayerController, const FVector& Location, FVector& PixelWorldLocation, float& Depth, float& Distance, FVector& Normal)
{
	ESlRetrieveResult Result = UZEDFunctionLibrary::GetDepthAndNormalAtWorldLocation(PlayerController, Location, Depth, Distance, Normal);

	if (Result != ESlRetrieveResult::RR_LocationNotValid)
	{
		PixelWorldLocation = (Location - GZedViewPointLocation).GetUnsafeNormal() * Distance + GZedViewPointLocation;
	}

	return Result;
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetPixelsLocationAtScreenPositions(AZEDPlayerController* PlayerController, const TArray<FVector2D>& ScreenPositions, TArray<FVector>& PixelsWorldLocation, TArray<float>& Depths, TArray<float>& Distances)
{
	TArray<ESlRetrieveResult> RetrieveResults = UZEDFunctionLibrary::GetDepthsAtScreenPositions(PlayerController, ScreenPositions, Depths, Distances, PixelsWorldLocation);

	int ResulstsNum = RetrieveResults.Num();
	for (int ResultsIndex = 0; ResultsIndex < ResulstsNum; ++ResultsIndex)
	{
		if (RetrieveResults[ResultsIndex] == ESlRetrieveResult::RR_RetrieveValid)
		{
			PixelsWorldLocation[ResultsIndex] = PixelsWorldLocation[ResultsIndex] * Distances[ResultsIndex] + GZedViewPointLocation;
		}
	}

	return RetrieveResults;
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetPixelsLocationAtWorldLocations(AZEDPlayerController* PlayerController, const TArray<FVector>& Locations, TArray<FVector>& PixelsWorldLocation, TArray<float>& Depths, TArray<float>& Distances)
{
	PixelsWorldLocation.Reserve(Locations.Num());

	TArray<ESlRetrieveResult> RetrieveResults = UZEDFunctionLibrary::GetDepthsAtWorldLocations(PlayerController, Locations, Depths, Distances);

	int ResulstsNum = RetrieveResults.Num();
	for (int ResultsIndex = 0; ResultsIndex < ResulstsNum; ++ResultsIndex)
	{
		if (RetrieveResults[ResultsIndex] == ESlRetrieveResult::RR_RetrieveValid)
		{
			PixelsWorldLocation.Add((Locations[ResultsIndex] - GZedViewPointLocation).GetUnsafeNormal() * Distances[ResultsIndex] + GZedViewPointLocation);
		}
		else
		{
			PixelsWorldLocation.Add(FVector::ZeroVector);
		}
	}

	return RetrieveResults;
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetPixelsLocationAndNormalAtScreenPositions(AZEDPlayerController* PlayerController, const TArray<FVector2D>& ScreenPositions, TArray<FVector>& PixesWorldLocation, TArray<float>& Depths, TArray<float>& Distances, TArray<FVector>& Normals)
{
	TArray<ESlRetrieveResult> RetrieveResults = UZEDFunctionLibrary::GetDepthsAndNormalsAtScreenPositions(PlayerController, ScreenPositions, Depths, Distances, Normals, PixesWorldLocation);

	int ResulstsNum = RetrieveResults.Num();
	for (int ResultsIndex = 0; ResultsIndex < ResulstsNum; ++ResultsIndex)
	{
		if (RetrieveResults[ResultsIndex] != ESlRetrieveResult::RR_LocationNotValid)
		{
			PixesWorldLocation[ResultsIndex] = PixesWorldLocation[ResultsIndex] * Distances[ResultsIndex] + GZedViewPointLocation;
		}
	}

	return RetrieveResults;
}

TArray<ESlRetrieveResult> UZEDFunctionLibrary::GetPixelsLocationAndNormalAtWorldLocations(AZEDPlayerController* PlayerController, const TArray<FVector>& Locations, TArray<FVector>& PixelsWorldLocation, TArray<float>& Depths, TArray<float>& Distances, TArray<FVector>& Normals)
{
	PixelsWorldLocation.Reserve(Locations.Num());

	TArray<ESlRetrieveResult> RetrieveResults =  UZEDFunctionLibrary::GetDepthsAndNormalsAtWorldLocations(PlayerController, Locations, Depths, Distances, Normals);

	int ResulstsNum = RetrieveResults.Num();
	for (int ResultsIndex = 0; ResultsIndex < ResulstsNum; ++ResultsIndex)
	{
		if (RetrieveResults[ResultsIndex] != ESlRetrieveResult::RR_LocationNotValid)
		{
			PixelsWorldLocation.Add((Locations[ResultsIndex] - GZedViewPointLocation).GetUnsafeNormal() * Distances[ResultsIndex] + GZedViewPointLocation);
		}
		else
		{
			PixelsWorldLocation.Add(FVector::ZeroVector);
		}
	}

	return RetrieveResults;
}

bool UZEDFunctionLibrary::HitTestReal(AZEDPlayerController* PlayerController, const FVector& Location, float HitThreshold, bool bGetNormal, bool bHitIfBehind, FZEDHitResult& HitResult)
{
	FVector PixelWorldLocation;
	ESlRetrieveResult RetrieveResult;

	if (bGetNormal)
	{
		RetrieveResult = UZEDFunctionLibrary::GetPixelLocationAndNormalAtWorldLocation(PlayerController, Location, HitResult.Location, HitResult.Depth, HitResult.Distance, HitResult.Normal);
	}
	else
	{
		RetrieveResult = UZEDFunctionLibrary::GetPixelLocationAtWorldLocation(PlayerController, Location, HitResult.Location, HitResult.Depth, HitResult.Distance);
	}

	if(RetrieveResult == ESlRetrieveResult::RR_LocationNotValid)
	{
		HitResult.Reset();

		return false;
	}

	float PixelToLocationOffset = HitResult.Distance * HitResult.Distance - (Location - GZedViewPointLocation).SizeSquared();
	bool bIsBehind = bHitIfBehind && (PixelToLocationOffset < 0);
	bool bHasHit = bIsBehind || // behind real
				   FMath::Abs(PixelToLocationOffset) <= HitThreshold * HitThreshold;	// Inferior to threshold

	if (bHasHit)
	{
		HitResult.ImpactPoint = Location;
		HitResult.bIsVisible = true;
		HitResult.bIsBehind = bIsBehind;
		HitResult.bNormalValid = bGetNormal && RetrieveResult != ESlRetrieveResult::RR_NormalNotValid;

		return true;
	}

	HitResult.Reset();

	return false;
}

bool UZEDFunctionLibrary::MultipleHitTestsReal(AZEDPlayerController* PlayerController, const TArray<FVector>& Locations, const TArray<float>& HitThresholds, bool bGetNormal, bool bHitIfBehind, TArray<FZEDHitResult>& HitResults)
{
	bool bOneThreshold = HitThresholds.Num() == 1;
#if WITH_EDITOR
	if (HitThresholds.Num() != 1 && HitThresholds.Num() != Locations.Num())
	{
		SL_LOG_W(ZEDFunctionLibrary, "Thresholds size differ from Locations size : %d - %d", HitThresholds.Num(), Locations.Num());
		SL_LOG_W(ZEDFunctionLibrary, "Thresholds size must be 1 or equals to Locations size.");
		SL_LOG_W(ZEDFunctionLibrary, "Only the first threshold will be used.");

		bOneThreshold = true;
	}
#endif

	float ThresholdTemp = HitThresholds[0];
	uint32 LocationsNum = Locations.Num();
	bool bHasHit = false;

	TArray<ESlRetrieveResult> RetrieveResults;
	TArray<FVector> PixelWorldLocations;
	TArray<float> Depths;
	TArray<float> Distances;
	TArray<FVector> Normals;

	HitResults.Reserve(LocationsNum);
	PixelWorldLocations.Reserve(LocationsNum);
	Depths.Reserve(LocationsNum);

	if (bGetNormal)
	{
		Normals.Reserve(LocationsNum);

		RetrieveResults = UZEDFunctionLibrary::GetPixelsLocationAndNormalAtWorldLocations(PlayerController, Locations, PixelWorldLocations, Depths, Distances, Normals);
	}
	else
	{
		RetrieveResults = UZEDFunctionLibrary::GetPixelsLocationAtWorldLocations(PlayerController, Locations, PixelWorldLocations, Depths, Distances);
	}

	for (uint32 LocationsIndex = 0; LocationsIndex < LocationsNum; ++LocationsIndex)
	{
		ESlRetrieveResult& RetrieveResult = RetrieveResults[LocationsIndex];
		if (RetrieveResult == ESlRetrieveResult::RR_LocationNotValid)
		{
			HitResults.Add(FZEDHitResult());
			continue;
		}
		
		float HitThreshold = bOneThreshold ? ThresholdTemp : HitThresholds[LocationsIndex];

		FVector& PixelWorldLocation = PixelWorldLocations[LocationsIndex];
		const FVector& Location = Locations[LocationsIndex];
		float& Distance = Distances[LocationsIndex];

		float PixelToLocationOffset = Distance * Distance - (Location - GZedViewPointLocation).SizeSquared();
		bool bIsBehind = bHitIfBehind && (PixelToLocationOffset < 0);
		bool bCurrentHasHit = bIsBehind || // behind real
							  FMath::Abs(PixelToLocationOffset) <= HitThreshold * HitThreshold; // Inferior to threshold

		if (bCurrentHasHit)
		{
			FZEDHitResult HitResult;

			HitResult.Location = PixelWorldLocation;
			HitResult.ImpactPoint = Location;
			HitResult.Depth = Depths[LocationsIndex];
			HitResult.Distance = Distances[LocationsIndex];
			HitResult.bIsVisible = true;	
			HitResult.bIsBehind = bIsBehind;
			HitResult.bNormalValid = bGetNormal && RetrieveResult != ESlRetrieveResult::RR_NormalNotValid;
			if (HitResult.bNormalValid)
			{
				HitResult.Normal = Normals[LocationsIndex];
			}

			bHasHit = true;
			HitResults.Add(HitResult);
		}
	}

	return bHasHit;
}

bool UZEDFunctionLibrary::MultipleHitTestsRealOneResult(AZEDPlayerController* PlayerController, const TArray<FVector>& Locations, const TArray<float>& HitThresholds, bool bGetNormal, bool bHitIfBehind, FZEDHitResult& HitResult)
{
	bool bOneThreshold = HitThresholds.Num() == 1;
#if WITH_EDITOR
	if (HitThresholds.Num() != 1 && HitThresholds.Num() != Locations.Num())
	{
		SL_LOG_W(ZEDFunctionLibrary, "Thresholds size differ from Locations size : %d - %d", HitThresholds.Num(), Locations.Num());
		SL_LOG_W(ZEDFunctionLibrary, "Thresholds size must be 1 or equals to Locations size.");
		SL_LOG_W(ZEDFunctionLibrary, "Only the first threshold will be used.");

		bOneThreshold = true;
	}
#endif

	uint32 LocationsNum = Locations.Num();
	float ThresholdTemp = HitThresholds[0];

	TArray<ESlRetrieveResult> RetrieveResults;
	TArray<FVector> PixelWorldLocations;
	TArray<float> Depths;
	TArray<float> Distances;
	TArray<FVector> Normals;

	PixelWorldLocations.Reserve(LocationsNum);
	Depths.Reserve(LocationsNum);

	if (bGetNormal)
	{
		Normals.Reserve(LocationsNum);

		RetrieveResults = UZEDFunctionLibrary::GetPixelsLocationAndNormalAtWorldLocations(PlayerController, Locations, PixelWorldLocations, Depths, Distances, Normals);
	}
	else
	{
		RetrieveResults = UZEDFunctionLibrary::GetPixelsLocationAtWorldLocations(PlayerController, Locations, PixelWorldLocations, Depths, Distances);
	}

	for (uint32 LocationsIndex = 0; LocationsIndex < LocationsNum; ++LocationsIndex)
	{
		ESlRetrieveResult& RetrieveResult = RetrieveResults[LocationsIndex];
		if (RetrieveResult == ESlRetrieveResult::RR_LocationNotValid)
		{
			continue;
		}

		float HitThreshold = bOneThreshold ? ThresholdTemp : HitThresholds[LocationsIndex];

		FVector& PixelWorldLocation = PixelWorldLocations[LocationsIndex];
		const FVector& Location = Locations[LocationsIndex];
		float& Distance = Distances[LocationsIndex];

		float PixelToLocationOffset = Distance * Distance - (Location - GZedViewPointLocation).SizeSquared();
		bool bIsBehind = bHitIfBehind && (PixelToLocationOffset < 0);
		bool bCurrentHasHit = bIsBehind ||// behind real
							  FMath::Abs(PixelToLocationOffset) <= HitThreshold * HitThreshold;	// Inferior to threshold

		if (bCurrentHasHit)
		{
			HitResult.Location = PixelWorldLocation;
			HitResult.ImpactPoint = Location;
			HitResult.Depth = Depths[LocationsIndex];
			HitResult.Distance = Distances[LocationsIndex];
			HitResult.bIsVisible = true;
			HitResult.bIsBehind = bIsBehind;
			HitResult.bNormalValid = bGetNormal && RetrieveResult != ESlRetrieveResult::RR_NormalNotValid;
			if (HitResult.bNormalValid)
			{
				HitResult.Normal = Normals[LocationsIndex];
			}

			return true;
		}
	}

	HitResult.Reset();

	return false;
}

bool UZEDFunctionLibrary::GetFloorPlaneAtScreenPosition(AZEDPlayerController* PlayerController, const FVector2D& ScreenPosition, FVector& PlaneLocation)
{
	float Depth;
	FVector Normal;
	float Distance;
	ESlRetrieveResult RetrieveResult = UZEDFunctionLibrary::GetPixelLocationAndNormalAtScreenPosition(PlayerController, ScreenPosition, PlaneLocation, Depth, Distance, Normal);

	if (RetrieveResult != ESlRetrieveResult::RR_RetrieveValid)
	{
		return false;
	}

	return (FVector::DotProduct(Normal, FVector(0.0f, 0.0f, 1.0f)) >= PLANE_THRESHOLD);
}

bool UZEDFunctionLibrary::GetFloorPlaneAtWorldLocation(AZEDPlayerController* PlayerController, const FVector& WorldLocation, FVector& PlaneLocation)
{
	float Depth;
	FVector Normal;
	float Distance;
	ESlRetrieveResult RetrieveResult = UZEDFunctionLibrary::GetPixelLocationAndNormalAtWorldLocation(PlayerController, WorldLocation, PlaneLocation, Depth, Distance, Normal);

	if (RetrieveResult != ESlRetrieveResult::RR_RetrieveValid)
	{
		return false;
	}

	return (FVector::DotProduct(Normal, FVector(0.0f, 0.0f, 1.0f)) >= PLANE_THRESHOLD);
}

bool UZEDFunctionLibrary::DeprojectScreenToWorld(AZEDPlayerController const* Player, const FVector2D& ScreenPosition, FVector& WorldPosition, FVector& WorldDirection)
{
#if WITH_EDITOR
	UZEDLocalPlayer* LP = Cast<UZEDLocalPlayer>(Player->GetLocalPlayer());

	checkf(LP, TEXT("Local player must be a ZED local player"));
#else
	UZEDLocalPlayer* LP = static_cast<UZEDLocalPlayer*>(Player->GetLocalPlayer());
#endif

	checkf(LP->ViewportClient, TEXT("Local player must have ViewportClient"));

	// get the projection data
	FSceneViewProjectionData ProjectionData;
	if (LP->GetZEDProjectionData(LP->ViewportClient->Viewport, /*out*/ ProjectionData))
	{
		FMatrix const InvViewProjMatrix = ProjectionData.ComputeViewProjectionMatrix().InverseFast();
		FSceneView::DeprojectScreenToWorld(ScreenPosition, ProjectionData.GetConstrainedViewRect(), InvViewProjMatrix, /*out*/ WorldPosition, /*out*/ WorldDirection);
		return true;
	}

	// something went wrong, zero things and return false
	WorldPosition = FVector::ZeroVector;
	WorldDirection = FVector::ZeroVector;
	return false;
}

bool UZEDFunctionLibrary::ProjectWorldToScreen(AZEDPlayerController const* Player, const FVector& WorldPosition, FVector2D& ScreenPosition)
{
#if WITH_EDITOR
	 UZEDLocalPlayer* LP = Cast<UZEDLocalPlayer>(Player->GetLocalPlayer());

	checkf(LP, TEXT("Local player must be a ZED local player"));
#else
	UZEDLocalPlayer* LP = static_cast<UZEDLocalPlayer*>(Player->GetLocalPlayer());
#endif

	checkf(LP->ViewportClient, TEXT("Local player must have ViewportClient"));

	// get the projection data
	FSceneViewProjectionData ProjectionData;
	if (LP->GetZEDProjectionData(LP->ViewportClient->Viewport, /*out*/ ProjectionData))
	{
		FMatrix const ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
		const bool bResult = FSceneView::ProjectWorldToScreen(WorldPosition, ProjectionData.GetConstrainedViewRect(), ViewProjectionMatrix, ScreenPosition);

		return bResult;
	}

	ScreenPosition = FVector2D::ZeroVector;
	return false;
}

void UZEDFunctionLibrary::LatencyCorrectorAddOffset(const int offset)
{
	sl::mr::latencyCorrectorAdjOffset(offset);
}

//#ifdef HAS_PCL
ESlRetrieveResult UZEDFunctionLibrary::GetPointCloudAtRoi(FBox2D ROI, pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr & PointcloudOut, float MinConfidence, float MaxConfidence)
{

	sl::Mat Mat;
	sl::Mat Normals;
	sl::Mat confidence;

	if (!GSlCameraProxy->RetrieveMeasure(Mat, ESlMeasure::M_XYZ_RGBA, ESlMemoryType::MT_CPU,
		GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution))
	{
		return ESlRetrieveResult::RR_LocationNotValid;
	}
	if (!GSlCameraProxy->RetrieveMeasure(Normals, ESlMeasure::M_Normals, ESlMemoryType::MT_CPU,
		GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution))
	{
		return ESlRetrieveResult::RR_NormalNotValid;
	}
	if (!GSlCameraProxy->RetrieveMeasure(confidence, ESlMeasure::M_Confidence, ESlMemoryType::MT_CPU,
		GSlCameraProxy->CameraInformation.CalibrationParameters.LeftCameraParameters.Resolution)) {
		UE_LOG(LogTemp, Error, TEXT("No Confidence Map available"));
		MinConfidence = 0;
		MaxConfidence = 100;
	}

	for (int y = ROI.Min.Y; y < ROI.Max.Y; ++y)
	{
		for (int x = ROI.Min.X; x < ROI.Max.X; ++x) {

			float conf;
			confidence.getValue(x, y, &conf);
			if (conf >= MinConfidence && conf <= MaxConfidence) {

				sl::float4 point;
				sl::float3 normal;
				Mat.getValue(x, y, &point);
				Normals.getValue(x, y, &normal);
				pcl::PointXYZRGBNormal pclPoint;
				pclPoint.x = point.x;
				pclPoint.y = point.y;
				pclPoint.z = point.z;
				unsigned char color[sizeof(float)];
				memcpy(color, &point[3], sizeof(float));
				pclPoint.r = color[0];
				pclPoint.g = color[1];
				pclPoint.b = color[2];
				pclPoint.normal_x = normal.x;
				pclPoint.normal_y = normal.y;
				pclPoint.normal_z = normal.z;
				PointcloudOut->push_back(pclPoint);
			}
		}
	}

	return ESlRetrieveResult::RR_RetrieveValid;

}
//#endif