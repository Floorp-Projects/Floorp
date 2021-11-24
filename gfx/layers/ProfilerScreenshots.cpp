/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ProfilerScreenshots.h"

#include "mozilla/TimeStamp.h"

#include "GeckoProfiler.h"
#include "gfxUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

struct ScreenshotMarker {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("CompositorScreenshot");
  }
  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
      const mozilla::ProfilerString8View& aScreenshotDataURL,
      const mozilla::gfx::IntSize& aWindowSize, uint32_t aWindowIdentifier) {
    if (aScreenshotDataURL.Length() != 0) {
      aWriter.UniqueStringProperty("url", aScreenshotDataURL);
    }

    aWriter.IntProperty("windowID", aWindowIdentifier);

    if (!aWindowSize.IsEmpty()) {
      aWriter.DoubleProperty("windowWidth", aWindowSize.width);
      aWriter.DoubleProperty("windowHeight", aWindowSize.height);
    }
  }
  static mozilla::MarkerSchema MarkerTypeDisplay() {
    return mozilla::MarkerSchema::SpecialFrontendLocation{};
  }
};

uint32_t ProfilerScreenshots::sWindowCounter = 0;

ProfilerScreenshots::ProfilerScreenshots()
    : mMutex("ProfilerScreenshots::mMutex"),
      mLiveSurfaceCount(0),
      mWindowIdentifier(++sWindowCounter) {}

ProfilerScreenshots::~ProfilerScreenshots() {
  if (mWindowIdentifier) {
    profiler_add_marker("CompositorScreenshotWindowDestroyed",
                        geckoprofiler::category::GRAPHICS,
                        MarkerThreadId::MainThread(), ScreenshotMarker{},
                        /* aScreenshotDataURL */ "", mozilla::gfx::IntSize{},
                        mWindowIdentifier);
  }
}

/* static */
bool ProfilerScreenshots::IsEnabled() {
  return profiler_feature_active(ProfilerFeature::Screenshots);
}

void ProfilerScreenshots::SubmitScreenshot(
    const gfx::IntSize& aOriginalSize, const IntSize& aScaledSize,
    const TimeStamp& aTimeStamp,
    const std::function<bool(DataSourceSurface*)>& aPopulateSurface) {
  RefPtr<DataSourceSurface> backingSurface = TakeNextSurface();
  if (!backingSurface) {
    return;
  }

  MOZ_RELEASE_ASSERT(aScaledSize <= backingSurface->GetSize());

  bool succeeded = aPopulateSurface(backingSurface);

  if (!succeeded) {
    PROFILER_MARKER_UNTYPED(
        "NoCompositorScreenshot because aPopulateSurface callback failed",
        GRAPHICS);
    ReturnSurface(backingSurface);
    return;
  }

  NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      "ProfilerScreenshots::SubmitScreenshot",
      [self = RefPtr<ProfilerScreenshots>{this},
       backingSurface = std::move(backingSurface),
       windowIdentifier = mWindowIdentifier, originalSize = aOriginalSize,
       scaledSize = aScaledSize, timeStamp = aTimeStamp]() {
        // Create a new surface that wraps backingSurface's data but has the
        // correct size.
        DataSourceSurface::ScopedMap scopedMap(backingSurface,
                                               DataSourceSurface::READ);
        RefPtr<DataSourceSurface> surf =
            Factory::CreateWrappingDataSourceSurface(
                scopedMap.GetData(), scopedMap.GetStride(), scaledSize,
                SurfaceFormat::B8G8R8A8);

        // Encode surf to a JPEG data URL.
        nsCString dataURL;
        nsresult rv = gfxUtils::EncodeSourceSurface(
            surf, ImageType::JPEG, u"quality=85"_ns, gfxUtils::eDataURIEncode,
            nullptr, &dataURL);
        if (NS_SUCCEEDED(rv)) {
          // Add a marker with the data URL.
          profiler_add_marker(
              "CompositorScreenshot", geckoprofiler::category::GRAPHICS,
              {MarkerThreadId::MainThread(),
               MarkerTiming::InstantAt(timeStamp)},
              ScreenshotMarker{}, dataURL, originalSize, windowIdentifier);
        }

        // Return backingSurface back to the surface pool.
        self->ReturnSurface(backingSurface);
      }));
}

already_AddRefed<DataSourceSurface> ProfilerScreenshots::TakeNextSurface() {
  MutexAutoLock mon(mMutex);
  if (!mAvailableSurfaces.IsEmpty()) {
    RefPtr<DataSourceSurface> surf = mAvailableSurfaces[0];
    mAvailableSurfaces.RemoveElementAt(0);
    return surf.forget();
  }
  if (mLiveSurfaceCount >= 8) {
    NS_WARNING(
        "already 8 surfaces in flight, skipping capture for this composite");
    return nullptr;
  }
  mLiveSurfaceCount++;
  return Factory::CreateDataSourceSurface(ScreenshotSize(),
                                          SurfaceFormat::B8G8R8A8);
}

void ProfilerScreenshots::ReturnSurface(DataSourceSurface* aSurface) {
  MutexAutoLock mon(this->mMutex);
  mAvailableSurfaces.AppendElement(aSurface);
}
