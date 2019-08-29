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
#include "nsITabSource.h"
#include "VideoUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "MediaTrackConstraints.h"
#include "Tracing.h"

namespace mozilla {

using namespace mozilla::gfx;

MediaEngineTabVideoSource::MediaEngineTabVideoSource()
    : mSettings(MakeAndAddRef<media::Refcountable<MediaTrackSettings>>()) {}

nsresult MediaEngineTabVideoSource::StartRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());
  mVideoSource->mStreamMain = mStream;
  mVideoSource->mTrackIDMain = mTrackID;
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
  mVideoSource->mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
  mVideoSource->mTrackIDMain = TRACK_NONE;
  mVideoSource->mStream = nullptr;
  return NS_OK;
}

nsresult MediaEngineTabVideoSource::InitRunnable::Run() {
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
  nsCOMPtr<nsIRunnable> start(
      new StartRunnable(mVideoSource, mStream, mTrackID, mPrincipal));
  start->Run();
  return NS_OK;
}

nsresult MediaEngineTabVideoSource::DestroyRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  mVideoSource->mWindow = nullptr;
  mVideoSource->mTabSource = nullptr;

  return NS_OK;
}

nsString MediaEngineTabVideoSource::GetName() const {
  return NS_LITERAL_STRING(u"&getUserMedia.videoSource.tabShare;");
}

nsCString MediaEngineTabVideoSource::GetUUID() const {
  return NS_LITERAL_CSTRING("tab");
}

nsString MediaEngineTabVideoSource::GetGroupId() const {
  return NS_LITERAL_STRING(u"&getUserMedia.videoSource.tabShareGroup;");
}

#define DEFAULT_TABSHARE_VIDEO_MAX_WIDTH 4096
#define DEFAULT_TABSHARE_VIDEO_MAX_HEIGHT 4096
#define DEFAULT_TABSHARE_VIDEO_FRAMERATE 30

nsresult MediaEngineTabVideoSource::Allocate(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const char** aOutBadConstraint) {
  AssertIsOnOwningThread();

  // windowId is not a proper constraint, so just read it.
  // It has no well-defined behavior in advanced, so ignore it there.

  mWindowId = aConstraints.mBrowserWindow.WasPassed()
                  ? aConstraints.mBrowserWindow.Value()
                  : -1;
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
       viewportOffsetY, viewportWidth, viewportHeight, windowId = mWindowId]() {
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
        if (windowId != -1) {
          mSettings->mBrowserWindow.Construct(windowId);
        }
      }));
  return NS_OK;
}

nsresult MediaEngineTabVideoSource::Deallocate() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == kAllocated || mState == kStopped);

  if (mStream && IsTrackIDExplicit(mTrackID)) {
    mStream->EndTrack(mTrackID);
  }

  NS_DispatchToMainThread(do_AddRef(new DestroyRunnable(this)));
  mState = kReleased;

  return NS_OK;
}

void MediaEngineTabVideoSource::SetTrack(
    const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
    const mozilla::PrincipalHandle& aPrincipal) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == kAllocated);

  MOZ_ASSERT(!mStream);
  MOZ_ASSERT(mTrackID == TRACK_NONE);
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(IsTrackIDExplicit(aTrackID));
  mStream = aStream;
  mTrackID = aTrackID;
  mPrincipalHandle = aPrincipal;
  mStream->AddTrack(mTrackID, new VideoSegment());
}

nsresult MediaEngineTabVideoSource::Start() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == kAllocated);

  nsCOMPtr<nsIRunnable> runnable;
  if (!mWindow) {
    runnable = new InitRunnable(this, mStream, mTrackID, mPrincipalHandle);
  } else {
    runnable = new StartRunnable(this, mStream, mTrackID, mPrincipalHandle);
  }
  NS_DispatchToMainThread(runnable);
  mState = kStarted;

  return NS_OK;
}

void MediaEngineTabVideoSource::Draw() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWindow && !mBlackedoutWindow) {
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
  // This can fail if either a) we haven't added the track yet, or b)
  // we've removed or ended the track.
  mStreamMain->AppendToTrack(mTrackIDMain, &segment);
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
