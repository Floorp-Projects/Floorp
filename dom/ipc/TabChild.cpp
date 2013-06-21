/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "TabChild.h"

#include "BasicLayers.h"
#include "Blob.h"
#include "ContentChild.h"
#include "IndexedDBChild.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/IntentionalCrash.h"
#include "mozilla/docshell/OfflineCacheUpdateChild.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/PContentDialogChild.h"
#include "mozilla/ipc/DocumentRendererChild.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/layout/RenderFrameChild.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozIApplication.h"
#include "nsComponentManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsEmbedCID.h"
#include "nsEventListenerManager.h"
#include <algorithm>
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif
#include "mozilla/dom/Element.h"
#include "nsIAppsService.h"
#include "nsIBaseWindow.h"
#include "nsICachedFileDescriptorListener.h"
#include "nsIComponentManager.h"
#include "nsIDocumentInlines.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIJSRuntimeService.h"
#include "nsISSLStatusProvider.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsISecureBrowserUI.h"
#include "nsIServiceManager.h"
#include "nsISupportsImpl.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsView.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserSetup.h"
#include "nsIWebProgress.h"
#include "nsIXPCSecurityManager.h"
#include "nsInterfaceHashtable.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsGlobalWindow.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "nsScriptLoader.h"
#include "nsSerializationHelper.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"
#include "PCOMContentPermissionRequestChild.h"
#include "PuppetWidget.h"
#include "StructuredCloneUtils.h"
#include "xpcpublic.h"
#include "nsViewportInfo.h"

#define BROWSER_ELEMENT_CHILD_SCRIPT \
    NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js")

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::docshell;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::widget;

NS_IMPL_ISUPPORTS1(ContentListener, nsIDOMEventListener)

static const CSSSize kDefaultViewportSize(980, 480);

static const char CANCEL_DEFAULT_PAN_ZOOM[] = "cancel-default-pan-zoom";
static const char BROWSER_ZOOM_TO_RECT[] = "browser-zoom-to-rect";
static const char BEFORE_FIRST_PAINT[] = "before-first-paint";
static const char DETECT_SCROLLABLE_SUBFRAME[] = "detect-scrollable-subframe";

NS_IMETHODIMP
ContentListener::HandleEvent(nsIDOMEvent* aEvent)
{
  RemoteDOMEvent remoteEvent;
  remoteEvent.mEvent = do_QueryInterface(aEvent);
  NS_ENSURE_STATE(remoteEvent.mEvent);
  mTabChild->SendEvent(remoteEvent);
  return NS_OK;
}

class ContentDialogChild : public PContentDialogChild
{
public:
  virtual bool Recv__delete__(const InfallibleTArray<int>& aIntParams,
                              const InfallibleTArray<nsString>& aStringParams);
};

class TabChild::CachedFileDescriptorInfo
{
    struct PathOnlyComparatorHelper
    {
        bool Equals(const nsAutoPtr<CachedFileDescriptorInfo>& a,
                    const CachedFileDescriptorInfo& b) const
        {
            return a->mPath == b.mPath;
        }
    };

    struct PathAndCallbackComparatorHelper
    {
        bool Equals(const nsAutoPtr<CachedFileDescriptorInfo>& a,
                    const CachedFileDescriptorInfo& b) const
        {
            return a->mPath == b.mPath &&
                   a->mCallback == b.mCallback;
        }
    };

public:
    nsString mPath;
    FileDescriptor mFileDescriptor;
    nsCOMPtr<nsICachedFileDescriptorListener> mCallback;
    bool mCanceled;

    CachedFileDescriptorInfo(const nsAString& aPath)
      : mPath(aPath), mCanceled(false)
    { }

    CachedFileDescriptorInfo(const nsAString& aPath,
                             const FileDescriptor& aFileDescriptor)
      : mPath(aPath), mFileDescriptor(aFileDescriptor), mCanceled(false)
    { }

    CachedFileDescriptorInfo(const nsAString& aPath,
                             nsICachedFileDescriptorListener* aCallback)
      : mPath(aPath), mCallback(aCallback), mCanceled(false)
    { }

    PathOnlyComparatorHelper PathOnlyComparator() const
    {
        return PathOnlyComparatorHelper();
    }

    PathAndCallbackComparatorHelper PathAndCallbackComparator() const
    {
        return PathAndCallbackComparatorHelper();
    }

    void FireCallback() const
    {
        mCallback->OnCachedFileDescriptor(mPath, mFileDescriptor);
    }
};

class TabChild::CachedFileDescriptorCallbackRunnable : public nsRunnable
{
    typedef TabChild::CachedFileDescriptorInfo CachedFileDescriptorInfo;

    nsAutoPtr<CachedFileDescriptorInfo> mInfo;

public:
    CachedFileDescriptorCallbackRunnable(CachedFileDescriptorInfo* aInfo)
      : mInfo(aInfo)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(aInfo);
        MOZ_ASSERT(!aInfo->mPath.IsEmpty());
        MOZ_ASSERT(aInfo->mCallback);
    }

    void Dispatch()
    {
        MOZ_ASSERT(NS_IsMainThread());

        nsresult rv = NS_DispatchToCurrentThread(this);
        NS_ENSURE_SUCCESS_VOID(rv);
    }

private:
    NS_IMETHOD Run()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mInfo);

        mInfo->FireCallback();
        return NS_OK;
    }
};

StaticRefPtr<TabChild> sPreallocatedTab;

/*static*/ void
TabChild::PreloadSlowThings()
{
    MOZ_ASSERT(!sPreallocatedTab);

    nsRefPtr<TabChild> tab(new TabChild(TabContext(), /* chromeFlags */ 0));
    if (!NS_SUCCEEDED(tab->Init()) ||
        !tab->InitTabChildGlobal(DONT_LOAD_SCRIPTS)) {
        return;
    }
    // Just load and compile these scripts, but don't run them.
    tab->TryCacheLoadAndCompileScript(BROWSER_ELEMENT_CHILD_SCRIPT);
    // Load, compile, and run these scripts.
    tab->RecvLoadRemoteScript(
        NS_LITERAL_STRING("chrome://global/content/preload.js"));

    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(tab->mWebNav);
    if (nsIPresShell* presShell = docShell->GetPresShell()) {
        // Initialize and do an initial reflow of the about:blank
        // PresShell to let it preload some things for us.
        presShell->Initialize(0, 0);
        nsIDocument* doc = presShell->GetDocument();
        doc->FlushPendingNotifications(Flush_Layout);
        // ... but after it's done, make sure it doesn't do any more
        // work.
        presShell->MakeZombie();
    }

    sPreallocatedTab = tab;
    ClearOnShutdown(&sPreallocatedTab);
}

/*static*/ already_AddRefed<TabChild>
TabChild::Create(const TabContext &aContext, uint32_t aChromeFlags)
{
    if (sPreallocatedTab &&
        sPreallocatedTab->mChromeFlags == aChromeFlags &&
        aContext.IsBrowserOrApp()) {

        nsRefPtr<TabChild> child = sPreallocatedTab.get();
        sPreallocatedTab = nullptr;

        MOZ_ASSERT(!child->mTriedBrowserInit);

        child->SetTabContext(aContext);
        child->NotifyTabContextUpdated();
        return child.forget();
    }

    nsRefPtr<TabChild> iframe = new TabChild(aContext, aChromeFlags);
    return NS_SUCCEEDED(iframe->Init()) ? iframe.forget() : nullptr;
}


TabChild::TabChild(const TabContext& aContext, uint32_t aChromeFlags)
  : TabContext(aContext)
  , mRemoteFrame(nullptr)
  , mTabChildGlobal(nullptr)
  , mChromeFlags(aChromeFlags)
  , mOuterRect(0, 0, 0, 0)
  , mInnerSize(0, 0)
  , mActivePointerId(-1)
  , mTapHoldTimer(nullptr)
  , mAppPackageFileDescriptorRecved(false)
  , mOldViewportWidth(0.0f)
  , mLastBackgroundColor(NS_RGB(255, 255, 255))
  , mDidFakeShow(false)
  , mNotified(false)
  , mContentDocumentIsDisplayed(false)
  , mTriedBrowserInit(false)
  , mOrientation(eScreenOrientation_PortraitPrimary)
{
    printf("creating %d!\n", NS_IsMainThread());
}

NS_IMETHODIMP
TabChild::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("DOMMetaAdded")) {
    // This meta data may or may not have been a meta viewport tag. If it was,
    // we should handle it immediately.
    HandlePossibleViewportChange();
  }

  return NS_OK;
}

