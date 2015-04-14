/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "TabChild.h"

#include "Layers.h"
#include "ContentChild.h"
#include "TabParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/workers/ServiceWorkerManager.h"
#include "mozilla/dom/indexedDB/PIndexedDBPermissionRequestChild.h"
#include "mozilla/plugins/PluginWidgetChild.h"
#include "mozilla/ipc/DocumentRendererChild.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layout/RenderFrameChild.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/unused.h"
#include "mozIApplication.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsEmbedCID.h"
#include <algorithm>
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif
#include "nsFilePickerProxy.h"
#include "mozilla/dom/Element.h"
#include "nsIBaseWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsICachedFileDescriptorListener.h"
#include "nsIDocumentInlines.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsFocusManager.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIURI.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserSetup.h"
#include "nsIWebProgress.h"
#include "nsIXULRuntime.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsLayoutUtils.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"
#include "nsWindowWatcher.h"
#include "PermissionMessageUtils.h"
#include "PuppetWidget.h"
#include "StructuredCloneUtils.h"
#include "nsViewportInfo.h"
#include "nsILoadContext.h"
#include "ipc/nsGUIEventIPC.h"
#include "mozilla/gfx/Matrix.h"
#include "UnitTransforms.h"
#include "ClientLayerManager.h"
#include "LayersLogging.h"
#include "nsIOService.h"
#include "nsDOMClassInfoID.h"
#include "nsColorPickerProxy.h"
#include "nsPresShell.h"
#include "nsIAppsService.h"
#include "nsNetUtil.h"
#include "nsIPermissionManager.h"
#include "nsIScriptError.h"

#define BROWSER_ELEMENT_CHILD_SCRIPT \
    NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js")

#define TABC_LOG(...)
// #define TABC_LOG(...) printf_stderr("TABC: " __VA_ARGS__)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::dom::workers;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::docshell;
using namespace mozilla::widget;
using namespace mozilla::jsipc;

NS_IMPL_ISUPPORTS(ContentListener, nsIDOMEventListener)

static const CSSSize kDefaultViewportSize(980, 480);

static const char BROWSER_ZOOM_TO_RECT[] = "browser-zoom-to-rect";
static const char BEFORE_FIRST_PAINT[] = "before-first-paint";

typedef nsDataHashtable<nsUint64HashKey, TabChild*> TabChildMap;
static TabChildMap* sTabChildren;

class TabChild::DelayedFireContextMenuEvent final : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit DelayedFireContextMenuEvent(TabChild* tabChild)
    : mTabChild(tabChild)
  {
  }

  NS_IMETHODIMP Notify(nsITimer*) override
  {
    mTabChild->FireContextMenuEvent();
    return NS_OK;
  }

private:
  ~DelayedFireContextMenuEvent()
  {
  }

  // Raw pointer is safe here because this object is held by a Timer, which is
  // held by TabChild, we won't stay alive if TabChild dies.
  TabChild *mTabChild;
};

NS_IMPL_ISUPPORTS(TabChild::DelayedFireContextMenuEvent,
                  nsITimerCallback)

TabChildBase::TabChildBase()
  : mContentDocumentIsDisplayed(false)
  , mTabChildGlobal(nullptr)
  , mInnerSize(0, 0)
{
  mozilla::HoldJSObjects(this);
}

TabChildBase::~TabChildBase()
{
  mAnonymousGlobalScopes.Clear();
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TabChildBase)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TabChildBase)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTabChildGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnonymousGlobalScopes)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWebBrowserChrome)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TabChildBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTabChildGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebBrowserChrome)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(TabChildBase)
  for (uint32_t i = 0; i < tmp->mAnonymousGlobalScopes.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAnonymousGlobalScopes[i])
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TabChildBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TabChildBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TabChildBase)

void
TabChildBase::InitializeRootMetrics()
{
  // Calculate a really simple resolution that we probably won't
  // be keeping, as well as putting the scroll offset back to
  // the top-left of the page.
  mLastRootMetrics.SetViewport(CSSRect(CSSPoint(), kDefaultViewportSize));
  mLastRootMetrics.mCompositionBounds = ParentLayerRect(
      ParentLayerPoint(),
      ParentLayerSize(ViewAs<ParentLayerPixel>(mInnerSize, PixelCastJustification::ScreenIsParentLayerForRoot)));
  mLastRootMetrics.SetZoom(CSSToParentLayerScale2D(mLastRootMetrics.CalculateIntrinsicScale()));
  mLastRootMetrics.SetDevPixelsPerCSSPixel(WebWidget()->GetDefaultScale());
  // We use ParentLayerToLayerScale(1) below in order to turn the
  // async zoom amount into the gecko zoom amount.
  mLastRootMetrics.SetCumulativeResolution(mLastRootMetrics.GetZoom() / mLastRootMetrics.GetDevPixelsPerCSSPixel() * ParentLayerToLayerScale(1));
  // This is the root layer, so the cumulative resolution is the same
  // as the resolution.
  mLastRootMetrics.SetPresShellResolution(mLastRootMetrics.GetCumulativeResolution().ToScaleFactor().scale);
  mLastRootMetrics.SetScrollOffset(CSSPoint(0, 0));

  TABC_LOG("After InitializeRootMetrics, mLastRootMetrics is %s\n",
    Stringify(mLastRootMetrics).c_str());
}

void
TabChildBase::SetCSSViewport(const CSSSize& aSize)
{
  mOldViewportSize = aSize;
  TABC_LOG("Setting CSS viewport to %s\n", Stringify(aSize).c_str());

  if (mContentDocumentIsDisplayed) {
    nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
    utils->SetCSSViewport(aSize.width, aSize.height);
  }
}

CSSSize
TabChildBase::GetPageSize(nsCOMPtr<nsIDocument> aDocument, const CSSSize& aViewport)
{
  nsCOMPtr<Element> htmlDOMElement = aDocument->GetHtmlElement();
  HTMLBodyElement* bodyDOMElement = aDocument->GetBodyElement();

  if (!htmlDOMElement && !bodyDOMElement) {
    // For non-HTML content (e.g. SVG), just assume page size == viewport size.
    return aViewport;
  }

  int32_t htmlWidth = 0, htmlHeight = 0;
  if (htmlDOMElement) {
    htmlWidth = htmlDOMElement->ScrollWidth();
    htmlHeight = htmlDOMElement->ScrollHeight();
  }
  int32_t bodyWidth = 0, bodyHeight = 0;
  if (bodyDOMElement) {
    bodyWidth = bodyDOMElement->ScrollWidth();
    bodyHeight = bodyDOMElement->ScrollHeight();
  }
  return CSSSize(std::max(htmlWidth, bodyWidth),
                 std::max(htmlHeight, bodyHeight));
}

// For the root frame, Screen and ParentLayer pixels are interchangeable.
// nsViewportInfo stores zoom values as CSSToScreenScale (because it's a
// data structure specific to the root frame), while FrameMetrics and
// ZoomConstraints store zoom values as CSSToParentLayerScale (because they
// are not specific to the root frame). We define convenience functions for
// converting between the two. As the name suggests, they should only be used
// when dealing with the root frame!
CSSToScreenScale ConvertScaleForRoot(CSSToParentLayerScale aScale) {
  return ViewTargetAs<ScreenPixel>(aScale, PixelCastJustification::ScreenIsParentLayerForRoot);
}
CSSToParentLayerScale ConvertScaleForRoot(CSSToScreenScale aScale) {
  return ViewTargetAs<ParentLayerPixel>(aScale, PixelCastJustification::ScreenIsParentLayerForRoot);
}

