/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineTabVideoSource.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/layers/SharedRGBImage.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/PresShell.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsGlobalWindow.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "gfxContext.h"
#include "gfx2DGlue.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "nsIInterfaceRequestorUtils.h"
#include "VideoUtils.h"
#include "nsServiceManagerUtils.h"
#include "MediaTrackConstraints.h"
#include "Tracing.h"

namespace mozilla {

using namespace mozilla::gfx;

MediaEngineTabVideoSource::MediaEngineTabVideoSource()
    : mSettings(MakeAndAddRef<media::Refcountable<MediaTrackSettings>>()) {}

nsresult MediaEngineTabVideoSource::StartRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mVideoSource->mWindowId != -1) {
    nsGlobalWindowOuter* globalWindow =
        nsGlobalWindowOuter::GetOuterWindowWithId(mVideoSource->mWindowId);
    if (!globalWindow) {
      // We can't access the window, just send a blacked out screen.
      mVideoSource->mWindow = nullptr;
      mVideoSource->mBlackedoutWindow = true;
    } else {
      mVideoSource->mWindow = globalWindow;
      mVideoSource->mBlackedoutWindow = false;
    }
  }
  if (!mVideoSource->mWindow && !mVideoSource->mBlackedoutWindow) {
    nsresult rv;
    mVideoSource->mTabSource =
        do_GetService(NS_TABSOURCESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIDOMWindowProxy> win;
    rv = mVideoSource->mTabSource->GetTabToStream(getter_AddRefs(win));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!win) {
      return NS_OK;
    }

    mVideoSource->mWindow = nsPIDOMWindowOuter::From(win);
    MOZ_ASSERT(mVideoSource->mWindow);
  }
  mVideoSource->mTimer = NS_NewTimer();
  mVideoSource->mTrackMain = mTrack;
  mVideoSource->mPrincipalHandleMain = mPrincipal;
  mVideoSource->Draw();
  mVideoSource->mTimer->InitWithNamedFuncCallback(
      [](nsITimer* aTimer, void* aClosure) mutable {
        auto source = static_cast<MediaEngineTabVideoSource*>(aClosure);
        source->Draw();
      },
      mVideoSource, mVideoSource->mTimePerFrame, nsITimer::TYPE_REPEATING_SLACK,
      "MediaEngineTabVideoSource DrawTimer");
  if (mVideoSource->mTabSource) {
    mVideoSource->mTabSource->NotifyStreamStart(mVideoSource->mWindow);
  }
  return NS_OK;
}

nsresult MediaEngineTabVideoSource::StopRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mVideoSource->mTimer) {
    mVideoSource->mTimer->Cancel();
    mVideoSource->mTimer = nullptr;
  }
  if (mVideoSource->mTabSource) {
    mVideoSource->mTabSource->NotifyStreamStop(mVideoSource->mWindow);
  }
  return NS_OK;
}

nsresult MediaEngineTabVideoSource::DestroyRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  mVideoSource->mWindow = nullptr;
  mVideoSource->mTabSource = nullptr;

  if (mVideoSource->mTrackMain) {
    mVideoSource->mTrackMain->End();
  }
  mVideoSource->mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
  mVideoSource->mTrackMain = nullptr;

  return NS_OK;
}

nsString MediaEngineTabVideoSource::GetName() const {
  return u"&getUserMedia.videoSource.tabShare;"_ns;
}

nsCString MediaEngineTabVideoSource::GetUUID() const { return "tab"_ns; }

nsString MediaEngineTabVideoSource::GetGroupId() const {
  return u"&getUserMedia.videoSource.tabShareGroup;"_ns;
}

#define DEFAULT_TABSHARE_VIDEO_MAX_WIDTH 4096
#define DEFAULT_TABSHARE_VIDEO_MAX_HEIGHT 4096
#define DEFAULT_TABSHARE_VIDEO_FRAMERATE 30

nsresult MediaEngineTabVideoSource::Allocate(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, uint64_t aWindowID,
    const char** aOutBadConstraint) {
  AssertIsOnOwningThread();

  // windowId is not a proper constraint, so just read it.
  // It has no well-defined behavior in advanced, so ignore it there.

  int64_t windowId = aConstraints.mBrowserWindow.WasPassed()
                         ? aConstraints.mBrowserWindow.Value()
                         : -1;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MediaEngineTabVideoSource::Allocate window id main thread setter",
      [self = RefPtr<MediaEngineTabVideoSource>(this), this, windowId] {
        mWindowId = windowId;
      }));
  mState = kAllocated;

  return Reconfigure(aConstraints, aPrefs, aOutBadConstraint);
}

