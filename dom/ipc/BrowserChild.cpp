/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BrowserChild.h"

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/DocAccessibleChild.h"
#endif
#include <algorithm>
#include <utility>

#include "BrowserParent.h"
#include "ContentChild.h"
#include "DocumentInlines.h"
#include "EventStateManager.h"
#include "Layers.h"
#include "MMPrinter.h"
#include "PermissionMessageUtils.h"
#include "PuppetWidget.h"
#include "StructuredCloneData.h"
#include "UnitTransforms.h"
#include "Units.h"
#include "VRManagerChild.h"
#include "ipc/nsGUIEventIPC.h"
#include "js/JSON.h"
#include "mozilla/Assertions.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventForwards.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/NativeKeyBindingsType.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/ToString.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/AutoPrintEventDispatcher.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/PBrowser.h"
#include "mozilla/dom/PaymentRequestChild.h"
#include "mozilla/dom/PointerEventHandler.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/SessionStoreChild.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/gfx/CrossProcessPaint.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/TouchActionHelper.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ContentProcessController.h"
#include "mozilla/layers/DoubleTapToZoom.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "nsBrowserStatusFilter.h"
#include "nsColorPickerProxy.h"
#include "nsCommandParams.h"
#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "nsDeviceContext.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsEmbedCID.h"
#include "nsExceptionHandler.h"
#include "nsFilePickerProxy.h"
#include "nsFocusManager.h"
#include "nsGlobalWindow.h"
#include "nsIBaseWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIClassifiedChannel.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsILoadContext.h"
#include "nsISHEntry.h"
#include "nsISHistory.h"
#include "nsIScreenManager.h"
#include "nsIScriptError.h"
#include "nsISecureBrowserUI.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIWebBrowser.h"
#include "nsIWebProgress.h"
#include "nsLayoutUtils.h"
#include "nsNetUtil.h"
#include "nsIOpenWindowInfo.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsPointerHashKeys.h"
#include "nsPrintfCString.h"
#include "nsQueryActor.h"
#include "nsQueryObject.h"
#include "nsRefreshDriver.h"
#include "nsSandboxFlags.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "nsViewManager.h"
#include "nsViewportInfo.h"
#include "nsWebBrowser.h"
#include "nsWindowWatcher.h"
#include "nsIXULRuntime.h"

#ifdef MOZ_WAYLAND
#  include "nsAppRunner.h"
#endif

#ifdef NS_PRINTING
#  include "mozilla/layout/RemotePrintJobChild.h"
#  include "nsIPrintSettings.h"
#  include "nsIPrintSettingsService.h"
#  include "nsIWebBrowserPrint.h"
#endif

static mozilla::LazyLogModule sApzChildLog("apz.child");

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::widget;
using mozilla::layers::GeckoContentController;

NS_IMPL_ISUPPORTS(ContentListener, nsIDOMEventListener)

static const char BEFORE_FIRST_PAINT[] = "before-first-paint";

static uint32_t sConsecutiveTouchMoveCount = 0;

using BrowserChildMap = nsTHashMap<nsUint64HashKey, BrowserChild*>;
static BrowserChildMap* sBrowserChildren;
StaticMutex sBrowserChildrenMutex;

already_AddRefed<Document> BrowserChild::GetTopLevelDocument() const {
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  nsCOMPtr<Document> doc = docShell ? docShell->GetExtantDocument() : nullptr;
  return doc.forget();
}

PresShell* BrowserChild::GetTopLevelPresShell() const {
  if (RefPtr<Document> doc = GetTopLevelDocument()) {
    return doc->GetPresShell();
  }
  return nullptr;
}

bool BrowserChild::UpdateFrame(const RepaintRequest& aRequest) {
  MOZ_ASSERT(aRequest.GetScrollId() != ScrollableLayerGuid::NULL_SCROLL_ID);

  if (aRequest.IsRootContent()) {
    if (PresShell* presShell = GetTopLevelPresShell()) {
      // Guard against stale updates (updates meant for a pres shell which
      // has since been torn down and destroyed).
      if (aRequest.GetPresShellId() == presShell->GetPresShellId()) {
        APZCCallbackHelper::UpdateRootFrame(aRequest);
        return true;
      }
    }
  } else {
    // aRequest.mIsRoot is false, so we are trying to update a subframe.
    // This requires special handling.
    APZCCallbackHelper::UpdateSubFrame(aRequest);
    return true;
  }
  return true;
}

NS_IMETHODIMP
ContentListener::HandleEvent(Event* aEvent) {
  RemoteDOMEvent remoteEvent;
  remoteEvent.mEvent = aEvent;
  NS_ENSURE_STATE(remoteEvent.mEvent);
  mBrowserChild->SendEvent(remoteEvent);
  return NS_OK;
}

class BrowserChild::DelayedDeleteRunnable final : public Runnable,
                                                  public nsIRunnablePriority {
  RefPtr<BrowserChild> mBrowserChild;

  // In order to try that this runnable runs after everything that could
  // possibly touch this tab, we send it through the event queue twice.
  bool mReadyToDelete = false;

 public:
  explicit DelayedDeleteRunnable(BrowserChild* aBrowserChild)
      : Runnable("BrowserChild::DelayedDeleteRunnable"),
        mBrowserChild(aBrowserChild) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aBrowserChild);
  }

  NS_DECL_ISUPPORTS_INHERITED

 private:
  ~DelayedDeleteRunnable() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mBrowserChild);
  }

  NS_IMETHOD GetPriority(uint32_t* aPriority) override {
    *aPriority = nsIRunnablePriority::PRIORITY_NORMAL;
    return NS_OK;
  }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mBrowserChild);

    if (!mReadyToDelete) {
      // This time run this runnable at input priority.
      mReadyToDelete = true;
      MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(this));
      return NS_OK;
    }

    // Check in case ActorDestroy was called after RecvDestroy message.
    if (mBrowserChild->IPCOpen()) {
      Unused << PBrowserChild::Send__delete__(mBrowserChild);
    }

    mBrowserChild = nullptr;
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS_INHERITED(BrowserChild::DelayedDeleteRunnable, Runnable,
                            nsIRunnablePriority)

namespace {
std::map<TabId, RefPtr<BrowserChild>>& NestedBrowserChildMap() {
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<TabId, RefPtr<BrowserChild>> sNestedBrowserChildMap;
  return sNestedBrowserChildMap;
}
}  // namespace

already_AddRefed<BrowserChild> BrowserChild::FindBrowserChild(
    const TabId& aTabId) {
  auto iter = NestedBrowserChildMap().find(aTabId);
  if (iter == NestedBrowserChildMap().end()) {
    return nullptr;
  }
  RefPtr<BrowserChild> browserChild = iter->second;
  return browserChild.forget();
}

/*static*/
already_AddRefed<BrowserChild> BrowserChild::Create(
    ContentChild* aManager, const TabId& aTabId, const TabContext& aContext,
    BrowsingContext* aBrowsingContext, uint32_t aChromeFlags,
    bool aIsTopLevel) {
  RefPtr<BrowserChild> iframe = new BrowserChild(
      aManager, aTabId, aContext, aBrowsingContext, aChromeFlags, aIsTopLevel);
  return iframe.forget();
}

BrowserChild::BrowserChild(ContentChild* aManager, const TabId& aTabId,
                           const TabContext& aContext,
                           BrowsingContext* aBrowsingContext,
                           uint32_t aChromeFlags, bool aIsTopLevel)
    : TabContext(aContext),
      mBrowserChildMessageManager(nullptr),
      mManager(aManager),
      mBrowsingContext(aBrowsingContext),
      mChromeFlags(aChromeFlags),
      mMaxTouchPoints(0),
      mLayersId{0},
      mEffectsInfo{EffectsInfo::FullyHidden()},
      mDynamicToolbarMaxHeight(0),
      mUniqueId(aTabId),
      mDidFakeShow(false),
      mTriedBrowserInit(false),
      mIgnoreKeyPressEvent(false),
      mHasValidInnerSize(false),
      mDestroyed(false),
      mIsTopLevel(aIsTopLevel),
      mHasSiblings(false),
      mIsTransparent(false),
      mIPCOpen(false),
      mDidSetRealShowInfo(false),
      mDidLoadURLInit(false),
      mSkipKeyPress(false),
      mDidSetEffectsInfo(false),
      mShouldSendWebProgressEventsToParent(false),
      mRenderLayers(true),
      mIsPreservingLayers(false),
      mPendingDocShellIsActive(false),
      mPendingDocShellReceivedMessage(false),
      mPendingRenderLayers(false),
      mPendingRenderLayersReceivedMessage(false),
      mLayersObserverEpoch{1},
#if defined(XP_WIN) && defined(ACCESSIBILITY)
      mNativeWindowHandle(0),
#endif
#if defined(ACCESSIBILITY)
      mTopLevelDocAccessibleChild(nullptr),
#endif
      mPendingLayersObserverEpoch{0},
      mPendingDocShellBlockers(0),
      mCancelContentJSEpoch(0) {
  mozilla::HoldJSObjects(this);

  // preloaded BrowserChild should not be added to child map
  if (mUniqueId) {
    MOZ_ASSERT(NestedBrowserChildMap().find(mUniqueId) ==
               NestedBrowserChildMap().end());
    NestedBrowserChildMap()[mUniqueId] = this;
  }
  mCoalesceMouseMoveEvents = StaticPrefs::dom_events_coalesce_mousemove();
  if (mCoalesceMouseMoveEvents) {
    mCoalescedMouseEventFlusher = new CoalescedMouseMoveFlusher(this);
  }

  if (StaticPrefs::dom_events_coalesce_touchmove()) {
    mCoalescedTouchMoveEventFlusher = new CoalescedTouchMoveFlusher(this);
  }
}

const CompositorOptions& BrowserChild::GetCompositorOptions() const {
  // If you're calling this before mCompositorOptions is set, well.. don't.
  MOZ_ASSERT(mCompositorOptions);
  return mCompositorOptions.ref();
}

bool BrowserChild::AsyncPanZoomEnabled() const {
  // This might get called by the TouchEvent::PrefEnabled code before we have
  // mCompositorOptions populated (bug 1370089). In that case we just assume
  // APZ is enabled because we're in a content process (because BrowserChild)
  // and APZ is probably going to be enabled here since e10s is enabled.
  return mCompositorOptions ? mCompositorOptions->UseAPZ() : true;
}

NS_IMETHODIMP
BrowserChild::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData) {
  if (!strcmp(aTopic, BEFORE_FIRST_PAINT)) {
    if (AsyncPanZoomEnabled()) {
      nsCOMPtr<Document> subject(do_QueryInterface(aSubject));
      nsCOMPtr<Document> doc(GetTopLevelDocument());

      if (subject == doc) {
        RefPtr<PresShell> presShell = doc->GetPresShell();
        if (presShell) {
          presShell->SetIsFirstPaint(true);
        }

        APZCCallbackHelper::InitializeRootDisplayport(presShell);
      }
    }
  }

  return NS_OK;
}

void BrowserChild::ContentReceivedInputBlock(uint64_t aInputBlockId,
                                             bool aPreventDefault) const {
  if (mApzcTreeManager) {
    mApzcTreeManager->ContentReceivedInputBlock(aInputBlockId, aPreventDefault);
  }
}

void BrowserChild::SetTargetAPZC(
    uint64_t aInputBlockId,
    const nsTArray<ScrollableLayerGuid>& aTargets) const {
  if (mApzcTreeManager) {
    mApzcTreeManager->SetTargetAPZC(aInputBlockId, aTargets);
  }
}

bool BrowserChild::DoUpdateZoomConstraints(
    const uint32_t& aPresShellId, const ViewID& aViewId,
    const Maybe<ZoomConstraints>& aConstraints) {
  if (!mApzcTreeManager || mDestroyed) {
    return false;
  }

  ScrollableLayerGuid guid =
      ScrollableLayerGuid(mLayersId, aPresShellId, aViewId);

  mApzcTreeManager->UpdateZoomConstraints(guid, aConstraints);
  return true;
}

nsresult BrowserChild::Init(mozIDOMWindowProxy* aParent,
                            WindowGlobalChild* aInitialWindowChild) {
  MOZ_ASSERT_IF(aInitialWindowChild,
                aInitialWindowChild->BrowsingContext() == mBrowsingContext);

  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(this);
  mPuppetWidget = static_cast<PuppetWidget*>(widget.get());
  if (!mPuppetWidget) {
    NS_ERROR("couldn't create fake widget");
    return NS_ERROR_FAILURE;
  }
  mPuppetWidget->InfallibleCreate(nullptr,
                                  nullptr,  // no parents
                                  LayoutDeviceIntRect(0, 0, 0, 0),
                                  nullptr);  // HandleWidgetEvent

  mWebBrowser = nsWebBrowser::Create(this, mPuppetWidget, mBrowsingContext,
                                     aInitialWindowChild);
  nsIWebBrowser* webBrowser = mWebBrowser;

  mWebNav = do_QueryInterface(webBrowser);
  NS_ASSERTION(mWebNav, "nsWebBrowser doesn't implement nsIWebNavigation?");

  // IPC uses a WebBrowser object for which DNS prefetching is turned off
  // by default. But here we really want it, so enable it explicitly
  mWebBrowser->SetAllowDNSPrefetch(true);

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  mStatusFilter = new nsBrowserStatusFilter();

  nsresult rv =
      mStatusFilter->AddProgressListener(this, nsIWebProgress::NOTIFY_ALL);
  NS_ENSURE_SUCCESS(rv, rv);

  {
    nsCOMPtr<nsIWebProgress> webProgress = do_QueryInterface(docShell);
    rv = webProgress->AddProgressListener(mStatusFilter,
                                          nsIWebProgress::NOTIFY_ALL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#ifdef DEBUG
  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(WebNavigation());
  MOZ_ASSERT(loadContext);
  MOZ_ASSERT(loadContext->UseRemoteTabs() ==
             !!(mChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW));
  MOZ_ASSERT(loadContext->UseRemoteSubframes() ==
             !!(mChromeFlags & nsIWebBrowserChrome::CHROME_FISSION_WINDOW));
#endif  // defined(DEBUG)

  // Few lines before, baseWindow->Create() will end up creating a new
  // window root in nsGlobalWindow::SetDocShell.
  // Then this chrome event handler, will be inherited to inner windows.
  // We want to also set it to the docshell so that inner windows
  // and any code that has access to the docshell
  // can all listen to the same chrome event handler.
  // XXX: ideally, we would set a chrome event handler earlier,
  // and all windows, even the root one, will use the docshell one.
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsCOMPtr<EventTarget> chromeHandler = window->GetChromeEventHandler();
  docShell->SetChromeEventHandler(chromeHandler);

  if (window->GetCurrentInnerWindow()) {
    window->SetKeyboardIndicators(ShowFocusRings());
  } else {
    // Skip ShouldShowFocusRing check if no inner window is available
    window->SetInitialKeyboardIndicators(ShowFocusRings());
  }

  // Window scrollbar flags only affect top level remote frames, not fission
  // frames.
  if (mIsTopLevel) {
    nsContentUtils::SetScrollbarsVisibility(
        docShell, !!(mChromeFlags & nsIWebBrowserChrome::CHROME_SCROLLBARS));
  }

  nsWeakPtr weakPtrThis = do_GetWeakReference(
      static_cast<nsIBrowserChild*>(this));  // for capture by the lambda
  ContentReceivedInputBlockCallback callback(
      [weakPtrThis](uint64_t aInputBlockId, bool aPreventDefault) {
        if (nsCOMPtr<nsIBrowserChild> browserChild =
                do_QueryReferent(weakPtrThis)) {
          static_cast<BrowserChild*>(browserChild.get())
              ->ContentReceivedInputBlock(aInputBlockId, aPreventDefault);
        }
      });
  mAPZEventState = new APZEventState(mPuppetWidget, std::move(callback));

  mIPCOpen = true;

  if constexpr (SessionStoreUtils::NATIVE_LISTENER) {
    mSessionStoreChild = SessionStoreChild::GetOrCreate(mBrowsingContext);
  }

  // We've all set up, make sure our visibility state is consistent. This is
  // important for OOP iframes, which start off as hidden.
  UpdateVisibility();

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowserChild)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowserChild)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserChildMessageManager)
  tmp->nsMessageManagerScriptExecutor::Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStatusFilter)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWebNav)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSessionStoreChild)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContentTransformPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowserChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserChildMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStatusFilter)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebNav)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSessionStoreChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContentTransformPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(BrowserChild)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowserChild)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
  NS_INTERFACE_MAP_ENTRY(nsIBrowserChild)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITooltipListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener2)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIBrowserChild)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(BrowserChild)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BrowserChild)