bool
TabChildBase::HandlePossibleViewportChange(const ScreenIntSize& aOldScreenSize)
{
  if (!gfxPrefs::AsyncPanZoomEnabled()) {
    return false;
  }

  TABC_LOG("HandlePossibleViewportChange aOldScreenSize=%s mInnerSize=%s\n",
    Stringify(aOldScreenSize).c_str(), Stringify(mInnerSize).c_str());

  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());

  nsViewportInfo viewportInfo = nsContentUtils::GetViewportInfo(document, mInnerSize);
  uint32_t presShellId = 0;
  mozilla::layers::FrameMetrics::ViewID viewId = FrameMetrics::NULL_SCROLL_ID;
  bool scrollIdentifiersValid = APZCCallbackHelper::GetOrCreateScrollIdentifiers(
        document->GetDocumentElement(), &presShellId, &viewId);
  if (scrollIdentifiersValid) {
    ZoomConstraints constraints(
      viewportInfo.IsZoomAllowed(),
      viewportInfo.IsDoubleTapZoomAllowed(),
      ConvertScaleForRoot(viewportInfo.GetMinZoom()),
      ConvertScaleForRoot(viewportInfo.GetMaxZoom()));
    DoUpdateZoomConstraints(presShellId,
                            viewId,
                            /* isRoot = */ true,
                            constraints);
  }

  float screenW = mInnerSize.width;
  float screenH = mInnerSize.height;
  CSSSize viewport(viewportInfo.GetSize());

  // We're not being displayed in any way; don't bother doing anything because
  // that will just confuse future adjustments.
  if (!screenW || !screenH) {
    return false;
  }

  TABC_LOG("HandlePossibleViewportChange mOldViewportSize=%s viewport=%s\n",
    Stringify(mOldViewportSize).c_str(), Stringify(viewport).c_str());
  CSSSize oldBrowserSize = mOldViewportSize;
  mLastRootMetrics.SetViewport(CSSRect(
    mLastRootMetrics.GetViewport().TopLeft(), viewport));
  if (oldBrowserSize == CSSSize()) {
    oldBrowserSize = kDefaultViewportSize;
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
    return false;
  }

  ScreenIntSize oldScreenSize = aOldScreenSize;
  if (oldScreenSize == ScreenIntSize()) {
    oldScreenSize = mInnerSize;
  }

  FrameMetrics metrics(mLastRootMetrics);
  metrics.SetViewport(CSSRect(CSSPoint(), viewport));
  metrics.mCompositionBounds = ParentLayerRect(
      ParentLayerPoint(),
      ParentLayerSize(ViewAs<ParentLayerPixel>(mInnerSize, PixelCastJustification::ScreenIsParentLayerForRoot)));
  metrics.SetRootCompositionSize(
      ScreenSize(mInnerSize) * ScreenToLayoutDeviceScale(1.0f) / metrics.GetDevPixelsPerCSSPixel());

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
  float oldIntrinsicScale =
      std::max(oldScreenSize.width / oldBrowserSize.width,
               oldScreenSize.height / oldBrowserSize.height);
  metrics.ZoomBy(metrics.CalculateIntrinsicScale().scale / oldIntrinsicScale);

  // Changing the zoom when we're not doing a first paint will get ignored
  // by AsyncPanZoomController and causes a blurry flash.
  bool isFirstPaint;
  nsresult rv = utils->GetIsFirstPaint(&isFirstPaint);
  if (NS_FAILED(rv) || isFirstPaint) {
    // FIXME/bug 799585(?): GetViewportInfo() returns a defaultZoom of
    // 0.0 to mean "did not calculate a zoom".  In that case, we default
    // it to the intrinsic scale.
    if (viewportInfo.GetDefaultZoom().scale < 0.01f) {
      viewportInfo.SetDefaultZoom(ConvertScaleForRoot(metrics.CalculateIntrinsicScale()));
    }

    CSSToScreenScale defaultZoom = viewportInfo.GetDefaultZoom();
    MOZ_ASSERT(viewportInfo.GetMinZoom() <= defaultZoom &&
               defaultZoom <= viewportInfo.GetMaxZoom());
    metrics.SetZoom(CSSToParentLayerScale2D(ConvertScaleForRoot(defaultZoom)));

    metrics.SetScrollId(viewId);
  }

  nsIPresShell* shell = document->GetShell();
  if (shell) {
    if (nsPresContext* context = shell->GetPresContext()) {
      metrics.SetDevPixelsPerCSSPixel(CSSToLayoutDeviceScale(
        (float)nsPresContext::AppUnitsPerCSSPixel() / context->AppUnitsPerDevPixel()));
    }
  }

  metrics.SetCumulativeResolution(metrics.GetZoom()
                                / metrics.GetDevPixelsPerCSSPixel()
                                * ParentLayerToLayerScale(1));
  // This is the root layer, so the cumulative resolution is the same
  // as the resolution.
  metrics.SetPresShellResolution(metrics.GetCumulativeResolution().ToScaleFactor().scale);
  if (shell) {
    nsLayoutUtils::SetResolutionAndScaleTo(shell, metrics.GetPresShellResolution());
  }

  CSSSize scrollPort = metrics.CalculateCompositedSizeInCssPixels();
  utils->SetScrollPositionClampingScrollPortSize(scrollPort.width, scrollPort.height);

  // The call to GetPageSize forces a resize event to content, so we need to
  // make sure that we have the right CSS viewport and
  // scrollPositionClampingScrollPortSize set up before that happens.

  CSSSize pageSize = GetPageSize(document, viewport);
  if (!pageSize.width) {
    // Return early rather than divide by 0.
    return false;
  }
  metrics.SetScrollableRect(CSSRect(CSSPoint(), pageSize));

  // Calculate a display port _after_ having a scrollable rect because the
  // display port is clamped to the scrollable rect.
  metrics.SetDisplayPortMargins(APZCTreeManager::CalculatePendingDisplayPort(
    // The page must have been refreshed in some way such as a new document or
    // new CSS viewport, so we know that there's no velocity, acceleration, and
    // we have no idea how long painting will take.
    metrics, ParentLayerPoint(0.0f, 0.0f), 0.0));
  metrics.SetUseDisplayPortMargins();

  // Force a repaint with these metrics. This, among other things, sets the
  // displayport, so we start with async painting.
  mLastRootMetrics = ProcessUpdateFrame(metrics);

  if (viewportInfo.IsZoomAllowed() && scrollIdentifiersValid) {
    // If the CSS viewport is narrower than the screen (i.e. width <= device-width)
    // then we disable double-tap-to-zoom behaviour.
    bool allowDoubleTapZoom = (viewport.width > screenW / metrics.GetDevPixelsPerCSSPixel().scale);
    if (allowDoubleTapZoom != viewportInfo.IsDoubleTapZoomAllowed()) {
      viewportInfo.SetAllowDoubleTapZoom(allowDoubleTapZoom);

      ZoomConstraints constraints(
        viewportInfo.IsZoomAllowed(),
        viewportInfo.IsDoubleTapZoomAllowed(),
        ConvertScaleForRoot(viewportInfo.GetMinZoom()),
        ConvertScaleForRoot(viewportInfo.GetMaxZoom()));
      DoUpdateZoomConstraints(presShellId,
                              viewId,
                              /* isRoot = */ true,
                              constraints);
    }
  }

  return true;
}

already_AddRefed<nsIDOMWindowUtils>
TabChildBase::GetDOMWindowUtils()
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(WebNavigation());
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  return utils.forget();
}

already_AddRefed<nsIDocument>
TabChildBase::GetDocument() const
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  WebNavigation()->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  return doc.forget();
}

void
TabChildBase::DispatchMessageManagerMessage(const nsAString& aMessageName,
                                            const nsAString& aJSONData)
{
    AutoSafeJSContext cx;
    JS::Rooted<JS::Value> json(cx, JSVAL_NULL);
    StructuredCloneData cloneData;
    JSAutoStructuredCloneBuffer buffer;
    if (JS_ParseJSON(cx,
                      static_cast<const char16_t*>(aJSONData.BeginReading()),
                      aJSONData.Length(),
                      &json)) {
        WriteStructuredClone(cx, json, buffer, cloneData.mClosure);
        cloneData.mData = buffer.data();
        cloneData.mDataLength = buffer.nbytes();
    }

    nsCOMPtr<nsIXPConnectJSObjectHolder> kungFuDeathGrip(GetGlobal());
    // Let the BrowserElementScrolling helper (if it exists) for this
    // content manipulate the frame state.
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildGlobal),
                       aMessageName, false, &cloneData, nullptr, nullptr, nullptr);
}

bool
TabChildBase::UpdateFrameHandler(const FrameMetrics& aFrameMetrics)
{
  MOZ_ASSERT(aFrameMetrics.GetScrollId() != FrameMetrics::NULL_SCROLL_ID);

  if (aFrameMetrics.GetIsRoot()) {
    nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
    if (APZCCallbackHelper::HasValidPresShellId(utils, aFrameMetrics)) {
      mLastRootMetrics = ProcessUpdateFrame(aFrameMetrics);
      return true;
    }
  } else {
    // aFrameMetrics.mIsRoot is false, so we are trying to update a subframe.
    // This requires special handling.
    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(
                                      aFrameMetrics.GetScrollId());
    if (content) {
      FrameMetrics newSubFrameMetrics(aFrameMetrics);
      APZCCallbackHelper::UpdateSubFrame(content, newSubFrameMetrics);
      return true;
    }
  }
  return true;
}

FrameMetrics
TabChildBase::ProcessUpdateFrame(const FrameMetrics& aFrameMetrics)
{
    if (!mGlobal || !mTabChildGlobal) {
        return aFrameMetrics;
    }

    nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());

    FrameMetrics newMetrics = aFrameMetrics;
    nsCOMPtr<nsIDocument> doc = GetDocument();
    if (doc && doc->GetShell()) {
      APZCCallbackHelper::UpdateRootFrame(utils, doc->GetShell(), newMetrics);
    }

    CSSSize cssCompositedSize = newMetrics.CalculateCompositedSizeInCssPixels();
    // The BrowserElementScrolling helper must know about these updated metrics
    // for other functions it performs, such as double tap handling.
    // Note, %f must not be used because it is locale specific!
    nsString data;
    data.AppendPrintf("{ \"x\" : %d", NS_lround(newMetrics.GetScrollOffset().x));
    data.AppendPrintf(", \"y\" : %d", NS_lround(newMetrics.GetScrollOffset().y));
    data.AppendLiteral(", \"viewport\" : ");
        data.AppendLiteral("{ \"width\" : ");
        data.AppendFloat(newMetrics.GetViewport().width);
        data.AppendLiteral(", \"height\" : ");
        data.AppendFloat(newMetrics.GetViewport().height);
        data.AppendLiteral(" }");
    data.AppendLiteral(", \"cssPageRect\" : ");
        data.AppendLiteral("{ \"x\" : ");
        data.AppendFloat(newMetrics.GetScrollableRect().x);
        data.AppendLiteral(", \"y\" : ");
        data.AppendFloat(newMetrics.GetScrollableRect().y);
        data.AppendLiteral(", \"width\" : ");
        data.AppendFloat(newMetrics.GetScrollableRect().width);
        data.AppendLiteral(", \"height\" : ");
        data.AppendFloat(newMetrics.GetScrollableRect().height);
        data.AppendLiteral(" }");
    data.AppendLiteral(", \"cssCompositedRect\" : ");
        data.AppendLiteral("{ \"width\" : ");
        data.AppendFloat(cssCompositedSize.width);
        data.AppendLiteral(", \"height\" : ");
        data.AppendFloat(cssCompositedSize.height);
        data.AppendLiteral(" }");
    data.AppendLiteral(" }");

    DispatchMessageManagerMessage(NS_LITERAL_STRING("Viewport:Change"), data);
    return newMetrics;
}