nsresult MediaEngineTabVideoSource::Reconfigure(
    const dom::MediaTrackConstraints& aConstraints,
    const mozilla::MediaEnginePrefs& aPrefs, const char** aOutBadConstraint) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState != kReleased);

  // scrollWithPage is not proper a constraint, so just read it.
  // It has no well-defined behavior in advanced, so ignore it there.

  const bool scrollWithPage = aConstraints.mScrollWithPage.WasPassed()
                                  ? aConstraints.mScrollWithPage.Value()
                                  : false;

  FlattenedConstraints c(aConstraints);

  const int32_t bufWidthMax = c.mWidth.Get(DEFAULT_TABSHARE_VIDEO_MAX_WIDTH);
  const int32_t bufHeightMax = c.mHeight.Get(DEFAULT_TABSHARE_VIDEO_MAX_HEIGHT);
  const double frameRate = c.mFrameRate.Get(DEFAULT_TABSHARE_VIDEO_FRAMERATE);
  const int32_t timePerFrame =
      std::max(10, int(1000.0 / (frameRate > 0 ? frameRate : 1)));

  Maybe<int32_t> viewportOffsetX;
  Maybe<int32_t> viewportOffsetY;
  Maybe<int32_t> viewportWidth;
  Maybe<int32_t> viewportHeight;
  if (!scrollWithPage) {
    viewportOffsetX = Some(c.mViewportOffsetX.Get(0));
    viewportOffsetY = Some(c.mViewportOffsetY.Get(0));
    viewportWidth = Some(c.mViewportWidth.Get(INT32_MAX));
    viewportHeight = Some(c.mViewportHeight.Get(INT32_MAX));
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MediaEngineTabVideoSource::Reconfigure main thread setter",
      [self = RefPtr<MediaEngineTabVideoSource>(this), this, scrollWithPage,
       bufWidthMax, bufHeightMax, frameRate, timePerFrame, viewportOffsetX,
       viewportOffsetY, viewportWidth, viewportHeight]() {
        mScrollWithPage = scrollWithPage;
        mBufWidthMax = bufWidthMax;
        mBufHeightMax = bufHeightMax;
        mTimePerFrame = timePerFrame;
        *mSettings = MediaTrackSettings();
        mSettings->mScrollWithPage.Construct(scrollWithPage);
        mSettings->mWidth.Construct(bufWidthMax);
        mSettings->mHeight.Construct(bufHeightMax);
        mSettings->mFrameRate.Construct(frameRate);
        if (viewportOffsetX.isSome()) {
          mSettings->mViewportOffsetX.Construct(*viewportOffsetX);
          mViewportOffsetX = *viewportOffsetX;
        }
        if (viewportOffsetY.isSome()) {
          mSettings->mViewportOffsetY.Construct(*viewportOffsetY);
          mViewportOffsetY = *viewportOffsetY;
        }
        if (viewportWidth.isSome()) {
          mSettings->mViewportWidth.Construct(*viewportWidth);
          mViewportWidth = *viewportWidth;
        }
        if (viewportHeight.isSome()) {
          mSettings->mViewportHeight.Construct(*viewportHeight);
          mViewportHeight = *viewportHeight;
        }
        if (mWindowId != -1) {
          mSettings->mBrowserWindow.Construct(mWindowId);
        }
      }));
  return NS_OK;
}

nsresult MediaEngineTabVideoSource::Deallocate() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == kAllocated || mState == kStopped);

  NS_DispatchToMainThread(do_AddRef(new DestroyRunnable(this)));
  mState = kReleased;

  return NS_OK;
}

void MediaEngineTabVideoSource::SetTrack(
    const RefPtr<SourceMediaTrack>& aTrack,
    const mozilla::PrincipalHandle& aPrincipal) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == kAllocated);

  MOZ_ASSERT(!mTrack);
  MOZ_ASSERT(aTrack);
  mTrack = aTrack;
  mPrincipalHandle = aPrincipal;
}

nsresult MediaEngineTabVideoSource::Start() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == kAllocated);

  NS_DispatchToMainThread(new StartRunnable(this, mTrack, mPrincipalHandle));
  mState = kStarted;

  return NS_OK;
}

