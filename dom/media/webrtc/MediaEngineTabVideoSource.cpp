/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineTabVideoSource.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsGlobalWindow.h"
#include "nsIDOMClientRect.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "gfxContext.h"
#include "gfx2DGlue.h"
#include "AllocationHandle.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMDocument.h"
#include "nsITabSource.h"
#include "VideoUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
#include "MediaTrackConstraints.h"

namespace mozilla {

using namespace mozilla::gfx;

MediaEngineTabVideoSource::MediaEngineTabVideoSource()
  : mMutex("MediaEngineTabVideoSource::mMutex") {}

nsresult
MediaEngineTabVideoSource::StartRunnable::Run()
{
  mVideoSource->Draw();
  mVideoSource->mTimer->InitWithNamedFuncCallback(
      [](nsITimer* aTimer, void* aClosure) mutable {
        auto source = static_cast<MediaEngineTabVideoSource*>(aClosure);
        source->Draw();
      },
      mVideoSource,
      mVideoSource->mTimePerFrame,
      nsITimer::TYPE_REPEATING_SLACK,
      "MediaEngineTabVideoSource DrawTimer");
  if (mVideoSource->mTabSource) {
    mVideoSource->mTabSource->NotifyStreamStart(mVideoSource->mWindow);
  }
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::StopRunnable::Run()
{
  if (mVideoSource->mTimer) {
    mVideoSource->mTimer->Cancel();
    mVideoSource->mTimer = nullptr;
  }
  if (mVideoSource->mTabSource) {
    mVideoSource->mTabSource->NotifyStreamStop(mVideoSource->mWindow);
  }
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::InitRunnable::Run()
{
  if (mVideoSource->mWindowId != -1) {
    nsGlobalWindowOuter* globalWindow =
      nsGlobalWindowOuter::GetOuterWindowWithId(mVideoSource->mWindowId);
    if (!globalWindow) {
      // We can't access the window, just send a blacked out screen.
      mVideoSource->mWindow = nullptr;
      mVideoSource->mBlackedoutWindow = true;
    } else {
      nsCOMPtr<nsPIDOMWindowOuter> window = globalWindow->AsOuter();
      if (window) {
        mVideoSource->mWindow = window;
        mVideoSource->mBlackedoutWindow = false;
      }
    }
  }
  if (!mVideoSource->mWindow && !mVideoSource->mBlackedoutWindow) {
    nsresult rv;
    mVideoSource->mTabSource = do_GetService(NS_TABSOURCESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIDOMWindowProxy> win;
    rv = mVideoSource->mTabSource->GetTabToStream(getter_AddRefs(win));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!win)
      return NS_OK;

    mVideoSource->mWindow = nsPIDOMWindowOuter::From(win);
    MOZ_ASSERT(mVideoSource->mWindow);
  }
  mVideoSource->mTimer = NS_NewTimer();
  nsCOMPtr<nsIRunnable> start(new StartRunnable(mVideoSource));
  start->Run();
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::DestroyRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  mVideoSource->mWindow = nullptr;
  mVideoSource->mTabSource = nullptr;

  return NS_OK;
}

nsString
MediaEngineTabVideoSource::GetName() const
{
  return NS_LITERAL_STRING(u"&getUserMedia.videoSource.tabShare;");
}

nsCString
MediaEngineTabVideoSource::GetUUID() const
{
  return NS_LITERAL_CSTRING("tab");
}

#define DEFAULT_TABSHARE_VIDEO_MAX_WIDTH 4096
#define DEFAULT_TABSHARE_VIDEO_MAX_HEIGHT 4096
#define DEFAULT_TABSHARE_VIDEO_FRAMERATE 30

nsresult
MediaEngineTabVideoSource::Allocate(const dom::MediaTrackConstraints& aConstraints,
                                    const MediaEnginePrefs& aPrefs,
                                    const nsString& aDeviceId,
                                    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                                    AllocationHandle** aOutHandle,
                                    const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();

  // windowId is not a proper constraint, so just read it.
  // It has no well-defined behavior in advanced, so ignore it there.

  mWindowId = aConstraints.mBrowserWindow.WasPassed() ?
              aConstraints.mBrowserWindow.Value() : -1;
  *aOutHandle = nullptr;

  {
    MutexAutoLock lock(mMutex);
    mState = kAllocated;
  }

  return Reconfigure(nullptr, aConstraints, aPrefs, aDeviceId, aOutBadConstraint);
}

nsresult
MediaEngineTabVideoSource::Reconfigure(const RefPtr<AllocationHandle>& aHandle,
                                       const dom::MediaTrackConstraints& aConstraints,
                                       const mozilla::MediaEnginePrefs& aPrefs,
                                       const nsString& aDeviceId,
                                       const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aHandle);
  MOZ_ASSERT(mState != kReleased);

  // scrollWithPage is not proper a constraint, so just read it.
  // It has no well-defined behavior in advanced, so ignore it there.

  mScrollWithPage = aConstraints.mScrollWithPage.WasPassed() ?
                    aConstraints.mScrollWithPage.Value() : false;

  FlattenedConstraints c(aConstraints);

  mBufWidthMax = c.mWidth.Get(DEFAULT_TABSHARE_VIDEO_MAX_WIDTH);
  mBufHeightMax = c.mHeight.Get(DEFAULT_TABSHARE_VIDEO_MAX_HEIGHT);
  double frameRate = c.mFrameRate.Get(DEFAULT_TABSHARE_VIDEO_FRAMERATE);
  mTimePerFrame = std::max(10, int(1000.0 / (frameRate > 0? frameRate : 1)));

  if (!mScrollWithPage) {
    mViewportOffsetX = c.mViewportOffsetX.Get(0);
    mViewportOffsetY = c.mViewportOffsetY.Get(0);
    mViewportWidth = c.mViewportWidth.Get(INT32_MAX);
    mViewportHeight = c.mViewportHeight.Get(INT32_MAX);
  }
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::Deallocate(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aHandle);
  MOZ_ASSERT(mState == kAllocated || mState == kStopped);

  MOZ_ASSERT(IsTrackIDExplicit(mTrackID));

  NS_DispatchToMainThread(do_AddRef(new DestroyRunnable(this)));

  {
    MutexAutoLock lock(mMutex);
    mState = kReleased;
  }

  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::Start(SourceMediaStream* aStream,
                                 TrackID aTrackID,
                                 const PrincipalHandle& aPrincipalHandle)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == kAllocated);

  MOZ_ASSERT(!mStream);
  MOZ_ASSERT(mTrackID == TRACK_NONE);
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(IsTrackIDExplicit(aTrackID));

  nsCOMPtr<nsIRunnable> runnable;
  if (!mWindow) {
    runnable = new InitRunnable(this);
  } else {
    runnable = new StartRunnable(this);
  }
  NS_DispatchToMainThread(runnable);
  mStream = aStream;
  mTrackID = aTrackID;
  mStream->AddTrack(mTrackID, 0, new VideoSegment());

  {
    MutexAutoLock lock(mMutex);
    mState = kStarted;
  }

  return NS_OK;
}

void
MediaEngineTabVideoSource::Pull(const RefPtr<const AllocationHandle>& aHandle,
                                const RefPtr<SourceMediaStream>& aStream,
                                TrackID aTrackID,
                                StreamTime aDesiredTime,
                                const PrincipalHandle& aPrincipalHandle)
{
  VideoSegment segment;
  RefPtr<layers::Image> image;

  {
    MutexAutoLock lock(mMutex);
    if (mState == kReleased) {
      // We end the track before setting the state to released.
      return;
    }
    image = mImage;
  }

  // Note: we're not giving up mImage here
  StreamTime delta = aDesiredTime - aStream->GetEndOfAppendedData(aTrackID);
  if (delta <= 0) {
    return;
  }

  // nullptr images are allowed
  gfx::IntSize size = image ? image->GetSize() : IntSize(0, 0);
  segment.AppendFrame(image.forget(), delta, size,
                      aPrincipalHandle);
  // This can fail if either a) we haven't added the track yet, or b)
  // we've removed or ended the track.
  aStream->AppendToTrack(aTrackID, &(segment));
}

void
MediaEngineTabVideoSource::Draw() {
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

  uint32_t stride = StrideForFormatAndWidth(SurfaceFormat::X8R8G8B8_UINT32,
                                            size.width);

  if (mDataSize < static_cast<size_t>(stride * size.height)) {
    mDataSize = stride * size.height;
    mData = MakeUniqueFallible<unsigned char[]>(mDataSize);
  }
  if (!mData) {
    return;
  }

  nsCOMPtr<nsIPresShell> presShell;
  if (mWindow) {
    RefPtr<nsPresContext> presContext;
    nsIDocShell* docshell = mWindow->GetDocShell();
    if (docshell) {
      docshell->GetPresContext(getter_AddRefs(presContext));
    }
    if (!presContext) {
      return;
    }
    presShell = presContext->PresShell();
  }

  RefPtr<layers::ImageContainer> container =
    layers::LayerManager::CreateImageContainer(layers::ImageContainer::ASYNCHRONOUS);
  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(gfxPlatform::GetPlatform()->GetSoftwareBackend(),
                                     mData.get(),
                                     size,
                                     stride,
                                     SurfaceFormat::B8G8R8X8,
                                     true);
  if (!dt || !dt->IsValid()) {
    return;
  }

  if (mWindow) {
    RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt);
    MOZ_ASSERT(context); // already checked the draw target above
    context->SetMatrix(context->CurrentMatrix().PreScale((((float) size.width)/mViewportWidth),
                                                         (((float) size.height)/mViewportHeight)));

    nscolor bgColor = NS_RGB(255, 255, 255);
    uint32_t renderDocFlags = mScrollWithPage? 0 :
      (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
       nsIPresShell::RENDER_DOCUMENT_RELATIVE);
    nsRect r(nsPresContext::CSSPixelsToAppUnits((float)mViewportOffsetX),
             nsPresContext::CSSPixelsToAppUnits((float)mViewportOffsetY),
             nsPresContext::CSSPixelsToAppUnits((float)mViewportWidth),
             nsPresContext::CSSPixelsToAppUnits((float)mViewportHeight));
    NS_ENSURE_SUCCESS_VOID(presShell->RenderDocument(r, renderDocFlags, bgColor, context));
  } else {
    dt->ClearRect(Rect(0, 0, size.width, size.height));
  }

  RefPtr<SourceSurface> surface = dt->Snapshot();
  if (!surface) {
    return;
  }

  RefPtr<layers::SourceSurfaceImage> image = new layers::SourceSurfaceImage(size, surface);

  MutexAutoLock lock(mMutex);
  mImage = image;
}

nsresult
MediaEngineTabVideoSource::Stop(SourceMediaStream* aStream,
                                TrackID aTrackID)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == kStarted);
  MOZ_ASSERT(mStream == aStream);
  MOZ_ASSERT(mTrackID == aTrackID);

  // If mBlackedoutWindow is true, we may be running
  // despite mWindow == nullptr.
  if (!mWindow && !mBlackedoutWindow) {
    return NS_OK;
  }

  NS_DispatchToMainThread(new StopRunnable(this));

  {
    MutexAutoLock lock(mMutex);
    mState = kStopped;
    mStream->EndTrack(mTrackID);
    mStream = nullptr;
    mTrackID = TRACK_NONE;
  }
  return NS_OK;
}

}