NS_IMETHODIMP
TabChild::Observe(nsISupports *aSubject,
                  const char *aTopic,
                  const PRUnichar *aData)
{
  if (!strcmp(aTopic, CANCEL_DEFAULT_PAN_ZOOM)) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aSubject));
    nsCOMPtr<nsITabChild> tabChild(GetTabChildFrom(docShell));
    if (tabChild == this) {
      mRemoteFrame->CancelDefaultPanZoom();
    }
  } else if (!strcmp(aTopic, BROWSER_ZOOM_TO_RECT)) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aSubject));
    nsCOMPtr<nsITabChild> tabChild(GetTabChildFrom(docShell));
    if (tabChild == this) {
      gfxRect rect;
      sscanf(NS_ConvertUTF16toUTF8(aData).get(),
             "{\"x\":%lf,\"y\":%lf,\"w\":%lf,\"h\":%lf}",
             &rect.x, &rect.y, &rect.width, &rect.height);
      SendZoomToRect(rect);
    }
  } else if (!strcmp(aTopic, BEFORE_FIRST_PAINT)) {
    if (IsAsyncPanZoomEnabled()) {
      nsCOMPtr<nsIDocument> subject(do_QueryInterface(aSubject));
      nsCOMPtr<nsIDOMDocument> domDoc;
      mWebNav->GetDocument(getter_AddRefs(domDoc));
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));

      if (SameCOMIdentity(subject, doc)) {
        nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());

        mContentDocumentIsDisplayed = true;

        // Reset CSS viewport and zoom to default on new page, then
        // calculate them properly using the actual metadata from the
        // page.
        SetCSSViewport(kDefaultViewportSize);

        // Calculate a really simple resolution that we probably won't
        // be keeping, as well as putting the scroll offset back to
        // the top-left of the page.
        mLastMetrics.mZoom = ScreenToScreenScale(1.0);
        mLastMetrics.mViewport = CSSRect(CSSPoint(), kDefaultViewportSize);
        mLastMetrics.mCompositionBounds = ScreenIntRect(ScreenIntPoint(), mInnerSize);
        CSSToScreenScale resolution =
          AsyncPanZoomController::CalculateResolution(mLastMetrics);
        // We use ScreenToLayerScale(1) below in order to ask gecko to render
        // what's currently visible on the screen. This is effectively turning
        // the async zoom amount into the gecko zoom amount.
        mLastMetrics.mResolution =
          resolution / mLastMetrics.mDevPixelsPerCSSPixel * ScreenToLayerScale(1);
        mLastMetrics.mScrollOffset = CSSPoint(0, 0);
        utils->SetResolution(mLastMetrics.mResolution.scale,
                             mLastMetrics.mResolution.scale);

        HandlePossibleViewportChange();
      }
    }
  } else if (!strcmp(aTopic, DETECT_SCROLLABLE_SUBFRAME)) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aSubject));
    nsCOMPtr<nsITabChild> tabChild(GetTabChildFrom(docShell));
    if (tabChild == this) {
      mRemoteFrame->DetectScrollableSubframe();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnStateChange(nsIWebProgress* aWebProgress,
                        nsIRequest* aRequest,
                        uint32_t aStateFlags,
                        nsresult aStatus)
{
  NS_NOTREACHED("not implemented in TabChild");
  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnProgressChange(nsIWebProgress* aWebProgress,
                           nsIRequest* aRequest,
                           int32_t aCurSelfProgress,
                           int32_t aMaxSelfProgress,
                           int32_t aCurTotalProgress,
                           int32_t aMaxTotalProgress)
{
  NS_NOTREACHED("not implemented in TabChild");
  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnLocationChange(nsIWebProgress* aWebProgress,
                           nsIRequest* aRequest,
                           nsIURI *aLocation,
                           uint32_t aFlags)
{
  if (!IsAsyncPanZoomEnabled()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindow> window;
  aWebProgress->GetDOMWindow(getter_AddRefs(window));
  if (!window) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindowUtils> utils(do_GetInterface(window));
  utils->SetIsFirstPaint(true);

  nsCOMPtr<nsIDOMDocument> progressDoc;
  window->GetDocument(getter_AddRefs(progressDoc));
  if (!progressDoc) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  mWebNav->GetDocument(getter_AddRefs(domDoc));
  if (!domDoc || !SameCOMIdentity(domDoc, progressDoc)) {
    return NS_OK;
  }

  nsCOMPtr<nsIURIFixup> urifixup(do_GetService(NS_URIFIXUP_CONTRACTID));
  if (!urifixup) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> exposableURI;
  urifixup->CreateExposableURI(aLocation, getter_AddRefs(exposableURI));
  if (!exposableURI) {
    return NS_OK;
  }

  if (!(aFlags & nsIWebProgressListener::LOCATION_CHANGE_SAME_DOCUMENT)) {
    mContentDocumentIsDisplayed = false;
  } else if (mLastURI != nullptr) {
    bool exposableEqualsLast, exposableEqualsNew;
    exposableURI->Equals(mLastURI.get(), &exposableEqualsLast);
    exposableURI->Equals(aLocation, &exposableEqualsNew);
    if (exposableEqualsLast && !exposableEqualsNew) {
      mContentDocumentIsDisplayed = false;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnStatusChange(nsIWebProgress* aWebProgress,
                         nsIRequest* aRequest,
                         nsresult aStatus,
                         const PRUnichar* aMessage)
{
  NS_NOTREACHED("not implemented in TabChild");
  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnSecurityChange(nsIWebProgress* aWebProgress,
                           nsIRequest* aRequest,
                           uint32_t aState)
{
  NS_NOTREACHED("not implemented in TabChild");
  return NS_OK;
}

void
TabChild::SetCSSViewport(const CSSSize& aSize)
{
  mOldViewportWidth = aSize.width;

  if (mContentDocumentIsDisplayed) {
    nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
    utils->SetCSSViewport(aSize.width, aSize.height);
  }
}

void
TabChild::HandlePossibleViewportChange()
{
  if (!IsAsyncPanZoomEnabled()) {
    return;
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  mWebNav->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> document(do_QueryInterface(domDoc));

  nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());

  nsViewportInfo viewportInfo =
    nsContentUtils::GetViewportInfo(document, mInnerSize.width, mInnerSize.height);
  SendUpdateZoomConstraints(viewportInfo.IsZoomAllowed(),
                            viewportInfo.GetMinZoom(),
                            viewportInfo.GetMaxZoom());

  float screenW = mInnerSize.width;
  float screenH = mInnerSize.height;
  CSSSize viewport(viewportInfo.GetWidth(), viewportInfo.GetHeight());

  // We're not being displayed in any way; don't bother doing anything because
  // that will just confuse future adjustments.
  if (!screenW || !screenH) {
    return;
  }

  // Make sure the viewport height is not shorter than the window when the page
  // is zoomed out to show its full width. Note that before we set the viewport
  // width, the "full width" of the page isn't properly defined, so that's why
  // we have to call SetCSSViewport twice - once to set the width, and the
  // second time to figure out the height based on the layout at that width.
  float oldBrowserWidth = mOldViewportWidth;
  mLastMetrics.mViewport.SizeTo(viewport);
  if (!oldBrowserWidth) {
    oldBrowserWidth = kDefaultViewportSize.width;
  }
  SetCSSViewport(viewport);

  // If this page has not been painted yet, then this must be getting run
  // because a meta-viewport element was added (via the DOMMetaAdded handler).
  // in this case, we should not do anything that forces a reflow (see bug
  // 759678) such as requesting the page size or sending a viewport update. this
  // code will get run again in the before-first-paint handler and that point we
  // will run though all of it. the reason we even bother executing up to this
  // point on the DOMMetaAdded handler is so that scripts that use
  // window.innerWidth before they are painted have a correct value (bug
  // 771575).
  if (!mContentDocumentIsDisplayed) {
    return;
  }

  float minScale = 1.0f;

  nsCOMPtr<nsIDOMElement> htmlDOMElement = do_QueryInterface(document->GetHtmlElement());
  HTMLBodyElement* bodyDOMElement = document->GetBodyElement();

  int32_t htmlWidth = 0, htmlHeight = 0;
  if (htmlDOMElement) {
    htmlDOMElement->GetScrollWidth(&htmlWidth);
    htmlDOMElement->GetScrollHeight(&htmlHeight);
  }
  int32_t bodyWidth = 0, bodyHeight = 0;
  if (bodyDOMElement) {
    bodyWidth = bodyDOMElement->ScrollWidth();
    bodyHeight = bodyDOMElement->ScrollHeight();
  }

  CSSSize pageSize;
  if (htmlDOMElement || bodyDOMElement) {
    pageSize = CSSSize(std::max(htmlWidth, bodyWidth),
                       std::max(htmlHeight, bodyHeight));
  } else {
    // For non-HTML content (e.g. SVG), just assume page size == viewport size.
    pageSize = viewport;
  }
  if (!pageSize.width) {
    // Return early rather than divide by 0.
    return;
  }

  minScale = mInnerSize.width / pageSize.width;
  minScale = clamped((double)minScale, viewportInfo.GetMinZoom(),
                     viewportInfo.GetMaxZoom());
  NS_ENSURE_TRUE_VOID(minScale); // (return early rather than divide by 0)

  viewport.height = std::max(viewport.height, screenH / minScale);
  SetCSSViewport(viewport);

  // This change to the zoom accounts for all types of changes I can conceive:
  // 1. screen size changes, CSS viewport does not (pages with no meta viewport
  //    or a fixed size viewport)
  // 2. screen size changes, CSS viewport also does (pages with a device-width
  //    viewport)
  // 3. screen size remains constant, but CSS viewport changes (meta viewport
  //    tag is added or removed)
  // 4. neither screen size nor CSS viewport changes
  //
  // In all of these cases, we maintain how much actual content is visible
  // within the screen width. Note that "actual content" may be different with
  // respect to CSS pixels because of the CSS viewport size changing.
  int32_t oldScreenWidth = mLastMetrics.mCompositionBounds.width;
  if (!oldScreenWidth) {
    oldScreenWidth = mInnerSize.width;
  }

  FrameMetrics metrics(mLastMetrics);
  metrics.mViewport = CSSRect(CSSPoint(), viewport);
  metrics.mScrollableRect = CSSRect(CSSPoint(), pageSize);
  metrics.mCompositionBounds = ScreenIntRect(ScreenIntPoint(), mInnerSize);

  // Changing the zoom when we're not doing a first paint will get ignored
  // by AsyncPanZoomController and causes a blurry flash.
  bool isFirstPaint;
  nsresult rv = utils->GetIsFirstPaint(&isFirstPaint);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (NS_FAILED(rv) || isFirstPaint) {
    CSSToScreenScale intrinsicScale =
        AsyncPanZoomController::CalculateIntrinsicScale(metrics);
    // FIXME/bug 799585(?): GetViewportInfo() returns a defaultZoom of
    // 0.0 to mean "did not calculate a zoom".  In that case, we default
    // it to the intrinsic scale.
    if (viewportInfo.GetDefaultZoom() < 0.01f) {
      viewportInfo.SetDefaultZoom(intrinsicScale.scale);
    }

    double defaultZoom = viewportInfo.GetDefaultZoom();
    MOZ_ASSERT(viewportInfo.GetMinZoom() <= defaultZoom &&
               defaultZoom <= viewportInfo.GetMaxZoom());
    // GetViewportInfo() returns a resolution-dependent scale factor.
    // Convert that to a resolution-indepedent zoom.
    metrics.mZoom = ScreenToScreenScale(defaultZoom / intrinsicScale.scale);
  }

  metrics.mDisplayPort = AsyncPanZoomController::CalculatePendingDisplayPort(
    // The page must have been refreshed in some way such as a new document or
    // new CSS viewport, so we know that there's no velocity, acceleration, and
    // we have no idea how long painting will take.
    metrics, gfx::Point(0.0f, 0.0f), gfx::Point(0.0f, 0.0f), 0.0);
  CSSToScreenScale resolution = AsyncPanZoomController::CalculateResolution(metrics);
  metrics.mResolution = resolution / metrics.mDevPixelsPerCSSPixel * ScreenToLayerScale(1);
  utils->SetResolution(metrics.mResolution.scale, metrics.mResolution.scale);

  // Force a repaint with these metrics. This, among other things, sets the
  // displayport, so we start with async painting.
  ProcessUpdateFrame(metrics);
}

nsresult
TabChild::Init()
{
  nsCOMPtr<nsIWebBrowser> webBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  if (!webBrowser) {
    NS_ERROR("Couldn't create a nsWebBrowser?");
    return NS_ERROR_FAILURE;
  }

  webBrowser->SetContainerWindow(this);
  mWebNav = do_QueryInterface(webBrowser);
  NS_ASSERTION(mWebNav, "nsWebBrowser doesn't implement nsIWebNavigation?");

  nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(mWebNav));
  docShellItem->SetItemType(nsIDocShellTreeItem::typeContentWrapper);
  
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebNav);
  if (!baseWindow) {
    NS_ERROR("mWebNav doesn't QI to nsIBaseWindow");
    return NS_ERROR_FAILURE;
  }

  mWidget = nsIWidget::CreatePuppetWidget(this);
  if (!mWidget) {
    NS_ERROR("couldn't create fake widget");
    return NS_ERROR_FAILURE;
  }
  mWidget->Create(
    nullptr, 0,              // no parents
    nsIntRect(nsIntPoint(0, 0), nsIntSize(0, 0)),
    nullptr,                 // HandleWidgetEvent
    nullptr                  // nsDeviceContext
  );

  baseWindow->InitWindow(0, mWidget, 0, 0, 0, 0);
  baseWindow->Create();

  NotifyTabContextUpdated();

  // IPC uses a WebBrowser object for which DNS prefetching is turned off
  // by default. But here we really want it, so enable it explicitly
  nsCOMPtr<nsIWebBrowserSetup> webBrowserSetup =
    do_QueryInterface(baseWindow);
  if (webBrowserSetup) {
    webBrowserSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_DNS_PREFETCH,
                                 true);
  } else {
    NS_WARNING("baseWindow doesn't QI to nsIWebBrowserSetup, skipping "
               "DNS prefetching enable step.");
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebNav);
  MOZ_ASSERT(docShell);

  docShell->SetAffectPrivateSessionLifetime(
      mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME);
  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(mWebNav);
  MOZ_ASSERT(loadContext);
  loadContext->SetPrivateBrowsing(
      mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW);

  nsCOMPtr<nsIWebProgress> webProgress = do_GetInterface(docShell);
  NS_ENSURE_TRUE(webProgress, NS_ERROR_FAILURE);
  webProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_LOCATION);

  return NS_OK;
}

void
TabChild::NotifyTabContextUpdated()
{
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebNav);
    MOZ_ASSERT(docShell);

    if (docShell) {
        // nsDocShell will do the right thing if we pass NO_APP_ID or
        // UNKNOWN_APP_ID for aOwnOrContainingAppId.
        if (IsBrowserElement()) {
          docShell->SetIsBrowserInsideApp(BrowserOwnerAppId());
        } else {
          docShell->SetIsApp(OwnAppId());
        }
    }
}

NS_INTERFACE_MAP_BEGIN(TabChild)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsITabChild)
  NS_INTERFACE_MAP_ENTRY(nsIDialogCreator)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsSupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(TabChild)
NS_IMPL_RELEASE(TabChild)

NS_IMETHODIMP
TabChild::SetStatus(uint32_t aStatusType, const PRUnichar* aStatus)
{
  // FIXME/bug 617804: should the platform support this?
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
  NS_NOTREACHED("TabChild::GetWebBrowser not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
  NS_NOTREACHED("TabChild::SetWebBrowser not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetChromeFlags(uint32_t* aChromeFlags)
{
  *aChromeFlags = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetChromeFlags(uint32_t aChromeFlags)
{
  NS_NOTREACHED("trying to SetChromeFlags from content process?");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::DestroyBrowserWindow()
{
  NS_NOTREACHED("TabChild::SetWebBrowser not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SizeBrowserTo(int32_t aCX, int32_t aCY)
{
  NS_NOTREACHED("TabChild::SizeBrowserTo not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::ShowAsModal()
{
  NS_NOTREACHED("TabChild::ShowAsModal not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::IsWindowModal(bool* aRetVal)
{
  *aRetVal = false;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::ExitModalEventLoop(nsresult aStatus)
{
  NS_NOTREACHED("TabChild::ExitModalEventLoop not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetStatusWithContext(uint32_t aStatusType,
                                    const nsAString& aStatusText,
                                    nsISupports* aStatusContext)
{
  // FIXME/bug 617804: should the platform support this?
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetDimensions(uint32_t aFlags, int32_t aX, int32_t aY,
                             int32_t aCx, int32_t aCy)
{
  NS_NOTREACHED("TabChild::SetDimensions not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetDimensions(uint32_t aFlags, int32_t* aX,
                             int32_t* aY, int32_t* aCx, int32_t* aCy)
{
  if (aX) {
    *aX = mOuterRect.x;
  }
  if (aY) {
    *aY = mOuterRect.y;
  }
  if (aCx) {
    *aCx = mOuterRect.width;
  }
  if (aCy) {
    *aCy = mOuterRect.height;
  }

  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetFocus()
{
  NS_NOTREACHED("TabChild::SetFocus not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetVisibility(bool* aVisibility)
{
  *aVisibility = true;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetVisibility(bool aVisibility)
{
  // should the platform support this? Bug 666365
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetTitle(PRUnichar** aTitle)
{
  NS_NOTREACHED("TabChild::GetTitle not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetTitle(const PRUnichar* aTitle)
{
  // FIXME/bug 617804: should the platform support this?
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetSiteWindow(void** aSiteWindow)
{
  NS_NOTREACHED("TabChild::GetSiteWindow not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::Blur()
{
  NS_NOTREACHED("TabChild::Blur not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::FocusNextElement()
{
  SendMoveFocus(true);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::FocusPrevElement()
{
  SendMoveFocus(false);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetInterface(const nsIID & aIID, void **aSink)
{
    // XXXbz should we restrict the set of interfaces we hand out here?
    // See bug 537429
    return QueryInterface(aIID, aSink);
}

NS_IMETHODIMP
TabChild::ProvideWindow(nsIDOMWindow* aParent, uint32_t aChromeFlags,
                        bool aCalledFromJS,
                        bool aPositionSpecified, bool aSizeSpecified,
                        nsIURI* aURI, const nsAString& aName,
                        const nsACString& aFeatures, bool* aWindowIsNew,
                        nsIDOMWindow** aReturn)
{
    *aReturn = nullptr;

    // If aParent is inside an <iframe mozbrowser> or <iframe mozapp> and this
    // isn't a request to open a modal-type window, we're going to create a new
    // <iframe mozbrowser/mozapp> and return its window here.
    nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
    if (docshell && docshell->GetIsInBrowserOrApp() &&
        !(aChromeFlags & (nsIWebBrowserChrome::CHROME_MODAL |
                          nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                          nsIWebBrowserChrome::CHROME_OPENAS_CHROME))) {

      // Note that BrowserFrameProvideWindow may return NS_ERROR_ABORT if the
      // open window call was canceled.  It's important that we pass this error
      // code back to our caller.
      return BrowserFrameProvideWindow(aParent, aURI, aName, aFeatures,
                                       aWindowIsNew, aReturn);
    }

    // Otherwise, create a new top-level window.
    PBrowserChild* newChild;
    if (!CallCreateWindow(&newChild)) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    *aWindowIsNew = true;
    nsCOMPtr<nsIDOMWindow> win =
        do_GetInterface(static_cast<TabChild*>(newChild)->mWebNav);
    win.forget(aReturn);
    return NS_OK;
}

nsresult
TabChild::BrowserFrameProvideWindow(nsIDOMWindow* aOpener,
                                    nsIURI* aURI,
                                    const nsAString& aName,
                                    const nsACString& aFeatures,
                                    bool* aWindowIsNew,
                                    nsIDOMWindow** aReturn)
{
  *aReturn = nullptr;

  nsRefPtr<TabChild> newChild =
      new TabChild(/* TabContext */ *this, /* chromeFlags */ 0);
  if (!NS_SUCCEEDED(newChild->Init())) {
      return NS_ERROR_ABORT;
  }

  // We must use PopupIPCTabContext here; ContentParent will not accept the
  // result of this->AsIPCTabContext() (which will be a
  // BrowserFrameIPCTabContext or an AppFrameIPCTabContext), for security
  // reasons.
  PopupIPCTabContext context;
  context.openerChild() = this;
  context.isBrowserElement() = IsBrowserElement();

  unused << Manager()->SendPBrowserConstructor(
      // We release this ref in DeallocPBrowserChild
      nsRefPtr<TabChild>(newChild).forget().get(),
      IPCTabContext(context, mScrolling), /* chromeFlags */ 0);

  nsAutoCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }

  NS_ConvertUTF8toUTF16 url(spec);
  nsString name(aName);
  NS_ConvertUTF8toUTF16 features(aFeatures);
  newChild->SendBrowserFrameOpenWindow(this, url, name,
                                       features, aWindowIsNew);
  if (!*aWindowIsNew) {
    PBrowserChild::Send__delete__(newChild);
    return NS_ERROR_ABORT;
  }

  // Unfortunately we don't get a window unless we've shown the frame.  That's
  // pretty bogus; see bug 763602.
  newChild->DoFakeShow();

  nsCOMPtr<nsIDOMWindow> win = do_GetInterface(newChild->mWebNav);
  win.forget(aReturn);
  return NS_OK;
}

already_AddRefed<nsIDOMWindowUtils>
TabChild::GetDOMWindowUtils()
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  return utils.forget();
}

static nsInterfaceHashtable<nsPtrHashKey<PContentDialogChild>, nsIDialogParamBlock> gActiveDialogs;

NS_IMETHODIMP
TabChild::OpenDialog(uint32_t aType, const nsACString& aName,
                     const nsACString& aFeatures,
                     nsIDialogParamBlock* aArguments,
                     nsIDOMElement* aFrameElement)
{
  if (!gActiveDialogs.IsInitialized()) {
    gActiveDialogs.Init();
  }
  InfallibleTArray<int32_t> intParams;
  InfallibleTArray<nsString> stringParams;
  ParamsToArrays(aArguments, intParams, stringParams);
  PContentDialogChild* dialog =
    SendPContentDialogConstructor(aType, nsCString(aName),
                                  nsCString(aFeatures), intParams, stringParams);
  gActiveDialogs.Put(dialog, aArguments);
  nsIThread *thread = NS_GetCurrentThread();
  while (gActiveDialogs.GetWeak(dialog)) {
    if (!NS_ProcessNextEvent(thread)) {
      break;
    }
  }
  return NS_OK;
}

bool
ContentDialogChild::Recv__delete__(const InfallibleTArray<int>& aIntParams,
                                   const InfallibleTArray<nsString>& aStringParams)
{
  nsCOMPtr<nsIDialogParamBlock> params;
  if (gActiveDialogs.Get(this, getter_AddRefs(params))) {
    TabChild::ArraysToParams(aIntParams, aStringParams, params);
    gActiveDialogs.Remove(this);
  }
  return true;
}

void
TabChild::ParamsToArrays(nsIDialogParamBlock* aParams,
                         InfallibleTArray<int>& aIntParams,
                         InfallibleTArray<nsString>& aStringParams)
{
  if (aParams) {
    for (int32_t i = 0; i < 8; ++i) {
      int32_t val = 0;
      aParams->GetInt(i, &val);
      aIntParams.AppendElement(val);
    }
    int32_t j = 0;
    nsXPIDLString strVal;
    while (NS_SUCCEEDED(aParams->GetString(j, getter_Copies(strVal)))) {
      aStringParams.AppendElement(strVal);
      ++j;
    }
  }
}

void
TabChild::ArraysToParams(const InfallibleTArray<int>& aIntParams,
                         const InfallibleTArray<nsString>& aStringParams,
                         nsIDialogParamBlock* aParams)
{
  if (aParams) {
    for (int32_t i = 0; uint32_t(i) < aIntParams.Length(); ++i) {
      aParams->SetInt(i, aIntParams[i]);
    }
    for (int32_t j = 0; uint32_t(j) < aStringParams.Length(); ++j) {
      aParams->SetString(j, aStringParams[j].get());
    }
  }
}

void
TabChild::DestroyWindow()
{
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebNav);
    if (baseWindow)
        baseWindow->Destroy();

    // NB: the order of mWidget->Destroy() and mRemoteFrame->Destroy()
    // is important: we want to kill off remote layers before their
    // frames
    if (mWidget) {
        mWidget->Destroy();
    }

    if (mRemoteFrame) {
        mRemoteFrame->Destroy();
        mRemoteFrame = nullptr;
    }
}

bool
TabChild::UseDirectCompositor()
{
    return !!CompositorChild::Get();
}

void
TabChild::ActorDestroy(ActorDestroyReason why)
{
  if (mTabChildGlobal) {
    // The messageManager relays messages via the TabChild which
    // no longer exists.
    static_cast<nsFrameMessageManager*>
      (mTabChildGlobal->mMessageManager.get())->Disconnect();
    mTabChildGlobal->mMessageManager = nullptr;
  }
}

TabChild::~TabChild()
{
    DestroyWindow();

    nsCOMPtr<nsIWebBrowser> webBrowser = do_QueryInterface(mWebNav);
    if (webBrowser) {
      webBrowser->SetContainerWindow(nullptr);
    }
    if (mCx) {
      DestroyCx();
    }
    
    if (mTabChildGlobal) {
      nsEventListenerManager* elm = mTabChildGlobal->GetListenerManager(false);
      if (elm) {
        elm->Disconnect();
      }
      mTabChildGlobal->mTabChild = nullptr;
    }
}

void
TabChild::SetProcessNameToAppName()
{
  nsCOMPtr<mozIApplication> app = GetOwnApp();
  if (!app) {
    return;
  }

  nsAutoString appName;
  nsresult rv = app->GetName(appName);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to retrieve app name");
    return;
  }

  ContentChild::GetSingleton()->SetProcessName(appName);
}

bool
TabChild::IsRootContentDocument()
{
    // A TabChild is a "root content document" if it's
    //
    //  - <iframe mozapp> not inside another <iframe mozapp>,
    //  - <iframe mozbrowser> (not mozapp), or
    //  - a vanilla remote frame (<html:iframe remote=true> or <xul:browser
    //    remote=true>).
    //
    // Put another way, an iframe is /not/ a "root content document" iff it's a
    // mozapp inside a mozapp.  (This corresponds exactly to !HasAppOwnerApp.)
    //
    // Note that we're lying through our teeth here (thus the scare quotes).
    // <html:iframe remote=true> or <xul:browser remote=true> inside another
    // content iframe is not actually a root content document, but we say it is.
    //
    // We do this because we make a remote frame opaque iff
    // IsRootContentDocument(), and making vanilla remote frames transparent
    // breaks our remote reftests.

    return !HasAppOwnerApp();
}

bool
TabChild::RecvLoadURL(const nsCString& uri)
{
    printf("loading %s, %d\n", uri.get(), NS_IsMainThread());
    SetProcessNameToAppName();

    nsresult rv = mWebNav->LoadURI(NS_ConvertUTF8toUTF16(uri).get(),
                                   nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
                                   nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_OWNER,
                                   NULL, NULL, NULL);
    if (NS_FAILED(rv)) {
        NS_WARNING("mWebNav->LoadURI failed. Eating exception, what else can I do?");
    }

#ifdef MOZ_CRASHREPORTER
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("URL"), uri);
#endif

    return true;
}

bool
TabChild::RecvCacheFileDescriptor(const nsString& aPath,
                                  const FileDescriptor& aFileDescriptor)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!aPath.IsEmpty());
    MOZ_ASSERT(!mAppPackageFileDescriptorRecved);

    mAppPackageFileDescriptorRecved = true;

    // aFileDescriptor may be invalid here, but the callback will choose how to
    // handle it.

    // First see if we already have a request for this path.
    const CachedFileDescriptorInfo search(aPath);
    uint32_t index =
        mCachedFileDescriptorInfos.IndexOf(search, 0,
                                           search.PathOnlyComparator());
    if (index == mCachedFileDescriptorInfos.NoIndex) {
        // We haven't had any requests for this path yet. Assume that we will
        // in a little while and save the file descriptor here.
        mCachedFileDescriptorInfos.AppendElement(
            new CachedFileDescriptorInfo(aPath, aFileDescriptor));
        return true;
    }

    nsAutoPtr<CachedFileDescriptorInfo>& info =
        mCachedFileDescriptorInfos[index];

    MOZ_ASSERT(info);
    MOZ_ASSERT(info->mPath == aPath);
    MOZ_ASSERT(!info->mFileDescriptor.IsValid());
    MOZ_ASSERT(info->mCallback);

    // If this callback has been canceled then we can simply close the file
    // descriptor and forget about the callback.
    if (info->mCanceled) {
        // Only close if this is a valid file descriptor.
        if (aFileDescriptor.IsValid()) {
            nsRefPtr<CloseFileRunnable> runnable =
                new CloseFileRunnable(aFileDescriptor);
            runnable->Dispatch();
        }
    } else {
        // Not canceled so fire the callback.
        info->mFileDescriptor = aFileDescriptor;

        // We don't need a runnable here because we should already be at the top
        // of the event loop. Just fire immediately.
        info->FireCallback();
    }

    mCachedFileDescriptorInfos.RemoveElementAt(index);
    return true;
}

bool
TabChild::GetCachedFileDescriptor(const nsAString& aPath,
                                  nsICachedFileDescriptorListener* aCallback)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!aPath.IsEmpty());
    MOZ_ASSERT(aCallback);

    // First see if we've already received a cached file descriptor for this
    // path.
    const CachedFileDescriptorInfo search(aPath);
    uint32_t index =
        mCachedFileDescriptorInfos.IndexOf(search, 0,
                                           search.PathOnlyComparator());
    if (index == mCachedFileDescriptorInfos.NoIndex) {
        // We haven't received a file descriptor for this path yet. Assume that
        // we will in a little while and save the request here.
        if (!mAppPackageFileDescriptorRecved) {
          mCachedFileDescriptorInfos.AppendElement(
              new CachedFileDescriptorInfo(aPath, aCallback));
        }
        return false;
    }

    nsAutoPtr<CachedFileDescriptorInfo>& info =
        mCachedFileDescriptorInfos[index];

    MOZ_ASSERT(info);
    MOZ_ASSERT(info->mPath == aPath);

    // If we got a previous request for this file descriptor that was then
    // canceled, insert the new request ahead of the old in the queue so that
    // it will be serviced first.
    if (info->mCanceled) {
        // This insertion will change the array and invalidate |info|, so
        // be careful not to touch |info| after this.
        mCachedFileDescriptorInfos.InsertElementAt(index,
            new CachedFileDescriptorInfo(aPath, aCallback));
        return false;
    }

    MOZ_ASSERT(!info->mCallback);
    info->mCallback = aCallback;

    nsRefPtr<CachedFileDescriptorCallbackRunnable> runnable =
        new CachedFileDescriptorCallbackRunnable(info.forget());
    runnable->Dispatch();

    mCachedFileDescriptorInfos.RemoveElementAt(index);
    return true;
}

void
TabChild::CancelCachedFileDescriptorCallback(
                                     const nsAString& aPath,
                                     nsICachedFileDescriptorListener* aCallback)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!aPath.IsEmpty());
    MOZ_ASSERT(aCallback);

    if (mAppPackageFileDescriptorRecved) {
      // Already received cached file descriptor for the app package. Nothing to do here.
      return;
    }

    const CachedFileDescriptorInfo search(aPath, aCallback);
    uint32_t index =
        mCachedFileDescriptorInfos.IndexOf(search, 0,
                                           search.PathAndCallbackComparator());
    if (index == mCachedFileDescriptorInfos.NoIndex) {
        // Nothing to do here.
        return;
    }

    nsAutoPtr<CachedFileDescriptorInfo>& info =
        mCachedFileDescriptorInfos[index];

    MOZ_ASSERT(info);
    MOZ_ASSERT(info->mPath == aPath);
    MOZ_ASSERT(!info->mFileDescriptor.IsValid());
    MOZ_ASSERT(info->mCallback == aCallback);
    MOZ_ASSERT(!info->mCanceled);

    // Set this flag so that we will close the file descriptor when it arrives.
    info->mCanceled = true;
}

void
TabChild::DoFakeShow()
{
  RecvShow(nsIntSize(0, 0));
  mDidFakeShow = true;
}

bool
TabChild::RecvShow(const nsIntSize& size)
{

    if (mDidFakeShow) {
        return true;
    }

    printf("[TabChild] SHOW (w,h)= (%d, %d)\n", size.width, size.height);

    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebNav);
    if (!baseWindow) {
        NS_ERROR("mWebNav doesn't QI to nsIBaseWindow");
        return false;
    }

    if (!InitRenderingState()) {
        // We can fail to initialize our widget if the <browser
        // remote> has already been destroyed, and we couldn't hook
        // into the parent-process's layer system.  That's not a fatal
        // error.
        return true;
    }

    baseWindow->SetVisibility(true);

    return InitTabChildGlobal();
}

bool
TabChild::RecvUpdateDimensions(const nsRect& rect, const nsIntSize& size, const ScreenOrientation& orientation)
{
    if (!mRemoteFrame) {
        return true;
    }

    mOuterRect.x = rect.x;
    mOuterRect.y = rect.y;
    mOuterRect.width = rect.width;
    mOuterRect.height = rect.height;

    mOrientation = orientation;
    mInnerSize = ScreenIntSize::FromUnknownSize(
      gfx::IntSize(size.width, size.height));
    mWidget->Resize(0, 0, size.width, size.height,
                    true);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mWebNav);
    baseWin->SetPositionAndSize(0, 0, size.width, size.height,
                                true);

    HandlePossibleViewportChange();

    return true;
}

void
TabChild::DispatchMessageManagerMessage(const nsAString& aMessageName,
                                        const nsACString& aJSONData)
{
    AutoSafeJSContext cx;
    JS::Rooted<JS::Value> json(cx, JSVAL_NULL);
    StructuredCloneData cloneData;
    JSAutoStructuredCloneBuffer buffer;
    if (JS_ParseJSON(cx,
                      static_cast<const jschar*>(NS_ConvertUTF8toUTF16(aJSONData).get()),
                      aJSONData.Length(),
                      &json)) {
        WriteStructuredClone(cx, json, buffer, cloneData.mClosure);
        cloneData.mData = buffer.data();
        cloneData.mDataLength = buffer.nbytes();
    }

    nsFrameScriptCx frameScriptCx(static_cast<nsIWebBrowserChrome*>(this), this);
    // Let the BrowserElementScrolling helper (if it exists) for this
    // content manipulate the frame state.
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildGlobal),
                       aMessageName, false, &cloneData, JS::NullPtr(), nullptr);
}

static void
ScrollWindowTo(nsIDOMWindow* aWindow, const CSSPoint& aPoint)
{
    nsGlobalWindow* window = static_cast<nsGlobalWindow*>(aWindow);
    nsIScrollableFrame* sf = window->GetScrollFrame();

    if (sf) {
        sf->ScrollToCSSPixelsApproximate(aPoint);
    }
}

bool
TabChild::RecvUpdateFrame(const FrameMetrics& aFrameMetrics)
{
    nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());

    uint32_t presShellId;
    nsresult rv = utils->GetPresShellId(&presShellId);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_SUCCEEDED(rv) && aFrameMetrics.mPresShellId != presShellId) {
        // We've recieved a message that is out of date and we want to ignore.
        // However we can't reply without painting so we reply by painting the
        // exact same thing as we did before.
        return ProcessUpdateFrame(mLastMetrics);
    }
    return ProcessUpdateFrame(aFrameMetrics);
}

bool
TabChild::ProcessUpdateFrame(const FrameMetrics& aFrameMetrics)
  {
    if (!mCx || !mTabChildGlobal) {
        return true;
    }

    CSSRect cssCompositedRect =
      AsyncPanZoomController::CalculateCompositedRectInCssPixels(aFrameMetrics);
    // The BrowserElementScrolling helper must know about these updated metrics
    // for other functions it performs, such as double tap handling.
    nsCString data;
    data += nsPrintfCString("{ \"x\" : %d", NS_lround(aFrameMetrics.mScrollOffset.x));
    data += nsPrintfCString(", \"y\" : %d", NS_lround(aFrameMetrics.mScrollOffset.y));
    data += nsPrintfCString(", \"viewport\" : ");
        data += nsPrintfCString("{ \"width\" : %f", aFrameMetrics.mViewport.width);
        data += nsPrintfCString(", \"height\" : %f", aFrameMetrics.mViewport.height);
        data += nsPrintfCString(" }");
    data += nsPrintfCString(", \"displayPort\" : ");
        data += nsPrintfCString("{ \"x\" : %f", aFrameMetrics.mDisplayPort.x);
        data += nsPrintfCString(", \"y\" : %f", aFrameMetrics.mDisplayPort.y);
        data += nsPrintfCString(", \"width\" : %f", aFrameMetrics.mDisplayPort.width);
        data += nsPrintfCString(", \"height\" : %f", aFrameMetrics.mDisplayPort.height);
        data += nsPrintfCString(" }");
    data += nsPrintfCString(", \"compositionBounds\" : ");
        data += nsPrintfCString("{ \"x\" : %d", aFrameMetrics.mCompositionBounds.x);
        data += nsPrintfCString(", \"y\" : %d", aFrameMetrics.mCompositionBounds.y);
        data += nsPrintfCString(", \"width\" : %d", aFrameMetrics.mCompositionBounds.width);
        data += nsPrintfCString(", \"height\" : %d", aFrameMetrics.mCompositionBounds.height);
        data += nsPrintfCString(" }");
    data += nsPrintfCString(", \"cssPageRect\" : ");
        data += nsPrintfCString("{ \"x\" : %f", aFrameMetrics.mScrollableRect.x);
        data += nsPrintfCString(", \"y\" : %f", aFrameMetrics.mScrollableRect.y);
        data += nsPrintfCString(", \"width\" : %f", aFrameMetrics.mScrollableRect.width);
        data += nsPrintfCString(", \"height\" : %f", aFrameMetrics.mScrollableRect.height);
        data += nsPrintfCString(" }");
    data += nsPrintfCString(", \"cssCompositedRect\" : ");
            data += nsPrintfCString("{ \"width\" : %f", cssCompositedRect.width);
            data += nsPrintfCString(", \"height\" : %f", cssCompositedRect.height);
            data += nsPrintfCString(" }");
    data += nsPrintfCString(" }");

    DispatchMessageManagerMessage(NS_LITERAL_STRING("Viewport:Change"), data);

    nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
    nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mWebNav);

    utils->SetScrollPositionClampingScrollPortSize(
      cssCompositedRect.width, cssCompositedRect.height);
    ScrollWindowTo(window, aFrameMetrics.mScrollOffset);
    CSSToScreenScale resolution = AsyncPanZoomController::CalculateResolution(
      aFrameMetrics);
    utils->SetResolution(resolution.scale, resolution.scale);

    nsCOMPtr<nsIDOMDocument> domDoc;
    nsCOMPtr<nsIDOMElement> docElement;
    mWebNav->GetDocument(getter_AddRefs(domDoc));
    if (domDoc) {
      domDoc->GetDocumentElement(getter_AddRefs(docElement));
      if (docElement) {
        utils->SetDisplayPortForElement(
          aFrameMetrics.mDisplayPort.x, aFrameMetrics.mDisplayPort.y,
          aFrameMetrics.mDisplayPort.width, aFrameMetrics.mDisplayPort.height,
          docElement);
      }
    }

    mLastMetrics = aFrameMetrics;

    return true;
}

bool
TabChild::RecvHandleDoubleTap(const CSSIntPoint& aPoint)
{
    if (!mCx || !mTabChildGlobal) {
        return true;
    }

    nsCString data;
    data += nsPrintfCString("{ \"x\" : %d", aPoint.x);
    data += nsPrintfCString(", \"y\" : %d", aPoint.y);
    data += nsPrintfCString(" }");

    DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:DoubleTap"), data);

    return true;
}

bool
TabChild::RecvHandleSingleTap(const CSSIntPoint& aPoint)
{
  if (!mCx || !mTabChildGlobal) {
    return true;
  }

  RecvMouseEvent(NS_LITERAL_STRING("mousemove"), aPoint.x, aPoint.y, 0, 1, 0, false);
  RecvMouseEvent(NS_LITERAL_STRING("mousedown"), aPoint.x, aPoint.y, 0, 1, 0, false);
  RecvMouseEvent(NS_LITERAL_STRING("mouseup"), aPoint.x, aPoint.y, 0, 1, 0, false);

  return true;
}

bool
TabChild::RecvHandleLongTap(const CSSIntPoint& aPoint)
{
  if (!mCx || !mTabChildGlobal) {
    return true;
  }

  RecvMouseEvent(NS_LITERAL_STRING("contextmenu"), aPoint.x, aPoint.y,
                 2 /* Right button */,
                 1 /* Click count */,
                 0 /* Modifiers */,
                 false /* Ignore root scroll frame */);

  return true;
}

bool
TabChild::RecvActivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(mWebNav);
  browser->Activate();
  return true;
}

bool TabChild::RecvDeactivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(mWebNav);
  browser->Deactivate();
  return true;
}

bool
TabChild::RecvMouseEvent(const nsString& aType,
                         const float&    aX,
                         const float&    aY,
                         const int32_t&  aButton,
                         const int32_t&  aClickCount,
                         const int32_t&  aModifiers,
                         const bool&     aIgnoreRootScrollFrame)
{
  DispatchMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
                     aIgnoreRootScrollFrame);
  return true;
}

bool
TabChild::RecvRealMouseEvent(const nsMouseEvent& event)
{
  nsMouseEvent localEvent(event);
  DispatchWidgetEvent(localEvent);
  return true;
}

bool
TabChild::RecvMouseWheelEvent(const WheelEvent& event)
{
  WheelEvent localEvent(event);
  DispatchWidgetEvent(localEvent);
  return true;
}

void
TabChild::DispatchSynthesizedMouseEvent(uint32_t aMsg, uint64_t aTime,
                                        const nsIntPoint& aRefPoint)
{
  MOZ_ASSERT(aMsg == NS_MOUSE_MOVE || aMsg == NS_MOUSE_BUTTON_DOWN ||
             aMsg == NS_MOUSE_BUTTON_UP);

  nsMouseEvent event(true, aMsg, NULL,
      nsMouseEvent::eReal, nsMouseEvent::eNormal);
  event.refPoint = aRefPoint;
  event.time = aTime;
  event.button = nsMouseEvent::eLeftButton;
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  if (aMsg != NS_MOUSE_MOVE) {
    event.clickCount = 1;
  }

  DispatchWidgetEvent(event);
}

static Touch*
GetTouchForIdentifier(const nsTouchEvent& aEvent, int32_t aId)
{
  for (uint32_t i = 0; i < aEvent.touches.Length(); ++i) {
    Touch* touch = static_cast<Touch*>(aEvent.touches[i].get());
    if (touch->mIdentifier == aId) {
      return touch;
    }
  }
  return nullptr;
}

void
TabChild::UpdateTapState(const nsTouchEvent& aEvent, nsEventStatus aStatus)
{
  static bool sHavePrefs;
  static bool sClickHoldContextMenusEnabled;
  static nsIntSize sDragThreshold;
  static int32_t sContextMenuDelayMs;
  if (!sHavePrefs) {
    sHavePrefs = true;
    Preferences::AddBoolVarCache(&sClickHoldContextMenusEnabled,
                                 "ui.click_hold_context_menus", true);
    Preferences::AddIntVarCache(&sDragThreshold.width,
                                "ui.dragThresholdX", 25);
    Preferences::AddIntVarCache(&sDragThreshold.height,
                                "ui.dragThresholdY", 25);
    Preferences::AddIntVarCache(&sContextMenuDelayMs,
                                "ui.click_hold_context_menus.delay", 500);
  }

  bool currentlyTrackingTouch = (mActivePointerId >= 0);
  if (aEvent.message == NS_TOUCH_START) {
    if (currentlyTrackingTouch || aEvent.touches.Length() > 1) {
      // We're tracking a possible tap for another point, or we saw a
      // touchstart for a later pointer after we canceled tracking of
      // the first point.  Ignore this one.
      return;
    }
    if (aStatus == nsEventStatus_eConsumeNoDefault ||
        nsIPresShell::gPreventMouseEvents) {
      return;
    }

    Touch* touch = static_cast<Touch*>(aEvent.touches[0].get());
    mGestureDownPoint = touch->mRefPoint;
    mActivePointerId = touch->mIdentifier;
    if (sClickHoldContextMenusEnabled) {
      MOZ_ASSERT(!mTapHoldTimer);
      mTapHoldTimer = NewRunnableMethod(this,
                                        &TabChild::FireContextMenuEvent);
      MessageLoop::current()->PostDelayedTask(FROM_HERE, mTapHoldTimer,
                                              sContextMenuDelayMs);
    }
    return;
  }

  // If we're not tracking a touch or this event doesn't include the
  // one we care about, bail.
  if (!currentlyTrackingTouch) {
    return;
  }
  Touch* trackedTouch = GetTouchForIdentifier(aEvent, mActivePointerId);
  if (!trackedTouch) {
    return;
  }

  nsIntPoint currentPoint = trackedTouch->mRefPoint;
  int64_t time = aEvent.time;
  switch (aEvent.message) {
  case NS_TOUCH_MOVE:
    if (abs(currentPoint.x - mGestureDownPoint.x) > sDragThreshold.width ||
        abs(currentPoint.y - mGestureDownPoint.y) > sDragThreshold.height) {
      CancelTapTracking();
    }
    return;

  case NS_TOUCH_END:
    if (!nsIPresShell::gPreventMouseEvents) {
      DispatchSynthesizedMouseEvent(NS_MOUSE_MOVE, time, currentPoint);
      DispatchSynthesizedMouseEvent(NS_MOUSE_BUTTON_DOWN, time, currentPoint);
      DispatchSynthesizedMouseEvent(NS_MOUSE_BUTTON_UP, time, currentPoint);
    }
    // fall through
  case NS_TOUCH_CANCEL:
    CancelTapTracking();
    return;

  default:
    NS_WARNING("Unknown touch event type");
  }
}

void
TabChild::FireContextMenuEvent()
{
  MOZ_ASSERT(mTapHoldTimer && mActivePointerId >= 0);
  bool defaultPrevented = DispatchMouseEvent(NS_LITERAL_STRING("contextmenu"),
                                             mGestureDownPoint.x, mGestureDownPoint.y,
                                             2 /* Right button */,
                                             1 /* Click count */,
                                             0 /* Modifiers */,
                                             false /* Ignore root scroll frame */);

  // Fire a click event if someone didn't call preventDefault() on the context
  // menu event.
  if (defaultPrevented) {
    CancelTapTracking();
  } else if (mTapHoldTimer) {
    mTapHoldTimer->Cancel();
    mTapHoldTimer = nullptr;
  }
}

void
TabChild::CancelTapTracking()
{
  mActivePointerId = -1;
  if (mTapHoldTimer) {
    mTapHoldTimer->Cancel();
  }
  mTapHoldTimer = nullptr;
}

bool
TabChild::RecvRealTouchEvent(const nsTouchEvent& aEvent)
{
  nsTouchEvent localEvent(aEvent);
  nsEventStatus status = DispatchWidgetEvent(localEvent);

  if (IsAsyncPanZoomEnabled()) {
    nsCOMPtr<nsPIDOMWindow> outerWindow = do_GetInterface(mWebNav);
    nsCOMPtr<nsPIDOMWindow> innerWindow = outerWindow->GetCurrentInnerWindow();

    if (innerWindow && innerWindow->HasTouchEventListeners()) {
      SendContentReceivedTouch(nsIPresShell::gPreventMouseEvents);
    }
  } else {
    UpdateTapState(aEvent, status);
  }

  return true;
}

bool
TabChild::RecvRealTouchMoveEvent(const nsTouchEvent& aEvent)
{
  return RecvRealTouchEvent(aEvent);
}

bool
TabChild::RecvRealKeyEvent(const nsKeyEvent& event)
{
  nsKeyEvent localEvent(event);
  DispatchWidgetEvent(localEvent);
  return true;
}

bool
TabChild::RecvKeyEvent(const nsString& aType,
                       const int32_t& aKeyCode,
                       const int32_t& aCharCode,
                       const int32_t& aModifiers,
                       const bool& aPreventDefault)
{
  nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
  NS_ENSURE_TRUE(utils, true);
  bool ignored = false;
  utils->SendKeyEvent(aType, aKeyCode, aCharCode,
                      aModifiers, aPreventDefault, &ignored);
  return true;
}

bool
TabChild::RecvCompositionEvent(const nsCompositionEvent& event)
{
  nsCompositionEvent localEvent(event);
  DispatchWidgetEvent(localEvent);
  return true;
}

bool
TabChild::RecvTextEvent(const nsTextEvent& event)
{
  nsTextEvent localEvent(event);
  DispatchWidgetEvent(localEvent);
  IPC::ParamTraits<nsTextEvent>::Free(event);
  return true;
}

bool
TabChild::RecvSelectionEvent(const nsSelectionEvent& event)
{
  nsSelectionEvent localEvent(event);
  DispatchWidgetEvent(localEvent);
  return true;
}

nsEventStatus
TabChild::DispatchWidgetEvent(nsGUIEvent& event)
{
  if (!mWidget)
    return nsEventStatus_eConsumeNoDefault;

  nsEventStatus status;
  event.widget = mWidget;
  NS_ENSURE_SUCCESS(mWidget->DispatchEvent(&event, status),
                    nsEventStatus_eConsumeNoDefault);
  return status;
}

PDocumentRendererChild*
TabChild::AllocPDocumentRenderer(const nsRect& documentRect,
                                 const gfxMatrix& transform,
                                 const nsString& bgcolor,
                                 const uint32_t& renderFlags,
                                 const bool& flushLayout,
                                 const nsIntSize& renderSize)
{
    return new DocumentRendererChild();
}

bool
TabChild::DeallocPDocumentRenderer(PDocumentRendererChild* actor)
{
    delete actor;
    return true;
}

bool
TabChild::RecvPDocumentRendererConstructor(PDocumentRendererChild* actor,
                                           const nsRect& documentRect,
                                           const gfxMatrix& transform,
                                           const nsString& bgcolor,
                                           const uint32_t& renderFlags,
                                           const bool& flushLayout,
                                           const nsIntSize& renderSize)
{
    DocumentRendererChild *render = static_cast<DocumentRendererChild *>(actor);

    nsCOMPtr<nsIWebBrowser> browser = do_QueryInterface(mWebNav);
    if (!browser)
        return true; // silently ignore
    nsCOMPtr<nsIDOMWindow> window;
    if (NS_FAILED(browser->GetContentDOMWindow(getter_AddRefs(window))) ||
        !window)
    {
        return true; // silently ignore
    }

    nsCString data;
    bool ret = render->RenderDocument(window,
                                      documentRect, transform,
                                      bgcolor,
                                      renderFlags, flushLayout,
                                      renderSize, data);
    if (!ret)
        return true; // silently ignore

    return PDocumentRendererChild::Send__delete__(actor, renderSize, data);
}

PContentDialogChild*
TabChild::AllocPContentDialog(const uint32_t&,
                              const nsCString&,
                              const nsCString&,
                              const InfallibleTArray<int>&,
                              const InfallibleTArray<nsString>&)
{
  return new ContentDialogChild();
}

bool
TabChild::DeallocPContentDialog(PContentDialogChild* aDialog)
{
  delete aDialog;
  return true;
}

PContentPermissionRequestChild*
TabChild::AllocPContentPermissionRequest(const nsCString& aType, const nsCString& aAccess, const IPC::Principal&)
{
  NS_RUNTIMEABORT("unused");
  return nullptr;
}

bool
TabChild::DeallocPContentPermissionRequest(PContentPermissionRequestChild* actor)
{
    PCOMContentPermissionRequestChild* child =
        static_cast<PCOMContentPermissionRequestChild*>(actor);
#ifdef DEBUG
    child->mIPCOpen = false;
#endif /* DEBUG */
    child->IPDLRelease();
    return true;
}

bool
TabChild::RecvActivateFrameEvent(const nsString& aType, const bool& capture)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
  NS_ENSURE_TRUE(window, true);
  nsCOMPtr<EventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, true);
  nsRefPtr<ContentListener> listener = new ContentListener(this);
  chromeHandler->AddEventListener(aType, listener, capture);
  return true;
}

POfflineCacheUpdateChild*
TabChild::AllocPOfflineCacheUpdate(const URIParams& manifestURI,
                                   const URIParams& documentURI,
                                   const bool& stickDocument)
{
  NS_RUNTIMEABORT("unused");
  return nullptr;
}

bool
TabChild::DeallocPOfflineCacheUpdate(POfflineCacheUpdateChild* actor)
{
  OfflineCacheUpdateChild* offlineCacheUpdate = static_cast<OfflineCacheUpdateChild*>(actor);
  delete offlineCacheUpdate;
  return true;
}

bool
TabChild::RecvLoadRemoteScript(const nsString& aURL)
{
  if (!mCx && !InitTabChildGlobal())
    // This can happen if we're half-destroyed.  It's not a fatal
    // error.
    return true;

  LoadFrameScriptInternal(aURL);
  return true;
}

bool
TabChild::RecvAsyncMessage(const nsString& aMessage,
                           const ClonedMessageData& aData)
{
  if (mTabChildGlobal) {
    nsFrameScriptCx cx(static_cast<nsIWebBrowserChrome*>(this), this);
    StructuredCloneData cloneData = UnpackClonedMessageDataForChild(aData);
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildGlobal),
                       aMessage, false, &cloneData, JS::NullPtr(), nullptr);
  }
  return true;
}

class UnloadScriptEvent : public nsRunnable
{
public:
  UnloadScriptEvent(TabChild* aTabChild, TabChildGlobal* aTabChildGlobal)
    : mTabChild(aTabChild), mTabChildGlobal(aTabChildGlobal)
  { }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIDOMEvent> event;
    NS_NewDOMEvent(getter_AddRefs(event), mTabChildGlobal, nullptr, nullptr);
    if (event) {
      event->InitEvent(NS_LITERAL_STRING("unload"), false, false);
      event->SetTrusted(true);

      bool dummy;
      mTabChildGlobal->DispatchEvent(event, &dummy);
    }

    return NS_OK;
  }

  nsRefPtr<TabChild> mTabChild;
  TabChildGlobal* mTabChildGlobal;
};

bool
TabChild::RecvDestroy()
{
  if (mTabChildGlobal) {
    // Let the frame scripts know the child is being closed
    nsContentUtils::AddScriptRunner(
      new UnloadScriptEvent(this, mTabChildGlobal)
    );
  }

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

  observerService->RemoveObserver(this, CANCEL_DEFAULT_PAN_ZOOM);
  observerService->RemoveObserver(this, BROWSER_ZOOM_TO_RECT);
  observerService->RemoveObserver(this, BEFORE_FIRST_PAINT);
  observerService->RemoveObserver(this, DETECT_SCROLLABLE_SUBFRAME);

  const InfallibleTArray<PIndexedDBChild*>& idbActors =
    ManagedPIndexedDBChild();
  for (uint32_t i = 0; i < idbActors.Length(); ++i) {
    static_cast<IndexedDBChild*>(idbActors[i])->Disconnect();
  }

  // XXX what other code in ~TabChild() should we be running here?
  DestroyWindow();

  return Send__delete__(this);
}

PRenderFrameChild*
TabChild::AllocPRenderFrame(ScrollingBehavior* aScrolling,
                            TextureFactoryIdentifier* aTextureFactoryIdentifier,
                            uint64_t* aLayersId)
{
    return new RenderFrameChild();
}

bool
TabChild::DeallocPRenderFrame(PRenderFrameChild* aFrame)
{
    delete aFrame;
    return true;
}

bool
TabChild::InitTabChildGlobal(FrameScriptLoading aScriptLoading)
{
  if (!mCx && !mTabChildGlobal) {
    nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
    NS_ENSURE_TRUE(window, false);
    nsCOMPtr<EventTarget> chromeHandler =
      do_QueryInterface(window->GetChromeEventHandler());
    NS_ENSURE_TRUE(chromeHandler, false);

    nsRefPtr<TabChildGlobal> scope = new TabChildGlobal(this);
    mTabChildGlobal = scope;

    nsISupports* scopeSupports = NS_ISUPPORTS_CAST(EventTarget*, scope);

    NS_NAMED_LITERAL_CSTRING(globalId, "outOfProcessTabChildGlobal");
    NS_ENSURE_TRUE(InitTabChildGlobalInternal(scopeSupports, globalId), false);

    scope->Init();

    nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
    NS_ENSURE_TRUE(root, false);
    root->SetParentTarget(scope);

    chromeHandler->AddEventListener(NS_LITERAL_STRING("DOMMetaAdded"), this, false);
  }

  if (aScriptLoading != DONT_LOAD_SCRIPTS && !mTriedBrowserInit) {
    mTriedBrowserInit = true;
    // Initialize the child side of the browser element machinery,
    // if appropriate.
    if (IsBrowserOrApp()) {
      RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT);
    }
  }

  return true;
}

bool
TabChild::InitRenderingState()
{
    static_cast<PuppetWidget*>(mWidget.get())->InitIMEState();

    uint64_t id;
    RenderFrameChild* remoteFrame =
        static_cast<RenderFrameChild*>(SendPRenderFrameConstructor(
                                         &mScrolling, &mTextureFactoryIdentifier, &id));
    if (!remoteFrame) {
      NS_WARNING("failed to construct RenderFrame");
      return false;
    }

    PLayerTransactionChild* shadowManager = nullptr;
    if (id != 0) {
        // Pushing layers transactions directly to a separate
        // compositor context.
		PCompositorChild* compositorChild = CompositorChild::Get();
        if (!compositorChild) {
          NS_WARNING("failed to get CompositorChild instance");
          return false;
        }
        shadowManager =
            compositorChild->SendPLayerTransactionConstructor(mTextureFactoryIdentifier.mParentBackend,
                                                              id, &mTextureFactoryIdentifier);
    } else {
        // Pushing transactions to the parent content.
        shadowManager = remoteFrame->SendPLayerTransactionConstructor();
    }

    if (!shadowManager) {
      NS_WARNING("failed to construct LayersChild");
      // This results in |remoteFrame| being deleted.
      PRenderFrameChild::Send__delete__(remoteFrame);
      return false;
    }

    ShadowLayerForwarder* lf =
        mWidget->GetLayerManager(shadowManager, mTextureFactoryIdentifier.mParentBackend)
               ->AsShadowForwarder();
    NS_ABORT_IF_FALSE(lf && lf->HasShadowManager(),
                      "PuppetWidget should have shadow manager");
    lf->IdentifyTextureHost(mTextureFactoryIdentifier);

    mRemoteFrame = remoteFrame;

    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

    if (observerService) {
        observerService->AddObserver(this,
                                     CANCEL_DEFAULT_PAN_ZOOM,
                                     false);
        observerService->AddObserver(this,
                                     BROWSER_ZOOM_TO_RECT,
                                     false);
        observerService->AddObserver(this,
                                     BEFORE_FIRST_PAINT,
                                     false);
        observerService->AddObserver(this,
                                     DETECT_SCROLLABLE_SUBFRAME,
                                     false);
    }

    return true;
}

void
TabChild::SetBackgroundColor(const nscolor& aColor)
{
  if (mLastBackgroundColor != aColor) {
    mLastBackgroundColor = aColor;
    SendSetBackgroundColor(mLastBackgroundColor);
  }
}

void
TabChild::GetDPI(float* aDPI)
{
    *aDPI = -1.0;
    if (!mRemoteFrame) {
        return;
    }

    SendGetDPI(aDPI);
}

void
TabChild::GetDefaultScale(double* aScale)
{
    *aScale = -1.0;
    if (!mRemoteFrame) {
        return;
    }

    SendGetDefaultScale(aScale);
}

void
TabChild::NotifyPainted()
{
    // Normally we only need to notify the content process once, but with BasicCompositor
    // we need to notify content every change so that it can compute an invalidation
    // region and send that to the widget.
    if (UseDirectCompositor() &&
        (!mNotified || mTextureFactoryIdentifier.mParentBackend == LAYERS_BASIC)) {
        mRemoteFrame->SendNotifyCompositorTransaction();
        mNotified = true;
    }
}

bool
TabChild::IsAsyncPanZoomEnabled()
{
    return mScrolling == ASYNC_PAN_ZOOM;
}

bool
TabChild::DispatchMouseEvent(const nsString& aType,
                             const float&    aX,
                             const float&    aY,
                             const int32_t&  aButton,
                             const int32_t&  aClickCount,
                             const int32_t&  aModifiers,
                             const bool&     aIgnoreRootScrollFrame)
{
  nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
  NS_ENSURE_TRUE(utils, true);
  
  bool defaultPrevented = false;
  utils->SendMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
                        aIgnoreRootScrollFrame, 0, 0, &defaultPrevented);
  return defaultPrevented;
}

void
TabChild::MakeVisible()
{
    if (mWidget) {
        mWidget->Show(true);
    }
}

void
TabChild::MakeHidden()
{
    if (mWidget) {
        mWidget->Show(false);
    }
}

NS_IMETHODIMP
TabChild::GetMessageManager(nsIContentFrameMessageManager** aResult)
{
  if (mTabChildGlobal) {
    NS_ADDREF(*aResult = mTabChildGlobal);
    return NS_OK;
  }
  *aResult = nullptr;
  return NS_ERROR_FAILURE;
}

PIndexedDBChild*
TabChild::AllocPIndexedDB(const nsCString& aASCIIOrigin, bool* /* aAllowed */)
{
  NS_NOTREACHED("Should never get here!");
  return NULL;
}

bool
TabChild::DeallocPIndexedDB(PIndexedDBChild* aActor)
{
  delete aActor;
  return true;
}

bool
TabChild::DoSendSyncMessage(const nsAString& aMessage,
                            const StructuredCloneData& aData,
                            InfallibleTArray<nsString>* aJSONRetVal)
{
  ContentChild* cc = static_cast<ContentChild*>(Manager());
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(cc, aData, data)) {
    return false;
  }
  return SendSyncMessage(nsString(aMessage), data, aJSONRetVal);
}

bool
TabChild::DoSendAsyncMessage(const nsAString& aMessage,
                             const StructuredCloneData& aData)
{
  ContentChild* cc = static_cast<ContentChild*>(Manager());
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(cc, aData, data)) {
    return false;
  }
  return SendAsyncMessage(nsString(aMessage), data);
}


TabChildGlobal::TabChildGlobal(TabChild* aTabChild)
: mTabChild(aTabChild)
{
}

void
TabChildGlobal::Init()
{
  NS_ASSERTION(!mMessageManager, "Re-initializing?!?");
  mMessageManager = new nsFrameMessageManager(mTabChild,
                                              nullptr,
                                              mTabChild->GetJSContext(),
                                              MM_CHILD);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(TabChildGlobal, nsDOMEventTargetHelper,
                                     mMessageManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TabChildGlobal)
  NS_INTERFACE_MAP_ENTRY(nsIMessageListenerManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsISyncMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentFrameMessageManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TabChildGlobal, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TabChildGlobal, nsDOMEventTargetHelper)

/* [notxpcom] boolean markForCC (); */
// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
TabChildGlobal::MarkForCC()
{
  return mMessageManager ? mMessageManager->MarkForCC() : false;
}

NS_IMETHODIMP
TabChildGlobal::GetContent(nsIDOMWindow** aContent)
{
  *aContent = nullptr;
  if (!mTabChild)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mTabChild->WebNavigation());
  window.swap(*aContent);
  return NS_OK;
}

NS_IMETHODIMP
TabChildGlobal::PrivateNoteIntentionalCrash()
{
    mozilla::NoteIntentionalCrash("tab");
    return NS_OK;
}

NS_IMETHODIMP
TabChildGlobal::GetDocShell(nsIDocShell** aDocShell)
{
  *aDocShell = nullptr;
  if (!mTabChild)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mTabChild->WebNavigation());
  docShell.swap(*aDocShell);
  return NS_OK;
}

NS_IMETHODIMP
TabChildGlobal::Btoa(const nsAString& aBinaryData,
                     nsAString& aAsciiBase64String)
{
  return nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

NS_IMETHODIMP
TabChildGlobal::Atob(const nsAString& aAsciiString,
                     nsAString& aBinaryData)
{
  return nsContentUtils::Atob(aAsciiString, aBinaryData);
}

JSContext*
TabChildGlobal::GetJSContextForEventHandlers()
{
  if (!mTabChild)
    return nullptr;
  return mTabChild->GetJSContext();
}

nsIPrincipal* 
TabChildGlobal::GetPrincipal()
{
  if (!mTabChild)
    return nullptr;
  return mTabChild->GetPrincipal();
}