NS_IMETHODIMP
ContentListener::HandleEvent(nsIDOMEvent* aEvent)
{
  RemoteDOMEvent remoteEvent;
  remoteEvent.mEvent = do_QueryInterface(aEvent);
  NS_ENSURE_STATE(remoteEvent.mEvent);
  mTabChild->SendEvent(remoteEvent);
  return NS_OK;
}

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

    explicit CachedFileDescriptorInfo(const nsAString& aPath)
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
    explicit CachedFileDescriptorCallbackRunnable(CachedFileDescriptorInfo* aInfo)
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

class TabChild::DelayedDeleteRunnable final
  : public nsRunnable
{
    nsRefPtr<TabChild> mTabChild;

public:
    explicit DelayedDeleteRunnable(TabChild* aTabChild)
      : mTabChild(aTabChild)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(aTabChild);
    }

private:
    ~DelayedDeleteRunnable()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(!mTabChild);
    }

    NS_IMETHOD
    Run()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mTabChild);

        // Check in case ActorDestroy was called after RecvDestroy message.
        if (mTabChild->IPCOpen()) {
            unused << PBrowserChild::Send__delete__(mTabChild);
        }

        mTabChild = nullptr;
        return NS_OK;
    }
};

namespace {
StaticRefPtr<TabChild> sPreallocatedTab;

std::map<TabId, nsRefPtr<TabChild>>&
NestedTabChildMap()
{
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<TabId, nsRefPtr<TabChild>> sNestedTabChildMap;
  return sNestedTabChildMap;
}
} // anonymous namespace

already_AddRefed<TabChild>
TabChild::FindTabChild(const TabId& aTabId)
{
  auto iter = NestedTabChildMap().find(aTabId);
  if (iter == NestedTabChildMap().end()) {
    return nullptr;
  }
  nsRefPtr<TabChild> tabChild = iter->second;
  return tabChild.forget();
}

/*static*/ void
TabChild::PreloadSlowThings()
{
    MOZ_ASSERT(!sPreallocatedTab);

    // Pass nullptr to aManager since at this point the TabChild is
    // not connected to any manager. Any attempt to use the TabChild
    // in IPC will crash.
    nsRefPtr<TabChild> tab(new TabChild(nullptr,
                                        TabId(0),
                                        TabContext(), /* chromeFlags */ 0));
    if (!NS_SUCCEEDED(tab->Init()) ||
        !tab->InitTabChildGlobal(DONT_LOAD_SCRIPTS)) {
        return;
    }
    // Just load and compile these scripts, but don't run them.
    tab->TryCacheLoadAndCompileScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
    // Load, compile, and run these scripts.
    tab->RecvLoadRemoteScript(
        NS_LITERAL_STRING("chrome://global/content/preload.js"),
        true);

    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(tab->WebNavigation());
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

/*static*/ void
TabChild::PostForkPreload()
{
    // Preallocated Tab can be null if we are forked directly from b2g. In such
    // case we don't need to preload anything, just return.
    if (!sPreallocatedTab) {
        return;
    }

    // Rebuild connections to parent.
    sPreallocatedTab->RecvLoadRemoteScript(
      NS_LITERAL_STRING("chrome://global/content/post-fork-preload.js"),
      true);
}

/*static*/ already_AddRefed<TabChild>
TabChild::Create(nsIContentChild* aManager,
                 const TabId& aTabId,
                 const TabContext &aContext,
                 uint32_t aChromeFlags)
{
    if (sPreallocatedTab &&
        sPreallocatedTab->mChromeFlags == aChromeFlags &&
        aContext.IsBrowserOrApp()) {

        nsRefPtr<TabChild> child = sPreallocatedTab.get();
        sPreallocatedTab = nullptr;

        MOZ_ASSERT(!child->mTriedBrowserInit);

        child->mManager = aManager;
        child->SetTabId(aTabId);
        child->SetTabContext(aContext);
        child->NotifyTabContextUpdated();
        return child.forget();
    }

    nsRefPtr<TabChild> iframe = new TabChild(aManager, aTabId,
                                             aContext, aChromeFlags);
    return NS_SUCCEEDED(iframe->Init()) ? iframe.forget() : nullptr;
}

class TabChildSetAllowedTouchBehaviorCallback : public SetAllowedTouchBehaviorCallback {
public:
  explicit TabChildSetAllowedTouchBehaviorCallback(TabChild* aTabChild)
    : mTabChild(do_GetWeakReference(static_cast<nsITabChild*>(aTabChild)))
  {}

  void Run(uint64_t aInputBlockId, const nsTArray<TouchBehaviorFlags>& aFlags) const override {
    if (nsCOMPtr<nsITabChild> tabChild = do_QueryReferent(mTabChild)) {
      static_cast<TabChild*>(tabChild.get())->SendSetAllowedTouchBehavior(aInputBlockId, aFlags);
    }
  }

private:
  nsWeakPtr mTabChild;
};

class TabChildContentReceivedInputBlockCallback : public ContentReceivedInputBlockCallback {
public:
  explicit TabChildContentReceivedInputBlockCallback(TabChild* aTabChild)
    : mTabChild(do_GetWeakReference(static_cast<nsITabChild*>(aTabChild)))
  {}

  void Run(const ScrollableLayerGuid& aGuid, uint64_t aInputBlockId, bool aPreventDefault) const override {
    if (nsCOMPtr<nsITabChild> tabChild = do_QueryReferent(mTabChild)) {
      static_cast<TabChild*>(tabChild.get())->SendContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
    }
  }
private:
  nsWeakPtr mTabChild;
};

TabChild::TabChild(nsIContentChild* aManager,
                   const TabId& aTabId,
                   const TabContext& aContext,
                   uint32_t aChromeFlags)
  : TabContext(aContext)
  , mRemoteFrame(nullptr)
  , mManager(aManager)
  , mChromeFlags(aChromeFlags)
  , mLayersId(0)
  , mOuterRect(0, 0, 0, 0)
  , mActivePointerId(-1)
  , mAppPackageFileDescriptorRecved(false)
  , mLastBackgroundColor(NS_RGB(255, 255, 255))
  , mDidFakeShow(false)
  , mNotified(false)
  , mTriedBrowserInit(false)
  , mOrientation(eScreenOrientation_PortraitPrimary)
  , mUpdateHitRegion(false)
  , mIgnoreKeyPressEvent(false)
  , mSetAllowedTouchBehaviorCallback(new TabChildSetAllowedTouchBehaviorCallback(this))
  , mHasValidInnerSize(false)
  , mDestroyed(false)
  , mUniqueId(aTabId)
  , mDPI(0)
  , mDefaultScale(0)
  , mIPCOpen(true)
  , mParentIsActive(false)
{
  // preloaded TabChild should not be added to child map
  if (mUniqueId) {
    MOZ_ASSERT(NestedTabChildMap().find(mUniqueId) == NestedTabChildMap().end());
    NestedTabChildMap()[mUniqueId] = this;
  }
}

NS_IMETHODIMP
TabChild::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("DOMMetaAdded")) {
    // This meta data may or may not have been a meta viewport tag. If it was,
    // we should handle it immediately.
    HandlePossibleViewportChange(mInnerSize);
  } else if (eventType.EqualsLiteral("FullZoomChange")) {
    HandlePossibleViewportChange(mInnerSize);
  }

  return NS_OK;
}

NS_IMETHODIMP
TabChild::Observe(nsISupports *aSubject,
                  const char *aTopic,
                  const char16_t *aData)
{
  if (!strcmp(aTopic, BROWSER_ZOOM_TO_RECT)) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aSubject));
    nsCOMPtr<nsITabChild> tabChild(TabChild::GetFrom(docShell));
    if (tabChild == this) {
      nsCOMPtr<nsIDocument> doc(GetDocument());
      uint32_t presShellId;
      ViewID viewId;
      if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(doc->GetDocumentElement(),
                                                           &presShellId, &viewId)) {
        CSSRect rect;
        sscanf(NS_ConvertUTF16toUTF8(aData).get(),
               "{\"x\":%f,\"y\":%f,\"w\":%f,\"h\":%f}",
               &rect.x, &rect.y, &rect.width, &rect.height);
        SendZoomToRect(presShellId, viewId, rect);
      }
    }
  } else if (!strcmp(aTopic, BEFORE_FIRST_PAINT)) {
    if (gfxPrefs::AsyncPanZoomEnabled()) {
      nsCOMPtr<nsIDocument> subject(do_QueryInterface(aSubject));
      nsCOMPtr<nsIDocument> doc(GetDocument());

      if (SameCOMIdentity(subject, doc)) {
        nsCOMPtr<nsIDOMWindowUtils> utils(GetDOMWindowUtils());
        utils->SetIsFirstPaint(true);

        mContentDocumentIsDisplayed = true;

        // In some cases before-first-paint gets called before
        // RecvUpdateDimensions is called and therefore before we have an
        // mInnerSize value set. In such cases defer initializing the viewport
        // until we we get an inner size.
        if (HasValidInnerSize()) {
          InitializeRootMetrics();
          if (nsIPresShell* shell = doc->GetShell()) {
            nsLayoutUtils::SetResolutionAndScaleTo(shell, mLastRootMetrics.GetPresShellResolution());
          }
          HandlePossibleViewportChange(mInnerSize);
        }
      }
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
  if (!gfxPrefs::AsyncPanZoomEnabled()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindow> window;
  aWebProgress->GetDOMWindow(getter_AddRefs(window));
  if (!window) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDocument> progressDoc;
  window->GetDocument(getter_AddRefs(progressDoc));
  if (!progressDoc) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  WebNavigation()->GetDocument(getter_AddRefs(domDoc));
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
                         const char16_t* aMessage)
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

bool
TabChild::DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                  const ViewID& aViewId,
                                  const bool& aIsRoot,
                                  const ZoomConstraints& aConstraints)
{
  return SendUpdateZoomConstraints(aPresShellId,
                                   aViewId,
                                   aIsRoot,
                                   aConstraints);
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

  nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(WebNavigation()));
  docShellItem->SetItemType(nsIDocShellTreeItem::typeContentWrapper);
  
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
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
    nullptr                  // HandleWidgetEvent
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

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  docShell->SetAffectPrivateSessionLifetime(
      mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME);
  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(WebNavigation());
  MOZ_ASSERT(loadContext);
  loadContext->SetPrivateBrowsing(
      mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW);
  loadContext->SetRemoteTabs(
      mChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW);

  nsCOMPtr<nsIWebProgress> webProgress = do_GetInterface(docShell);
  NS_ENSURE_TRUE(webProgress, NS_ERROR_FAILURE);
  webProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_LOCATION);

  // Few lines before, baseWindow->Create() will end up creating a new
  // window root in nsGlobalWindow::SetDocShell.
  // Then this chrome event handler, will be inherited to inner windows.
  // We want to also set it to the docshell so that inner windows
  // and any code that has access to the docshell
  // can all listen to the same chrome event handler.
  // XXX: ideally, we would set a chrome event handler earlier,
  // and all windows, even the root one, will use the docshell one.
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsCOMPtr<EventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  docShell->SetChromeEventHandler(chromeHandler);

  mAPZEventState = new APZEventState(mWidget,
      new TabChildContentReceivedInputBlockCallback(this));

  return NS_OK;
}