void MediaEngineTabVideoSource::Draw() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWindow && !mBlackedoutWindow) {
    return;
  }

  if (!mTrackMain || mTrackMain->IsDestroyed()) {
    // The track is already gone or destroyed by MediaManager. This can happen
    // because stopping the draw timer is async.
    return;
  }

  if (mWindow) {
    if (mScrollWithPage || mViewportWidth == INT32_MAX) {
      mWindow->GetInnerWidth(&mViewportWidth);
    }
    if (mScrollWithPage || mViewportHeight == INT32_MAX) {
      mWindow->GetInnerHeight(&mViewportHeight);
    }
    if (!mViewportWidth || !mViewportHeight) {
      return;
    }
  } else {
    mViewportWidth = 640;
    mViewportHeight = 480;
  }

  IntSize size;
  {
    float pixelRatio;
    if (mWindow) {
      pixelRatio = mWindow->GetDevicePixelRatio(dom::CallerType::System);
    } else {
      pixelRatio = 1.0f;
    }
    const int32_t deviceWidth = (int32_t)(pixelRatio * mViewportWidth);
    const int32_t deviceHeight = (int32_t)(pixelRatio * mViewportHeight);

    if ((deviceWidth <= mBufWidthMax) && (deviceHeight <= mBufHeightMax)) {
      size = IntSize(deviceWidth, deviceHeight);
    } else {
      const float scaleWidth = (float)mBufWidthMax / (float)deviceWidth;
      const float scaleHeight = (float)mBufHeightMax / (float)deviceHeight;
      const float scale = scaleWidth < scaleHeight ? scaleWidth : scaleHeight;

      size = IntSize((int)(scale * deviceWidth), (int)(scale * deviceHeight));
    }
  }

  RefPtr<PresShell> presShell;
  if (mWindow) {
    nsIDocShell* docshell = mWindow->GetDocShell();
    if (docshell) {
      presShell = docshell->GetPresShell();
    }
    if (!presShell) {
      return;
    }
  }

  if (!mImageContainer) {
    mImageContainer = layers::LayerManager::CreateImageContainer(
        layers::ImageContainer::ASYNCHRONOUS);
  }

  RefPtr<layers::SharedRGBImage> rgbImage =
      mImageContainer->CreateSharedRGBImage();
  if (!rgbImage) {
    NS_WARNING("Failed to create SharedRGBImage");
    return;
  }
  if (!rgbImage->Allocate(size, SurfaceFormat::B8G8R8X8)) {
    NS_WARNING("Failed to allocate a shared image");
    return;
  }

  RefPtr<layers::TextureClient> texture =
      rgbImage->GetTextureClient(/* aKnowsCompositor */ nullptr);
  if (!texture) {
    NS_WARNING("Failed to allocate TextureClient");
    return;
  }

  {
    layers::TextureClientAutoLock autoLock(texture,
                                           layers::OpenMode::OPEN_WRITE_ONLY);
    if (!autoLock.Succeeded()) {
      NS_WARNING("Failed to lock TextureClient");
      return;
    }

    RefPtr<gfx::DrawTarget> dt = texture->BorrowDrawTarget();
    if (!dt || !dt->IsValid()) {
      NS_WARNING("Failed to borrow DrawTarget");
      return;
    }

    if (mWindow) {
      RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt);
      MOZ_ASSERT(context);  // already checked the draw target above
      context->SetMatrix(context->CurrentMatrix().PreScale(
          (((float)size.width) / mViewportWidth),
          (((float)size.height) / mViewportHeight)));

      nscolor bgColor = NS_RGB(255, 255, 255);
      RenderDocumentFlags renderDocFlags =
          mScrollWithPage ? RenderDocumentFlags::None
                          : (RenderDocumentFlags::IgnoreViewportScrolling |
                             RenderDocumentFlags::DocumentRelative);
      nsRect r(nsPresContext::CSSPixelsToAppUnits((float)mViewportOffsetX),
               nsPresContext::CSSPixelsToAppUnits((float)mViewportOffsetY),
               nsPresContext::CSSPixelsToAppUnits((float)mViewportWidth),
               nsPresContext::CSSPixelsToAppUnits((float)mViewportHeight));
      NS_ENSURE_SUCCESS_VOID(
          presShell->RenderDocument(r, renderDocFlags, bgColor, context));
    } else {
      dt->ClearRect(Rect(0, 0, size.width, size.height));
    }
  }

  VideoSegment segment;
  segment.AppendFrame(do_AddRef(rgbImage), size, mPrincipalHandle);
  mTrackMain->AppendData(&segment);
}

nsresult MediaEngineTabVideoSource::FocusOnSelectedSource() {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult MediaEngineTabVideoSource::Stop() {
  AssertIsOnOwningThread();

  if (mState == kStopped || mState == kAllocated) {
    return NS_OK;
  }

  MOZ_ASSERT(mState == kStarted);

  // If mBlackedoutWindow is true, we may be running
  // despite mWindow == nullptr.
  if (!mWindow && !mBlackedoutWindow) {
    return NS_OK;
  }

  NS_DispatchToMainThread(new StopRunnable(this));
  mState = kStopped;
  return NS_OK;
}

void MediaEngineTabVideoSource::GetSettings(
    MediaTrackSettings& aOutSettings) const {
  MOZ_ASSERT(NS_IsMainThread());
  aOutSettings = *mSettings;
}

}  // namespace mozilla
