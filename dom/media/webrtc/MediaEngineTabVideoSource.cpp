/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineTabVideoSource.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsGlobalWindow.h"
#include "nsIDOMClientRect.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "gfxContext.h"
#include "gfx2DGlue.h"
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

NS_IMPL_ISUPPORTS(MediaEngineTabVideoSource, nsIDOMEventListener, nsITimerCallback)

MediaEngineTabVideoSource::MediaEngineTabVideoSource()
  : mBufWidthMax(0)
  , mBufHeightMax(0)
  , mWindowId(0)
  , mScrollWithPage(false)
  , mViewportOffsetX(0)
  , mViewportOffsetY(0)
  , mViewportWidth(0)
  , mViewportHeight(0)
  , mTimePerFrame(0)
  , mDataSize(0)
  , mMonitor("MediaEngineTabVideoSource") {}

nsresult
MediaEngineTabVideoSource::StartRunnable::Run()
{
  mVideoSource->Draw();
  mVideoSource->mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  mVideoSource->mTimer->InitWithCallback(mVideoSource, mVideoSource->mTimePerFrame, nsITimer:: TYPE_REPEATING_SLACK);
  if (mVideoSource->mTabSource) {
    mVideoSource->mTabSource->NotifyStreamStart(mVideoSource->mWindow);
  }
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::StopRunnable::Run()
{
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(mVideoSource->mWindow);

  if (mVideoSource->mTimer) {
    mVideoSource->mTimer->Cancel();
    mVideoSource->mTimer = nullptr;
  }
  if (mVideoSource->mTabSource) {
    mVideoSource->mTabSource->NotifyStreamStop(mVideoSource->mWindow);
  }
  return NS_OK;
}

NS_IMETHODIMP
MediaEngineTabVideoSource::HandleEvent(nsIDOMEvent *event) {
  Draw();
  return NS_OK;
}

NS_IMETHODIMP
MediaEngineTabVideoSource::Notify(nsITimer*) {
  Draw();
  return NS_OK;
}
#define LOGTAG "TabVideo"

nsresult
MediaEngineTabVideoSource::InitRunnable::Run()
{
  if (mVideoSource->mWindowId != -1) {
    nsCOMPtr<nsPIDOMWindow> window  = nsGlobalWindow::GetOuterWindowWithId(mVideoSource->mWindowId);
    if (window) {
      mVideoSource->mWindow = window;
    }
  }
  if (!mVideoSource->mWindow) {
    nsresult rv;
    mVideoSource->mTabSource = do_GetService(NS_TABSOURCESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMWindow> win;
    rv = mVideoSource->mTabSource->GetTabToStream(getter_AddRefs(win));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!win)
      return NS_OK;

    mVideoSource->mWindow = win;
  }
  nsCOMPtr<nsIRunnable> start(new StartRunnable(mVideoSource));
  start->Run();
  return NS_OK;
}

void
MediaEngineTabVideoSource::GetName(nsAString_internal& aName)
{
  aName.AssignLiteral(MOZ_UTF16("&getUserMedia.videoSource.tabShare;"));
}

void
MediaEngineTabVideoSource::GetUUID(nsACString_internal& aUuid)
{
  aUuid.AssignLiteral("tab");
}

#define DEFAULT_TABSHARE_VIDEO_MAX_WIDTH 4096
#define DEFAULT_TABSHARE_VIDEO_MAX_HEIGHT 4096
#define DEFAULT_TABSHARE_VIDEO_FRAMERATE 30

nsresult
MediaEngineTabVideoSource::Allocate(const dom::MediaTrackConstraints& aConstraints,
                                    const MediaEnginePrefs& aPrefs,
                                    const nsString& aDeviceId)
{
  // windowId is not a proper constraint, so just read it.
  // It has no well-defined behavior in advanced, so ignore it there.

  mWindowId = aConstraints.mBrowserWindow.WasPassed() ?
              aConstraints.mBrowserWindow.Value() : -1;

  return Restart(aConstraints, aPrefs, aDeviceId);
}

nsresult
MediaEngineTabVideoSource::Restart(const dom::MediaTrackConstraints& aConstraints,
                                   const mozilla::MediaEnginePrefs& aPrefs,
                                   const nsString& aDeviceId)
{
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
MediaEngineTabVideoSource::Deallocate()
{
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::Start(SourceMediaStream* aStream, TrackID aID)
{
  nsCOMPtr<nsIRunnable> runnable;
  if (!mWindow)
    runnable = new InitRunnable(this);
  else
    runnable = new StartRunnable(this);
  NS_DispatchToMainThread(runnable);
  aStream->AddTrack(aID, 0, new VideoSegment());

  return NS_OK;
}

void
MediaEngineTabVideoSource::NotifyPull(MediaStreamGraph*,
                                      SourceMediaStream* aSource,
                                      TrackID aID, StreamTime aDesiredTime)
{
  VideoSegment segment;
  MonitorAutoLock mon(mMonitor);

  // Note: we're not giving up mImage here
  RefPtr<layers::SourceSurfaceImage> image = mImage;
  StreamTime delta = aDesiredTime - aSource->GetEndOfAppendedData(aID);
  if (delta > 0) {
    // nullptr images are allowed
    gfx::IntSize size = image ? image->GetSize() : IntSize(0, 0);
    segment.AppendFrame(image.forget().downcast<layers::Image>(), delta, size);
    // This can fail if either a) we haven't added the track yet, or b)
    // we've removed or finished the track.
    aSource->AppendToTrack(aID, &(segment));
  }
}

void
MediaEngineTabVideoSource::Draw() {
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(mWindow);

  if (!win) {
    return;
  }

  if (mScrollWithPage || mViewportWidth == INT32_MAX) {
    win->GetInnerWidth(&mViewportWidth);
  }
  if (mScrollWithPage || mViewportHeight == INT32_MAX) {
    win->GetInnerHeight(&mViewportHeight);
  }
  if (!mViewportWidth || !mViewportHeight) {
    return;
  }

  IntSize size;
  {
    float pixelRatio;
    win->GetDevicePixelRatio(&pixelRatio);
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

  gfxImageFormat format = gfxImageFormat::RGB24;
  uint32_t stride = gfxASurface::FormatStrideForWidth(format, size.width);

  if (mDataSize < static_cast<size_t>(stride * size.height)) {
    mDataSize = stride * size.height;
    mData = static_cast<unsigned char*>(malloc(mDataSize));
  }
  if (!mData) {
    return;
  }

  nsCOMPtr<nsIPresShell> presShell;
  {
    RefPtr<nsPresContext> presContext;
    nsIDocShell* docshell = win->GetDocShell();
    if (docshell) {
      docshell->GetPresContext(getter_AddRefs(presContext));
    }
    if (!presContext) {
      return;
    }
    presShell = presContext->PresShell();
  }

  nscolor bgColor = NS_RGB(255, 255, 255);
  uint32_t renderDocFlags = mScrollWithPage? 0 :
      (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
       nsIPresShell::RENDER_DOCUMENT_RELATIVE);
  nsRect r(nsPresContext::CSSPixelsToAppUnits((float)mViewportOffsetX),
           nsPresContext::CSSPixelsToAppUnits((float)mViewportOffsetY),
           nsPresContext::CSSPixelsToAppUnits((float)mViewportWidth),
           nsPresContext::CSSPixelsToAppUnits((float)mViewportHeight));

  RefPtr<layers::ImageContainer> container = layers::LayerManager::CreateImageContainer();
  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     mData.rwget(),
                                     size,
                                     stride,
                                     SurfaceFormat::B8G8R8X8);
  if (!dt) {
    return;
  }
  RefPtr<gfxContext> context = new gfxContext(dt);
  context->SetMatrix(context->CurrentMatrix().Scale((((float) size.width)/mViewportWidth),
                                                    (((float) size.height)/mViewportHeight)));

  NS_ENSURE_SUCCESS_VOID(presShell->RenderDocument(r, renderDocFlags, bgColor, context));

  RefPtr<SourceSurface> surface = dt->Snapshot();
  if (!surface) {
    return;
  }

  RefPtr<layers::SourceSurfaceImage> image = new layers::SourceSurfaceImage(size, surface);

  MonitorAutoLock mon(mMonitor);
  mImage = image;
}

nsresult
MediaEngineTabVideoSource::Stop(mozilla::SourceMediaStream*, mozilla::TrackID)
{
  if (!mWindow)
    return NS_OK;

  NS_DispatchToMainThread(new StopRunnable(this));
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::Config(bool, uint32_t, bool, uint32_t, bool, uint32_t, int32_t)
{
  return NS_OK;
}

bool
MediaEngineTabVideoSource::IsFake()
{
  return false;
}

}
