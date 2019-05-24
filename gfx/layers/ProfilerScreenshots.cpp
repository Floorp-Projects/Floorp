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
#ifdef MOZ_GECKO_PROFILER
#  include "ProfilerMarkerPayload.h"
#endif

using namespace mozilla;
using namespace mozilla::layers;

ProfilerScreenshots::ProfilerScreenshots()
    : mMutex("ProfilerScreenshots::mMutex"), mLiveSurfaceCount(0) {}

ProfilerScreenshots::~ProfilerScreenshots() {
  if (mThread) {
    // Shut down mThread. Do the actual shutdown on the main thread, because it
    // has to happen on an XPCOM thread, and ~ProfilerScreenshots() may not be
    // running on an XPCOM thread - it usually runs on the Compositor thread
    // which is a chromium thread.
    SystemGroup::Dispatch(
        TaskCategory::Other,
        NewRunnableMethod("ProfilerScreenshots::~ProfilerScreenshots", mThread,
                          &nsIThread::AsyncShutdown));
    mThread = nullptr;
  }
}

/* static */
bool ProfilerScreenshots::IsEnabled() {
#ifdef MOZ_GECKO_PROFILER
  return profiler_feature_active(ProfilerFeature::Screenshots);
#else
  return false;
#endif
}

void ProfilerScreenshots::SubmitScreenshot(
    uintptr_t aWindowIdentifier, const gfx::IntSize& aOriginalSize,
    const IntSize& aScaledSize, const TimeStamp& aTimeStamp,
    const std::function<bool(DataSourceSurface*)>& aPopulateSurface) {
#ifdef MOZ_GECKO_PROFILER
  RefPtr<DataSourceSurface> backingSurface = TakeNextSurface();
  if (!backingSurface) {
    return;
  }

  MOZ_RELEASE_ASSERT(aScaledSize <= backingSurface->GetSize());

  bool succeeded = aPopulateSurface(backingSurface);

  if (!succeeded) {
    PROFILER_ADD_MARKER(
        "NoCompositorScreenshot because aPopulateSurface callback failed",
        GRAPHICS);
    ReturnSurface(backingSurface);
    return;
  }

  if (!mThread) {
    nsresult rv = NS_NewNamedThread("ProfScreenshot", getter_AddRefs(mThread));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      PROFILER_ADD_MARKER(
          "NoCompositorScreenshot because ProfilerScreenshots thread creation "
          "failed",
          DOM);
      ReturnSurface(backingSurface);
      return;
    }
  }

  int sourceThread = profiler_current_thread_id();
  uintptr_t windowIdentifier = aWindowIdentifier;
  IntSize originalSize = aOriginalSize;
  IntSize scaledSize = aScaledSize;
  TimeStamp timeStamp = aTimeStamp;

  RefPtr<ProfilerScreenshots> self = this;

  mThread->Dispatch(NS_NewRunnableFunction(
      "ProfilerScreenshots::SubmitScreenshot",
      [self{std::move(self)}, backingSurface, sourceThread, windowIdentifier,
       originalSize, scaledSize, timeStamp]() {
        // Create a new surface that wraps backingSurface's data but has the
        // correct size.
        {
          DataSourceSurface::ScopedMap scopedMap(backingSurface,
                                                 DataSourceSurface::READ);
          RefPtr<DataSourceSurface> surf =
              Factory::CreateWrappingDataSourceSurface(
                  scopedMap.GetData(), scopedMap.GetStride(), scaledSize,
                  SurfaceFormat::B8G8R8A8);

          // Encode surf to a JPEG data URL.
          nsCString dataURL;
          nsresult rv = gfxUtils::EncodeSourceSurface(
              surf, ImageType::JPEG, NS_LITERAL_STRING("quality=85"),
              gfxUtils::eDataURIEncode, nullptr, &dataURL);
          if (NS_SUCCEEDED(rv)) {
            // Add a marker with the data URL.
            profiler_add_marker_for_thread(
                sourceThread, JS::ProfilingCategoryPair::GRAPHICS,
                "CompositorScreenshot",
                MakeUnique<ScreenshotPayload>(timeStamp, std::move(dataURL),
                                              originalSize, windowIdentifier));
          }
        }

        // Return backingSurface back to the surface pool.
        self->ReturnSurface(backingSurface);
      }));
#endif
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