void
TabChild::NotifyTabContextUpdated()
{
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TabChild)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsITabChild)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
NS_INTERFACE_MAP_END_INHERITING(TabChildBase)

NS_IMPL_ADDREF_INHERITED(TabChild, TabChildBase);
NS_IMPL_RELEASE_INHERITED(TabChild, TabChildBase);

NS_IMETHODIMP
TabChild::SetStatus(uint32_t aStatusType, const char16_t* aStatus)
{
  return SetStatusWithContext(aStatusType,
      aStatus ? static_cast<const nsString &>(nsDependentString(aStatus))
              : EmptyString(),
      nullptr);
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
  NS_NOTREACHED("TabChild::DestroyBrowserWindow not supported in TabChild");

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
  // We can only send the status after the ipc machinery is set up,
  // mRemoteFrame is a good indicator.
  if (mRemoteFrame)
    SendSetStatus(aStatusType, nsString(aStatusText));
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetDimensions(uint32_t aFlags, int32_t aX, int32_t aY,
                             int32_t aCx, int32_t aCy)
{
  unused << PBrowserChild::SendSetDimensions(aFlags, aX, aY, aCx, aCy);

  return NS_OK;
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
  NS_WARNING("TabChild::SetFocus not supported in TabChild");

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
TabChild::GetTitle(char16_t** aTitle)
{
  NS_NOTREACHED("TabChild::GetTitle not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetTitle(const char16_t* aTitle)
{
  // JavaScript sends the "DOMTitleChanged" event to the parent
  // via the message manager.
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
  NS_WARNING("TabChild::Blur not supported in TabChild");

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
    if (aIID.Equals(NS_GET_IID(nsIWebBrowserChrome3))) {
      NS_IF_ADDREF(((nsISupports *) (*aSink = mWebBrowserChrome)));
      return NS_OK;
    }

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
    bool iframeMoz = (docshell && docshell->GetIsInBrowserOrApp() &&
                      !(aChromeFlags & (nsIWebBrowserChrome::CHROME_MODAL |
                                        nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                                        nsIWebBrowserChrome::CHROME_OPENAS_CHROME)));

    if (!iframeMoz) {
      int32_t openLocation =
        nsWindowWatcher::GetWindowOpenLocation(aParent, aChromeFlags, aCalledFromJS,
                                               aPositionSpecified, aSizeSpecified);

      // If it turns out we're opening in the current browser, just hand over the
      // current browser's docshell.
      if (openLocation == nsIBrowserDOMWindow::OPEN_CURRENTWINDOW) {
        nsCOMPtr<nsIWebBrowser> browser = do_GetInterface(WebNavigation());
        *aWindowIsNew = false;
        return browser->GetContentDOMWindow(aReturn);
      }
    }

    // Note that ProvideWindowCommon may return NS_ERROR_ABORT if the
    // open window call was canceled.  It's important that we pass this error
    // code back to our caller.
    return ProvideWindowCommon(aParent,
                               iframeMoz,
                               aChromeFlags,
                               aCalledFromJS,
                               aPositionSpecified,
                               aSizeSpecified,
                               aURI,
                               aName,
                               aFeatures,
                               aWindowIsNew,
                               aReturn);
}

nsresult
TabChild::ProvideWindowCommon(nsIDOMWindow* aOpener,
                              bool aIframeMoz,
                              uint32_t aChromeFlags,
                              bool aCalledFromJS,
                              bool aPositionSpecified,
                              bool aSizeSpecified,
                              nsIURI* aURI,
                              const nsAString& aName,
                              const nsACString& aFeatures,
                              bool* aWindowIsNew,
                              nsIDOMWindow** aReturn)
{
  *aReturn = nullptr;

  ContentChild* cc = ContentChild::GetSingleton();
  const TabId openerTabId = GetTabId();

  // We must use PopupIPCTabContext here; ContentParent will not accept the
  // result of this->AsIPCTabContext() (which will be a
  // BrowserFrameIPCTabContext or an AppFrameIPCTabContext), for security
  // reasons.
  PopupIPCTabContext context;
  context.opener() = openerTabId;
  context.isBrowserElement() = IsBrowserElement();

  IPCTabContext ipcContext(context);

  TabId tabId;
  cc->SendAllocateTabId(openerTabId,
                        ipcContext,
                        cc->GetID(),
                        &tabId);

  nsRefPtr<TabChild> newChild = new TabChild(ContentChild::GetSingleton(), tabId,
                                             /* TabContext */ *this, aChromeFlags);
  if (NS_FAILED(newChild->Init())) {
    return NS_ERROR_ABORT;
  }

  context.opener() = this;
  unused << Manager()->SendPBrowserConstructor(
      // We release this ref in DeallocPBrowserChild
      nsRefPtr<TabChild>(newChild).forget().take(),
      tabId, IPCTabContext(context), aChromeFlags,
      cc->GetID(), cc->IsForApp(), cc->IsForBrowser());

  nsAutoCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }

  NS_ConvertUTF8toUTF16 url(spec);
  nsString name(aName);
  nsAutoCString features(aFeatures);
  nsTArray<FrameScriptInfo> frameScripts;
  nsCString urlToLoad;

  if (aIframeMoz) {
    newChild->SendBrowserFrameOpenWindow(this, url, name,
                                         NS_ConvertUTF8toUTF16(features),
                                         aWindowIsNew);
  } else {
    nsCOMPtr<nsIDOMDocument> domDoc;
    aOpener->GetDocument(getter_AddRefs(domDoc));
    if (!domDoc) {
      NS_ERROR("Could retrieve document from nsIBaseWindow");
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDocument> doc;
    doc = do_QueryInterface(domDoc);
    if (!doc) {
      NS_ERROR("Document from nsIBaseWindow didn't QI to nsIDocument");
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIURI> baseURI = doc->GetDocBaseURI();
    if (!baseURI) {
      NS_ERROR("nsIDocument didn't return a base URI");
      return NS_ERROR_FAILURE;
    }

    nsAutoCString baseURIString;
    baseURI->GetSpec(baseURIString);

    // We can assume that if content is requesting to open a window from a remote
    // tab, then we want to enforce that the new window is also a remote tab.
    features.AppendLiteral(",remote");

    if (!SendCreateWindow(newChild,
                          aChromeFlags, aCalledFromJS, aPositionSpecified,
                          aSizeSpecified, url,
                          name, NS_ConvertUTF8toUTF16(features),
                          NS_ConvertUTF8toUTF16(baseURIString),
                          aWindowIsNew,
                          &frameScripts,
                          &urlToLoad)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }
  if (!*aWindowIsNew) {
    PBrowserChild::Send__delete__(newChild);
    return NS_ERROR_ABORT;
  }

  TextureFactoryIdentifier textureFactoryIdentifier;
  uint64_t layersId = 0;
  PRenderFrameChild* renderFrame = newChild->SendPRenderFrameConstructor();
  newChild->SendGetRenderFrameInfo(renderFrame,
                                   &textureFactoryIdentifier,
                                   &layersId);
  if (layersId == 0) { // if renderFrame is invalid.
    PRenderFrameChild::Send__delete__(renderFrame);
    renderFrame = nullptr;
  }

  // Unfortunately we don't get a window unless we've shown the frame.  That's
  // pretty bogus; see bug 763602.
  newChild->DoFakeShow(textureFactoryIdentifier, layersId, renderFrame);

  for (size_t i = 0; i < frameScripts.Length(); i++) {
    FrameScriptInfo& info = frameScripts[i];
    if (!newChild->RecvLoadRemoteScript(info.url(), info.runInGlobalScope())) {
      MOZ_CRASH();
    }
  }

  if (!urlToLoad.IsEmpty()) {
    newChild->RecvLoadURL(urlToLoad, BrowserConfiguration());
  }

  nsCOMPtr<nsIDOMWindow> win = do_GetInterface(newChild->WebNavigation());
  win.forget(aReturn);
  return NS_OK;
}

bool
TabChild::HasValidInnerSize()
{
  return mHasValidInnerSize;
}

void
TabChild::DestroyWindow()
{
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
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


    if (mLayersId != 0) {
      MOZ_ASSERT(sTabChildren);
      sTabChildren->Remove(mLayersId);
      if (!sTabChildren->Count()) {
        delete sTabChildren;
        sTabChildren = nullptr;
      }
      mLayersId = 0;
    }

    for (uint32_t index = 0, count = mCachedFileDescriptorInfos.Length();
         index < count;
         index++) {
        nsAutoPtr<CachedFileDescriptorInfo>& info =
            mCachedFileDescriptorInfos[index];

        MOZ_ASSERT(!info->mCallback);

        if (info->mFileDescriptor.IsValid()) {
            MOZ_ASSERT(!info->mCanceled);

            nsRefPtr<CloseFileRunnable> runnable =
                new CloseFileRunnable(info->mFileDescriptor);
            runnable->Dispatch();
        }
    }

    mCachedFileDescriptorInfos.Clear();
}

void
TabChild::ActorDestroy(ActorDestroyReason why)
{
  mIPCOpen = false;

  DestroyWindow();

  if (mTabChildGlobal) {
    // The messageManager relays messages via the TabChild which
    // no longer exists.
    static_cast<nsFrameMessageManager*>
      (mTabChildGlobal->mMessageManager.get())->Disconnect();
    mTabChildGlobal->mMessageManager = nullptr;
  }

  CompositorChild* compositorChild = static_cast<CompositorChild*>(CompositorChild::Get());
  compositorChild->CancelNotifyAfterRemotePaint(this);

  if (GetTabId() != 0) {
    NestedTabChildMap().erase(GetTabId());
  }
}

TabChild::~TabChild()
{
    DestroyWindow();

    nsCOMPtr<nsIWebBrowser> webBrowser = do_QueryInterface(WebNavigation());
    if (webBrowser) {
      webBrowser->SetContainerWindow(nullptr);
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

  ContentChild::GetSingleton()->SetProcessName(appName, true);
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
TabChild::RecvLoadURL(const nsCString& aURI,
                      const BrowserConfiguration& aConfiguration)
{
    if (!InitTabChildGlobal()) {
      return false;
    }

    SetProcessNameToAppName();

    nsresult rv = WebNavigation()->LoadURI(NS_ConvertUTF8toUTF16(aURI).get(),
                                           nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
                                           nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_OWNER,
                                           nullptr, nullptr, nullptr);
    if (NS_FAILED(rv)) {
        NS_WARNING("WebNavigation()->LoadURI failed. Eating exception, what else can I do?");
    }

#ifdef MOZ_CRASHREPORTER
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("URL"), aURI);
#endif

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    MOZ_ASSERT(swm);
    swm->LoadRegistrations(aConfiguration.serviceWorkerRegistrations());

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
    size_t index =
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
    size_t index =
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
    size_t index =
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

    // No need to hold the callback any longer.
    info->mCallback = nullptr;

    // Set this flag so that we will close the file descriptor when it arrives.
    info->mCanceled = true;
}

void
TabChild::DoFakeShow(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                     const uint64_t& aLayersId,
                     PRenderFrameChild* aRenderFrame)
{
  ShowInfo info(EmptyString(), false, false, 0, 0);
  RecvShow(ScreenIntSize(0, 0), info, aTextureFactoryIdentifier,
           aLayersId, aRenderFrame, mParentIsActive);
  mDidFakeShow = true;
}

void
TabChild::ApplyShowInfo(const ShowInfo& aInfo)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (docShell) {
    nsCOMPtr<nsIDocShellTreeItem> item = do_GetInterface(docShell);
    if (IsBrowserOrApp()) {
      // B2G allows window.name to be set by changing the name attribute on the
      // <iframe mozbrowser> element. window.open calls cause this attribute to
      // be set to the correct value. A normal <xul:browser> element has no such
      // attribute. The data we get here comes from reading the attribute, so we
      // shouldn't trust it for <xul:browser> elements.
      item->SetName(aInfo.name());
    }
    docShell->SetFullscreenAllowed(aInfo.fullscreenAllowed());
    if (aInfo.isPrivate()) {
      bool nonBlank;
      docShell->GetHasLoadedNonBlankURI(&nonBlank);
      if (nonBlank) {
        nsContentUtils::ReportToConsoleNonLocalized(
          NS_LITERAL_STRING("We should not switch to Private Browsing after loading a document."),
          nsIScriptError::warningFlag,
          NS_LITERAL_CSTRING("mozprivatebrowsing"),
          nullptr);
      } else {
        nsCOMPtr<nsILoadContext> context = do_GetInterface(docShell);
        context->SetUsePrivateBrowsing(true);
      }
    }
  }
  mDPI = aInfo.dpi();
  mDefaultScale = aInfo.defaultScale();
}

#ifdef MOZ_WIDGET_GONK
void
TabChild::MaybeRequestPreinitCamera()
{
    // Check if this tab will use the `camera` permission.
    nsCOMPtr<nsIAppsService> appsService = do_GetService("@mozilla.org/AppsService;1");
    if (NS_WARN_IF(!appsService)) {
      return;
    }

    nsString manifestUrl = EmptyString();
    appsService->GetManifestURLByLocalId(OwnAppId(), manifestUrl);
    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    if (NS_WARN_IF(!secMan)) {
      return;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), manifestUrl);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<nsIPrincipal> principal;
    rv = secMan->GetAppCodebasePrincipal(uri, OwnAppId(), false,
                                         getter_AddRefs(principal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    uint16_t status = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
    principal->GetAppStatus(&status);
    bool isCertified = status == nsIPrincipal::APP_STATUS_CERTIFIED;
    if (!isCertified) {
      return;
    }

    nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
    if (NS_WARN_IF(!permMgr)) {
      return;
    }

    uint32_t permission = nsIPermissionManager::DENY_ACTION;
    permMgr->TestPermissionFromPrincipal(principal, "camera", &permission);
    bool hasPermission = permission == nsIPermissionManager::ALLOW_ACTION;
    if (!hasPermission) {
      return;
    }

    nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return;
    }

    observerService->NotifyObservers(nullptr, "init-camera-hw", nullptr);
}
#endif

bool
TabChild::RecvShow(const ScreenIntSize& aSize,
                   const ShowInfo& aInfo,
                   const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                   const uint64_t& aLayersId,
                   PRenderFrameChild* aRenderFrame,
                   const bool& aParentIsActive)
{
    MOZ_ASSERT((!mDidFakeShow && aRenderFrame) || (mDidFakeShow && !aRenderFrame));

    if (mDidFakeShow) {
        ApplyShowInfo(aInfo);
        RecvParentActivated(aParentIsActive);
        return true;
    }

    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
    if (!baseWindow) {
        NS_ERROR("WebNavigation() doesn't QI to nsIBaseWindow");
        return false;
    }

    if (!InitRenderingState(aTextureFactoryIdentifier, aLayersId, aRenderFrame)) {
        // We can fail to initialize our widget if the <browser
        // remote> has already been destroyed, and we couldn't hook
        // into the parent-process's layer system.  That's not a fatal
        // error.
        return true;
    }

    baseWindow->SetVisibility(true);

#ifdef MOZ_WIDGET_GONK
    MaybeRequestPreinitCamera();
#endif

    bool res = InitTabChildGlobal();
    ApplyShowInfo(aInfo);
    RecvParentActivated(aParentIsActive);
    return res;
}

bool
TabChild::RecvUpdateDimensions(const nsIntRect& rect, const ScreenIntSize& size,
                               const ScreenOrientation& orientation, const nsIntPoint& chromeDisp)
{
    if (!mRemoteFrame) {
        return true;
    }

    mOuterRect = rect;
    mChromeDisp = chromeDisp;

    bool initialSizing = !HasValidInnerSize()
                      && (size.width != 0 && size.height != 0);
    if (initialSizing) {
      mHasValidInnerSize = true;
    }

    mOrientation = orientation;
    ScreenIntSize oldScreenSize = mInnerSize;
    mInnerSize = size;
    mWidget->Resize(rect.x + chromeDisp.x, rect.y + chromeDisp.y, size.width, size.height,
                     true);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
    baseWin->SetPositionAndSize(0, 0, size.width, size.height,
                                true);

    if (initialSizing && mContentDocumentIsDisplayed) {
      // If this is the first time we're getting a valid mInnerSize, and the
      // before-first-paint event has already been handled, then we need to set
      // up our default viewport here. See the corresponding call to
      // InitializeRootMetrics in the before-first-paint handler.
      InitializeRootMetrics();
    }

    HandlePossibleViewportChange(oldScreenSize);

    return true;
}

bool
TabChild::RecvUpdateFrame(const FrameMetrics& aFrameMetrics)
{
  return TabChildBase::UpdateFrameHandler(aFrameMetrics);
}

bool
TabChild::RecvRequestFlingSnap(const FrameMetrics::ViewID& aScrollId,
                               const mozilla::CSSPoint& aDestination)
{
  APZCCallbackHelper::RequestFlingSnap(aScrollId, aDestination);
  return true;
}

bool
TabChild::RecvAcknowledgeScrollUpdate(const ViewID& aScrollId,
                                      const uint32_t& aScrollGeneration)
{
  APZCCallbackHelper::AcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
  return true;
}

bool
TabChild::RecvHandleDoubleTap(const CSSPoint& aPoint, const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid)
{
    TABC_LOG("Handling double tap at %s with %p %p\n",
      Stringify(aPoint).c_str(), mGlobal.get(), mTabChildGlobal.get());

    if (!mGlobal || !mTabChildGlobal) {
        return true;
    }

    // Note: there is nothing to do with the modifiers here, as we are not
    // synthesizing any sort of mouse event.
    CSSPoint point = APZCCallbackHelper::ApplyCallbackTransform(aPoint, aGuid,
        GetPresShellResolution());
    nsString data;
    data.AppendLiteral("{ \"x\" : ");
    data.AppendFloat(point.x);
    data.AppendLiteral(", \"y\" : ");
    data.AppendFloat(point.y);
    data.AppendLiteral(" }");

    DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:DoubleTap"), data);

    return true;
}

bool
TabChild::RecvHandleSingleTap(const CSSPoint& aPoint, const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid)
{
  if (mGlobal && mTabChildGlobal) {
    mAPZEventState->ProcessSingleTap(aPoint, aModifiers, aGuid, GetPresShellResolution());
  }
  return true;
}

bool
TabChild::RecvHandleLongTap(const CSSPoint& aPoint, const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId)
{
  if (mGlobal && mTabChildGlobal) {
    mAPZEventState->ProcessLongTap(GetDOMWindowUtils(), aPoint, aModifiers, aGuid,
        aInputBlockId, GetPresShellResolution());
  }
  return true;
}

bool
TabChild::RecvNotifyAPZStateChange(const ViewID& aViewId,
                                   const APZStateChange& aChange,
                                   const int& aArg)
{
  mAPZEventState->ProcessAPZStateChange(GetDocument(), aViewId, aChange, aArg);
  return true;
}

bool
TabChild::RecvActivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(WebNavigation());
  browser->Activate();
  return true;
}

bool TabChild::RecvDeactivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(WebNavigation());
  browser->Deactivate();
  return true;
}

bool TabChild::RecvParentActivated(const bool& aActivated)
{
  mParentIsActive = aActivated;

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, true);

  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(WebNavigation());
  fm->ParentActivated(window, aActivated);
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
  APZCCallbackHelper::DispatchMouseEvent(GetDOMWindowUtils(), aType, CSSPoint(aX, aY),
      aButton, aClickCount, aModifiers, aIgnoreRootScrollFrame, nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN);
  return true;
}

bool
TabChild::RecvRealMouseMoveEvent(const WidgetMouseEvent& event)
{
  return RecvRealMouseButtonEvent(event);
}

bool
TabChild::RecvRealMouseButtonEvent(const WidgetMouseEvent& event)
{
  WidgetMouseEvent localEvent(event);
  localEvent.widget = mWidget;
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  return true;
}

bool
TabChild::RecvMouseWheelEvent(const WidgetWheelEvent& aEvent,
                              const ScrollableLayerGuid& aGuid,
                              const uint64_t& aInputBlockId)
{
  if (gfxPrefs::AsyncPanZoomEnabled()) {
    nsCOMPtr<nsIDocument> document(GetDocument());
    APZCCallbackHelper::SendSetTargetAPZCNotification(WebWidget(), document, aEvent, aGuid,
        aInputBlockId);
  }

  WidgetWheelEvent event(aEvent);
  event.widget = mWidget;
  APZCCallbackHelper::DispatchWidgetEvent(event);

  if (gfxPrefs::AsyncPanZoomEnabled()) {
    mAPZEventState->ProcessWheelEvent(event, aGuid, aInputBlockId);
  }
  return true;
}

bool
TabChild::RecvMouseScrollTestEvent(const FrameMetrics::ViewID& aScrollId, const nsString& aEvent)
{
  APZCCallbackHelper::NotifyMozMouseScrollEvent(aScrollId, aEvent);
  return true;
}

static Touch*
GetTouchForIdentifier(const WidgetTouchEvent& aEvent, int32_t aId)
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
TabChild::UpdateTapState(const WidgetTouchEvent& aEvent, nsEventStatus aStatus)
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

  if (aEvent.touches.Length() == 0) {
    return;
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
        TouchManager::gPreventMouseEvents ||
        aEvent.mFlags.mMultipleActionsPrevented) {
      return;
    }

    Touch* touch = aEvent.touches[0];
    mGestureDownPoint = LayoutDevicePoint(touch->mRefPoint.x, touch->mRefPoint.y);
    mActivePointerId = touch->mIdentifier;
    if (sClickHoldContextMenusEnabled) {
      MOZ_ASSERT(!mTapHoldTimer);
      mTapHoldTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
      nsRefPtr<DelayedFireContextMenuEvent> callback =
        new DelayedFireContextMenuEvent(this);
      mTapHoldTimer->InitWithCallback(callback,
                                      sContextMenuDelayMs,
                                      nsITimer::TYPE_ONE_SHOT);
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

  LayoutDevicePoint currentPoint = LayoutDevicePoint(trackedTouch->mRefPoint.x, trackedTouch->mRefPoint.y);
  int64_t time = aEvent.time;
  switch (aEvent.message) {
  case NS_TOUCH_MOVE:
    if (std::abs(currentPoint.x - mGestureDownPoint.x) > sDragThreshold.width ||
        std::abs(currentPoint.y - mGestureDownPoint.y) > sDragThreshold.height) {
      CancelTapTracking();
    }
    return;

  case NS_TOUCH_END:
    if (!TouchManager::gPreventMouseEvents) {
      APZCCallbackHelper::DispatchSynthesizedMouseEvent(NS_MOUSE_MOVE, time, currentPoint, 0, mWidget);
      APZCCallbackHelper::DispatchSynthesizedMouseEvent(NS_MOUSE_BUTTON_DOWN, time, currentPoint, 0, mWidget);
      APZCCallbackHelper::DispatchSynthesizedMouseEvent(NS_MOUSE_BUTTON_UP, time, currentPoint, 0, mWidget);
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
  if (mDestroyed) {
    return;
  }

  double scale;
  GetDefaultScale(&scale);
  if (scale < 0) {
    scale = 1;
  }

  MOZ_ASSERT(mTapHoldTimer && mActivePointerId >= 0);
  bool defaultPrevented = APZCCallbackHelper::DispatchMouseEvent(
      GetDOMWindowUtils(),
      NS_LITERAL_STRING("contextmenu"),
      mGestureDownPoint / CSSToLayoutDeviceScale(scale),
      2 /* Right button */,
      1 /* Click count */,
      0 /* Modifiers */,
      true /* Ignore root scroll frame */,
      nsIDOMMouseEvent::MOZ_SOURCE_TOUCH);

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

float
TabChild::GetPresShellResolution() const
{
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsIPresShell* shell = document->GetShell();
  if (!shell) {
    return 1.0f;
  }
  return shell->GetResolution();
}

bool
TabChild::RecvRealTouchEvent(const WidgetTouchEvent& aEvent,
                             const ScrollableLayerGuid& aGuid,
                             const uint64_t& aInputBlockId)
{
  TABC_LOG("Receiving touch event of type %d\n", aEvent.message);

  WidgetTouchEvent localEvent(aEvent);
  localEvent.widget = mWidget;

  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
      mWidget->GetDefaultScale(), GetPresShellResolution());

  if (localEvent.message == NS_TOUCH_START && gfxPrefs::AsyncPanZoomEnabled()) {
    if (gfxPrefs::TouchActionEnabled()) {
      APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(WebWidget(),
          localEvent, aInputBlockId, mSetAllowedTouchBehaviorCallback);
    }
    nsCOMPtr<nsIDocument> document = GetDocument();
    APZCCallbackHelper::SendSetTargetAPZCNotification(WebWidget(), document,
        localEvent, aGuid, aInputBlockId);
  }

  // Dispatch event to content (potentially a long-running operation)
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  if (!gfxPrefs::AsyncPanZoomEnabled()) {
    UpdateTapState(localEvent, status);
    return true;
  }

  mAPZEventState->ProcessTouchEvent(localEvent, aGuid, aInputBlockId,
                                    nsEventStatus_eIgnore);
  return true;
}

bool
TabChild::RecvRealTouchMoveEvent(const WidgetTouchEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 const uint64_t& aInputBlockId)
{
  return RecvRealTouchEvent(aEvent, aGuid, aInputBlockId);
}

bool
TabChild::RecvRealDragEvent(const WidgetDragEvent& aEvent,
                            const uint32_t& aDragAction,
                            const uint32_t& aDropEffect)
{
  WidgetDragEvent localEvent(aEvent);
  localEvent.widget = mWidget;

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (dragSession) {
    dragSession->SetDragAction(aDragAction);
    nsCOMPtr<nsIDOMDataTransfer> initialDataTransfer;
    dragSession->GetDataTransfer(getter_AddRefs(initialDataTransfer));
    if (initialDataTransfer) {
      initialDataTransfer->SetDropEffectInt(aDropEffect);
    }
  }

  if (aEvent.message == NS_DRAGDROP_DROP) {
    bool canDrop = true;
    if (!dragSession || NS_FAILED(dragSession->GetCanDrop(&canDrop)) ||
        !canDrop) {
      localEvent.message = NS_DRAGDROP_EXIT;
    }
  } else if (aEvent.message == NS_DRAGDROP_OVER) {
    nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
    if (dragService) {
      // This will dispatch 'drag' event at the source if the
      // drag transaction started in this process.
      dragService->FireDragEventAtSource(NS_DRAGDROP_DRAG);
    }
  }

  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  return true;
}

void
TabChild::RequestNativeKeyBindings(AutoCacheNativeKeyCommands* aAutoCache,
                                   WidgetKeyboardEvent* aEvent)
{
  MaybeNativeKeyBinding maybeBindings;
  if (!SendRequestNativeKeyBindings(*aEvent, &maybeBindings)) {
    return;
  }

  if (maybeBindings.type() == MaybeNativeKeyBinding::TNativeKeyBinding) {
    const NativeKeyBinding& bindings = maybeBindings;
    aAutoCache->Cache(bindings.singleLineCommands(),
                      bindings.multiLineCommands(),
                      bindings.richTextCommands());
  } else {
    aAutoCache->CacheNoCommands();
  }
}

bool
TabChild::RecvNativeSynthesisResponse(const uint64_t& aObserverId,
                                      const nsCString& aResponse)
{
  mozilla::widget::AutoObserverNotifier::NotifySavedObserver(aObserverId, aResponse.get());
  return true;
}

bool
TabChild::RecvRealKeyEvent(const WidgetKeyboardEvent& event,
                           const MaybeNativeKeyBinding& aBindings)
{
  PuppetWidget* widget = static_cast<PuppetWidget*>(mWidget.get());
  AutoCacheNativeKeyCommands autoCache(widget);

  if (event.message == NS_KEY_PRESS) {
    if (aBindings.type() == MaybeNativeKeyBinding::TNativeKeyBinding) {
      const NativeKeyBinding& bindings = aBindings;
      autoCache.Cache(bindings.singleLineCommands(),
                      bindings.multiLineCommands(),
                      bindings.richTextCommands());
    } else {
      autoCache.CacheNoCommands();
    }
  }
  // If content code called preventDefault() on a keydown event, then we don't
  // want to process any following keypress events.
  if (event.message == NS_KEY_PRESS && mIgnoreKeyPressEvent) {
    return true;
  }

  WidgetKeyboardEvent localEvent(event);
  localEvent.widget = mWidget;
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  if (event.message == NS_KEY_DOWN) {
    mIgnoreKeyPressEvent = status == nsEventStatus_eConsumeNoDefault;
  }

  if (localEvent.mFlags.mWantReplyFromContentProcess) {
    SendReplyKeyEvent(localEvent);
  }

  if (PresShell::BeforeAfterKeyboardEventEnabled()) {
    SendDispatchAfterKeyboardEvent(localEvent);
  }

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
TabChild::RecvCompositionEvent(const WidgetCompositionEvent& event)
{
  WidgetCompositionEvent localEvent(event);
  localEvent.widget = mWidget;
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  return true;
}

bool
TabChild::RecvSelectionEvent(const WidgetSelectionEvent& event)
{
  WidgetSelectionEvent localEvent(event);
  localEvent.widget = mWidget;
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  return true;
}

PDocumentRendererChild*
TabChild::AllocPDocumentRendererChild(const nsRect& documentRect,
                                      const mozilla::gfx::Matrix& transform,
                                      const nsString& bgcolor,
                                      const uint32_t& renderFlags,
                                      const bool& flushLayout,
                                      const nsIntSize& renderSize)
{
    return new DocumentRendererChild();
}

bool
TabChild::DeallocPDocumentRendererChild(PDocumentRendererChild* actor)
{
    delete actor;
    return true;
}

bool
TabChild::RecvPDocumentRendererConstructor(PDocumentRendererChild* actor,
                                           const nsRect& documentRect,
                                           const mozilla::gfx::Matrix& transform,
                                           const nsString& bgcolor,
                                           const uint32_t& renderFlags,
                                           const bool& flushLayout,
                                           const nsIntSize& renderSize)
{
    DocumentRendererChild *render = static_cast<DocumentRendererChild *>(actor);

    nsCOMPtr<nsIWebBrowser> browser = do_QueryInterface(WebNavigation());
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

PColorPickerChild*
TabChild::AllocPColorPickerChild(const nsString&, const nsString&)
{
  NS_RUNTIMEABORT("unused");
  return nullptr;
}

bool
TabChild::DeallocPColorPickerChild(PColorPickerChild* aColorPicker)
{
  nsColorPickerProxy* picker = static_cast<nsColorPickerProxy*>(aColorPicker);
  NS_RELEASE(picker);
  return true;
}

PFilePickerChild*
TabChild::AllocPFilePickerChild(const nsString&, const int16_t&)
{
  NS_RUNTIMEABORT("unused");
  return nullptr;
}

bool
TabChild::DeallocPFilePickerChild(PFilePickerChild* actor)
{
  nsFilePickerProxy* filePicker = static_cast<nsFilePickerProxy*>(actor);
  NS_RELEASE(filePicker);
  return true;
}

auto
TabChild::AllocPIndexedDBPermissionRequestChild(const Principal& aPrincipal)
  -> PIndexedDBPermissionRequestChild*
{
  MOZ_CRASH("PIndexedDBPermissionRequestChild actors should always be created "
            "manually!");
}

bool
TabChild::DeallocPIndexedDBPermissionRequestChild(
                                       PIndexedDBPermissionRequestChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

bool
TabChild::RecvActivateFrameEvent(const nsString& aType, const bool& capture)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, true);
  nsCOMPtr<EventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, true);
  nsRefPtr<ContentListener> listener = new ContentListener(this);
  chromeHandler->AddEventListener(aType, listener, capture);
  return true;
}

bool
TabChild::RecvLoadRemoteScript(const nsString& aURL, const bool& aRunInGlobalScope)
{
  if (!mGlobal && !InitTabChildGlobal())
    // This can happen if we're half-destroyed.  It's not a fatal
    // error.
    return true;

  LoadScriptInternal(aURL, aRunInGlobalScope);
  return true;
}

bool
TabChild::RecvAsyncMessage(const nsString& aMessage,
                           const ClonedMessageData& aData,
                           InfallibleTArray<CpowEntry>&& aCpows,
                           const IPC::Principal& aPrincipal)
{
  if (mTabChildGlobal) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> kungFuDeathGrip(GetGlobal());
    StructuredCloneData cloneData = UnpackClonedMessageDataForChild(aData);
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    CrossProcessCpowHolder cpows(Manager(), aCpows);
    mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildGlobal),
                       aMessage, false, &cloneData, &cpows, aPrincipal, nullptr);
  }
  return true;
}

bool
TabChild::RecvAppOfflineStatus(const uint32_t& aId, const bool& aOffline)
{
  // Instantiate the service to make sure gIOService is initialized
  nsCOMPtr<nsIIOService> ioService = mozilla::services::GetIOService();
  if (gIOService && ioService) {
    gIOService->SetAppOfflineInternal(aId, aOffline ?
      nsIAppOfflineInfo::OFFLINE : nsIAppOfflineInfo::ONLINE);
  }
  return true;
}

bool
TabChild::RecvDestroy()
{
  MOZ_ASSERT(mDestroyed == false);
  mDestroyed = true;

  if (mTabChildGlobal) {
    // Message handlers are called from the event loop, so it better be safe to
    // run script.
    MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
    mTabChildGlobal->DispatchTrustedEvent(NS_LITERAL_STRING("unload"));
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  observerService->RemoveObserver(this, BROWSER_ZOOM_TO_RECT);
  observerService->RemoveObserver(this, BEFORE_FIRST_PAINT);

  // XXX what other code in ~TabChild() should we be running here?
  DestroyWindow();

  // Bounce through the event loop once to allow any delayed teardown runnables
  // that were just generated to have a chance to run.
  nsCOMPtr<nsIRunnable> deleteRunnable = new DelayedDeleteRunnable(this);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(deleteRunnable)));

  return true;
}

bool
TabChild::RecvSetUpdateHitRegion(const bool& aEnabled)
{
    mUpdateHitRegion = aEnabled;

    // We need to trigger a repaint of the child frame to ensure that it
    // recomputes and sends its region.
    if (!mUpdateHitRegion) {
      return true;
    }

    nsCOMPtr<nsIDocument> document(GetDocument());
    NS_ENSURE_TRUE(document, true);
    nsCOMPtr<nsIPresShell> presShell = document->GetShell();
    NS_ENSURE_TRUE(presShell, true);
    nsRefPtr<nsPresContext> presContext = presShell->GetPresContext();
    NS_ENSURE_TRUE(presContext, true);
    presContext->InvalidatePaintedLayers();

    return true;
}

bool
TabChild::RecvSetIsDocShellActive(const bool& aIsActive)
{
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
    if (docShell) {
      docShell->SetIsActive(aIsActive);
    }
    return true;
}

PRenderFrameChild*
TabChild::AllocPRenderFrameChild()
{
    return new RenderFrameChild();
}

bool
TabChild::DeallocPRenderFrameChild(PRenderFrameChild* aFrame)
{
    delete aFrame;
    return true;
}

bool
TabChild::InitTabChildGlobal(FrameScriptLoading aScriptLoading)
{
  if (!mGlobal && !mTabChildGlobal) {
    nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(WebNavigation());
    NS_ENSURE_TRUE(window, false);
    nsCOMPtr<EventTarget> chromeHandler =
      do_QueryInterface(window->GetChromeEventHandler());
    NS_ENSURE_TRUE(chromeHandler, false);

    nsRefPtr<TabChildGlobal> scope = new TabChildGlobal(this);
    mTabChildGlobal = scope;

    nsISupports* scopeSupports = NS_ISUPPORTS_CAST(EventTarget*, scope);

    NS_NAMED_LITERAL_CSTRING(globalId, "outOfProcessTabChildGlobal");
    NS_ENSURE_TRUE(InitChildGlobalInternal(scopeSupports, globalId), false);

    scope->Init();

    nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
    NS_ENSURE_TRUE(root, false);
    root->SetParentTarget(scope);

    chromeHandler->AddEventListener(NS_LITERAL_STRING("DOMMetaAdded"), this, false);
    chromeHandler->AddEventListener(NS_LITERAL_STRING("FullZoomChange"), this, false);
  }

  if (aScriptLoading != DONT_LOAD_SCRIPTS && !mTriedBrowserInit) {
    mTriedBrowserInit = true;
    // Initialize the child side of the browser element machinery,
    // if appropriate.
    if (IsBrowserOrApp()) {
      RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
    }
  }

  return true;
}

bool
TabChild::InitRenderingState(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                             const uint64_t& aLayersId,
                             PRenderFrameChild* aRenderFrame)
{
    static_cast<PuppetWidget*>(mWidget.get())->InitIMEState();

    RenderFrameChild* remoteFrame = static_cast<RenderFrameChild*>(aRenderFrame);
    if (!remoteFrame) {
        NS_WARNING("failed to construct RenderFrame");
        return false;
    }

    MOZ_ASSERT(aLayersId != 0);
    mTextureFactoryIdentifier = aTextureFactoryIdentifier;

    // Pushing layers transactions directly to a separate
    // compositor context.
    PCompositorChild* compositorChild = CompositorChild::Get();
    if (!compositorChild) {
      NS_WARNING("failed to get CompositorChild instance");
      PRenderFrameChild::Send__delete__(remoteFrame);
      return false;
    }
    nsTArray<LayersBackend> backends;
    backends.AppendElement(mTextureFactoryIdentifier.mParentBackend);
    bool success;
    PLayerTransactionChild* shadowManager =
        compositorChild->SendPLayerTransactionConstructor(backends,
                                                          aLayersId, &mTextureFactoryIdentifier, &success);
    if (!success) {
      NS_WARNING("failed to properly allocate layer transaction");
      PRenderFrameChild::Send__delete__(remoteFrame);
      return false;
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
    MOZ_ASSERT(lf && lf->HasShadowManager(),
               "PuppetWidget should have shadow manager");
    lf->IdentifyTextureHost(mTextureFactoryIdentifier);
    ImageBridgeChild::IdentifyCompositorTextureHost(mTextureFactoryIdentifier);

    mRemoteFrame = remoteFrame;
    if (aLayersId != 0) {
      if (!sTabChildren) {
        sTabChildren = new TabChildMap;
      }
      MOZ_ASSERT(!sTabChildren->Get(aLayersId));
      sTabChildren->Put(aLayersId, this);
      mLayersId = aLayersId;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    if (observerService) {
        observerService->AddObserver(this,
                                     BROWSER_ZOOM_TO_RECT,
                                     false);
        observerService->AddObserver(this,
                                     BEFORE_FIRST_PAINT,
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

    if (mDPI > 0) {
      *aDPI = mDPI;
      return;
    }

    // Fallback to a sync call if needed.
    SendGetDPI(aDPI);
}

void
TabChild::GetDefaultScale(double* aScale)
{
    *aScale = -1.0;
    if (!mRemoteFrame) {
        return;
    }

    if (mDefaultScale > 0) {
      *aScale = mDefaultScale;
      return;
    }

    // Fallback to a sync call if needed.
    SendGetDefaultScale(aScale);
}

void
TabChild::NotifyPainted()
{
    if (!mNotified) {
        mRemoteFrame->SendNotifyCompositorTransaction();
        mNotified = true;
    }
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

void
TabChild::UpdateHitRegion(const nsRegion& aRegion)
{
    mRemoteFrame->SendUpdateHitRegion(aRegion);
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

NS_IMETHODIMP
TabChild::GetWebBrowserChrome(nsIWebBrowserChrome3** aWebBrowserChrome)
{
  NS_IF_ADDREF(*aWebBrowserChrome = mWebBrowserChrome);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetWebBrowserChrome(nsIWebBrowserChrome3* aWebBrowserChrome)
{
  mWebBrowserChrome = aWebBrowserChrome;
  return NS_OK;
}

void
TabChild::SendRequestFocus(bool aCanFocus)
{
  PBrowserChild::SendRequestFocus(aCanFocus);
}

void
TabChild::EnableDisableCommands(const nsAString& aAction,
                                nsTArray<nsCString>& aEnabledCommands,
                                nsTArray<nsCString>& aDisabledCommands)
{
  PBrowserChild::SendEnableDisableCommands(PromiseFlatString(aAction),
                                           aEnabledCommands, aDisabledCommands);
}


NS_IMETHODIMP
TabChild::GetTabId(uint64_t* aId)
{
  *aId = GetTabId();
  return NS_OK;
}

void
TabChild::SetTabId(const TabId& aTabId)
{
  MOZ_ASSERT(mUniqueId == 0);

  mUniqueId = aTabId;
  NestedTabChildMap()[mUniqueId] = this;
}

bool
TabChild::DoSendBlockingMessage(JSContext* aCx,
                                const nsAString& aMessage,
                                const StructuredCloneData& aData,
                                JS::Handle<JSObject *> aCpows,
                                nsIPrincipal* aPrincipal,
                                InfallibleTArray<nsString>* aJSONRetVal,
                                bool aIsSync)
{
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(Manager(), aData, data)) {
    return false;
  }
  InfallibleTArray<CpowEntry> cpows;
  if (aCpows && !Manager()->GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
    return false;
  }
  if (aIsSync) {
    return SendSyncMessage(PromiseFlatString(aMessage), data, cpows,
                           Principal(aPrincipal), aJSONRetVal);
  }

  return SendRpcMessage(PromiseFlatString(aMessage), data, cpows,
                        Principal(aPrincipal), aJSONRetVal);
}

bool
TabChild::DoSendAsyncMessage(JSContext* aCx,
                             const nsAString& aMessage,
                             const StructuredCloneData& aData,
                             JS::Handle<JSObject *> aCpows,
                             nsIPrincipal* aPrincipal)
{
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(Manager(), aData, data)) {
    return false;
  }
  InfallibleTArray<CpowEntry> cpows;
  if (aCpows && !Manager()->GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
    return false;
  }
  return SendAsyncMessage(PromiseFlatString(aMessage), data, cpows,
                          Principal(aPrincipal));
}

TabChild*
TabChild::GetFrom(nsIPresShell* aPresShell)
{
  nsIDocument* doc = aPresShell->GetDocument();
  if (!doc) {
      return nullptr;
  }
  nsCOMPtr<nsIDocShell> docShell(doc->GetDocShell());
  return GetFrom(docShell);
}

TabChild*
TabChild::GetFrom(uint64_t aLayersId)
{
  if (!sTabChildren) {
    return nullptr;
  }
  return sTabChildren->Get(aLayersId);
}

void
TabChild::DidComposite(uint64_t aTransactionId)
{
  MOZ_ASSERT(mWidget);
  MOZ_ASSERT(mWidget->GetLayerManager());
  MOZ_ASSERT(mWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT);

  ClientLayerManager *manager = static_cast<ClientLayerManager*>(mWidget->GetLayerManager());
  manager->DidComposite(aTransactionId);
}

NS_IMETHODIMP
TabChild::OnShowTooltip(int32_t aXCoords, int32_t aYCoords, const char16_t *aTipText)
{
    nsString str(aTipText);
    SendShowTooltip(aXCoords, aYCoords, str);
    return NS_OK;
}

NS_IMETHODIMP
TabChild::OnHideTooltip()
{
    SendHideTooltip();
    return NS_OK;
}

bool
TabChild::RecvRequestNotifyAfterRemotePaint()
{
  // Get the CompositorChild instance for this content thread.
  CompositorChild* compositor = CompositorChild::Get();

  // Tell the CompositorChild that, when it gets a RemotePaintIsReady
  // message that it should forward it us so that we can bounce it to our
  // RenderFrameParent.
  compositor->RequestNotifyAfterRemotePaint(this);
  return true;
}

bool
TabChild::RecvUIResolutionChanged()
{
  mDPI = 0;
  mDefaultScale = 0;
  static_cast<PuppetWidget*>(mWidget.get())->ClearBackingScaleCache();
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  nsRefPtr<nsPresContext> presContext = presShell->GetPresContext();
  presContext->UIResolutionChanged();
  return true;
}

mozilla::plugins::PPluginWidgetChild*
TabChild::AllocPPluginWidgetChild()
{
    return new mozilla::plugins::PluginWidgetChild();
}

bool
TabChild::DeallocPPluginWidgetChild(mozilla::plugins::PPluginWidgetChild* aActor)
{
    delete aActor;
    return true;
}

nsresult
TabChild::CreatePluginWidget(nsIWidget* aParent, nsIWidget** aOut)
{
  *aOut = nullptr;
  mozilla::plugins::PluginWidgetChild* child =
    static_cast<mozilla::plugins::PluginWidgetChild*>(SendPPluginWidgetConstructor());
  if (!child) {
    NS_ERROR("couldn't create PluginWidgetChild");
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<nsIWidget> pluginWidget = nsIWidget::CreatePluginProxyWidget(this, child);
  if (!pluginWidget) {
    NS_ERROR("couldn't create PluginWidgetProxy");
    return NS_ERROR_UNEXPECTED;
  }

  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_plugin_ipc_content;
  initData.mUnicode = false;
  initData.clipChildren = true;
  initData.clipSiblings = true;
  nsresult rv = pluginWidget->Create(aParent, nullptr, nsIntRect(nsIntPoint(0, 0),
                                     nsIntSize(0, 0)), &initData);
  if (NS_FAILED(rv)) {
    NS_WARNING("Creating native plugin widget on the chrome side failed.");
  }
  pluginWidget.forget(aOut);
  return rv;
}

TabChildGlobal::TabChildGlobal(TabChildBase* aTabChild)
: mTabChild(aTabChild)
{
  SetIsNotDOMBinding();
}

TabChildGlobal::~TabChildGlobal()
{
}

void
TabChildGlobal::Init()
{
  NS_ASSERTION(!mMessageManager, "Re-initializing?!?");
  mMessageManager = new nsFrameMessageManager(mTabChild,
                                              nullptr,
                                              MM_CHILD);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(TabChildGlobal, DOMEventTargetHelper,
                                   mMessageManager, mTabChild)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TabChildGlobal)
  NS_INTERFACE_MAP_ENTRY(nsIMessageListenerManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsISyncMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentFrameMessageManager)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TabChildGlobal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TabChildGlobal, DOMEventTargetHelper)

/* [notxpcom] boolean markForCC (); */
// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
TabChildGlobal::MarkForCC()
{
  if (mTabChild) {
    mTabChild->MarkScopesForCC();
  }
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
TabChildGlobal::GetDocShell(nsIDocShell** aDocShell)
{
  *aDocShell = nullptr;
  if (!mTabChild)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mTabChild->WebNavigation());
  docShell.swap(*aDocShell);
  return NS_OK;
}

JSContext*
TabChildGlobal::GetJSContextForEventHandlers()
{
  return nsContentUtils::GetSafeJSContext();
}

nsIPrincipal*
TabChildGlobal::GetPrincipal()
{
  if (!mTabChild)
    return nullptr;
  return mTabChild->GetPrincipal();
}

JSObject*
TabChildGlobal::GetGlobalJSObject()
{
  NS_ENSURE_TRUE(mTabChild, nullptr);
  nsCOMPtr<nsIXPConnectJSObjectHolder> ref = mTabChild->GetGlobal();
  NS_ENSURE_TRUE(ref, nullptr);
  return ref->GetJSObject();
}

