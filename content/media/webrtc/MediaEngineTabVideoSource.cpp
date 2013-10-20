#include "nsGlobalWindow.h"
#include "nsDOMWindowUtils.h"
#include "nsIDOMClientRect.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMDocument.h"
#include "nsITabSource.h"
#include "MediaEngineTabVideoSource.h"
#include "VideoUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefService.h"
namespace mozilla {

NS_IMPL_ISUPPORTS1(MediaEngineTabVideoSource, MediaEngineVideoSource)

MediaEngineTabVideoSource::MediaEngineTabVideoSource()
  : mName(NS_LITERAL_STRING("share tab")), mUuid(NS_LITERAL_STRING("uuid")),
  mMonitor("MediaEngineTabVideoSource")
{
}

nsresult
MediaEngineTabVideoSource::StartRunnable::Run()
{
  mVideoSource->Draw();
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(mVideoSource->mWindow);
  if (privateDOMWindow) {
    privateDOMWindow->GetChromeEventHandler()->AddEventListener(NS_LITERAL_STRING("MozAfterPaint"), mVideoSource, false);
  } else {
    mVideoSource->mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    mVideoSource->mTimer->InitWithCallback(mVideoSource, mVideoSource->mTimePerFrame, nsITimer:: TYPE_REPEATING_SLACK);
  }
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::StopRunnable::Run()
{
  nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(mVideoSource->mWindow);
  if (privateDOMWindow && mVideoSource && privateDOMWindow->GetChromeEventHandler()) {
    privateDOMWindow->GetChromeEventHandler()->RemoveEventListener(NS_LITERAL_STRING("MozAfterPaint"), mVideoSource, false);
  }

  if (mVideoSource->mTimer) {
    mVideoSource->mTimer->Cancel();
    mVideoSource->mTimer = NULL;
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

nsresult
MediaEngineTabVideoSource::InitRunnable::Run()
{
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
  if (!branch)
    return NS_OK;
  branch->GetIntPref("media.tabstreaming.width", &mVideoSource->mBufW);
  branch->GetIntPref("media.tabstreaming.height", &mVideoSource->mBufH);
  branch->GetIntPref("media.tabstreaming.time_per_frame", &mVideoSource->mTimePerFrame);
  mVideoSource->mData = (unsigned char*)malloc(mVideoSource->mBufW * mVideoSource->mBufH * 4);

  nsCOMPtr<nsITabSource> tabSource = do_GetService(NS_TABSOURCESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMWindow> win;
  rv = tabSource->GetTabToStream(getter_AddRefs(win));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!win)
    return NS_OK;

  mVideoSource->mWindow = win;
  nsCOMPtr<nsIRunnable> start(new StartRunnable(mVideoSource));
  start->Run();
  return NS_OK;
}

void
MediaEngineTabVideoSource::GetName(nsAString_internal& aName)
{
  aName.Assign(mName);

}

void
MediaEngineTabVideoSource::GetUUID(nsAString_internal& aUuid)
{
  aUuid.Assign(mUuid);
}

nsresult
MediaEngineTabVideoSource::Allocate(const mozilla::MediaEnginePrefs&)
{
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::Deallocate()
{
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::Start(mozilla::SourceMediaStream* aStream, mozilla::TrackID aID)
{
  nsCOMPtr<nsIRunnable> runnable;
  if (!mWindow)
    runnable = new InitRunnable(this);
  else
    runnable = new StartRunnable(this);
  NS_DispatchToMainThread(runnable);
  aStream->AddTrack(aID, USECS_PER_S, 0, new VideoSegment());
  aStream->AdvanceKnownTracksTime(STREAM_TIME_MAX);

  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::Snapshot(uint32_t, nsIDOMFile**)
{
  return NS_OK;
}

void
MediaEngineTabVideoSource::
NotifyPull(MediaStreamGraph*, SourceMediaStream* aSource, mozilla::TrackID aID, mozilla::StreamTime aDesiredTime, mozilla::TrackTicks& aLastEndTime)
{
  VideoSegment segment;
  MonitorAutoLock mon(mMonitor);

  // Note: we're not giving up mImage here
  nsRefPtr<layers::CairoImage> image = mImage;
  TrackTicks target = TimeToTicksRoundUp(USECS_PER_S, aDesiredTime);
  TrackTicks delta = target - aLastEndTime;
  if (delta > 0) {
    // NULL images are allowed
    if (image) {
      gfxIntSize size = image->GetSize();
      segment.AppendFrame(image.forget(), delta, size);
    } else {
      segment.AppendFrame(nullptr, delta, gfxIntSize(0,0));
    }
    // This can fail if either a) we haven't added the track yet, or b)
    // we've removed or finished the track.
    if (aSource->AppendToTrack(aID, &(segment))) {
      aLastEndTime = target;
    }
  }
}

void
MediaEngineTabVideoSource::Draw() {

  nsIntSize size(mBufW, mBufH);

  nsresult rv;
  float scale = 1.0;

  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(mWindow);

  if (!win) {
    return;
  }

  // take a screenshot, as wide as possible, proportional to the destination size
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(win);
  if (!utils) {
    return;
  }

  nsCOMPtr<nsIDOMClientRect> rect;
  rv = utils->GetRootBounds(getter_AddRefs(rect));
  NS_ENSURE_SUCCESS_VOID(rv);
  if (!rect) {
    return;
  }

  float left, top, width, height;
  rect->GetLeft(&left);
  rect->GetTop(&top);
  rect->GetWidth(&width);
  rect->GetHeight(&height);

  if (width == 0 || height == 0) {
    return;
  }

  int32_t srcX = left;
  int32_t srcY = top;
  int32_t srcW;
  int32_t srcH;

  float aspectRatio = ((float) size.width) / size.height;
  if (width / aspectRatio < height) {
    srcW = width;
    srcH = width / aspectRatio;
  } else {
    srcW = height * aspectRatio;
    srcH = height;
  }

  nsRefPtr<nsPresContext> presContext;
  nsIDocShell* docshell = win->GetDocShell();
  if (docshell) {
    docshell->GetPresContext(getter_AddRefs(presContext));
  }
  if (!presContext) {
    return;
  }
  nscolor bgColor = NS_RGB(255, 255, 255);
  nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();
  uint32_t renderDocFlags = (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                             nsIPresShell::RENDER_DOCUMENT_RELATIVE);
  nsRect r(nsPresContext::CSSPixelsToAppUnits(srcX / scale),
           nsPresContext::CSSPixelsToAppUnits(srcY / scale),
           nsPresContext::CSSPixelsToAppUnits(srcW / scale),
           nsPresContext::CSSPixelsToAppUnits(srcH / scale));

  uint32_t stride = size.width * 4;

  nsRefPtr<layers::ImageContainer> container = layers::LayerManager::CreateImageContainer();
  nsRefPtr<gfxASurface> surf;
  surf = new gfxImageSurface(static_cast<unsigned char*>(mData), size,
                             stride, gfxImageFormatRGB24);
  if (surf->CairoStatus() != 0) {
    return;
  }
  nsRefPtr<gfxContext> context = new gfxContext(surf);
  gfxPoint pt(0, 0);
  context->Translate(pt);
  context->Scale(scale * size.width / srcW, scale * size.height / srcH);
  rv = presShell->RenderDocument(r, renderDocFlags, bgColor, context);

  NS_ENSURE_SUCCESS_VOID(rv);

  layers::CairoImage::Data cairoData;
  cairoData.mSurface = surf;
  cairoData.mSize = size;

  nsRefPtr<layers::CairoImage> image = new layers::CairoImage();

  image->SetData(cairoData);

  MonitorAutoLock mon(mMonitor);
  mImage = image;
}

nsresult
MediaEngineTabVideoSource::Stop(mozilla::SourceMediaStream*, mozilla::TrackID)
{
  NS_DispatchToMainThread(new StopRunnable(this));
  return NS_OK;
}

nsresult
MediaEngineTabVideoSource::Config(bool, uint32_t, bool, uint32_t, bool, uint32_t)
{
  return NS_OK;
}

bool
MediaEngineTabVideoSource::IsFake()
{
  return false;
}

}