NS_IMETHODIMP
BrowserChild::GetChromeFlags(uint32_t* aChromeFlags) {
  *aChromeFlags = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::SetChromeFlags(uint32_t aChromeFlags) {
  NS_WARNING("trying to SetChromeFlags from content process?");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BrowserChild::RemoteSizeShellTo(int32_t aWidth, int32_t aHeight,
                                int32_t aShellItemWidth,
                                int32_t aShellItemHeight) {
  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(ourDocShell));
  NS_ENSURE_STATE(docShellAsWin);

  int32_t width, height;
  docShellAsWin->GetSize(&width, &height);

  uint32_t flags = 0;
  if (width == aWidth) {
    flags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX;
  }

  if (height == aHeight) {
    flags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY;
  }

  bool sent = SendSizeShellTo(flags, aWidth, aHeight, aShellItemWidth,
                              aShellItemHeight);

  return sent ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
BrowserChild::RemoteDropLinks(
    const nsTArray<RefPtr<nsIDroppedLinkItem>>& aLinks) {
  nsTArray<nsString> linksArray;
  nsresult rv = NS_OK;
  for (nsIDroppedLinkItem* link : aLinks) {
    nsString tmp;
    rv = link->GetUrl(tmp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    linksArray.AppendElement(tmp);

    rv = link->GetName(tmp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    linksArray.AppendElement(tmp);

    rv = link->GetType(tmp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    linksArray.AppendElement(tmp);
  }
  bool sent = SendDropLinks(linksArray);

  return sent ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
BrowserChild::ShowAsModal() {
  NS_WARNING("BrowserChild::ShowAsModal not supported in BrowserChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BrowserChild::IsWindowModal(bool* aRetVal) {
  *aRetVal = false;
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::SetLinkStatus(const nsAString& aStatusText) {
  // We can only send the status after the ipc machinery is set up
  if (IPCOpen()) {
    SendSetLinkStatus(nsString(aStatusText));
  }
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::SetDimensions(uint32_t aFlags, int32_t aX, int32_t aY,
                            int32_t aCx, int32_t aCy) {
  // The parent is in charge of the dimension changes. If JS code wants to
  // change the dimensions (moveTo, screenX, etc.) we send a message to the
  // parent about the new requested dimension, the parent does the resize/move
  // then send a message to the child to update itself. For APIs like screenX
  // this function is called with the current value for the non-changed values.
  // In a series of calls like window.screenX = 10; window.screenY = 10; for
  // the second call, since screenX is not yet updated we might accidentally
  // reset back screenX to it's old value. To avoid this if a parameter did not
  // change we want the parent to ignore its value.
  int32_t x, y, cx, cy;
  GetDimensions(aFlags, &x, &y, &cx, &cy);

  if (x == aX) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_X;
  }

  if (y == aY) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_Y;
  }

  if (cx == aCx) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX;
  }

  if (cy == aCy) {
    aFlags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY;
  }

  double scale = mPuppetWidget ? mPuppetWidget->GetDefaultScale().scale : 1.0;

  Unused << SendSetDimensions(aFlags, aX, aY, aCx, aCy, scale);

  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::GetDimensions(uint32_t aFlags, int32_t* aX, int32_t* aY,
                            int32_t* aCx, int32_t* aCy) {
  ScreenIntRect rect = GetOuterRect();
  if (aX) {
    *aX = rect.x;
  }
  if (aY) {
    *aY = rect.y;
  }
  if (aCx) {
    *aCx = rect.width;
  }
  if (aCy) {
    *aCy = rect.height;
  }

  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::GetVisibility(bool* aVisibility) {
  *aVisibility = true;
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::SetVisibility(bool aVisibility) {
  // should the platform support this? Bug 666365
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::GetTitle(nsAString& aTitle) {
  NS_WARNING("BrowserChild::GetTitle not supported in BrowserChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BrowserChild::SetTitle(const nsAString& aTitle) {
  // JavaScript sends the "DOMTitleChanged" event to the parent
  // via the message manager.
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::GetSiteWindow(void** aSiteWindow) {
  NS_WARNING("BrowserChild::GetSiteWindow not supported in BrowserChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BrowserChild::Blur() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
BrowserChild::FocusNextElement(bool aForDocumentNavigation) {
  SendMoveFocus(true, aForDocumentNavigation);
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::FocusPrevElement(bool aForDocumentNavigation) {
  SendMoveFocus(false, aForDocumentNavigation);
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::GetInterface(const nsIID& aIID, void** aSink) {
  // XXXbz should we restrict the set of interfaces we hand out here?
  // See bug 537429
  return QueryInterface(aIID, aSink);
}

NS_IMETHODIMP
BrowserChild::ProvideWindow(nsIOpenWindowInfo* aOpenWindowInfo,
                            uint32_t aChromeFlags, bool aCalledFromJS,
                            nsIURI* aURI, const nsAString& aName,
                            const nsACString& aFeatures, bool aForceNoOpener,
                            bool aForceNoReferrer, bool aIsPopupRequested,
                            nsDocShellLoadState* aLoadState, bool* aWindowIsNew,
                            BrowsingContext** aReturn) {
  *aReturn = nullptr;

  RefPtr<BrowsingContext> parent = aOpenWindowInfo->GetParent();

  int32_t openLocation = nsWindowWatcher::GetWindowOpenLocation(
      parent->GetDOMWindow(), aChromeFlags, aCalledFromJS,
      aOpenWindowInfo->GetIsForPrinting());

  // If it turns out we're opening in the current browser, just hand over the
  // current browser's docshell.
  if (openLocation == nsIBrowserDOMWindow::OPEN_CURRENTWINDOW) {
    nsCOMPtr<nsIWebBrowser> browser = do_GetInterface(WebNavigation());
    *aWindowIsNew = false;

    nsCOMPtr<mozIDOMWindowProxy> win;
    MOZ_TRY(browser->GetContentDOMWindow(getter_AddRefs(win)));

    RefPtr<BrowsingContext> bc(
        nsPIDOMWindowOuter::From(win)->GetBrowsingContext());
    bc.forget(aReturn);
    return NS_OK;
  }

  // Note that ProvideWindowCommon may return NS_ERROR_ABORT if the
  // open window call was canceled.  It's important that we pass this error
  // code back to our caller.
  ContentChild* cc = ContentChild::GetSingleton();
  return cc->ProvideWindowCommon(
      this, aOpenWindowInfo, aChromeFlags, aCalledFromJS, aURI, aName,
      aFeatures, aForceNoOpener, aForceNoReferrer, aIsPopupRequested,
      aLoadState, aWindowIsNew, aReturn);
}

void BrowserChild::DestroyWindow() {
  mBrowsingContext = nullptr;

  if (mStatusFilter) {
    if (nsCOMPtr<nsIWebProgress> webProgress =
            do_QueryInterface(WebNavigation())) {
      webProgress->RemoveProgressListener(mStatusFilter);
    }

    mStatusFilter->RemoveProgressListener(this);
    mStatusFilter = nullptr;
  }

  if (mCoalescedMouseEventFlusher) {
    mCoalescedMouseEventFlusher->RemoveObserver();
    mCoalescedMouseEventFlusher = nullptr;
  }

  if (mCoalescedTouchMoveEventFlusher) {
    mCoalescedTouchMoveEventFlusher->RemoveObserver();
    mCoalescedTouchMoveEventFlusher = nullptr;
  }

  if (mSessionStoreChild) {
    mSessionStoreChild->Stop();
    mSessionStoreChild = nullptr;
  }

  // In case we don't have chance to process all entries, clean all data in
  // the queue.
  while (mToBeDispatchedMouseData.GetSize() > 0) {
    UniquePtr<CoalescedMouseData> data(
        static_cast<CoalescedMouseData*>(mToBeDispatchedMouseData.PopFront()));
    data.reset();
  }

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
  if (baseWindow) baseWindow->Destroy();

  if (mPuppetWidget) {
    mPuppetWidget->Destroy();
  }

  mLayersConnected = Nothing();

  if (mLayersId.IsValid()) {
    StaticMutexAutoLock lock(sBrowserChildrenMutex);

    MOZ_ASSERT(sBrowserChildren);
    sBrowserChildren->Remove(uint64_t(mLayersId));
    if (!sBrowserChildren->Count()) {
      delete sBrowserChildren;
      sBrowserChildren = nullptr;
    }
    mLayersId = layers::LayersId{0};
  }
}

void BrowserChild::ActorDestroy(ActorDestroyReason why) {
  mIPCOpen = false;

  DestroyWindow();

  if (mBrowserChildMessageManager) {
    // We should have a message manager if the global is alive, but it
    // seems sometimes we don't.  Assert in aurora/nightly, but don't
    // crash in release builds.
    MOZ_DIAGNOSTIC_ASSERT(mBrowserChildMessageManager->GetMessageManager());
    if (mBrowserChildMessageManager->GetMessageManager()) {
      // The messageManager relays messages via the BrowserChild which
      // no longer exists.
      mBrowserChildMessageManager->DisconnectMessageManager();
    }
  }

  if (GetTabId() != 0) {
    NestedBrowserChildMap().erase(GetTabId());
  }
}

BrowserChild::~BrowserChild() {
  mAnonymousGlobalScopes.Clear();

  DestroyWindow();

  nsCOMPtr<nsIWebBrowser> webBrowser = do_QueryInterface(WebNavigation());
  if (webBrowser) {
    webBrowser->SetContainerWindow(nullptr);
  }

  mozilla::DropJSObjects(this);
}

mozilla::ipc::IPCResult BrowserChild::RecvWillChangeProcess() {
  if (mWebBrowser) {
    mWebBrowser->SetWillChangeProcess();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvLoadURL(
    nsDocShellLoadState* aLoadState, const ParentShowInfo& aInfo) {
  if (!mDidLoadURLInit) {
    mDidLoadURLInit = true;
    if (!InitBrowserChildMessageManager()) {
      return IPC_FAIL_NO_REASON(this);
    }

    ApplyParentShowInfo(aInfo);
  }
  nsAutoCString spec;
  aLoadState->URI()->GetSpec(spec);

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    NS_WARNING("WebNavigation does not have a docshell");
    return IPC_OK();
  }
  docShell->LoadURI(aLoadState, true);

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::URL, spec);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvCreateAboutBlankContentViewer(
    nsIPrincipal* aPrincipal, nsIPrincipal* aPartitionedPrincipal) {
  if (aPrincipal->GetIsExpandedPrincipal() ||
      aPartitionedPrincipal->GetIsExpandedPrincipal()) {
    return IPC_FAIL(this, "Cannot create document with an expanded principal");
  }
  if (aPrincipal->IsSystemPrincipal() ||
      aPartitionedPrincipal->IsSystemPrincipal()) {
    MOZ_ASSERT_UNREACHABLE(
        "Cannot use CreateAboutBlankContentViewer to create system principal "
        "document in content");
    return IPC_OK();
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    MOZ_ASSERT_UNREACHABLE("WebNavigation does not have a docshell");
    return IPC_OK();
  }

  nsCOMPtr<nsIURI> currentURI;
  MOZ_ALWAYS_SUCCEEDS(
      WebNavigation()->GetCurrentURI(getter_AddRefs(currentURI)));
  if (!currentURI || !NS_IsAboutBlank(currentURI)) {
    NS_WARNING("Can't create a ContentViewer unless on about:blank");
    return IPC_OK();
  }

  docShell->CreateAboutBlankContentViewer(aPrincipal, aPartitionedPrincipal,
                                          nullptr);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvResumeLoad(
    const uint64_t& aPendingSwitchID, const ParentShowInfo& aInfo) {
  if (!mDidLoadURLInit) {
    mDidLoadURLInit = true;
    if (!InitBrowserChildMessageManager()) {
      return IPC_FAIL_NO_REASON(this);
    }

    ApplyParentShowInfo(aInfo);
  }

  nsresult rv = WebNavigation()->ResumeRedirectedLoad(aPendingSwitchID, -1);
  if (NS_FAILED(rv)) {
    NS_WARNING("WebNavigation()->ResumeRedirectedLoad failed");
  }

  return IPC_OK();
}

nsresult BrowserChild::CloneDocumentTreeIntoSelf(
    const MaybeDiscarded<BrowsingContext>& aSourceBC,
    const embedding::PrintData& aPrintData) {
#ifdef NS_PRINTING
  if (NS_WARN_IF(aSourceBC.IsNullOrDiscarded())) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<Document> sourceDocument = aSourceBC.get()->GetDocument();
  if (NS_WARN_IF(!sourceDocument)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  if (NS_WARN_IF(!ourDocShell)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContentViewer> cv;
  ourDocShell->GetContentViewer(getter_AddRefs(cv));
  if (NS_WARN_IF(!cv)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (NS_WARN_IF(!printSettingsSvc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrintSettings> printSettings;
  nsresult rv =
      printSettingsSvc->CreateNewPrintSettings(getter_AddRefs(printSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  printSettingsSvc->DeserializeToPrintSettings(aPrintData, printSettings);

  RefPtr<Document> clone;
  {
    AutoPrintEventDispatcher dispatcher(*sourceDocument);
    nsAutoScriptBlocker scriptBlocker;
    bool hasInProcessCallbacks = false;
    clone = sourceDocument->CreateStaticClone(ourDocShell, cv, printSettings,
                                              &hasInProcessCallbacks);
    if (NS_WARN_IF(!clone)) {
      return NS_ERROR_FAILURE;
    }
  }

  rv = UpdateRemotePrintSettings(aPrintData);
  if (NS_FAILED(rv)) {
    return rv;
  }

#endif
  return NS_OK;
}

mozilla::ipc::IPCResult BrowserChild::RecvCloneDocumentTreeIntoSelf(
    const MaybeDiscarded<BrowsingContext>& aSourceBC,
    const embedding::PrintData& aPrintData,
    CloneDocumentTreeIntoSelfResolver&& aResolve) {
  nsresult rv = NS_OK;

#ifdef NS_PRINTING
  rv = CloneDocumentTreeIntoSelf(aSourceBC, aPrintData);
#endif

  aResolve(NS_SUCCEEDED(rv));
  return IPC_OK();
}

nsresult BrowserChild::UpdateRemotePrintSettings(
    const embedding::PrintData& aPrintData) {
#ifdef NS_PRINTING
  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  if (NS_WARN_IF(!ourDocShell)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<Document> doc = ourDocShell->GetExtantDocument();
  if (NS_WARN_IF(!doc) || NS_WARN_IF(!doc->IsStaticDocument())) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<BrowsingContext> bc = ourDocShell->GetBrowsingContext();
  if (NS_WARN_IF(!bc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (NS_WARN_IF(!printSettingsSvc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrintSettings> printSettings;
  nsresult rv =
      printSettingsSvc->CreateNewPrintSettings(getter_AddRefs(printSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  printSettingsSvc->DeserializeToPrintSettings(aPrintData, printSettings);

  bc->PreOrderWalk([&](BrowsingContext* aBc) {
    if (nsCOMPtr<nsIDocShell> inProcess = aBc->GetDocShell()) {
      nsCOMPtr<nsIContentViewer> cv;
      inProcess->GetContentViewer(getter_AddRefs(cv));
      if (NS_WARN_IF(!cv)) {
        return BrowsingContext::WalkFlag::Skip;
      }
      // The CanRunScript analysis is not smart enough to see across
      // the std::function PreOrderWalk uses, so we cheat a bit here, but it is
      // fine because PreOrderWalk does deal with arbitrary script changing the
      // BC tree, and our code above is simple enough and keeps strong refs to
      // everything.
      ([&]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
        RefPtr<RemotePrintJobChild> printJob =
            static_cast<RemotePrintJobChild*>(aPrintData.remotePrintJobChild());
        cv->SetPrintSettingsForSubdocument(printSettings, printJob);
      }());
    } else if (RefPtr<BrowserBridgeChild> remoteChild =
                   BrowserBridgeChild::GetFrom(aBc->GetEmbedderElement())) {
      Unused << remoteChild->SendUpdateRemotePrintSettings(aPrintData);
      return BrowsingContext::WalkFlag::Skip;
    }
    return BrowsingContext::WalkFlag::Next;
  });
#endif

  return NS_OK;
}

mozilla::ipc::IPCResult BrowserChild::RecvUpdateRemotePrintSettings(
    const embedding::PrintData& aPrintData) {
#ifdef NS_PRINTING
  UpdateRemotePrintSettings(aPrintData);
#endif

  return IPC_OK();
}

void BrowserChild::DoFakeShow(const ParentShowInfo& aParentShowInfo) {
  OwnerShowInfo ownerInfo{ScreenIntSize(), ScrollbarPreference::Auto,
                          nsSizeMode_Normal};
  RecvShow(aParentShowInfo, ownerInfo);
  mDidFakeShow = true;
}

void BrowserChild::ApplyParentShowInfo(const ParentShowInfo& aInfo) {
  // Even if we already set real show info, the dpi / rounding & scale may still
  // be invalid (if BrowserParent wasn't able to get widget it would just send
  // 0). So better to always set up-to-date values here.
  if (aInfo.dpi() > 0) {
    mPuppetWidget->UpdateBackingScaleCache(aInfo.dpi(), aInfo.widgetRounding(),
                                           aInfo.defaultScale());
  }

  if (mDidSetRealShowInfo) {
    return;
  }

  if (!aInfo.fakeShowInfo()) {
    // Once we've got one ShowInfo from parent, no need to update the values
    // anymore.
    mDidSetRealShowInfo = true;
  }

  mIsTransparent = aInfo.isTransparent();
}

mozilla::ipc::IPCResult BrowserChild::RecvShow(
    const ParentShowInfo& aParentInfo, const OwnerShowInfo& aOwnerInfo) {
  bool res = true;

  mPuppetWidget->SetSizeMode(aOwnerInfo.sizeMode());
  if (!mDidFakeShow) {
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
    if (!baseWindow) {
      NS_ERROR("WebNavigation() doesn't QI to nsIBaseWindow");
      return IPC_FAIL_NO_REASON(this);
    }

    baseWindow->SetVisibility(true);
    res = InitBrowserChildMessageManager();
  }

  ApplyParentShowInfo(aParentInfo);

  if (!mIsTopLevel) {
    RecvScrollbarPreferenceChanged(aOwnerInfo.scrollbarPreference());
  }

  if (!res) {
    return IPC_FAIL_NO_REASON(this);
  }

  UpdateVisibility();

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvInitRendering(
    const TextureFactoryIdentifier& aTextureFactoryIdentifier,
    const layers::LayersId& aLayersId,
    const CompositorOptions& aCompositorOptions, const bool& aLayersConnected) {
  mLayersConnected = Some(aLayersConnected);
  mLayersConnectRequested = Some(aLayersConnected);
  InitRenderingState(aTextureFactoryIdentifier, aLayersId, aCompositorOptions);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvScrollbarPreferenceChanged(
    ScrollbarPreference aPreference) {
  MOZ_ASSERT(!mIsTopLevel,
             "Scrollbar visibility should be derived from chrome flags for "
             "top-level windows");
  if (nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation())) {
    nsDocShell::Cast(docShell)->SetScrollbarPreference(aPreference);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvCompositorOptionsChanged(
    const CompositorOptions& aNewOptions) {
  MOZ_ASSERT(mCompositorOptions);

  // The only compositor option we currently support changing is APZ
  // enablement. Even that is only partially supported for now:
  //   * Going from APZ to non-APZ is fine - we just flip the stored flag.
  //     Note that we keep the actors (mApzcTreeManager, and the APZChild
  //     created in InitAPZState()) around (read on for why).
  //   * Going from non-APZ to APZ is only supported if we were using
  //     APZ initially (at InitRendering() time) and we are transitioning
  //     back. In this case, we just reuse the actors which we kept around.
  //     Fully supporting a non-APZ to APZ transition (i.e. even in cases
  //     where we initialized as non-APZ) would require setting up the actors
  //     here. (In that case, we would also have the options of destroying
  //     the actors in the APZ --> non-APZ case, and always re-creating them
  //     during a non-APZ --> APZ transition).
  mCompositorOptions->SetUseAPZ(aNewOptions.UseAPZ());
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvUpdateDimensions(
    const DimensionInfo& aDimensionInfo) {
  if (mLayersConnected.isNothing()) {
    return IPC_OK();
  }

  mUnscaledOuterRect = aDimensionInfo.rect();
  mClientOffset = aDimensionInfo.clientOffset();
  mChromeOffset = aDimensionInfo.chromeOffset();
  MOZ_ASSERT_IF(!IsTopLevel(), mChromeOffset == LayoutDeviceIntPoint());

  SetUnscaledInnerSize(aDimensionInfo.size());
  if (!mHasValidInnerSize && aDimensionInfo.size().width != 0 &&
      aDimensionInfo.size().height != 0) {
    mHasValidInnerSize = true;
  }

  ScreenIntSize screenSize = GetInnerSize();
  ScreenIntRect screenRect = GetOuterRect();

  // Make sure to set the size on the document viewer first.  The
  // MobileViewportManager needs the content viewer size to be updated before
  // the reflow, otherwise it gets a stale size when it computes a new CSS
  // viewport.
  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
  baseWin->SetPositionAndSize(0, 0, screenSize.width, screenSize.height,
                              nsIBaseWindow::eRepaint);

  mPuppetWidget->Resize(screenRect.x + mClientOffset.x + mChromeOffset.x,
                        screenRect.y + mClientOffset.y + mChromeOffset.y,
                        screenSize.width, screenSize.height, true);

  RecvSafeAreaInsetsChanged(mPuppetWidget->GetSafeAreaInsets());

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvSizeModeChanged(
    const nsSizeMode& aSizeMode) {
  mPuppetWidget->SetSizeMode(aSizeMode);
  if (!mPuppetWidget->IsVisible()) {
    return IPC_OK();
  }
  nsCOMPtr<Document> document(GetTopLevelDocument());
  if (!document) {
    return IPC_OK();
  }
  nsPresContext* presContext = document->GetPresContext();
  if (presContext) {
    presContext->SizeModeChanged(aSizeMode);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvChildToParentMatrix(
    const Maybe<gfx::Matrix4x4>& aMatrix,
    const ScreenRect& aTopLevelViewportVisibleRectInBrowserCoords) {
  mChildToParentConversionMatrix =
      LayoutDeviceToLayoutDeviceMatrix4x4::FromUnknownMatrix(aMatrix);
  mTopLevelViewportVisibleRectInBrowserCoords =
      aTopLevelViewportVisibleRectInBrowserCoords;

  if (mContentTransformPromise) {
    mContentTransformPromise->MaybeResolveWithUndefined();
    mContentTransformPromise = nullptr;
  }

  // Trigger an intersection observation update since ancestor viewports
  // changed.
  if (RefPtr<Document> toplevelDoc = GetTopLevelDocument()) {
    if (nsPresContext* pc = toplevelDoc->GetPresContext()) {
      pc->RefreshDriver()->EnsureIntersectionObservationsUpdateHappens();
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvSetIsUnderHiddenEmbedderElement(
    const bool& aIsUnderHiddenEmbedderElement) {
  if (RefPtr<PresShell> presShell = GetTopLevelPresShell()) {
    presShell->SetIsUnderHiddenEmbedderElement(aIsUnderHiddenEmbedderElement);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvDynamicToolbarMaxHeightChanged(
    const ScreenIntCoord& aHeight) {
#if defined(MOZ_WIDGET_ANDROID)
  mDynamicToolbarMaxHeight = aHeight;

  RefPtr<Document> document = GetTopLevelDocument();
  if (!document) {
    return IPC_OK();
  }

  if (RefPtr<nsPresContext> presContext = document->GetPresContext()) {
    presContext->SetDynamicToolbarMaxHeight(aHeight);
  }
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvDynamicToolbarOffsetChanged(
    const ScreenIntCoord& aOffset) {
#if defined(MOZ_WIDGET_ANDROID)
  RefPtr<Document> document = GetTopLevelDocument();
  if (!document) {
    return IPC_OK();
  }

  if (nsPresContext* presContext = document->GetPresContext()) {
    presContext->UpdateDynamicToolbarOffset(aOffset);
  }
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvSuppressDisplayport(
    const bool& aEnabled) {
  if (RefPtr<PresShell> presShell = GetTopLevelPresShell()) {
    presShell->SuppressDisplayport(aEnabled);
  }
  return IPC_OK();
}

void BrowserChild::HandleDoubleTap(const CSSPoint& aPoint,
                                   const Modifiers& aModifiers,
                                   const ScrollableLayerGuid& aGuid) {
  MOZ_LOG(
      sApzChildLog, LogLevel::Debug,
      ("Handling double tap at %s with %p %p\n", ToString(aPoint).c_str(),
       mBrowserChildMessageManager ? mBrowserChildMessageManager->GetWrapper()
                                   : nullptr,
       mBrowserChildMessageManager.get()));

  if (!mBrowserChildMessageManager) {
    return;
  }

  // Note: there is nothing to do with the modifiers here, as we are not
  // synthesizing any sort of mouse event.
  RefPtr<Document> document = GetTopLevelDocument();
  ZoomTarget zoomTarget = CalculateRectToZoomTo(document, aPoint);
  // The double-tap can be dispatched by any scroll frame (so |aGuid| could be
  // the guid of any scroll frame), but the zoom-to-rect operation must be
  // performed by the root content scroll frame, so query its identifiers
  // for the SendZoomToRect() call rather than using the ones from |aGuid|.
  uint32_t presShellId;
  ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(
          document->GetDocumentElement(), &presShellId, &viewId) &&
      mApzcTreeManager) {
    ScrollableLayerGuid guid(mLayersId, presShellId, viewId);

    mApzcTreeManager->ZoomToRect(guid, zoomTarget,
                                 ZoomToRectBehavior::DEFAULT_BEHAVIOR);
  }
}

mozilla::ipc::IPCResult BrowserChild::RecvHandleTap(
    const GeckoContentController::TapType& aType,
    const LayoutDevicePoint& aPoint, const Modifiers& aModifiers,
    const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId) {
  // IPDL doesn't hold a strong reference to protocols as they're not required
  // to be refcounted. This function can run script, which may trigger a nested
  // event loop, which may release this, so we hold a strong reference here.
  RefPtr<BrowserChild> kungFuDeathGrip(this);
  RefPtr<PresShell> presShell = GetTopLevelPresShell();
  if (!presShell) {
    return IPC_OK();
  }
  if (!presShell->GetPresContext()) {
    return IPC_OK();
  }
  CSSToLayoutDeviceScale scale(
      presShell->GetPresContext()->CSSToDevPixelScale());
  CSSPoint point = aPoint / scale;

  // Stash the guid in InputAPZContext so that when the visual-to-layout
  // transform is applied to the event's coordinates, we use the right transform
  // based on the scroll frame being targeted.
  // The other values don't really matter.
  InputAPZContext context(aGuid, aInputBlockId, nsEventStatus_eSentinel);

  switch (aType) {
    case GeckoContentController::TapType::eSingleTap:
      if (mBrowserChildMessageManager) {
        mAPZEventState->ProcessSingleTap(point, scale, aModifiers, 1);
      }
      break;
    case GeckoContentController::TapType::eDoubleTap:
      HandleDoubleTap(point, aModifiers, aGuid);
      break;
    case GeckoContentController::TapType::eSecondTap:
      if (mBrowserChildMessageManager) {
        mAPZEventState->ProcessSingleTap(point, scale, aModifiers, 2);
      }
      break;
    case GeckoContentController::TapType::eLongTap:
      if (mBrowserChildMessageManager) {
        RefPtr<APZEventState> eventState(mAPZEventState);
        eventState->ProcessLongTap(presShell, point, scale, aModifiers,
                                   aInputBlockId);
      }
      break;
    case GeckoContentController::TapType::eLongTapUp:
      if (mBrowserChildMessageManager) {
        RefPtr<APZEventState> eventState(mAPZEventState);
        eventState->ProcessLongTapUp(presShell, point, scale, aModifiers);
      }
      break;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityHandleTap(
    const GeckoContentController::TapType& aType,
    const LayoutDevicePoint& aPoint, const Modifiers& aModifiers,
    const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId) {
  // IPDL doesn't hold a strong reference to protocols as they're not required
  // to be refcounted. This function can run script, which may trigger a nested
  // event loop, which may release this, so we hold a strong reference here.
  RefPtr<BrowserChild> kungFuDeathGrip(this);
  return RecvHandleTap(aType, aPoint, aModifiers, aGuid, aInputBlockId);
}

bool BrowserChild::NotifyAPZStateChange(
    const ViewID& aViewId,
    const layers::GeckoContentController::APZStateChange& aChange,
    const int& aArg) {
  mAPZEventState->ProcessAPZStateChange(aViewId, aChange, aArg);
  if (aChange ==
      layers::GeckoContentController::APZStateChange::eTransformEnd) {
    // This is used by tests to determine when the APZ is done doing whatever
    // it's doing. XXX generify this as needed when writing additional tests.
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    observerService->NotifyObservers(nullptr, "APZ:TransformEnd", nullptr);
  }
  return true;
}

void BrowserChild::StartScrollbarDrag(
    const layers::AsyncDragMetrics& aDragMetrics) {
  ScrollableLayerGuid guid(mLayersId, aDragMetrics.mPresShellId,
                           aDragMetrics.mViewId);

  if (mApzcTreeManager) {
    mApzcTreeManager->StartScrollbarDrag(guid, aDragMetrics);
  }
}

void BrowserChild::ZoomToRect(const uint32_t& aPresShellId,
                              const ScrollableLayerGuid::ViewID& aViewId,
                              const CSSRect& aRect, const uint32_t& aFlags) {
  ScrollableLayerGuid guid(mLayersId, aPresShellId, aViewId);

  if (mApzcTreeManager) {
    mApzcTreeManager->ZoomToRect(guid, ZoomTarget{aRect}, aFlags);
  }
}

mozilla::ipc::IPCResult BrowserChild::RecvActivate(uint64_t aActionId) {
  MOZ_ASSERT(mWebBrowser);
  mWebBrowser->FocusActivate(aActionId);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvDeactivate(uint64_t aActionId) {
  MOZ_ASSERT(mWebBrowser);
  mWebBrowser->FocusDeactivate(aActionId);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvSetKeyboardIndicators(
    const UIStateChangeType& aShowFocusRings) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, IPC_OK());
  window->SetKeyboardIndicators(aShowFocusRings);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvStopIMEStateManagement() {
  IMEStateManager::StopIMEStateManagement();
  return IPC_OK();
}

void BrowserChild::ProcessPendingCoalescedTouchData() {
  MOZ_ASSERT(StaticPrefs::dom_events_coalesce_touchmove());

  if (mCoalescedTouchData.IsEmpty()) {
    return;
  }

  if (mCoalescedTouchMoveEventFlusher) {
    mCoalescedTouchMoveEventFlusher->RemoveObserver();
  }

  UniquePtr<WidgetTouchEvent> touchMoveEvent =
      mCoalescedTouchData.TakeCoalescedEvent();
  Unused << RecvRealTouchEvent(*touchMoveEvent,
                               mCoalescedTouchData.GetScrollableLayerGuid(),
                               mCoalescedTouchData.GetInputBlockId(),
                               mCoalescedTouchData.GetApzResponse());
}

void BrowserChild::ProcessPendingCoalescedMouseDataAndDispatchEvents() {
  if (!mCoalesceMouseMoveEvents || !mCoalescedMouseEventFlusher) {
    // We don't enable mouse coalescing or we are destroying BrowserChild.
    return;
  }

  // We may reentry the event loop and push more data to
  // mToBeDispatchedMouseData while dispatching an event.

  // We may have some pending coalesced data while dispatch an event and reentry
  // the event loop. In that case we don't have chance to consume the remainding
  // pending data until we get new mouse events. Get some helps from
  // mCoalescedMouseEventFlusher to trigger it.
  mCoalescedMouseEventFlusher->StartObserver();

  while (mToBeDispatchedMouseData.GetSize() > 0) {
    UniquePtr<CoalescedMouseData> data(
        static_cast<CoalescedMouseData*>(mToBeDispatchedMouseData.PopFront()));

    UniquePtr<WidgetMouseEvent> event = data->TakeCoalescedEvent();
    if (event) {
      // Dispatch the pending events. Using HandleRealMouseButtonEvent
      // to bypass the coalesce handling in RecvRealMouseMoveEvent. Can't use
      // RecvRealMouseButtonEvent because we may also put some mouse events
      // other than mousemove.
      HandleRealMouseButtonEvent(*event, data->GetScrollableLayerGuid(),
                                 data->GetInputBlockId());
    }
  }
  // mCoalescedMouseEventFlusher may be destroyed when reentrying the event
  // loop.
  if (mCoalescedMouseEventFlusher) {
    mCoalescedMouseEventFlusher->RemoveObserver();
  }
}

LayoutDeviceToLayoutDeviceMatrix4x4
BrowserChild::GetChildToParentConversionMatrix() const {
  if (mChildToParentConversionMatrix) {
    return *mChildToParentConversionMatrix;
  }
  LayoutDevicePoint offset(GetChromeOffset());
  return LayoutDeviceToLayoutDeviceMatrix4x4::Translation(offset);
}

Maybe<ScreenRect> BrowserChild::GetTopLevelViewportVisibleRectInBrowserCoords()
    const {
  if (!mChildToParentConversionMatrix) {
    return Nothing();
  }
  return Some(mTopLevelViewportVisibleRectInBrowserCoords);
}

void BrowserChild::FlushAllCoalescedMouseData() {
  MOZ_ASSERT(mCoalesceMouseMoveEvents);

  // Move all entries from mCoalescedMouseData to mToBeDispatchedMouseData.
  for (const auto& data : mCoalescedMouseData.Values()) {
    if (!data || data->IsEmpty()) {
      continue;
    }
    UniquePtr<CoalescedMouseData> dispatchData =
        MakeUnique<CoalescedMouseData>();

    dispatchData->RetrieveDataFrom(*data);
    mToBeDispatchedMouseData.Push(dispatchData.release());
  }
  mCoalescedMouseData.Clear();
}

mozilla::ipc::IPCResult BrowserChild::RecvRealMouseMoveEvent(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  if (mCoalesceMouseMoveEvents && mCoalescedMouseEventFlusher) {
    CoalescedMouseData* data =
        mCoalescedMouseData.GetOrInsertNew(aEvent.pointerId);
    MOZ_ASSERT(data);
    if (data->CanCoalesce(aEvent, aGuid, aInputBlockId)) {
      data->Coalesce(aEvent, aGuid, aInputBlockId);
      mCoalescedMouseEventFlusher->StartObserver();
      return IPC_OK();
    }
    // Can't coalesce current mousemove event. Put the coalesced mousemove data
    // with the same pointer id to mToBeDispatchedMouseData, coalesce the
    // current one, and process all pending data in mToBeDispatchedMouseData.
    UniquePtr<CoalescedMouseData> dispatchData =
        MakeUnique<CoalescedMouseData>();

    dispatchData->RetrieveDataFrom(*data);
    mToBeDispatchedMouseData.Push(dispatchData.release());

    // Put new data to replace the old one in the hash table.
    CoalescedMouseData* newData =
        mCoalescedMouseData
            .InsertOrUpdate(aEvent.pointerId, MakeUnique<CoalescedMouseData>())
            .get();
    newData->Coalesce(aEvent, aGuid, aInputBlockId);

    // Dispatch all pending mouse events.
    ProcessPendingCoalescedMouseDataAndDispatchEvents();
    mCoalescedMouseEventFlusher->StartObserver();
  } else if (!RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvRealMouseMoveEventForTests(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  return RecvRealMouseMoveEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityRealMouseMoveEvent(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  return RecvRealMouseMoveEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult
BrowserChild::RecvNormalPriorityRealMouseMoveEventForTests(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  return RecvRealMouseMoveEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult BrowserChild::RecvSynthMouseMoveEvent(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  if (!RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPrioritySynthMouseMoveEvent(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  return RecvSynthMouseMoveEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult BrowserChild::RecvRealMouseButtonEvent(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  if (mCoalesceMouseMoveEvents && mCoalescedMouseEventFlusher &&
      aEvent.mMessage != eMouseMove) {
    // When receiving a mouse event other than mousemove, we have to dispatch
    // all coalesced events before it. However, we can't dispatch all pending
    // coalesced events directly because we may reentry the event loop while
    // dispatching. To make sure we won't dispatch disorder events, we move all
    // coalesced mousemove events and current event to a deque to dispatch them.
    // When reentrying the event loop and dispatching more events, we put new
    // events in the end of the nsQueue and dispatch events from the beginning.
    FlushAllCoalescedMouseData();

    UniquePtr<CoalescedMouseData> dispatchData =
        MakeUnique<CoalescedMouseData>();

    dispatchData->Coalesce(aEvent, aGuid, aInputBlockId);
    mToBeDispatchedMouseData.Push(dispatchData.release());

    ProcessPendingCoalescedMouseDataAndDispatchEvents();
    return IPC_OK();
  }
  HandleRealMouseButtonEvent(aEvent, aGuid, aInputBlockId);
  return IPC_OK();
}

void BrowserChild::HandleRealMouseButtonEvent(const WidgetMouseEvent& aEvent,
                                              const ScrollableLayerGuid& aGuid,
                                              const uint64_t& aInputBlockId) {
  WidgetMouseEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  // We need one InputAPZContext here to propagate |aGuid| to places in
  // SendSetTargetAPZCNotification() which apply the visual-to-layout transform,
  // and another below to propagate the |postLayerization| flag (whose value
  // we don't know until SendSetTargetAPZCNotification() returns) into
  // the event dispatch code.
  InputAPZContext context1(aGuid, aInputBlockId, nsEventStatus_eSentinel);

  // Mouse events like eMouseEnterIntoWidget, that are created in the parent
  // process EventStateManager code, have an input block id which they get from
  // the InputAPZContext in the parent process stack. However, they did not
  // actually go through the APZ code and so their mHandledByAPZ flag is false.
  // Since thos events didn't go through APZ, we don't need to send
  // notifications for them.
  RefPtr<DisplayportSetListener> postLayerization;
  if (aInputBlockId && localEvent.mFlags.mHandledByAPZ) {
    nsCOMPtr<Document> document(GetTopLevelDocument());
    postLayerization = APZCCallbackHelper::SendSetTargetAPZCNotification(
        mPuppetWidget, document, localEvent, aGuid.mLayersId, aInputBlockId);
  }

  InputAPZContext context2(aGuid, aInputBlockId, nsEventStatus_eSentinel,
                           postLayerization != nullptr);

  DispatchWidgetEventViaAPZ(localEvent);

  if (aInputBlockId && localEvent.mFlags.mHandledByAPZ) {
    mAPZEventState->ProcessMouseEvent(localEvent, aInputBlockId);
  }

  // Do this after the DispatchWidgetEventViaAPZ call above, so that if the
  // mouse event triggered a post-refresh AsyncDragMetrics message to be sent
  // to APZ (from scrollbar dragging in nsSliderFrame), then that will reach
  // APZ before the SetTargetAPZC message. This ensures the drag input block
  // gets the drag metrics before handling the input events.
  if (postLayerization) {
    postLayerization->Register();
  }
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityRealMouseButtonEvent(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  return RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult BrowserChild::RecvRealMouseEnterExitWidgetEvent(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  return RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult
BrowserChild::RecvNormalPriorityRealMouseEnterExitWidgetEvent(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  return RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId);
}

nsEventStatus BrowserChild::DispatchWidgetEventViaAPZ(WidgetGUIEvent& aEvent) {
  aEvent.ResetWaitingReplyFromRemoteProcessState();
  return APZCCallbackHelper::DispatchWidgetEvent(aEvent);
}

void BrowserChild::DispatchCoalescedWheelEvent() {
  UniquePtr<WidgetWheelEvent> wheelEvent =
      mCoalescedWheelData.TakeCoalescedEvent();
  MOZ_ASSERT(wheelEvent);
  DispatchWheelEvent(*wheelEvent, mCoalescedWheelData.GetScrollableLayerGuid(),
                     mCoalescedWheelData.GetInputBlockId());
}

void BrowserChild::DispatchWheelEvent(const WidgetWheelEvent& aEvent,
                                      const ScrollableLayerGuid& aGuid,
                                      const uint64_t& aInputBlockId) {
  WidgetWheelEvent localEvent(aEvent);
  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    nsCOMPtr<Document> document(GetTopLevelDocument());
    RefPtr<DisplayportSetListener> postLayerization =
        APZCCallbackHelper::SendSetTargetAPZCNotification(
            mPuppetWidget, document, aEvent, aGuid.mLayersId, aInputBlockId);
    if (postLayerization) {
      postLayerization->Register();
    }
  }

  localEvent.mWidget = mPuppetWidget;

  // Stash the guid in InputAPZContext so that when the visual-to-layout
  // transform is applied to the event's coordinates, we use the right transform
  // based on the scroll frame being targeted.
  // The other values don't really matter.
  InputAPZContext context(aGuid, aInputBlockId, nsEventStatus_eSentinel);

  DispatchWidgetEventViaAPZ(localEvent);

  if (localEvent.mCanTriggerSwipe) {
    SendRespondStartSwipeEvent(aInputBlockId, localEvent.TriggersSwipe());
  }

  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    mAPZEventState->ProcessWheelEvent(localEvent, aInputBlockId);
  }
}

mozilla::ipc::IPCResult BrowserChild::RecvMouseWheelEvent(
    const WidgetWheelEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  bool isNextWheelEvent = false;
  // We only coalesce the current event when
  // 1. It's eWheel (we don't coalesce eOperationStart and eWheelOperationEnd)
  // 2. It has same attributes as the coalesced wheel event which is not yet
  //    fired.
  if (aEvent.mMessage == eWheel) {
    GetIPCChannel()->PeekMessages(
        [&isNextWheelEvent](const IPC::Message& aMsg) -> bool {
          if (aMsg.type() == mozilla::dom::PBrowser::Msg_MouseWheelEvent__ID) {
            isNextWheelEvent = true;
          }
          return false;  // Stop peeking.
        });

    if (!mCoalescedWheelData.IsEmpty() &&
        !mCoalescedWheelData.CanCoalesce(aEvent, aGuid, aInputBlockId)) {
      DispatchCoalescedWheelEvent();
      MOZ_ASSERT(mCoalescedWheelData.IsEmpty());
    }
    mCoalescedWheelData.Coalesce(aEvent, aGuid, aInputBlockId);

    MOZ_ASSERT(!mCoalescedWheelData.IsEmpty());
    // If the next event isn't a wheel event, make sure we dispatch.
    if (!isNextWheelEvent) {
      DispatchCoalescedWheelEvent();
    }
  } else {
    DispatchWheelEvent(aEvent, aGuid, aInputBlockId);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityMouseWheelEvent(
    const WidgetWheelEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  return RecvMouseWheelEvent(aEvent, aGuid, aInputBlockId);
}

mozilla::ipc::IPCResult BrowserChild::RecvRealTouchEvent(
    const WidgetTouchEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId, const nsEventStatus& aApzResponse) {
  MOZ_LOG(sApzChildLog, LogLevel::Debug,
          ("Receiving touch event of type %d\n", aEvent.mMessage));

  if (StaticPrefs::dom_events_coalesce_touchmove()) {
    if (aEvent.mMessage == eTouchEnd || aEvent.mMessage == eTouchStart) {
      ProcessPendingCoalescedTouchData();
    }

    if (aEvent.mMessage != eTouchMove) {
      sConsecutiveTouchMoveCount = 0;
    }
  }

  WidgetTouchEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  // Stash the guid in InputAPZContext so that when the visual-to-layout
  // transform is applied to the event's coordinates, we use the right transform
  // based on the scroll frame being targeted.
  // The other values don't really matter.
  InputAPZContext context(aGuid, aInputBlockId, aApzResponse);

  nsTArray<TouchBehaviorFlags> allowedTouchBehaviors;
  if (localEvent.mMessage == eTouchStart && AsyncPanZoomEnabled()) {
    nsCOMPtr<Document> document = GetTopLevelDocument();
    allowedTouchBehaviors = TouchActionHelper::GetAllowedTouchBehavior(
        mPuppetWidget, document, localEvent);
    if (!allowedTouchBehaviors.IsEmpty() && mApzcTreeManager) {
      mApzcTreeManager->SetAllowedTouchBehavior(aInputBlockId,
                                                allowedTouchBehaviors);
    }
    RefPtr<DisplayportSetListener> postLayerization =
        APZCCallbackHelper::SendSetTargetAPZCNotification(
            mPuppetWidget, document, localEvent, aGuid.mLayersId,
            aInputBlockId);
    if (postLayerization) {
      postLayerization->Register();
    }
  }

  // Dispatch event to content (potentially a long-running operation)
  nsEventStatus status = DispatchWidgetEventViaAPZ(localEvent);

  if (!AsyncPanZoomEnabled()) {
    // We shouldn't have any e10s platforms that have touch events enabled
    // without APZ.
    MOZ_ASSERT(false);
    return IPC_OK();
  }

  mAPZEventState->ProcessTouchEvent(localEvent, aGuid, aInputBlockId,
                                    aApzResponse, status,
                                    std::move(allowedTouchBehaviors));
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityRealTouchEvent(
    const WidgetTouchEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId, const nsEventStatus& aApzResponse) {
  return RecvRealTouchEvent(aEvent, aGuid, aInputBlockId, aApzResponse);
}

mozilla::ipc::IPCResult BrowserChild::RecvRealTouchMoveEvent(
    const WidgetTouchEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId, const nsEventStatus& aApzResponse) {
  if (StaticPrefs::dom_events_coalesce_touchmove()) {
    ++sConsecutiveTouchMoveCount;
    if (mCoalescedTouchMoveEventFlusher) {
      MOZ_ASSERT(aEvent.mMessage == eTouchMove);
      if (mCoalescedTouchData.IsEmpty() ||
          mCoalescedTouchData.CanCoalesce(aEvent, aGuid, aInputBlockId,
                                          aApzResponse)) {
        mCoalescedTouchData.Coalesce(aEvent, aGuid, aInputBlockId,
                                     aApzResponse);
      } else {
        UniquePtr<WidgetTouchEvent> touchMoveEvent =
            mCoalescedTouchData.TakeCoalescedEvent();

        mCoalescedTouchData.Coalesce(aEvent, aGuid, aInputBlockId,
                                     aApzResponse);

        if (!RecvRealTouchEvent(*touchMoveEvent,
                                mCoalescedTouchData.GetScrollableLayerGuid(),
                                mCoalescedTouchData.GetInputBlockId(),
                                mCoalescedTouchData.GetApzResponse())) {
          return IPC_FAIL_NO_REASON(this);
        }
      }

      if (sConsecutiveTouchMoveCount > 1) {
        mCoalescedTouchMoveEventFlusher->StartObserver();
      } else {
        // Flush the pending coalesced touch in order to avoid the first
        // touchmove be overridden by the second one.
        ProcessPendingCoalescedTouchData();
      }
      return IPC_OK();
    }
  }

  if (!RecvRealTouchEvent(aEvent, aGuid, aInputBlockId, aApzResponse)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityRealTouchMoveEvent(
    const WidgetTouchEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId, const nsEventStatus& aApzResponse) {
  return RecvRealTouchMoveEvent(aEvent, aGuid, aInputBlockId, aApzResponse);
}

mozilla::ipc::IPCResult BrowserChild::RecvRealDragEvent(
    const WidgetDragEvent& aEvent, const uint32_t& aDragAction,
    const uint32_t& aDropEffect, nsIPrincipal* aPrincipal,
    nsIContentSecurityPolicy* aCsp) {
  WidgetDragEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (dragSession) {
    dragSession->SetDragAction(aDragAction);
    dragSession->SetTriggeringPrincipal(aPrincipal);
    dragSession->SetCsp(aCsp);
    RefPtr<DataTransfer> initialDataTransfer = dragSession->GetDataTransfer();
    if (initialDataTransfer) {
      initialDataTransfer->SetDropEffectInt(aDropEffect);
    }
  }

  if (aEvent.mMessage == eDrop) {
    bool canDrop = true;
    if (!dragSession || NS_FAILED(dragSession->GetCanDrop(&canDrop)) ||
        !canDrop) {
      localEvent.mMessage = eDragExit;
    }
  } else if (aEvent.mMessage == eDragOver) {
    nsCOMPtr<nsIDragService> dragService =
        do_GetService("@mozilla.org/widget/dragservice;1");
    if (dragService) {
      // This will dispatch 'drag' event at the source if the
      // drag transaction started in this process.
      dragService->FireDragEventAtSource(eDrag, aEvent.mModifiers);
    }
  }

  DispatchWidgetEventViaAPZ(localEvent);
  return IPC_OK();
}

void BrowserChild::RequestEditCommands(NativeKeyBindingsType aType,
                                       const WidgetKeyboardEvent& aEvent,
                                       nsTArray<CommandInt>& aCommands) {
  MOZ_ASSERT(aCommands.IsEmpty());

  if (NS_WARN_IF(aEvent.IsEditCommandsInitialized(aType))) {
    aCommands = aEvent.EditCommandsConstRef(aType).Clone();
    return;
  }

  switch (aType) {
    case NativeKeyBindingsType::SingleLineEditor:
    case NativeKeyBindingsType::MultiLineEditor:
    case NativeKeyBindingsType::RichTextEditor:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid native key bindings type");
  }

  // Don't send aEvent to the parent process directly because it'll be marked
  // as posted to remote process.
  WidgetKeyboardEvent localEvent(aEvent);
  SendRequestNativeKeyBindings(static_cast<uint32_t>(aType), localEvent,
                               &aCommands);
}

mozilla::ipc::IPCResult BrowserChild::RecvNativeSynthesisResponse(
    const uint64_t& aObserverId, const nsCString& aResponse) {
  mozilla::widget::AutoObserverNotifier::NotifySavedObserver(aObserverId,
                                                             aResponse.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvUpdateSHistory() {
  if (mSessionStoreChild) {
    mSessionStoreChild->UpdateSHistoryChanges();
  }
  return IPC_OK();
}

// In case handling repeated keys takes much time, we skip firing new ones.
bool BrowserChild::SkipRepeatedKeyEvent(const WidgetKeyboardEvent& aEvent) {
  if (mRepeatedKeyEventTime.IsNull() || !aEvent.CanSkipInRemoteProcess() ||
      (aEvent.mMessage != eKeyDown && aEvent.mMessage != eKeyPress)) {
    mRepeatedKeyEventTime = TimeStamp();
    mSkipKeyPress = false;
    return false;
  }

  if ((aEvent.mMessage == eKeyDown &&
       (mRepeatedKeyEventTime > aEvent.mTimeStamp)) ||
      (mSkipKeyPress && (aEvent.mMessage == eKeyPress))) {
    // If we skip a keydown event, also the following keypress events should be
    // skipped.
    mSkipKeyPress |= aEvent.mMessage == eKeyDown;
    return true;
  }

  if (aEvent.mMessage == eKeyDown) {
    // If keydown wasn't skipped, nor should the possible following keypress.
    mRepeatedKeyEventTime = TimeStamp();
    mSkipKeyPress = false;
  }
  return false;
}

void BrowserChild::UpdateRepeatedKeyEventEndTime(
    const WidgetKeyboardEvent& aEvent) {
  if (aEvent.mIsRepeat &&
      (aEvent.mMessage == eKeyDown || aEvent.mMessage == eKeyPress)) {
    mRepeatedKeyEventTime = TimeStamp::Now();
  }
}

mozilla::ipc::IPCResult BrowserChild::RecvRealKeyEvent(
    const WidgetKeyboardEvent& aEvent, const nsID& aUUID) {
  MOZ_ASSERT_IF(aEvent.mMessage == eKeyPress,
                aEvent.AreAllEditCommandsInitialized());

  // If content code called preventDefault() on a keydown event, then we don't
  // want to process any following keypress events.
  const bool isPrecedingKeyDownEventConsumed =
      aEvent.mMessage == eKeyPress && mIgnoreKeyPressEvent;

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  localEvent.mUniqueId = aEvent.mUniqueId;

  if (!SkipRepeatedKeyEvent(aEvent) && !isPrecedingKeyDownEventConsumed) {
    nsEventStatus status = DispatchWidgetEventViaAPZ(localEvent);

    // Update the end time of the possible repeated event so that we can skip
    // some incoming events in case event handling took long time.
    UpdateRepeatedKeyEventEndTime(localEvent);

    if (aEvent.mMessage == eKeyDown) {
      mIgnoreKeyPressEvent = status == nsEventStatus_eConsumeNoDefault;
    }

    if (localEvent.mFlags.mIsSuppressedOrDelayed) {
      localEvent.PreventDefault();
    }

    // If the event's default isn't prevented but the status is no default,
    // That means that the event was consumed by EventStateManager or something
    // which is not a usual event handler.  In such case, prevent its default
    // as a default handler.  For example, when an eKeyPress event matches
    // with a content accesskey, and it's executed, preventDefault() of the
    // event won't be called but the status is set to "no default".  Then,
    // the event shouldn't be handled by nsMenuBarListener in the main process.
    if (!localEvent.DefaultPrevented() &&
        status == nsEventStatus_eConsumeNoDefault) {
      localEvent.PreventDefault();
    }

    MOZ_DIAGNOSTIC_ASSERT(!localEvent.PropagationStopped());
  }
  // The keyboard event which we ignore should not be handled in the main
  // process for shortcut key handling.  For notifying if we skipped it, we can
  // use "stop propagation" flag here because it must be cleared by
  // `EventTargetChainItem` if we've dispatched it.
  else {
    localEvent.StopPropagation();
  }

  // If we don't need to send a rely for the given keyboard event, we do nothing
  // anymore here.
  if (!aEvent.WantReplyFromContentProcess()) {
    return IPC_OK();
  }

  // This is an ugly hack, mNoRemoteProcessDispatch is set to true when the
  // event's PreventDefault() or StopScrollProcessForwarding() is called.
  // And then, it'll be checked by ParamTraits<mozilla::WidgetEvent>::Write()
  // whether the event is being sent to remote process unexpectedly.
  // However, unfortunately, it cannot check the destination.  Therefore,
  // we need to clear the flag explicitly here because ParamTraits should
  // keep checking the flag for avoiding regression.
  localEvent.mFlags.mNoRemoteProcessDispatch = false;
  SendReplyKeyEvent(localEvent, aUUID);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityRealKeyEvent(
    const WidgetKeyboardEvent& aEvent, const nsID& aUUID) {
  return RecvRealKeyEvent(aEvent, aUUID);
}

mozilla::ipc::IPCResult BrowserChild::RecvCompositionEvent(
    const WidgetCompositionEvent& aEvent) {
  WidgetCompositionEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  DispatchWidgetEventViaAPZ(localEvent);
  Unused << SendOnEventNeedingAckHandled(aEvent.mMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityCompositionEvent(
    const WidgetCompositionEvent& aEvent) {
  return RecvCompositionEvent(aEvent);
}

mozilla::ipc::IPCResult BrowserChild::RecvSelectionEvent(
    const WidgetSelectionEvent& aEvent) {
  WidgetSelectionEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  DispatchWidgetEventViaAPZ(localEvent);
  Unused << SendOnEventNeedingAckHandled(aEvent.mMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPrioritySelectionEvent(
    const WidgetSelectionEvent& aEvent) {
  return RecvSelectionEvent(aEvent);
}

mozilla::ipc::IPCResult BrowserChild::RecvInsertText(
    const nsString& aStringToInsert) {
  // Use normal event path to reach focused document.
  WidgetContentCommandEvent localEvent(true, eContentCommandInsertText,
                                       mPuppetWidget);
  localEvent.mString = Some(aStringToInsert);
  DispatchWidgetEventViaAPZ(localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityInsertText(
    const nsString& aStringToInsert) {
  return RecvInsertText(aStringToInsert);
}

mozilla::ipc::IPCResult BrowserChild::RecvPasteTransferable(
    const IPCDataTransfer& aDataTransfer, const bool& aIsPrivateData,
    nsIPrincipal* aRequestingPrincipal,
    const nsContentPolicyType& aContentPolicyType) {
  nsresult rv;
  nsCOMPtr<nsITransferable> trans =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  trans->Init(nullptr);

  rv = nsContentUtils::IPCTransferableToTransferable(
      aDataTransfer, aIsPrivateData, aRequestingPrincipal, aContentPolicyType,
      true /* aAddDataFlavor */, trans, this);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  if (NS_WARN_IF(!ourDocShell)) {
    return IPC_OK();
  }

  RefPtr<nsCommandParams> params = new nsCommandParams();
  rv = params->SetISupports("transferable", trans);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  ourDocShell->DoCommandWithParams("cmd_pasteTransferable", params);
  return IPC_OK();
}

#ifdef ACCESSIBILITY
a11y::PDocAccessibleChild* BrowserChild::AllocPDocAccessibleChild(
    PDocAccessibleChild*, const uint64_t&, const MaybeDiscardedBrowsingContext&,
    const uint32_t&, const IAccessibleHolder&) {
  MOZ_ASSERT(false, "should never call this!");
  return nullptr;
}

bool BrowserChild::DeallocPDocAccessibleChild(
    a11y::PDocAccessibleChild* aChild) {
  delete static_cast<mozilla::a11y::DocAccessibleChild*>(aChild);
  return true;
}
#endif

PColorPickerChild* BrowserChild::AllocPColorPickerChild(const nsString&,
                                                        const nsString&) {
  MOZ_CRASH("unused");
  return nullptr;
}

bool BrowserChild::DeallocPColorPickerChild(PColorPickerChild* aColorPicker) {
  nsColorPickerProxy* picker = static_cast<nsColorPickerProxy*>(aColorPicker);
  NS_RELEASE(picker);
  return true;
}

PFilePickerChild* BrowserChild::AllocPFilePickerChild(const nsString&,
                                                      const int16_t&) {
  MOZ_CRASH("unused");
  return nullptr;
}

bool BrowserChild::DeallocPFilePickerChild(PFilePickerChild* actor) {
  nsFilePickerProxy* filePicker = static_cast<nsFilePickerProxy*>(actor);
  NS_RELEASE(filePicker);
  return true;
}

RefPtr<VsyncMainChild> BrowserChild::GetVsyncChild() {
  // Initializing mVsyncChild here turns on per-BrowserChild Vsync for a
  // given platform. Note: this only makes sense if nsWindow returns a
  // window-specific VsyncSource.
#if defined(MOZ_WAYLAND)
  if (IsWaylandEnabled() && !mVsyncChild) {
    mVsyncChild = MakeRefPtr<VsyncMainChild>();
    if (!SendPVsyncConstructor(mVsyncChild)) {
      mVsyncChild = nullptr;
    }
  }
#endif
  return mVsyncChild;
}

mozilla::ipc::IPCResult BrowserChild::RecvActivateFrameEvent(
    const nsString& aType, const bool& capture) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, IPC_OK());
  nsCOMPtr<EventTarget> chromeHandler = window->GetChromeEventHandler();
  NS_ENSURE_TRUE(chromeHandler, IPC_OK());
  RefPtr<ContentListener> listener = new ContentListener(this);
  chromeHandler->AddEventListener(aType, listener, capture);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvLoadRemoteScript(
    const nsString& aURL, const bool& aRunInGlobalScope) {
  if (!InitBrowserChildMessageManager())
    // This can happen if we're half-destroyed.  It's not a fatal
    // error.
    return IPC_OK();

  JS::Rooted<JSObject*> mm(RootingCx(),
                           mBrowserChildMessageManager->GetOrCreateWrapper());
  if (!mm) {
    // This can happen if we're half-destroyed.  It's not a fatal error.
    return IPC_OK();
  }

  LoadScriptInternal(mm, aURL, !aRunInGlobalScope);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvAsyncMessage(
    const nsString& aMessage, const ClonedMessageData& aData) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("BrowserChild::RecvAsyncMessage",
                                             OTHER, aMessage);
  MMPrinter::Print("BrowserChild::RecvAsyncMessage", aMessage, aData);

  if (!mBrowserChildMessageManager) {
    return IPC_OK();
  }

  RefPtr<nsFrameMessageManager> mm =
      mBrowserChildMessageManager->GetMessageManager();

  // We should have a message manager if the global is alive, but it
  // seems sometimes we don't.  Assert in aurora/nightly, but don't
  // crash in release builds.
  MOZ_DIAGNOSTIC_ASSERT(mm);
  if (!mm) {
    return IPC_OK();
  }

  JS::Rooted<JSObject*> kungFuDeathGrip(
      dom::RootingCx(), mBrowserChildMessageManager->GetWrapper());
  StructuredCloneData data;
  UnpackClonedMessageData(aData, data);
  mm->ReceiveMessage(static_cast<EventTarget*>(mBrowserChildMessageManager),
                     nullptr, aMessage, false, &data, nullptr, IgnoreErrors());
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvSwappedWithOtherRemoteLoader(
    const IPCTabContext& aContext) {
  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  if (NS_WARN_IF(!ourDocShell)) {
    return IPC_OK();
  }

  nsCOMPtr<nsPIDOMWindowOuter> ourWindow = ourDocShell->GetWindow();
  if (NS_WARN_IF(!ourWindow)) {
    return IPC_OK();
  }

  RefPtr<nsDocShell> docShell = static_cast<nsDocShell*>(ourDocShell.get());

  nsCOMPtr<EventTarget> ourEventTarget = nsGlobalWindowOuter::Cast(ourWindow);

  docShell->SetInFrameSwap(true);

  nsContentUtils::FirePageShowEventForFrameLoaderSwap(
      ourDocShell, ourEventTarget, false, true);
  nsContentUtils::FirePageHideEventForFrameLoaderSwap(ourDocShell,
                                                      ourEventTarget, true);

  // Owner content type may have changed, so store the possibly updated context
  // and notify others.
  MaybeInvalidTabContext maybeContext(aContext);
  if (!maybeContext.IsValid()) {
    NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                             "the parent process. (%s)",
                             maybeContext.GetInvalidReason())
                 .get());
    MOZ_CRASH("Invalid TabContext received from the parent process.");
  }

  if (!UpdateTabContextAfterSwap(maybeContext.GetTabContext())) {
    MOZ_CRASH("Update to TabContext after swap was denied.");
  }

  // Ignore previous value of mTriedBrowserInit since owner content has changed.
  mTriedBrowserInit = true;

  nsContentUtils::FirePageShowEventForFrameLoaderSwap(
      ourDocShell, ourEventTarget, true, true);

  docShell->SetInFrameSwap(false);

  // This is needed to get visibility state right in cases when we swapped a
  // visible tab (foreground in visible window) with a non-visible tab.
  if (RefPtr<Document> doc = docShell->GetDocument()) {
    doc->UpdateVisibilityState();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvHandleAccessKey(
    const WidgetKeyboardEvent& aEvent, nsTArray<uint32_t>&& aCharCodes) {
  nsCOMPtr<Document> document(GetTopLevelDocument());
  RefPtr<nsPresContext> pc = document->GetPresContext();
  if (pc) {
    if (!pc->EventStateManager()->HandleAccessKey(
            &(const_cast<WidgetKeyboardEvent&>(aEvent)), pc, aCharCodes)) {
      // If no accesskey was found, inform the parent so that accesskeys on
      // menus can be handled.
      WidgetKeyboardEvent localEvent(aEvent);
      localEvent.mWidget = mPuppetWidget;
      SendAccessKeyNotHandled(localEvent);
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvPrintPreview(
    const PrintData& aPrintData, const MaybeDiscardedBrowsingContext& aSourceBC,
    PrintPreviewResolver&& aCallback) {
#ifdef NS_PRINTING
  // If we didn't succeed in passing off ownership of aCallback, then something
  // went wrong.
  auto sendCallbackError = MakeScopeExit([&] {
    if (aCallback) {
      // signal error
      aCallback(PrintPreviewResultInfo(0, 0, false, false, false, {}));
    }
  });

  if (NS_WARN_IF(aSourceBC.IsDiscarded())) {
    return IPC_OK();
  }

  RefPtr<nsGlobalWindowOuter> sourceWindow;
  if (!aSourceBC.IsNull()) {
    sourceWindow = nsGlobalWindowOuter::Cast(aSourceBC.get()->GetDOMWindow());
    if (NS_WARN_IF(!sourceWindow)) {
      return IPC_OK();
    }
  } else {
    nsCOMPtr<nsPIDOMWindowOuter> ourWindow = do_GetInterface(WebNavigation());
    if (NS_WARN_IF(!ourWindow)) {
      return IPC_OK();
    }
    sourceWindow = nsGlobalWindowOuter::Cast(ourWindow);
  }

  RefPtr<nsIPrintSettings> printSettings;
  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (NS_WARN_IF(!printSettingsSvc)) {
    return IPC_OK();
  }
  printSettingsSvc->CreateNewPrintSettings(getter_AddRefs(printSettings));
  if (NS_WARN_IF(!printSettings)) {
    return IPC_OK();
  }
  printSettingsSvc->DeserializeToPrintSettings(aPrintData, printSettings);

  nsCOMPtr<nsIDocShell> docShellToCloneInto;
  if (!aSourceBC.IsNull()) {
    docShellToCloneInto = do_GetInterface(WebNavigation());
    if (NS_WARN_IF(!docShellToCloneInto)) {
      return IPC_OK();
    }
  }

  sourceWindow->Print(printSettings,
                      /* aRemotePrintJob = */ nullptr,
                      /* aListener = */ nullptr, docShellToCloneInto,
                      nsGlobalWindowOuter::IsPreview::Yes,
                      nsGlobalWindowOuter::IsForWindowDotPrint::No,
                      std::move(aCallback), IgnoreErrors());
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvExitPrintPreview() {
#ifdef NS_PRINTING
  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint =
      do_GetInterface(ToSupports(WebNavigation()));
  if (NS_WARN_IF(!webBrowserPrint)) {
    return IPC_OK();
  }
  webBrowserPrint->ExitPrintPreview();
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvPrint(
    const MaybeDiscardedBrowsingContext& aBc, const PrintData& aPrintData) {
#ifdef NS_PRINTING
  if (NS_WARN_IF(aBc.IsNullOrDiscarded())) {
    return IPC_OK();
  }
  RefPtr<nsGlobalWindowOuter> outerWindow =
      nsGlobalWindowOuter::Cast(aBc.get()->GetDOMWindow());
  if (NS_WARN_IF(!outerWindow)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (NS_WARN_IF(!printSettingsSvc)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrintSettings> printSettings;
  nsresult rv =
      printSettingsSvc->CreateNewPrintSettings(getter_AddRefs(printSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_OK();
  }

  printSettingsSvc->DeserializeToPrintSettings(aPrintData, printSettings);
  {
    IgnoredErrorResult rv;
    outerWindow->Print(
        printSettings,
        static_cast<RemotePrintJobChild*>(aPrintData.remotePrintJobChild()),
        /* aListener = */ nullptr,
        /* aWindowToCloneInto = */ nullptr, nsGlobalWindowOuter::IsPreview::No,
        nsGlobalWindowOuter::IsForWindowDotPrint::No,
        /* aPrintPreviewCallback = */ nullptr, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return IPC_OK();
    }
  }
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvUpdateNativeWindowHandle(
    const uintptr_t& aNewHandle) {
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  mNativeWindowHandle = aNewHandle;
  return IPC_OK();
#else
  return IPC_FAIL_NO_REASON(this);
#endif
}

mozilla::ipc::IPCResult BrowserChild::RecvDestroy() {
  MOZ_ASSERT(mDestroyed == false);
  mDestroyed = true;

  nsTArray<PContentPermissionRequestChild*> childArray =
      nsContentPermissionUtils::GetContentPermissionRequestChildById(
          GetTabId());

  // Need to close undeleted ContentPermissionRequestChilds before tab is
  // closed.
  for (auto& permissionRequestChild : childArray) {
    auto child = static_cast<RemotePermissionRequest*>(permissionRequestChild);
    child->Destroy();
  }

  if (mBrowserChildMessageManager) {
    // Message handlers are called from the event loop, so it better be safe to
    // run script.
    MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
    mBrowserChildMessageManager->DispatchTrustedEvent(u"unload"_ns);
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  observerService->RemoveObserver(this, BEFORE_FIRST_PAINT);

  // XXX what other code in ~BrowserChild() should we be running here?
  DestroyWindow();

  // Bounce through the event loop once to allow any delayed teardown runnables
  // that were just generated to have a chance to run.
  nsCOMPtr<nsIRunnable> deleteRunnable = new DelayedDeleteRunnable(this);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(deleteRunnable));

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvRenderLayers(
    const bool& aEnabled, const layers::LayersObserverEpoch& aEpoch) {
  if (mPendingDocShellBlockers > 0) {
    mPendingRenderLayersReceivedMessage = true;
    mPendingRenderLayers = aEnabled;
    mPendingLayersObserverEpoch = aEpoch;
    return IPC_OK();
  }

  // Since requests to change the rendering state come in from both the hang
  // monitor channel and the PContent channel, we have an ordering problem. This
  // code ensures that we respect the order in which the requests were made and
  // ignore stale requests.
  if (mLayersObserverEpoch >= aEpoch) {
    return IPC_OK();
  }
  mLayersObserverEpoch = aEpoch;

  auto clearPaintWhileInterruptingJS = MakeScopeExit([&] {
    // We might force a paint, or we might already have painted and this is a
    // no-op. In either case, once we exit this scope, we need to alert the
    // ProcessHangMonitor that we've finished responding to what might have
    // been a request to force paint. This is so that the BackgroundHangMonitor
    // for force painting can be made to wait again.
    if (aEnabled) {
      ProcessHangMonitor::ClearPaintWhileInterruptingJS(mLayersObserverEpoch);
    }
  });

  if (aEnabled) {
    ProcessHangMonitor::MaybeStartPaintWhileInterruptingJS();
  }

  if (mCompositorOptions) {
    MOZ_ASSERT(mPuppetWidget);
    RefPtr<WebRenderLayerManager> lm =
        mPuppetWidget->GetWindowRenderer()->AsWebRender();
    if (lm) {
      // We send the current layer observer epoch to the compositor so that
      // BrowserParent knows whether a layer update notification corresponds to
      // the latest RecvRenderLayers request that was made.
      lm->SetLayersObserverEpoch(mLayersObserverEpoch);
    }
  }

  mRenderLayers = aEnabled;

  if (aEnabled && IsVisible()) {
    // This request is a no-op.
    // In this case, we still want a MozLayerTreeReady notification to fire
    // in the parent (so that it knows that the child has updated its epoch).
    // PaintWhileInterruptingJSNoOp does that.
    if (IPCOpen()) {
      Unused << SendPaintWhileInterruptingJSNoOp(mLayersObserverEpoch);
    }
    return IPC_OK();
  }

  // FIXME(emilio): Probably / maybe this shouldn't be needed? See the comment
  // in MakeVisible(), having the two separate states is not great.
  UpdateVisibility();

  if (!aEnabled) {
    return IPC_OK();
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    return IPC_OK();
  }

  // We don't use BrowserChildBase::GetPresShell() here because that would
  // create a content viewer if one doesn't exist yet. Creating a content
  // viewer can cause JS to run, which we want to avoid.
  // nsIDocShell::GetPresShell returns null if no content viewer exists yet.
  RefPtr<PresShell> presShell = docShell->GetPresShell();
  if (!presShell) {
    return IPC_OK();
  }

  if (nsIFrame* root = presShell->GetRootFrame()) {
    root->SchedulePaint();
  }

  Telemetry::AutoTimer<Telemetry::TABCHILD_PAINT_TIME> timer;
  // If we need to repaint, let's do that right away. No sense waiting until
  // we get back to the event loop again. We suppress the display port so
  // that we only paint what's visible. This ensures that the tab we're
  // switching to paints as quickly as possible.
  presShell->SuppressDisplayport(true);
  if (nsContentUtils::IsSafeToRunScript()) {
    WebWidget()->PaintNowIfNeeded();
  } else {
    RefPtr<nsViewManager> vm = presShell->GetViewManager();
    if (nsView* view = vm->GetRootView()) {
      presShell->PaintAndRequestComposite(view, PaintFlags::None);
    }
  }
  presShell->SuppressDisplayport(false);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNavigateByKey(
    const bool& aForward, const bool& aForDocumentNavigation) {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return IPC_OK();
  }

  RefPtr<Element> result;
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());

  // Move to the first or last document.
  {
    uint32_t type =
        aForward
            ? (aForDocumentNavigation
                   ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FIRSTDOC)
                   : static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_ROOT))
            : (aForDocumentNavigation
                   ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LASTDOC)
                   : static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LAST));
    uint32_t flags = nsIFocusManager::FLAG_BYKEY;
    if (aForward || aForDocumentNavigation) {
      flags |= nsIFocusManager::FLAG_NOSCROLL;
    }
    fm->MoveFocus(window, nullptr, type, flags, getter_AddRefs(result));
  }

  // No valid root element was found, so move to the first focusable element.
  if (!result && aForward && !aForDocumentNavigation) {
    fm->MoveFocus(window, nullptr, nsIFocusManager::MOVEFOCUS_FIRST,
                  nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));
  }

  SendRequestFocus(false, CallerType::System);
  return IPC_OK();
}

bool BrowserChild::InitBrowserChildMessageManager() {
  mShouldSendWebProgressEventsToParent = true;

  if (!mBrowserChildMessageManager) {
    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
    NS_ENSURE_TRUE(window, false);
    nsCOMPtr<EventTarget> chromeHandler = window->GetChromeEventHandler();
    NS_ENSURE_TRUE(chromeHandler, false);

    RefPtr<BrowserChildMessageManager> scope = mBrowserChildMessageManager =
        new BrowserChildMessageManager(this);

    MOZ_ALWAYS_TRUE(nsMessageManagerScriptExecutor::Init());

    nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
    if (NS_WARN_IF(!root)) {
      mBrowserChildMessageManager = nullptr;
      return false;
    }
    root->SetParentTarget(scope);
  }

  if (!mTriedBrowserInit) {
    mTriedBrowserInit = true;
  }

  return true;
}

void BrowserChild::InitRenderingState(
    const TextureFactoryIdentifier& aTextureFactoryIdentifier,
    const layers::LayersId& aLayersId,
    const CompositorOptions& aCompositorOptions) {
  mPuppetWidget->InitIMEState();

  MOZ_ASSERT(aLayersId.IsValid());
  mTextureFactoryIdentifier = aTextureFactoryIdentifier;

  // Pushing layers transactions directly to a separate
  // compositor context.
  PCompositorBridgeChild* compositorChild = CompositorBridgeChild::Get();
  if (!compositorChild) {
    mLayersConnected = Some(false);
    NS_WARNING("failed to get CompositorBridgeChild instance");
    return;
  }

  mCompositorOptions = Some(aCompositorOptions);

  if (aLayersId.IsValid()) {
    StaticMutexAutoLock lock(sBrowserChildrenMutex);

    if (!sBrowserChildren) {
      sBrowserChildren = new BrowserChildMap;
    }
    MOZ_ASSERT(!sBrowserChildren->Contains(uint64_t(aLayersId)));
    sBrowserChildren->InsertOrUpdate(uint64_t(aLayersId), this);
    mLayersId = aLayersId;
  }

  // Depending on timing, we might paint too early and fall back to basic
  // layers. CreateRemoteLayerManager will destroy us if we manage to get a
  // remote layer manager though, so that's fine.
  MOZ_ASSERT(!mPuppetWidget->HasWindowRenderer() ||
             mPuppetWidget->GetWindowRenderer()->GetBackendType() ==
                 layers::LayersBackend::LAYERS_NONE);
  bool success = false;
  if (mLayersConnected == Some(true)) {
    success = CreateRemoteLayerManager(compositorChild);
  }

  if (success) {
    MOZ_ASSERT(mLayersConnected == Some(true));
    // Succeeded to create "remote" layer manager
    ImageBridgeChild::IdentifyCompositorTextureHost(mTextureFactoryIdentifier);
    gfx::VRManagerChild::IdentifyTextureHost(mTextureFactoryIdentifier);
    InitAPZState();
    RefPtr<WebRenderLayerManager> lm =
        mPuppetWidget->GetWindowRenderer()->AsWebRender();
    if (lm) {
      lm->SetLayersObserverEpoch(mLayersObserverEpoch);
    }
  } else {
    NS_WARNING("Fallback to BasicLayerManager");
    mLayersConnected = Some(false);
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  if (observerService) {
    observerService->AddObserver(this, BEFORE_FIRST_PAINT, false);
  }
}

bool BrowserChild::CreateRemoteLayerManager(
    mozilla::layers::PCompositorBridgeChild* aCompositorChild) {
  MOZ_ASSERT(aCompositorChild);

  return mPuppetWidget->CreateRemoteLayerManager(
      [&](WebRenderLayerManager* aLayerManager) -> bool {
        nsCString error;
        return aLayerManager->Initialize(aCompositorChild,
                                         wr::AsPipelineId(mLayersId),
                                         &mTextureFactoryIdentifier, error);
      });
}

void BrowserChild::InitAPZState() {
  if (!mCompositorOptions->UseAPZ()) {
    return;
  }
  auto cbc = CompositorBridgeChild::Get();

  // Initialize the ApzcTreeManager. This takes multiple casts because of ugly
  // multiple inheritance.
  PAPZCTreeManagerChild* baseProtocol =
      cbc->SendPAPZCTreeManagerConstructor(mLayersId);
  APZCTreeManagerChild* derivedProtocol =
      static_cast<APZCTreeManagerChild*>(baseProtocol);

  mApzcTreeManager = RefPtr<IAPZCTreeManager>(derivedProtocol);

  // Initialize the GeckoContentController for this tab. We don't hold a
  // reference because we don't need it. The ContentProcessController will hold
  // a reference to the tab, and will be destroyed by the compositor or ipdl
  // during destruction.
  RefPtr<GeckoContentController> contentController =
      new ContentProcessController(this);
  APZChild* apzChild = new APZChild(contentController);
  cbc->SendPAPZConstructor(apzChild, mLayersId);
}

IPCResult BrowserChild::RecvUpdateEffects(const EffectsInfo& aEffects) {
  mDidSetEffectsInfo = true;

  bool needInvalidate = false;
  if (mEffectsInfo.IsVisible() && aEffects.IsVisible() &&
      mEffectsInfo != aEffects) {
    // if we are staying visible and either the visrect or scale changed we need
    // to invalidate
    needInvalidate = true;
  }

  mEffectsInfo = aEffects;
  UpdateVisibility();

  if (needInvalidate) {
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
    if (docShell) {
      // We don't use BrowserChildBase::GetPresShell() here because that would
      // create a content viewer if one doesn't exist yet. Creating a content
      // viewer can cause JS to run, which we want to avoid.
      // nsIDocShell::GetPresShell returns null if no content viewer exists yet.
      RefPtr<PresShell> presShell = docShell->GetPresShell();
      if (presShell) {
        if (nsIFrame* root = presShell->GetRootFrame()) {
          root->InvalidateFrame();
        }
      }
    }
  }

  return IPC_OK();
}

bool BrowserChild::IsVisible() {
  return mPuppetWidget && mPuppetWidget->IsVisible();
}

void BrowserChild::UpdateVisibility() {
  bool shouldBeVisible = mIsTopLevel ? mRenderLayers : mEffectsInfo.IsVisible();
  bool isVisible = IsVisible();

  if (shouldBeVisible != isVisible) {
    if (shouldBeVisible) {
      MakeVisible();
    } else {
      MakeHidden();
    }
  }
}

void BrowserChild::MakeVisible() {
  if (IsVisible()) {
    return;
  }

  if (mPuppetWidget) {
    mPuppetWidget->Show(true);
  }

  PresShellActivenessMaybeChanged();
}

void BrowserChild::MakeHidden() {
  if (!IsVisible()) {
    return;
  }

  // Due to the nested event loop in ContentChild::ProvideWindowCommon,
  // it's possible to be told to become hidden before we're finished
  // setting up a layer manager. We should skip clearing cached layers
  // in that case, since doing so might accidentally put is into
  // BasicLayers mode.
  if (mPuppetWidget) {
    if (mPuppetWidget->HasWindowRenderer()) {
      ClearCachedResources();
    }
    mPuppetWidget->Show(false);
  }

  PresShellActivenessMaybeChanged();
}

IPCResult BrowserChild::RecvPreserveLayers(bool aPreserve) {
  mIsPreservingLayers = aPreserve;

  PresShellActivenessMaybeChanged();

  return IPC_OK();
}

void BrowserChild::PresShellActivenessMaybeChanged() {
  // We don't use BrowserChildBase::GetPresShell() here because that would
  // create a content viewer if one doesn't exist yet. Creating a content
  // viewer can cause JS to run, which we want to avoid.
  // nsIDocShell::GetPresShell returns null if no content viewer exists yet.
  //
  // When this method is called we don't want to go through the browsing context
  // because we don't want to change the visibility state of the document, which
  // has side effects like firing events to content, unblocking media playback,
  // unthrottling timeouts... PresShell activeness has a lot less side effects.
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    return;
  }
  RefPtr<PresShell> presShell = docShell->GetPresShell();
  if (!presShell) {
    return;
  }
  presShell->ActivenessMaybeChanged();
}

NS_IMETHODIMP
BrowserChild::GetMessageManager(ContentFrameMessageManager** aResult) {
  RefPtr<ContentFrameMessageManager> mm(mBrowserChildMessageManager);
  mm.forget(aResult);
  return *aResult ? NS_OK : NS_ERROR_FAILURE;
}

void BrowserChild::SendRequestFocus(bool aCanFocus, CallerType aCallerType) {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  if (!window) {
    return;
  }

  BrowsingContext* focusedBC = fm->GetFocusedBrowsingContext();
  if (focusedBC == window->GetBrowsingContext()) {
    // BrowsingContext has the focus already, do not request again.
    return;
  }

  PBrowserChild::SendRequestFocus(aCanFocus, aCallerType);
}

NS_IMETHODIMP
BrowserChild::GetTabId(uint64_t* aId) {
  *aId = GetTabId();
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::GetChromeOuterWindowID(uint64_t* aId) {
  *aId = ChromeOuterWindowID();
  return NS_OK;
}

bool BrowserChild::DoSendBlockingMessage(
    const nsAString& aMessage, StructuredCloneData& aData,
    nsTArray<StructuredCloneData>* aRetVal) {
  ClonedMessageData data;
  if (!BuildClonedMessageData(aData, data)) {
    return false;
  }
  return SendSyncMessage(PromiseFlatString(aMessage), data, aRetVal);
}

nsresult BrowserChild::DoSendAsyncMessage(const nsAString& aMessage,
                                          StructuredCloneData& aData) {
  ClonedMessageData data;
  if (!BuildClonedMessageData(aData, data)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }
  if (!SendAsyncMessage(PromiseFlatString(aMessage), data)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

/* static */
nsTArray<RefPtr<BrowserChild>> BrowserChild::GetAll() {
  StaticMutexAutoLock lock(sBrowserChildrenMutex);

  if (!sBrowserChildren) {
    return {};
  }

  return ToTArray<nsTArray<RefPtr<BrowserChild>>>(sBrowserChildren->Values());
}

BrowserChild* BrowserChild::GetFrom(PresShell* aPresShell) {
  Document* doc = aPresShell->GetDocument();
  if (!doc) {
    return nullptr;
  }
  nsCOMPtr<nsIDocShell> docShell(doc->GetDocShell());
  return GetFrom(docShell);
}

BrowserChild* BrowserChild::GetFrom(layers::LayersId aLayersId) {
  StaticMutexAutoLock lock(sBrowserChildrenMutex);
  if (!sBrowserChildren) {
    return nullptr;
  }
  return sBrowserChildren->Get(uint64_t(aLayersId));
}

void BrowserChild::DidComposite(mozilla::layers::TransactionId aTransactionId,
                                const TimeStamp& aCompositeStart,
                                const TimeStamp& aCompositeEnd) {
  MOZ_ASSERT(mPuppetWidget);
  RefPtr<WebRenderLayerManager> lm =
      mPuppetWidget->GetWindowRenderer()->AsWebRender();
  MOZ_ASSERT(lm);

  if (lm) {
    lm->DidComposite(aTransactionId, aCompositeStart, aCompositeEnd);
  }
}

void BrowserChild::DidRequestComposite(const TimeStamp& aCompositeReqStart,
                                       const TimeStamp& aCompositeReqEnd) {
  nsCOMPtr<nsIDocShell> docShellComPtr = do_GetInterface(WebNavigation());
  if (!docShellComPtr) {
    return;
  }

  nsDocShell* docShell = static_cast<nsDocShell*>(docShellComPtr.get());

  if (TimelineConsumers::HasConsumer(docShell)) {
    // Since we're assuming that it's impossible for content JS to directly
    // trigger a synchronous paint, we can avoid capturing a stack trace here,
    // which means we won't run into JS engine reentrancy issues like bug
    // 1310014.
    TimelineConsumers::AddMarkerForDocShell(
        docShell, "CompositeForwardTransaction", aCompositeReqStart,
        MarkerTracingType::START, MarkerStackRequest::NO_STACK);
    TimelineConsumers::AddMarkerForDocShell(
        docShell, "CompositeForwardTransaction", aCompositeReqEnd,
        MarkerTracingType::END, MarkerStackRequest::NO_STACK);
  }
}

void BrowserChild::ClearCachedResources() {
  MOZ_ASSERT(mPuppetWidget);
  RefPtr<WebRenderLayerManager> lm =
      mPuppetWidget->GetWindowRenderer()->AsWebRender();
  if (lm) {
    lm->ClearCachedResources();
  }

  if (nsCOMPtr<Document> document = GetTopLevelDocument()) {
    nsPresContext* presContext = document->GetPresContext();
    if (presContext) {
      presContext->NotifyPaintStatusReset();
    }
  }
}

void BrowserChild::InvalidateLayers() { MOZ_ASSERT(mPuppetWidget); }

void BrowserChild::SchedulePaint() {
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    return;
  }

  // We don't use BrowserChildBase::GetPresShell() here because that would
  // create a content viewer if one doesn't exist yet. Creating a content viewer
  // can cause JS to run, which we want to avoid. nsIDocShell::GetPresShell
  // returns null if no content viewer exists yet.
  if (RefPtr<PresShell> presShell = docShell->GetPresShell()) {
    if (nsIFrame* root = presShell->GetRootFrame()) {
      root->SchedulePaint();
    }
  }
}

void BrowserChild::ReinitRendering() {
  MOZ_ASSERT(mLayersId.IsValid());

  // In some cases, like when we create a windowless browser,
  // RemoteLayerTreeOwner/BrowserChild is not connected to a compositor.
  if (mLayersConnectRequested.isNothing() ||
      mLayersConnectRequested == Some(false)) {
    return;
  }

  // Before we establish a new PLayerTransaction, we must connect our layer tree
  // id, CompositorBridge, and the widget compositor all together again.
  // Normally this happens in BrowserParent before BrowserChild is given
  // rendering information.
  //
  // In this case, we will send a sync message to our BrowserParent, which in
  // turn will send a sync message to the Compositor of the widget owning this
  // tab. This guarantees the correct association is in place before our
  // PLayerTransaction constructor message arrives on the cross-process
  // compositor bridge.
  CompositorOptions options;
  SendEnsureLayersConnected(&options);
  mCompositorOptions = Some(options);

  bool success = false;
  RefPtr<CompositorBridgeChild> cb = CompositorBridgeChild::Get();

  if (cb) {
    success = CreateRemoteLayerManager(cb);
  }

  if (!success) {
    NS_WARNING("failed to recreate layer manager");
    return;
  }

  mLayersConnected = Some(true);
  ImageBridgeChild::IdentifyCompositorTextureHost(mTextureFactoryIdentifier);
  gfx::VRManagerChild::IdentifyTextureHost(mTextureFactoryIdentifier);

  InitAPZState();
  RefPtr<WebRenderLayerManager> lm =
      mPuppetWidget->GetWindowRenderer()->AsWebRender();
  if (lm) {
    lm->SetLayersObserverEpoch(mLayersObserverEpoch);
  }

  if (nsCOMPtr<Document> doc = GetTopLevelDocument()) {
    doc->NotifyLayerManagerRecreated();
  }

  if (mRenderLayers) {
    SchedulePaint();
  }
}

void BrowserChild::ReinitRenderingForDeviceReset() {
  InvalidateLayers();

  RefPtr<WebRenderLayerManager> lm =
      mPuppetWidget->GetWindowRenderer()->AsWebRender();
  if (lm) {
    lm->DoDestroy(/* aIsSync */ true);
  }

  // Proceed with destroying and recreating the layer manager.
  ReinitRendering();
}

NS_IMETHODIMP
BrowserChild::OnShowTooltip(int32_t aXCoords, int32_t aYCoords,
                            const nsAString& aTipText,
                            const nsAString& aTipDir) {
  nsString str(aTipText);
  nsString dir(aTipDir);
  SendShowTooltip(aXCoords, aYCoords, str, dir);
  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::OnHideTooltip() {
  SendHideTooltip();
  return NS_OK;
}

void BrowserChild::NotifyJankedAnimations(
    const nsTArray<uint64_t>& aJankedAnimations) {
  MOZ_ASSERT(mPuppetWidget);
  RefPtr<WebRenderLayerManager> lm =
      mPuppetWidget->GetWindowRenderer()->AsWebRender();
  if (lm) {
    lm->UpdatePartialPrerenderedAnimations(aJankedAnimations);
  }
}

mozilla::ipc::IPCResult BrowserChild::RecvUIResolutionChanged(
    const float& aDpi, const int32_t& aRounding, const double& aScale) {
  ScreenIntSize oldScreenSize = GetInnerSize();
  if (aDpi > 0) {
    mPuppetWidget->UpdateBackingScaleCache(aDpi, aRounding, aScale);
  }
  nsCOMPtr<Document> document(GetTopLevelDocument());
  RefPtr<nsPresContext> presContext =
      document ? document->GetPresContext() : nullptr;
  if (presContext) {
    presContext->UIResolutionChangedSync();
  }

  ScreenIntSize screenSize = GetInnerSize();
  if (mHasValidInnerSize && oldScreenSize != screenSize) {
    ScreenIntRect screenRect = GetOuterRect();

    // See RecvUpdateDimensions for the order of these operations.
    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
    baseWin->SetPositionAndSize(0, 0, screenSize.width, screenSize.height,
                                nsIBaseWindow::eRepaint);

    mPuppetWidget->Resize(screenRect.x + mClientOffset.x + mChromeOffset.x,
                          screenRect.y + mClientOffset.y + mChromeOffset.y,
                          screenSize.width, screenSize.height, true);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvSafeAreaInsetsChanged(
    const mozilla::ScreenIntMargin& aSafeAreaInsets) {
  mPuppetWidget->UpdateSafeAreaInsets(aSafeAreaInsets);

  nsCOMPtr<nsIScreenManager> screenMgr =
      do_GetService("@mozilla.org/gfx/screenmanager;1");
  ScreenIntMargin currentSafeAreaInsets;
  if (screenMgr) {
    // aSafeAreaInsets is for current screen. But we have to calculate
    // safe insets for content window.
    int32_t x, y, cx, cy;
    GetDimensions(0, &x, &y, &cx, &cy);
    nsCOMPtr<nsIScreen> screen;
    screenMgr->ScreenForRect(x, y, cx, cy, getter_AddRefs(screen));

    if (screen) {
      LayoutDeviceIntRect windowRect(x + mClientOffset.x + mChromeOffset.x,
                                     y + mClientOffset.y + mChromeOffset.y, cx,
                                     cy);
      currentSafeAreaInsets = nsContentUtils::GetWindowSafeAreaInsets(
          screen, aSafeAreaInsets, windowRect);
    }
  }

  if (nsCOMPtr<Document> document = GetTopLevelDocument()) {
    nsPresContext* presContext = document->GetPresContext();
    if (presContext) {
      presContext->SetSafeAreaInsets(currentSafeAreaInsets);
    }
  }

  // https://github.com/w3c/csswg-drafts/issues/4670
  // Actually we don't set this value on sub document. This behaviour is
  // same as Blink that safe area insets isn't set on sub document.

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvAllowScriptsToClose() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  if (window) {
    nsGlobalWindowOuter::Cast(window)->AllowScriptsToClose();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvReleaseAllPointerCapture() {
  PointerEventHandler::ReleaseAllPointerCapture();
  return IPC_OK();
}

PPaymentRequestChild* BrowserChild::AllocPPaymentRequestChild() {
  MOZ_CRASH(
      "We should never be manually allocating PPaymentRequestChild actors");
  return nullptr;
}

bool BrowserChild::DeallocPPaymentRequestChild(PPaymentRequestChild* actor) {
  delete actor;
  return true;
}

ScreenIntSize BrowserChild::GetInnerSize() {
  LayoutDeviceIntSize innerSize =
      RoundedToInt(mUnscaledInnerSize * mPuppetWidget->GetDefaultScale());
  return ViewAs<ScreenPixel>(
      innerSize, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
};

Maybe<nsRect> BrowserChild::GetVisibleRect() const {
  if (mIsTopLevel) {
    // We are conservative about visible rects for top-level browsers to avoid
    // artifacts when resizing
    return Nothing();
  }

  return mDidSetEffectsInfo ? Some(mEffectsInfo.mVisibleRect) : Nothing();
}

Maybe<LayoutDeviceRect>
BrowserChild::GetTopLevelViewportVisibleRectInSelfCoords() const {
  if (mIsTopLevel) {
    return Nothing();
  }

  if (!mChildToParentConversionMatrix) {
    // We have no way to tell this remote document visible rect right now.
    return Nothing();
  }

  Maybe<LayoutDeviceToLayoutDeviceMatrix4x4> inverse =
      mChildToParentConversionMatrix->MaybeInverse();
  if (!inverse) {
    return Nothing();
  }

  // Convert the remote document visible rect to the coordinate system of the
  // iframe document.
  Maybe<LayoutDeviceRect> rect = UntransformBy(
      *inverse,
      ViewAs<LayoutDevicePixel>(
          mTopLevelViewportVisibleRectInBrowserCoords,
          PixelCastJustification::ContentProcessIsLayerInUiProcess),
      LayoutDeviceRect::MaxIntRect());
  if (!rect) {
    return Nothing();
  }

  return rect;
}

ScreenIntRect BrowserChild::GetOuterRect() {
  LayoutDeviceIntRect outerRect =
      RoundedToInt(mUnscaledOuterRect * mPuppetWidget->GetDefaultScale());
  return ViewAs<ScreenPixel>(
      outerRect, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
}

void BrowserChild::PaintWhileInterruptingJS(
    const layers::LayersObserverEpoch& aEpoch) {
  if (!IPCOpen() || !mPuppetWidget || !mPuppetWidget->HasWindowRenderer()) {
    // Don't bother doing anything now. Better to wait until we receive the
    // message on the PContent channel.
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(nsContentUtils::IsSafeToRunScript());
  nsAutoScriptBlocker scriptBlocker;
  RecvRenderLayers(true /* aEnabled */, aEpoch);
}

nsresult BrowserChild::CanCancelContentJS(
    nsIRemoteTab::NavigationType aNavigationType, int32_t aNavigationIndex,
    nsIURI* aNavigationURI, int32_t aEpoch, bool* aCanCancel) {
  nsresult rv;
  *aCanCancel = false;

  if (aEpoch <= mCancelContentJSEpoch) {
    // The next page loaded before we got here, so we shouldn't try to cancel
    // the content JS.
    return NS_OK;
  }

  // If we have session history in the parent we've already performed
  // the checks following, so we can return early.
  if (mozilla::SessionHistoryInParent()) {
    *aCanCancel = true;
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  nsCOMPtr<nsISHistory> history;
  if (docShell) {
    history = nsDocShell::Cast(docShell)->GetSessionHistory()->LegacySHistory();
  }

  if (!history) {
    return NS_ERROR_FAILURE;
  }

  int32_t current;
  rv = history->GetIndex(&current);
  NS_ENSURE_SUCCESS(rv, rv);

  if (current == -1) {
    // This tab has no history! Just return.
    return NS_OK;
  }

  nsCOMPtr<nsISHEntry> entry;
  rv = history->GetEntryAtIndex(current, getter_AddRefs(entry));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> currentURI = entry->GetURI();
  if (!currentURI->SchemeIs("http") && !currentURI->SchemeIs("https") &&
      !currentURI->SchemeIs("file")) {
    // Only cancel content JS for http(s) and file URIs. Other URIs are probably
    // internal and we should just let them run to completion.
    return NS_OK;
  }

  if (aNavigationType == nsIRemoteTab::NAVIGATE_BACK) {
    aNavigationIndex = current - 1;
  } else if (aNavigationType == nsIRemoteTab::NAVIGATE_FORWARD) {
    aNavigationIndex = current + 1;
  } else if (aNavigationType == nsIRemoteTab::NAVIGATE_URL) {
    if (!aNavigationURI) {
      return NS_ERROR_FAILURE;
    }

    if (aNavigationURI->SchemeIs("javascript")) {
      // "javascript:" URIs don't (necessarily) trigger navigation to a
      // different page, so don't allow the current page's JS to terminate.
      return NS_OK;
    }

    // If navigating directly to a URL (e.g. via hitting Enter in the location
    // bar), then we can cancel anytime the next URL is different from the
    // current, *excluding* the ref ("#").
    bool equals;
    rv = currentURI->EqualsExceptRef(aNavigationURI, &equals);
    NS_ENSURE_SUCCESS(rv, rv);
    *aCanCancel = !equals;
    return NS_OK;
  }
  // Note: aNavigationType may also be NAVIGATE_INDEX, in which case we don't
  // need to do anything special.

  int32_t delta = aNavigationIndex > current ? 1 : -1;
  for (int32_t i = current + delta; i != aNavigationIndex + delta; i += delta) {
    nsCOMPtr<nsISHEntry> nextEntry;
    // If `i` happens to be negative, this call will fail (which is what we
    // would want to happen).
    rv = history->GetEntryAtIndex(i, getter_AddRefs(nextEntry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISHEntry> laterEntry = delta == 1 ? nextEntry : entry;
    nsCOMPtr<nsIURI> thisURI = entry->GetURI();
    nsCOMPtr<nsIURI> nextURI = nextEntry->GetURI();

    // If we changed origin and the load wasn't in a subframe, we know it was
    // a full document load, so we can cancel the content JS safely.
    if (!laterEntry->GetIsSubFrame()) {
      nsAutoCString thisHost;
      rv = thisURI->GetPrePath(thisHost);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoCString nextHost;
      rv = nextURI->GetPrePath(nextHost);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!thisHost.Equals(nextHost)) {
        *aCanCancel = true;
        return NS_OK;
      }
    }

    entry = nextEntry;
  }

  return NS_OK;
}

nsresult BrowserChild::GetHasSiblings(bool* aHasSiblings) {
  *aHasSiblings = mHasSiblings;
  return NS_OK;
}

nsresult BrowserChild::SetHasSiblings(bool aHasSiblings) {
  mHasSiblings = aHasSiblings;
  return NS_OK;
}

NS_IMETHODIMP BrowserChild::OnStateChange(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          uint32_t aStateFlags,
                                          nsresult aStatus) {
  if (!IPCOpen() || !mShouldSendWebProgressEventsToParent) {
    return NS_OK;
  }

  // We shouldn't need to notify the parent of redirect state changes, since
  // with DocumentChannel that only happens when we switch to the real channel,
  // and that's an implementation detail that we can hide.
  if (aStateFlags & nsIWebProgressListener::STATE_IS_REDIRECTED_DOCUMENT) {
    return NS_OK;
  }

  // Our OnStateChange call must have provided the nsIDocShell which the source
  // comes from. We'll use this to locate the corresponding BrowsingContext in
  // the parent process.
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aWebProgress);
  if (!docShell) {
    MOZ_ASSERT_UNREACHABLE("aWebProgress is null or not a nsIDocShell?");
    return NS_ERROR_UNEXPECTED;
  }

  WebProgressData webProgressData;
  Maybe<WebProgressStateChangeData> stateChangeData;
  RequestData requestData;

  MOZ_TRY(PrepareProgressListenerData(aWebProgress, aRequest, webProgressData,
                                      requestData));

  RefPtr<BrowsingContext> browsingContext = docShell->GetBrowsingContext();
  if (browsingContext->IsTopContent()) {
    stateChangeData.emplace();

    stateChangeData->isNavigating() = docShell->GetIsNavigating();
    stateChangeData->mayEnableCharacterEncodingMenu() =
        docShell->GetMayEnableCharacterEncodingMenu();

    RefPtr<Document> document = browsingContext->GetExtantDocument();
    if (document && aStateFlags & nsIWebProgressListener::STATE_STOP) {
      document->GetContentType(stateChangeData->contentType());
      document->GetCharacterSet(stateChangeData->charset());
      stateChangeData->documentURI() = document->GetDocumentURIObject();
    } else {
      stateChangeData->contentType().SetIsVoid(true);
      stateChangeData->charset().SetIsVoid(true);
    }
  }

  Unused << SendOnStateChange(webProgressData, requestData, aStateFlags,
                              aStatus, stateChangeData);

  return NS_OK;
}

NS_IMETHODIMP BrowserChild::OnProgressChange(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             int32_t aCurSelfProgress,
                                             int32_t aMaxSelfProgress,
                                             int32_t aCurTotalProgress,
                                             int32_t aMaxTotalProgress) {
  if (!IPCOpen() || !mShouldSendWebProgressEventsToParent) {
    return NS_OK;
  }

  // FIXME: We currently ignore ProgressChange events from out-of-process
  // subframes both here and in BrowserParent. We may want to change this
  // behaviour in the future.
  if (!GetBrowsingContext()->IsTopContent()) {
    return NS_OK;
  }

  // As we're being filtered by nsBrowserStatusFilter, we will be passed either
  // nullptr or 0 for all arguments other than aCurTotalProgress and
  // aMaxTotalProgress. Don't bother sending them.
  MOZ_ASSERT(!aWebProgress);
  MOZ_ASSERT(!aRequest);
  MOZ_ASSERT(aCurSelfProgress == 0);
  MOZ_ASSERT(aMaxSelfProgress == 0);

  Unused << SendOnProgressChange(aCurTotalProgress, aMaxTotalProgress);

  return NS_OK;
}

NS_IMETHODIMP BrowserChild::OnLocationChange(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             nsIURI* aLocation,
                                             uint32_t aFlags) {
  if (!IPCOpen() || !mShouldSendWebProgressEventsToParent) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aWebProgress);
  if (!docShell) {
    MOZ_ASSERT_UNREACHABLE("aWebProgress is null or not a nsIDocShell?");
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<BrowsingContext> browsingContext = docShell->GetBrowsingContext();
  RefPtr<Document> document = browsingContext->GetExtantDocument();
  if (!document) {
    return NS_OK;
  }

  WebProgressData webProgressData;
  RequestData requestData;

  MOZ_TRY(PrepareProgressListenerData(aWebProgress, aRequest, webProgressData,
                                      requestData));

  Maybe<WebProgressLocationChangeData> locationChangeData;

  bool canGoBack = false;
  bool canGoForward = false;
  if (!mozilla::SessionHistoryInParent()) {
    MOZ_TRY(WebNavigation()->GetCanGoBack(&canGoBack));
    MOZ_TRY(WebNavigation()->GetCanGoForward(&canGoForward));
  }

  if (browsingContext->IsTopContent()) {
    MOZ_ASSERT(
        browsingContext == GetBrowsingContext(),
        "Toplevel content BrowsingContext which isn't GetBrowsingContext()?");

    locationChangeData.emplace();

    document->GetContentType(locationChangeData->contentType());
    locationChangeData->isNavigating() = docShell->GetIsNavigating();
    locationChangeData->documentURI() = document->GetDocumentURIObject();
    document->GetTitle(locationChangeData->title());
    document->GetCharacterSet(locationChangeData->charset());

    locationChangeData->mayEnableCharacterEncodingMenu() =
        docShell->GetMayEnableCharacterEncodingMenu();

    locationChangeData->contentPrincipal() = document->NodePrincipal();
    locationChangeData->contentPartitionedPrincipal() =
        document->PartitionedPrincipal();
    locationChangeData->csp() = document->GetCsp();
    locationChangeData->referrerInfo() = document->ReferrerInfo();
    locationChangeData->isSyntheticDocument() = document->IsSyntheticDocument();

    if (nsCOMPtr<nsILoadGroup> loadGroup = document->GetDocumentLoadGroup()) {
      uint64_t requestContextID = 0;
      MOZ_TRY(loadGroup->GetRequestContextID(&requestContextID));
      locationChangeData->requestContextID() = Some(requestContextID);
    }

#ifdef MOZ_CRASHREPORTER
    if (CrashReporter::GetEnabled()) {
      nsCOMPtr<nsIURI> annotationURI;

      nsresult rv =
          NS_MutateURI(aLocation).SetUserPass(""_ns).Finalize(annotationURI);

      if (NS_FAILED(rv)) {
        // Ignore failures on about: URIs.
        annotationURI = aLocation;
      }

      CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::URL,
                                         annotationURI->GetSpecOrDefault());
    }
#endif
  }

  Unused << SendOnLocationChange(webProgressData, requestData, aLocation,
                                 aFlags, canGoBack, canGoForward,
                                 locationChangeData);

  return NS_OK;
}

NS_IMETHODIMP BrowserChild::OnStatusChange(nsIWebProgress* aWebProgress,
                                           nsIRequest* aRequest,
                                           nsresult aStatus,
                                           const char16_t* aMessage) {
  if (!IPCOpen() || !mShouldSendWebProgressEventsToParent) {
    return NS_OK;
  }

  // FIXME: We currently ignore StatusChange from out-of-process subframes both
  // here and in BrowserParent. We may want to change this behaviour in the
  // future.
  if (!GetBrowsingContext()->IsTopContent()) {
    return NS_OK;
  }

  // As we're being filtered by nsBrowserStatusFilter, we will be passed either
  // nullptr or NS_OK for all arguments other than aMessage. Don't bother
  // sending them.
  MOZ_ASSERT(!aWebProgress);
  MOZ_ASSERT(!aRequest);
  MOZ_ASSERT(aStatus == NS_OK);

  Unused << SendOnStatusChange(nsDependentString(aMessage));

  return NS_OK;
}

NS_IMETHODIMP BrowserChild::OnSecurityChange(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             uint32_t aState) {
  // Security changes are now handled entirely in the parent process
  // so we don't need to worry about forwarding them (and we shouldn't
  // be receiving any to forward).
  return NS_OK;
}

NS_IMETHODIMP BrowserChild::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                                   nsIRequest* aRequest,
                                                   uint32_t aEvent) {
  // The OnContentBlockingEvent only happenes in the parent process. It should
  // not be seen in the content process.
  MOZ_DIAGNOSTIC_ASSERT(
      false, "OnContentBlockingEvent should not be seen in content process.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP BrowserChild::OnProgressChange64(nsIWebProgress* aWebProgress,
                                               nsIRequest* aRequest,
                                               int64_t aCurSelfProgress,
                                               int64_t aMaxSelfProgress,
                                               int64_t aCurTotalProgress,
                                               int64_t aMaxTotalProgress) {
  // All the events we receive are filtered through an nsBrowserStatusFilter,
  // which accepts ProgressChange64 events, but truncates the progress values to
  // uint32_t and calls OnProgressChange.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP BrowserChild::OnRefreshAttempted(nsIWebProgress* aWebProgress,
                                               nsIURI* aRefreshURI,
                                               uint32_t aMillis, bool aSameURI,
                                               bool* aOut) {
  NS_ENSURE_ARG_POINTER(aOut);
  *aOut = true;

  return NS_OK;
}

NS_IMETHODIMP BrowserChild::NotifyNavigationFinished() {
  Unused << SendNavigationFinished();
  return NS_OK;
}

nsresult BrowserChild::PrepareRequestData(nsIRequest* aRequest,
                                          RequestData& aRequestData) {
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (!channel) {
    aRequestData.requestURI() = nullptr;
    return NS_OK;
  }

  nsresult rv = channel->GetURI(getter_AddRefs(aRequestData.requestURI()));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->GetOriginalURI(
      getter_AddRefs(aRequestData.originalRequestURI()));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIClassifiedChannel> classifiedChannel = do_QueryInterface(channel);
  if (classifiedChannel) {
    rv = classifiedChannel->GetMatchedList(aRequestData.matchedList());
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult BrowserChild::PrepareProgressListenerData(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest,
    WebProgressData& aWebProgressData, RequestData& aRequestData) {
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aWebProgress);
  if (!docShell) {
    MOZ_ASSERT_UNREACHABLE("aWebProgress is null or not a nsIDocShell?");
    return NS_ERROR_UNEXPECTED;
  }

  aWebProgressData.browsingContext() = docShell->GetBrowsingContext();
  nsresult rv = aWebProgress->GetLoadType(&aWebProgressData.loadType());
  NS_ENSURE_SUCCESS(rv, rv);

  return PrepareRequestData(aRequest, aRequestData);
}

void BrowserChild::UpdateSessionStore() {
  if (mSessionStoreChild) {
    mSessionStoreChild->UpdateSessionStore();
  }
}

#ifdef XP_WIN
RefPtr<PBrowserChild::IsWindowSupportingProtectedMediaPromise>
BrowserChild::DoesWindowSupportProtectedMedia() {
  MOZ_ASSERT(
      NS_IsMainThread(),
      "Protected media support check should be done on main thread only.");
  if (mWindowSupportsProtectedMedia) {
    // If we've already checked and have a cached result, resolve with that.
    return IsWindowSupportingProtectedMediaPromise::CreateAndResolve(
        mWindowSupportsProtectedMedia.value(), __func__);
  }
  RefPtr<BrowserChild> self = this;
  // We chain off the promise rather than passing it directly so we can cache
  // the result and use that for future calls.
  return SendIsWindowSupportingProtectedMedia(ChromeOuterWindowID())
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self](bool isSupported) {
            // If a result was cached while this check was inflight, ensure the
            // results match.
            MOZ_ASSERT_IF(
                self->mWindowSupportsProtectedMedia,
                self->mWindowSupportsProtectedMedia.value() == isSupported);
            // Cache the response as it will not change during the lifetime
            // of this object.
            self->mWindowSupportsProtectedMedia = Some(isSupported);
            return IsWindowSupportingProtectedMediaPromise::CreateAndResolve(
                self->mWindowSupportsProtectedMedia.value(), __func__);
          },
          [](ResponseRejectReason reason) {
            return IsWindowSupportingProtectedMediaPromise::CreateAndReject(
                reason, __func__);
          });
}
#endif

void BrowserChild::NotifyContentBlockingEvent(
    uint32_t aEvent, nsIChannel* aChannel, bool aBlocked,
    const nsACString& aTrackingOrigin,
    const nsTArray<nsCString>& aTrackingFullHashes,
    const Maybe<
        mozilla::ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
        aReason) {
  if (!IPCOpen()) {
    return;
  }

  RequestData requestData;
  if (NS_SUCCEEDED(PrepareRequestData(aChannel, requestData))) {
    Unused << SendNotifyContentBlockingEvent(
        aEvent, requestData, aBlocked, PromiseFlatCString(aTrackingOrigin),
        aTrackingFullHashes, aReason);
  }
}

NS_IMETHODIMP
BrowserChild::ContentTransformsReceived(JSContext* aCx,
                                        dom::Promise** aPromise) {
  auto* globalObject = xpc::CurrentNativeGlobal(aCx);
  ErrorResult rv;
  if (mChildToParentConversionMatrix) {
    // Already received content transforms
    RefPtr<Promise> promise =
        Promise::CreateResolvedWithUndefined(globalObject, rv);
    promise.forget(aPromise);
    return rv.StealNSResult();
  }

  if (!mContentTransformPromise) {
    mContentTransformPromise = Promise::Create(globalObject, rv);
  }

  MOZ_ASSERT(globalObject == mContentTransformPromise->GetGlobalObject());
  NS_IF_ADDREF(*aPromise = mContentTransformPromise);
  return rv.StealNSResult();
}

BrowserChildMessageManager::BrowserChildMessageManager(
    BrowserChild* aBrowserChild)
    : ContentFrameMessageManager(new nsFrameMessageManager(aBrowserChild)),
      mBrowserChild(aBrowserChild) {}

BrowserChildMessageManager::~BrowserChildMessageManager() = default;

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowserChildMessageManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BrowserChildMessageManager,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserChild);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BrowserChildMessageManager,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserChild)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowserChildMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(ContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BrowserChildMessageManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BrowserChildMessageManager, DOMEventTargetHelper)

JSObject* BrowserChildMessageManager::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ContentFrameMessageManager_Binding::Wrap(aCx, this, aGivenProto);
}

void BrowserChildMessageManager::MarkForCC() {
  if (mBrowserChild) {
    mBrowserChild->MarkScopesForCC();
  }
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->MarkForCC();
  }
  MessageManagerGlobal::MarkForCC();
}

Nullable<WindowProxyHolder> BrowserChildMessageManager::GetContent(
    ErrorResult& aError) {
  nsCOMPtr<nsIDocShell> docShell = GetDocShell(aError);
  if (!docShell) {
    return nullptr;
  }
  return WindowProxyHolder(docShell->GetBrowsingContext());
}

already_AddRefed<nsIDocShell> BrowserChildMessageManager::GetDocShell(
    ErrorResult& aError) {
  if (!mBrowserChild) {
    aError.Throw(NS_ERROR_NULL_POINTER);
    return nullptr;
  }
  nsCOMPtr<nsIDocShell> window =
      do_GetInterface(mBrowserChild->WebNavigation());
  return window.forget();
}

already_AddRefed<nsIEventTarget>
BrowserChildMessageManager::GetTabEventTarget() {
  nsCOMPtr<nsIEventTarget> target = EventTargetFor(TaskCategory::Other);
  return target.forget();
}

nsresult BrowserChildMessageManager::Dispatch(
    TaskCategory aCategory, already_AddRefed<nsIRunnable>&& aRunnable) {
  return DispatcherTrait::Dispatch(aCategory, std::move(aRunnable));
}

nsISerialEventTarget* BrowserChildMessageManager::EventTargetFor(
    TaskCategory aCategory) const {
  return DispatcherTrait::EventTargetFor(aCategory);
}

AbstractThread* BrowserChildMessageManager::AbstractMainThreadFor(
    TaskCategory aCategory) {
  return DispatcherTrait::AbstractMainThreadFor(aCategory);
}
