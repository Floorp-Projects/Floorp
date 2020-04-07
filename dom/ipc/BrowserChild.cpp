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
#include "ClientLayerManager.h"
#include "ContentChild.h"
#include "DocumentInlines.h"
#include "EventStateManager.h"
#include "FrameLayerBuilder.h"
#include "Layers.h"
#include "LayersLogging.h"
#include "MMPrinter.h"
#include "PermissionMessageUtils.h"
#include "PuppetWidget.h"
#include "StructuredCloneData.h"
#include "UnitTransforms.h"
#include "Units.h"
#include "VRManagerChild.h"
#include "ipc/nsGUIEventIPC.h"
#include "js/JSON.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BrowserElementParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventForwards.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/Unused.h"
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
#include "mozilla/dom/SessionStoreListener.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/gfx/CrossProcessPaint.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ContentProcessController.h"
#include "mozilla/layers/DoubleTapToZoom.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/plugins/PPluginWidgetChild.h"
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
#include "nsIScriptError.h"
#include "nsISecureBrowserUI.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIWebBrowser.h"
#include "nsIWebProgress.h"
#include "nsLayoutUtils.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsPointerHashKeys.h"
#include "nsPrintfCString.h"
#include "nsQueryActor.h"
#include "nsQueryObject.h"
#include "nsSandboxFlags.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "nsViewManager.h"
#include "nsViewportInfo.h"
#include "nsWebBrowser.h"
#include "nsWindowWatcher.h"

#ifdef XP_WIN
#  include "mozilla/plugins/PluginWidgetChild.h"
#endif

#ifdef NS_PRINTING
#  include "nsIPrintSession.h"
#  include "nsIPrintSettings.h"
#  include "nsIPrintSettingsService.h"
#  include "nsIWebBrowserPrint.h"
#endif

#define BROWSER_ELEMENT_CHILD_SCRIPT \
  NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js")

static mozilla::LazyLogModule sApzChildLog("apz.child");

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::docshell;
using namespace mozilla::widget;
using namespace mozilla::jsipc;
using mozilla::layers::GeckoContentController;

NS_IMPL_ISUPPORTS(ContentListener, nsIDOMEventListener)

static const char BEFORE_FIRST_PAINT[] = "before-first-paint";

nsTHashtable<nsPtrHashKey<BrowserChild>>* BrowserChild::sVisibleTabs;

typedef nsDataHashtable<nsUint64HashKey, BrowserChild*> BrowserChildMap;
static BrowserChildMap* sBrowserChildren;
StaticMutex sBrowserChildrenMutex;

already_AddRefed<Document> BrowserChild::GetTopLevelDocument() const {
  nsCOMPtr<Document> doc;
  WebNavigation()->GetDocument(getter_AddRefs(doc));
  return doc.forget();
}

PresShell* BrowserChild::GetTopLevelPresShell() const {
  if (RefPtr<Document> doc = GetTopLevelDocument()) {
    return doc->GetPresShell();
  }
  return nullptr;
}

void BrowserChild::DispatchMessageManagerMessage(const nsAString& aMessageName,
                                                 const nsAString& aJSONData) {
  AutoSafeJSContext cx;
  JS::Rooted<JS::Value> json(cx, JS::NullValue());
  dom::ipc::StructuredCloneData data;
  if (JS_ParseJSON(cx, static_cast<const char16_t*>(aJSONData.BeginReading()),
                   aJSONData.Length(), &json)) {
    ErrorResult rv;
    data.Write(cx, json, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return;
    }
  }

  RefPtr<BrowserChildMessageManager> kungFuDeathGrip(
      mBrowserChildMessageManager);
  RefPtr<nsFrameMessageManager> mm = kungFuDeathGrip->GetMessageManager();
  mm->ReceiveMessage(static_cast<EventTarget*>(kungFuDeathGrip), nullptr,
                     aMessageName, false, &data, nullptr, nullptr, nullptr,
                     IgnoreErrors());
}

bool BrowserChild::UpdateFrame(const RepaintRequest& aRequest) {
  MOZ_ASSERT(aRequest.GetScrollId() != ScrollableLayerGuid::NULL_SCROLL_ID);

  if (aRequest.IsRootContent()) {
    if (PresShell* presShell = GetTopLevelPresShell()) {
      // Guard against stale updates (updates meant for a pres shell which
      // has since been torn down and destroyed).
      if (aRequest.GetPresShellId() == presShell->GetPresShellId()) {
        ProcessUpdateFrame(aRequest);
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

void BrowserChild::ProcessUpdateFrame(const RepaintRequest& aRequest) {
  if (!mBrowserChildMessageManager) {
    return;
  }

  APZCCallbackHelper::UpdateRootFrame(aRequest);
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

  // In order to ensure that this runnable runs after everything that could
  // possibly touch this tab, we send it through the event queue twice. The
  // first time it runs at normal priority and the second time it runs at
  // input priority. This ensures that it runs after all events that were in
  // either queue at the time it was first dispatched. mReadyToDelete starts
  // out false (when it runs at normal priority) and is then set to true.
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
    *aPriority = mReadyToDelete ? nsIRunnablePriority::PRIORITY_INPUT_HIGH
                                : nsIRunnablePriority::PRIORITY_NORMAL;
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
      mDidFakeShow(false),
      mNotified(false),
      mTriedBrowserInit(false),
      mOrientation(hal::eScreenOrientation_PortraitPrimary),
      mIgnoreKeyPressEvent(false),
      mHasValidInnerSize(false),
      mDestroyed(false),
      mDynamicToolbarMaxHeight(0),
      mUniqueId(aTabId),
      mIsTopLevel(aIsTopLevel),
      mHasSiblings(false),
      mIsTransparent(false),
      mIPCOpen(false),
      mParentIsActive(false),
      mDidSetRealShowInfo(false),
      mDidLoadURLInit(false),
      mAwaitingLA(false),
      mSkipKeyPress(false),
      mLayersObserverEpoch{1},
#if defined(XP_WIN) && defined(ACCESSIBILITY)
      mNativeWindowHandle(0),
#endif
#if defined(ACCESSIBILITY)
      mTopLevelDocAccessibleChild(nullptr),
#endif
      mShouldSendWebProgressEventsToParent(false),
      mRenderLayers(true),
      mPendingDocShellIsActive(false),
      mPendingDocShellReceivedMessage(false),
      mPendingRenderLayers(false),
      mPendingRenderLayersReceivedMessage(false),
      mPendingLayersObserverEpoch{0},
      mPendingDocShellBlockers(0),
      mCancelContentJSEpoch(0),
      mWidgetNativeData(0) {
  mozilla::HoldJSObjects(this);

  nsWeakPtr weakPtrThis(do_GetWeakReference(
      static_cast<nsIBrowserChild*>(this)));  // for capture by the lambda
  mSetAllowedTouchBehaviorCallback =
      [weakPtrThis](uint64_t aInputBlockId,
                    const nsTArray<TouchBehaviorFlags>& aFlags) {
        if (nsCOMPtr<nsIBrowserChild> browserChild =
                do_QueryReferent(weakPtrThis)) {
          static_cast<BrowserChild*>(browserChild.get())
              ->SetAllowedTouchBehavior(aInputBlockId, aFlags);
        }
      };

  // preloaded BrowserChild should not be added to child map
  if (mUniqueId) {
    MOZ_ASSERT(NestedBrowserChildMap().find(mUniqueId) ==
               NestedBrowserChildMap().end());
    NestedBrowserChildMap()[mUniqueId] = this;
  }
  mCoalesceMouseMoveEvents =
      Preferences::GetBool("dom.event.coalesce_mouse_move");
  if (mCoalesceMouseMoveEvents) {
    mCoalescedMouseEventFlusher = new CoalescedMouseMoveFlusher(this);
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

      if (subject == doc && doc->IsTopLevelContentDocument()) {
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

void BrowserChild::SetAllowedTouchBehavior(
    uint64_t aInputBlockId,
    const nsTArray<TouchBehaviorFlags>& aTargets) const {
  if (mApzcTreeManager) {
    mApzcTreeManager->SetAllowedTouchBehavior(aInputBlockId, aTargets);
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

  mWebBrowser = nsWebBrowser::Create(this, mPuppetWidget, OriginAttributesRef(),
                                     mBrowsingContext, aInitialWindowChild);
  nsIWebBrowser* webBrowser = mWebBrowser;

  mWebNav = do_QueryInterface(webBrowser);
  NS_ASSERTION(mWebNav, "nsWebBrowser doesn't implement nsIWebNavigation?");

  // Set the tab context attributes then pass to docShell
  NotifyTabContextUpdated(false);

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

  docShell->SetAffectPrivateSessionLifetime(
      mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME);
  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(WebNavigation());
  MOZ_ASSERT(loadContext);
  loadContext->SetPrivateBrowsing(OriginAttributesRef().mPrivateBrowsingId > 0);
  loadContext->SetRemoteTabs(mChromeFlags &
                             nsIWebBrowserChrome::CHROME_REMOTE_WINDOW);
  loadContext->SetRemoteSubframes(mChromeFlags &
                                  nsIWebBrowserChrome::CHROME_FISSION_WINDOW);

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
        window->GetDocShell(),
        !!(mChromeFlags & nsIWebBrowserChrome::CHROME_SCROLLBARS));
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

#if !defined(MOZ_WIDGET_ANDROID) && !defined(MOZ_THUNDERBIRD) && \
    !defined(MOZ_SUITE)
  mSessionStoreListener = new TabListener(docShell, nullptr);
  rv = mSessionStoreListener->Init();
  NS_ENSURE_SUCCESS(rv, rv);
#endif
  return NS_OK;
}

void BrowserChild::NotifyTabContextUpdated(bool aIsPreallocated) {
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  if (!docShell) {
    return;
  }

  UpdateFrameType();

  if (aIsPreallocated) {
    nsDocShell::Cast(docShell)->SetOriginAttributes(OriginAttributesRef());
  }

  // Set SANDBOXED_AUXILIARY_NAVIGATION flag if this is a receiver page.
  if (!PresentationURL().IsEmpty()) {
    mBrowsingContext->SetSandboxFlags(SANDBOXED_AUXILIARY_NAVIGATION);
  }
}

void BrowserChild::UpdateFrameType() {
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  // TODO: Bug 1252794 - remove frameType from nsIDocShell.idl
  docShell->SetFrameType(IsMozBrowserElement()
                             ? nsIDocShell::FRAME_TYPE_BROWSER
                             : nsIDocShell::FRAME_TYPE_REGULAR);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BrowserChild)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BrowserChild)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserChildMessageManager)
  tmp->nsMessageManagerScriptExecutor::Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWebBrowserChrome)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStatusFilter)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWebNav)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowsingContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BrowserChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserChildMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebBrowserChrome)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStatusFilter)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebNav)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowsingContext)
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
BrowserChild::SetFocus() { return NS_ERROR_NOT_IMPLEMENTED; }

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
  if (aIID.Equals(NS_GET_IID(nsIWebBrowserChrome3))) {
    return GetWebBrowserChrome(reinterpret_cast<nsIWebBrowserChrome3**>(aSink));
  }

  // XXXbz should we restrict the set of interfaces we hand out here?
  // See bug 537429
  return QueryInterface(aIID, aSink);
}

NS_IMETHODIMP
BrowserChild::ProvideWindow(mozIDOMWindowProxy* aParent, uint32_t aChromeFlags,
                            bool aCalledFromJS, bool aWidthSpecified,
                            nsIURI* aURI, const nsAString& aName,
                            const nsACString& aFeatures, bool aForceNoOpener,
                            bool aForceNoReferrer,
                            nsDocShellLoadState* aLoadState, bool* aWindowIsNew,
                            BrowsingContext** aReturn) {
  *aReturn = nullptr;

  // If aParent is inside an <iframe mozbrowser> and this isn't a request to
  // open a modal-type window, we're going to create a new <iframe mozbrowser>
  // and return its window here.
  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
  bool iframeMoz =
      (docshell && docshell->GetIsInMozBrowser() &&
       !(aChromeFlags & (nsIWebBrowserChrome::CHROME_MODAL |
                         nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                         nsIWebBrowserChrome::CHROME_OPENAS_CHROME)));

  if (!iframeMoz) {
    int32_t openLocation = nsWindowWatcher::GetWindowOpenLocation(
        nsPIDOMWindowOuter::From(aParent), aChromeFlags, aCalledFromJS,
        aWidthSpecified);

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
  }

  // Note that ProvideWindowCommon may return NS_ERROR_ABORT if the
  // open window call was canceled.  It's important that we pass this error
  // code back to our caller.
  ContentChild* cc = ContentChild::GetSingleton();
  return cc->ProvideWindowCommon(this, aParent, iframeMoz, aChromeFlags,
                                 aCalledFromJS, aWidthSpecified, aURI, aName,
                                 aFeatures, aForceNoOpener, aForceNoReferrer,
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

  if (mSessionStoreListener) {
    mSessionStoreListener->RemoveListeners();
    mSessionStoreListener = nullptr;
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

  CompositorBridgeChild* compositorChild = CompositorBridgeChild::Get();
  if (compositorChild) {
    compositorChild->CancelNotifyAfterRemotePaint(this);
  }

  if (GetTabId() != 0) {
    NestedBrowserChildMap().erase(GetTabId());
  }
}

BrowserChild::~BrowserChild() {
  mAnonymousGlobalScopes.Clear();
  if (sVisibleTabs) {
    sVisibleTabs->RemoveEntry(this);
    if (sVisibleTabs->IsEmpty()) {
      delete sVisibleTabs;
      sVisibleTabs = nullptr;
    }
  }

  DestroyWindow();

  nsCOMPtr<nsIWebBrowser> webBrowser = do_QueryInterface(WebNavigation());
  if (webBrowser) {
    webBrowser->SetContainerWindow(nullptr);
  }

  mozilla::DropJSObjects(this);
}

mozilla::ipc::IPCResult BrowserChild::RecvWillChangeProcess(
    WillChangeProcessResolver&& aResolve) {
  if (mWebBrowser) {
    mWebBrowser->SetWillChangeProcess();
  }
  aResolve(true);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvLoadURL(const nsCString& aURI,
                                                  const ParentShowInfo& aInfo) {
  if (!mDidLoadURLInit) {
    mDidLoadURLInit = true;
    if (!InitBrowserChildMessageManager()) {
      return IPC_FAIL_NO_REASON(this);
    }

    ApplyParentShowInfo(aInfo);
  }

  LoadURIOptions loadURIOptions;
  loadURIOptions.mTriggeringPrincipal = nsContentUtils::GetSystemPrincipal();
  loadURIOptions.mLoadFlags =
      nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
      nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;

  nsIWebNavigation* webNav = WebNavigation();
  nsresult rv = webNav->LoadURI(NS_ConvertUTF8toUTF16(aURI), loadURIOptions);
  if (NS_FAILED(rv)) {
    NS_WARNING(
        "WebNavigation()->LoadURI failed. Eating exception, what else can I "
        "do?");
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (docShell) {
    nsDocShell::Cast(docShell)->MaybeClearStorageAccessFlag();
  }

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::URL, aURI);

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

void BrowserChild::DoFakeShow(const ParentShowInfo& aParentShowInfo) {
  OwnerShowInfo ownerInfo{ScreenIntSize(), ScrollbarPreference::Auto,
                          mParentIsActive, nsSizeMode_Normal};
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

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (docShell) {
    docShell->SetFullscreenAllowed(aInfo.fullscreenAllowed());
    if (aInfo.isPrivate()) {
      nsCOMPtr<nsILoadContext> context = do_GetInterface(docShell);
      // No need to re-set private browsing mode.
      if (!context->UsePrivateBrowsing()) {
        if (docShell->GetHasLoadedNonBlankURI()) {
          nsContentUtils::ReportToConsoleNonLocalized(
              NS_LITERAL_STRING("We should not switch to Private Browsing "
                                "after loading a document."),
              nsIScriptError::warningFlag,
              NS_LITERAL_CSTRING("mozprivatebrowsing"), nullptr);
        } else {
          OriginAttributes attrs(
              nsDocShell::Cast(docShell)->GetOriginAttributes());
          attrs.SyncAttributesWithPrivateBrowsing(true);
          nsDocShell::Cast(docShell)->SetOriginAttributes(attrs);
        }
      }
    }
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
  RecvParentActivated(aOwnerInfo.parentWindowIsActive());
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

  mOrientation = aDimensionInfo.orientation();
  SetUnscaledInnerSize(aDimensionInfo.size());
  if (!mHasValidInnerSize && aDimensionInfo.size().width != 0 &&
      aDimensionInfo.size().height != 0) {
    mHasValidInnerSize = true;
  }

  ScreenIntSize screenSize = GetInnerSize();
  ScreenIntRect screenRect = GetOuterRect();

  // Set the size on the document viewer before we update the widget and
  // trigger a reflow. Otherwise the MobileViewportManager reads the stale
  // size from the content viewer when it computes a new CSS viewport.
  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
  baseWin->SetPositionAndSize(0, 0, screenSize.width, screenSize.height,
                              nsIBaseWindow::eRepaint);

  mPuppetWidget->Resize(screenRect.x + mClientOffset.x + mChromeOffset.x,
                        screenRect.y + mClientOffset.y + mChromeOffset.y,
                        screenSize.width, screenSize.height, true);

  // For our devtools Responsive Design Mode, we need to send a special
  // event to indicate that we've finished processing a frame size change.
  // This is used by RDM to respond correctly to changes to full zoom,
  // which also change the window size.
  RefPtr<Document> doc = GetTopLevelDocument();
  BrowsingContext* bc = doc ? doc->GetBrowsingContext() : nullptr;
  if (bc && bc->InRDMPane()) {
    RefPtr<AsyncEventDispatcher> dispatcher = new AsyncEventDispatcher(
        doc, NS_LITERAL_STRING("mozupdatedremoteframedimensions"),
        CanBubble::eYes, ChromeOnlyDispatch::eYes);
    dispatcher->PostDOMEvent();
  }

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
      ("Handling double tap at %s with %p %p\n", Stringify(aPoint).c_str(),
       mBrowserChildMessageManager ? mBrowserChildMessageManager->GetWrapper()
                                   : nullptr,
       mBrowserChildMessageManager.get()));

  if (!mBrowserChildMessageManager) {
    return;
  }

  // Note: there is nothing to do with the modifiers here, as we are not
  // synthesizing any sort of mouse event.
  RefPtr<Document> document = GetTopLevelDocument();
  CSSRect zoomToRect = CalculateRectToZoomTo(document, aPoint);
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

    mApzcTreeManager->ZoomToRect(guid, zoomToRect, DEFAULT_BEHAVIOR);
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
  CSSPoint point =
      APZCCallbackHelper::ApplyCallbackTransform(aPoint / scale, aGuid);

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
    mApzcTreeManager->ZoomToRect(guid, aRect, aFlags);
  }
}

mozilla::ipc::IPCResult BrowserChild::RecvActivate() {
  MOZ_ASSERT(mWebBrowser);
  // Ensure that the PresShell exists, otherwise focusing
  // is definitely not going to work. GetPresShell should
  // create a PresShell if one doesn't exist yet.
  RefPtr<PresShell> presShell = GetTopLevelPresShell();
  NS_ASSERTION(presShell, "Need a PresShell to activate!");
  Unused << presShell;

  if (presShell) {
    mWebBrowser->FocusActivate();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvDeactivate() {
  MOZ_ASSERT(mWebBrowser);
  mWebBrowser->FocusDeactivate();
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvParentActivated(
    const bool& aActivated) {
  mParentIsActive = aActivated;

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, IPC_OK());

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  fm->ParentActivated(window, aActivated);
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

mozilla::ipc::IPCResult BrowserChild::RecvMouseEvent(
    const nsString& aType, const float& aX, const float& aY,
    const int32_t& aButton, const int32_t& aClickCount,
    const int32_t& aModifiers, const bool& aIgnoreRootScrollFrame) {
  // IPDL doesn't hold a strong reference to protocols as they're not required
  // to be refcounted. This function can run script, which may trigger a nested
  // event loop, which may release this, so we hold a strong reference here.
  RefPtr<BrowserChild> kungFuDeathGrip(this);
  RefPtr<PresShell> presShell = GetTopLevelPresShell();
  APZCCallbackHelper::DispatchMouseEvent(
      presShell, aType, CSSPoint(aX, aY), aButton, aClickCount, aModifiers,
      aIgnoreRootScrollFrame, MouseEvent_Binding::MOZ_SOURCE_UNKNOWN,
      0 /* Use the default value here. */);
  return IPC_OK();
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

ScreenRect BrowserChild::GetTopLevelViewportVisibleRectInBrowserCoords() const {
  return mTopLevelViewportVisibleRectInBrowserCoords;
}

void BrowserChild::FlushAllCoalescedMouseData() {
  MOZ_ASSERT(mCoalesceMouseMoveEvents);

  // Move all entries from mCoalescedMouseData to mToBeDispatchedMouseData.
  for (auto iter = mCoalescedMouseData.Iter(); !iter.Done(); iter.Next()) {
    CoalescedMouseData* data = iter.UserData();
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
        mCoalescedMouseData.LookupOrAdd(aEvent.pointerId);
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
    CoalescedMouseData* newData = new CoalescedMouseData();
    mCoalescedMouseData.Put(aEvent.pointerId, newData);
    newData->Coalesce(aEvent, aGuid, aInputBlockId);

    // Dispatch all pending mouse events.
    ProcessPendingCoalescedMouseDataAndDispatchEvents();
    mCoalescedMouseEventFlusher->StartObserver();
  } else if (!RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityRealMouseMoveEvent(
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
  // Mouse events like eMouseEnterIntoWidget, that are created in the parent
  // process EventStateManager code, have an input block id which they get from
  // the InputAPZContext in the parent process stack. However, they did not
  // actually go through the APZ code and so their mHandledByAPZ flag is false.
  // Since thos events didn't go through APZ, we don't need to send
  // notifications for them.
  UniquePtr<DisplayportSetListener> postLayerization;
  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    nsCOMPtr<Document> document(GetTopLevelDocument());
    postLayerization = APZCCallbackHelper::SendSetTargetAPZCNotification(
        mPuppetWidget, document, aEvent, aGuid.mLayersId, aInputBlockId);
  }

  InputAPZContext context(aGuid, aInputBlockId, nsEventStatus_eIgnore,
                          postLayerization != nullptr);

  WidgetMouseEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
                                             mPuppetWidget->GetDefaultScale());
  DispatchWidgetEventViaAPZ(localEvent);

  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    mAPZEventState->ProcessMouseEvent(aEvent, aInputBlockId);
  }

  // Do this after the DispatchWidgetEventViaAPZ call above, so that if the
  // mouse event triggered a post-refresh AsyncDragMetrics message to be sent
  // to APZ (from scrollbar dragging in nsSliderFrame), then that will reach
  // APZ before the SetTargetAPZC message. This ensures the drag input block
  // gets the drag metrics before handling the input events.
  if (postLayerization && postLayerization->Register()) {
    Unused << postLayerization.release();
  }
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityRealMouseButtonEvent(
    const WidgetMouseEvent& aEvent, const ScrollableLayerGuid& aGuid,
    const uint64_t& aInputBlockId) {
  return RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId);
}

// In case handling repeated mouse wheel takes much time, we skip firing current
// wheel event if it may be coalesced to the next one.
bool BrowserChild::MaybeCoalesceWheelEvent(const WidgetWheelEvent& aEvent,
                                           const ScrollableLayerGuid& aGuid,
                                           const uint64_t& aInputBlockId,
                                           bool* aIsNextWheelEvent) {
  MOZ_ASSERT(aIsNextWheelEvent);
  if (aEvent.mMessage == eWheel) {
    GetIPCChannel()->PeekMessages(
        [aIsNextWheelEvent](const IPC::Message& aMsg) -> bool {
          if (aMsg.type() == mozilla::dom::PBrowser::Msg_MouseWheelEvent__ID) {
            *aIsNextWheelEvent = true;
          }
          return false;  // Stop peeking.
        });
    // We only coalesce the current event when
    // 1. It's eWheel (we don't coalesce eOperationStart and eWheelOperationEnd)
    // 2. It's not the first wheel event.
    // 3. It's not the last wheel event.
    // 4. It's dispatched before the last wheel event was processed +
    //    the processing time of the last event.
    //    This way pages spending lots of time in wheel listeners get wheel
    //    events coalesced more aggressively.
    // 5. It has same attributes as the coalesced wheel event which is not yet
    //    fired.
    if (!mLastWheelProcessedTimeFromParent.IsNull() && *aIsNextWheelEvent &&
        aEvent.mTimeStamp < (mLastWheelProcessedTimeFromParent +
                             mLastWheelProcessingDuration) &&
        (mCoalescedWheelData.IsEmpty() ||
         mCoalescedWheelData.CanCoalesce(aEvent, aGuid, aInputBlockId))) {
      mCoalescedWheelData.Coalesce(aEvent, aGuid, aInputBlockId);
      return true;
    }
  }
  return false;
}

nsEventStatus BrowserChild::DispatchWidgetEventViaAPZ(WidgetGUIEvent& aEvent) {
  aEvent.ResetWaitingReplyFromRemoteProcessState();
  return APZCCallbackHelper::DispatchWidgetEvent(aEvent);
}

void BrowserChild::MaybeDispatchCoalescedWheelEvent() {
  if (mCoalescedWheelData.IsEmpty()) {
    return;
  }
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
    UniquePtr<DisplayportSetListener> postLayerization =
        APZCCallbackHelper::SendSetTargetAPZCNotification(
            mPuppetWidget, document, aEvent, aGuid.mLayersId, aInputBlockId);
    if (postLayerization && postLayerization->Register()) {
      Unused << postLayerization.release();
    }
  }

  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
                                             mPuppetWidget->GetDefaultScale());
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
  if (MaybeCoalesceWheelEvent(aEvent, aGuid, aInputBlockId,
                              &isNextWheelEvent)) {
    return IPC_OK();
  }
  if (isNextWheelEvent) {
    // Update mLastWheelProcessedTimeFromParent so that we can compare the end
    // time of the current event with the dispatched time of the next event.
    mLastWheelProcessedTimeFromParent = aEvent.mTimeStamp;
    mozilla::TimeStamp beforeDispatchingTime = TimeStamp::Now();
    MaybeDispatchCoalescedWheelEvent();
    DispatchWheelEvent(aEvent, aGuid, aInputBlockId);
    mLastWheelProcessingDuration = (TimeStamp::Now() - beforeDispatchingTime);
    mLastWheelProcessedTimeFromParent += mLastWheelProcessingDuration;
  } else {
    // This is the last wheel event. Set mLastWheelProcessedTimeFromParent to
    // null moment to avoid coalesce the next incoming wheel event.
    mLastWheelProcessedTimeFromParent = TimeStamp();
    MaybeDispatchCoalescedWheelEvent();
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

  WidgetTouchEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
                                             mPuppetWidget->GetDefaultScale());

  if (localEvent.mMessage == eTouchStart && AsyncPanZoomEnabled()) {
    nsCOMPtr<Document> document = GetTopLevelDocument();
    if (StaticPrefs::layout_css_touch_action_enabled()) {
      APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(
          mPuppetWidget, document, localEvent, aInputBlockId,
          mSetAllowedTouchBehaviorCallback);
    }
    UniquePtr<DisplayportSetListener> postLayerization =
        APZCCallbackHelper::SendSetTargetAPZCNotification(
            mPuppetWidget, document, localEvent, aGuid.mLayersId,
            aInputBlockId);
    if (postLayerization && postLayerization->Register()) {
      Unused << postLayerization.release();
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
                                    aApzResponse, status);
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

mozilla::ipc::IPCResult BrowserChild::RecvPluginEvent(
    const WidgetPluginEvent& aEvent) {
  WidgetPluginEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  nsEventStatus status = DispatchWidgetEventViaAPZ(localEvent);
  if (status != nsEventStatus_eConsumeNoDefault) {
    // If not consumed, we should call default action
    SendDefaultProcOfPluginEvent(aEvent);
  }
  return IPC_OK();
}

void BrowserChild::RequestEditCommands(nsIWidget::NativeKeyBindingsType aType,
                                       const WidgetKeyboardEvent& aEvent,
                                       nsTArray<CommandInt>& aCommands) {
  MOZ_ASSERT(aCommands.IsEmpty());

  if (NS_WARN_IF(aEvent.IsEditCommandsInitialized(aType))) {
    aCommands = aEvent.EditCommandsConstRef(aType);
    return;
  }

  switch (aType) {
    case nsIWidget::NativeKeyBindingsForSingleLineEditor:
    case nsIWidget::NativeKeyBindingsForMultiLineEditor:
    case nsIWidget::NativeKeyBindingsForRichTextEditor:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid native key bindings type");
  }

  // Don't send aEvent to the parent process directly because it'll be marked
  // as posted to remote process.
  WidgetKeyboardEvent localEvent(aEvent);
  SendRequestNativeKeyBindings(aType, localEvent, &aCommands);
}

mozilla::ipc::IPCResult BrowserChild::RecvNativeSynthesisResponse(
    const uint64_t& aObserverId, const nsCString& aResponse) {
  mozilla::widget::AutoObserverNotifier::NotifySavedObserver(aObserverId,
                                                             aResponse.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvFlushTabState(
    const uint32_t& aFlushId, const bool& aIsFinal) {
  UpdateSessionStore(aFlushId, aIsFinal);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvUpdateEpoch(const uint32_t& aEpoch) {
  mSessionStoreListener->SetEpoch(aEpoch);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvUpdateSHistory(
    const bool& aImmediately) {
  if (mSessionStoreListener) {
    mSessionStoreListener->UpdateSHistoryChanges(aImmediately);
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
    const WidgetKeyboardEvent& aEvent) {
  if (SkipRepeatedKeyEvent(aEvent)) {
    return IPC_OK();
  }

  MOZ_ASSERT(
      aEvent.mMessage != eKeyPress || aEvent.AreAllEditCommandsInitialized(),
      "eKeyPress event should have native key binding information");

  // If content code called preventDefault() on a keydown event, then we don't
  // want to process any following keypress events.
  if (aEvent.mMessage == eKeyPress && mIgnoreKeyPressEvent) {
    return IPC_OK();
  }

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  localEvent.mUniqueId = aEvent.mUniqueId;
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

  // If a response is desired from the content process, resend the key event.
  if (aEvent.WantReplyFromContentProcess()) {
    // If the event's default isn't prevented but the status is no default,
    // That means that the event was consumed by EventStateManager or something
    // which is not a usual event handler.  In such case, prevent its default
    // as a default handler.  For example, when an eKeyPress event matches
    // with a content accesskey, and it's executed, peventDefault() of the
    // event won't be called but the status is set to "no default".  Then,
    // the event shouldn't be handled by nsMenuBarListener in the main process.
    if (!localEvent.DefaultPrevented() &&
        status == nsEventStatus_eConsumeNoDefault) {
      localEvent.PreventDefault();
    }
    // This is an ugly hack, mNoRemoteProcessDispatch is set to true when the
    // event's PreventDefault() or StopScrollProcessForwarding() is called.
    // And then, it'll be checked by ParamTraits<mozilla::WidgetEvent>::Write()
    // whether the event is being sent to remote process unexpectedly.
    // However, unfortunately, it cannot check the destination.  Therefore,
    // we need to clear the flag explicitly here because ParamTraits should
    // keep checking the flag for avoiding regression.
    localEvent.mFlags.mNoRemoteProcessDispatch = false;
    SendReplyKeyEvent(localEvent);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNormalPriorityRealKeyEvent(
    const WidgetKeyboardEvent& aEvent) {
  return RecvRealKeyEvent(aEvent);
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

mozilla::ipc::IPCResult BrowserChild::RecvPasteTransferable(
    const IPCDataTransfer& aDataTransfer, const bool& aIsPrivateData,
    nsIPrincipal* aRequestingPrincipal, const uint32_t& aContentPolicyType) {
  nsresult rv;
  nsCOMPtr<nsITransferable> trans =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  trans->Init(nullptr);

  rv = nsContentUtils::IPCTransferableToTransferable(
      aDataTransfer, aIsPrivateData, aRequestingPrincipal, aContentPolicyType,
      trans, nullptr, this);
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
    PDocAccessibleChild*, const uint64_t&, const uint32_t&,
    const IAccessibleHolder&) {
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
    const nsString& aMessage, nsTArray<CpowEntry>&& aCpows,
    nsIPrincipal* aPrincipal, const ClonedMessageData& aData) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("BrowserChild::RecvAsyncMessage",
                                             OTHER, aMessage);
  MMPrinter::Print("BrowserChild::RecvAsyncMessage", aMessage, aData);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
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
  UnpackClonedMessageDataForChild(aData, data);
  mm->ReceiveMessage(static_cast<EventTarget*>(mBrowserChildMessageManager),
                     nullptr, aMessage, false, &data, &cpows, aPrincipal,
                     nullptr, IgnoreErrors());
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

  // Since mIsMozBrowserElement may change in UpdateTabContextAfterSwap, so we
  // call UpdateFrameType here to make sure the frameType on the docshell is
  // correct.
  UpdateFrameType();

  // Ignore previous value of mTriedBrowserInit since owner content has changed.
  mTriedBrowserInit = true;
  // Initialize the child side of the browser element machinery, if appropriate.
  if (IsMozBrowserElement()) {
    RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
  }

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

mozilla::ipc::IPCResult BrowserChild::RecvSetUseGlobalHistory(
    const bool& aUse) {
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  nsresult rv = docShell->SetUseGlobalHistory(aUse);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to set UseGlobalHistory on BrowserChild docShell");
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvPrint(const uint64_t& aOuterWindowID,
                                                const PrintData& aPrintData) {
#ifdef NS_PRINTING
  nsGlobalWindowOuter* outerWindow =
      nsGlobalWindowOuter::GetOuterWindowWithId(aOuterWindowID);
  if (NS_WARN_IF(!outerWindow)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint =
      do_GetInterface(ToSupports(outerWindow));
  if (NS_WARN_IF(!webBrowserPrint)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");
  if (NS_WARN_IF(!printSettingsSvc)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrintSettings> printSettings;
  nsresult rv =
      printSettingsSvc->GetNewPrintSettings(getter_AddRefs(printSettings));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrintSession> printSession =
      do_CreateInstance("@mozilla.org/gfx/printsession;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_OK();
  }

  printSettings->SetPrintSession(printSession);
  printSettingsSvc->DeserializeToPrintSettings(aPrintData, printSettings);
  rv = webBrowserPrint->Print(printSettings, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_OK();
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
    mBrowserChildMessageManager->DispatchTrustedEvent(
        NS_LITERAL_STRING("unload"));
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

void BrowserChild::AddPendingDocShellBlocker() { mPendingDocShellBlockers++; }

void BrowserChild::RemovePendingDocShellBlocker() {
  mPendingDocShellBlockers--;
  if (!mPendingDocShellBlockers && mPendingDocShellReceivedMessage) {
    mPendingDocShellReceivedMessage = false;
    InternalSetDocShellIsActive(mPendingDocShellIsActive);
  }
  if (!mPendingDocShellBlockers && mPendingRenderLayersReceivedMessage) {
    mPendingRenderLayersReceivedMessage = false;
    RecvRenderLayers(mPendingRenderLayers, mPendingLayersObserverEpoch);
  }
}

void BrowserChild::InternalSetDocShellIsActive(bool aIsActive) {
  if (nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation())) {
    docShell->SetIsActive(aIsActive);
  }
}

mozilla::ipc::IPCResult BrowserChild::RecvSetDocShellIsActive(
    const bool& aIsActive) {
  // If we're currently waiting for window opening to complete, we need to hold
  // off on setting the docshell active. We queue up the values we're receiving
  // in the mWindowOpenDocShellActiveStatus.
  if (mPendingDocShellBlockers > 0) {
    mPendingDocShellReceivedMessage = true;
    mPendingDocShellIsActive = aIsActive;
    return IPC_OK();
  }

  InternalSetDocShellIsActive(aIsActive);
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
    RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
    MOZ_ASSERT(lm);

    // We send the current layer observer epoch to the compositor so that
    // BrowserParent knows whether a layer update notification corresponds to
    // the latest RecvRenderLayers request that was made.
    lm->SetLayersObserverEpoch(mLayersObserverEpoch);
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
    FrameLayerBuilder::InvalidateAllLayersForFrame(
        nsLayoutUtils::GetDisplayRootFrame(root));
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
      presShell->Paint(view, view->GetBounds(), PaintFlags::PaintLayers);
    }
  }
  presShell->SuppressDisplayport(false);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvNavigateByKey(
    const bool& aForward, const bool& aForDocumentNavigation) {
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    RefPtr<Element> result;
    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());

    // Move to the first or last document.
    uint32_t type =
        aForward
            ? (aForDocumentNavigation
                   ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FIRSTDOC)
                   : static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_ROOT))
            : (aForDocumentNavigation
                   ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LASTDOC)
                   : static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LAST));
    fm->MoveFocus(window, nullptr, type, nsIFocusManager::FLAG_BYKEY,
                  getter_AddRefs(result));

    // No valid root element was found, so move to the first focusable element.
    if (!result && aForward && !aForDocumentNavigation) {
      fm->MoveFocus(window, nullptr, nsIFocusManager::MOVEFOCUS_FIRST,
                    nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));
    }

    SendRequestFocus(false, CallerType::System);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvHandledWindowedPluginKeyEvent(
    const NativeEventData& aKeyEventData, const bool& aIsConsumed) {
  if (NS_WARN_IF(!mPuppetWidget)) {
    return IPC_OK();
  }
  mPuppetWidget->HandledWindowedPluginKeyEvent(aKeyEventData, aIsConsumed);
  return IPC_OK();
}

bool BrowserChild::InitBrowserChildMessageManager() {
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
    // Initialize the child side of the browser element machinery,
    // if appropriate.
    if (IsMozBrowserElement()) {
      RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
    }
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
    MOZ_ASSERT(!sBrowserChildren->Get(uint64_t(aLayersId)));
    sBrowserChildren->Put(uint64_t(aLayersId), this);
    mLayersId = aLayersId;
  }

  MOZ_ASSERT(!mPuppetWidget->HasLayerManager());
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
    RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
    MOZ_ASSERT(lm);
    lm->SetLayersObserverEpoch(mLayersObserverEpoch);
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

  bool success = false;
  if (mCompositorOptions->UseWebRender()) {
    success = mPuppetWidget->CreateRemoteLayerManager(
        [&](LayerManager* aLayerManager) -> bool {
          MOZ_ASSERT(aLayerManager->AsWebRenderLayerManager());
          return aLayerManager->AsWebRenderLayerManager()->Initialize(
              aCompositorChild, wr::AsPipelineId(mLayersId),
              &mTextureFactoryIdentifier);
        });
  } else {
    nsTArray<LayersBackend> ignored;
    PLayerTransactionChild* shadowManager =
        aCompositorChild->SendPLayerTransactionConstructor(ignored,
                                                           GetLayersId());
    if (shadowManager &&
        shadowManager->SendGetTextureFactoryIdentifier(
            &mTextureFactoryIdentifier) &&
        mTextureFactoryIdentifier.mParentBackend !=
            LayersBackend::LAYERS_NONE) {
      success = true;
    }
    if (!success) {
      // Since no LayerManager is associated with the tab's widget, we will
      // never have an opportunity to destroy the PLayerTransaction on the next
      // device or compositor reset. Therefore, we make sure to forcefully close
      // it here. Failure to do so will cause the next layer tree to fail to
      // attach due since the compositor requires the old layer tree to be
      // disassociated.
      if (shadowManager) {
        static_cast<LayerTransactionChild*>(shadowManager)->Destroy();
        shadowManager = nullptr;
      }
      NS_WARNING("failed to allocate layer transaction");
    } else {
      success = mPuppetWidget->CreateRemoteLayerManager(
          [&](LayerManager* aLayerManager) -> bool {
            ShadowLayerForwarder* lf = aLayerManager->AsShadowForwarder();
            lf->SetShadowManager(shadowManager);
            lf->IdentifyTextureHost(mTextureFactoryIdentifier);
            return true;
          });
    }
  }
  return success;
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

void BrowserChild::NotifyPainted() {
  if (!mNotified) {
    SendNotifyCompositorTransaction();
    mNotified = true;
  }
}

IPCResult BrowserChild::RecvUpdateEffects(const EffectsInfo& aEffects) {
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

  if (!sVisibleTabs) {
    sVisibleTabs = new nsTHashtable<nsPtrHashKey<BrowserChild>>();
  }
  sVisibleTabs->PutEntry(this);

  if (mPuppetWidget) {
    mPuppetWidget->Show(true);
  }

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    return;
  }

  // For top level stuff, the browser / tab-switcher is responsible of fixing
  // the docshell state up explicitly via SetDocShellIsActive.
  //
  // We need it not to be observable, as this used via RecvRenderLayers and co.,
  // for stuff like async tab warming.
  //
  // We don't want to go through the docshell because we don't want to change
  // the visibility state of the document, which has side effects like firing
  // events to content and unblocking media playback.
  //
  // FIXME(emilio): This feels a bit sketchy. Ideally we'd be able to just not
  // update visibility of stuff in the tab warming case (we just want to paint
  // once so that stuff is there already, really...), and use the docshell here
  // all the time, but that makes some of the devtools tests fail (??).
  if (mIsTopLevel) {
    if (RefPtr<PresShell> presShell = docShell->GetPresShell()) {
      presShell->SetIsActive(true);
    }
  } else {
    docShell->SetIsActive(true);
  }
}

void BrowserChild::MakeHidden() {
  if (sVisibleTabs) {
    sVisibleTabs->RemoveEntry(this);
    // We don't delete sVisibleTabs here when it's empty since that
    // could cause a lot of churn. Instead, we wait until ~BrowserChild.
  }

  if (!IsVisible()) {
    return;
  }

  // Due to the nested event loop in ContentChild::ProvideWindowCommon,
  // it's possible to be told to become hidden before we're finished
  // setting up a layer manager. We should skip clearing cached layers
  // in that case, since doing so might accidentally put is into
  // BasicLayers mode.
  if (mPuppetWidget && mPuppetWidget->HasLayerManager()) {
    ClearCachedResources();
  }

  if (nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation())) {
    // Hide all plugins in this tab. We don't use
    // BrowserChildBase::GetPresShell() here because that would create a content
    // viewer if one doesn't exist yet. Creating a content viewer can cause JS
    // to run, which we want to avoid. nsIDocShell::GetPresShell returns null if
    // no content viewer exists yet.
    if (RefPtr<PresShell> presShell = docShell->GetPresShell()) {
      if (nsPresContext* presContext = presShell->GetPresContext()) {
        nsRootPresContext* rootPresContext = presContext->GetRootPresContext();
        nsIFrame* rootFrame = presShell->GetRootFrame();
        rootPresContext->ComputePluginGeometryUpdates(rootFrame, nullptr,
                                                      nullptr);
        rootPresContext->ApplyPluginGeometryUpdates();
      }
    }
    docShell->SetIsActive(false);
  }

  if (mPuppetWidget) {
    mPuppetWidget->Show(false);
  }
}

NS_IMETHODIMP
BrowserChild::GetMessageManager(ContentFrameMessageManager** aResult) {
  RefPtr<ContentFrameMessageManager> mm(mBrowserChildMessageManager);
  mm.forget(aResult);
  return *aResult ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
BrowserChild::GetWebBrowserChrome(nsIWebBrowserChrome3** aWebBrowserChrome) {
  nsCOMPtr<nsPIDOMWindowOuter> outer = do_GetInterface(WebNavigation());
  if (nsCOMPtr<nsIWebBrowserChrome3> chrome =
          do_QueryActor(u"WebBrowserChrome", outer)) {
    chrome.forget(aWebBrowserChrome);
  } else {
    // TODO: remove fallback
    NS_IF_ADDREF(*aWebBrowserChrome = mWebBrowserChrome);
  }

  return NS_OK;
}

NS_IMETHODIMP
BrowserChild::SetWebBrowserChrome(nsIWebBrowserChrome3* aWebBrowserChrome) {
  mWebBrowserChrome = aWebBrowserChrome;
  return NS_OK;
}

void BrowserChild::SendRequestFocus(bool aCanFocus, CallerType aCallerType) {
  PBrowserChild::SendRequestFocus(aCanFocus, aCallerType);
}

NS_IMETHODIMP
BrowserChild::GetTabId(uint64_t* aId) {
  *aId = GetTabId();
  return NS_OK;
}

void BrowserChild::SetTabId(const TabId& aTabId) {
  MOZ_ASSERT(mUniqueId == 0);

  mUniqueId = aTabId;
  NestedBrowserChildMap()[mUniqueId] = this;
}

bool BrowserChild::DoSendBlockingMessage(
    JSContext* aCx, const nsAString& aMessage, StructuredCloneData& aData,
    JS::Handle<JSObject*> aCpows, nsIPrincipal* aPrincipal,
    nsTArray<StructuredCloneData>* aRetVal, bool aIsSync) {
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(Manager(), aData, data)) {
    return false;
  }
  nsTArray<CpowEntry> cpows;
  if (aCpows) {
    jsipc::CPOWManager* mgr = Manager()->GetCPOWManager();
    if (!mgr || !mgr->Wrap(aCx, aCpows, &cpows)) {
      return false;
    }
  }
  if (aIsSync) {
    return SendSyncMessage(PromiseFlatString(aMessage), data, cpows, aPrincipal,
                           aRetVal);
  }

  return SendRpcMessage(PromiseFlatString(aMessage), data, cpows, aPrincipal,
                        aRetVal);
}

nsresult BrowserChild::DoSendAsyncMessage(JSContext* aCx,
                                          const nsAString& aMessage,
                                          StructuredCloneData& aData,
                                          JS::Handle<JSObject*> aCpows,
                                          nsIPrincipal* aPrincipal) {
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(Manager(), aData, data)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }
  nsTArray<CpowEntry> cpows;
  if (aCpows) {
    jsipc::CPOWManager* mgr = Manager()->GetCPOWManager();
    if (!mgr || !mgr->Wrap(aCx, aCpows, &cpows)) {
      return NS_ERROR_UNEXPECTED;
    }
  }
  if (!SendAsyncMessage(PromiseFlatString(aMessage), cpows, aPrincipal, data)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

/* static */
nsTArray<RefPtr<BrowserChild>> BrowserChild::GetAll() {
  StaticMutexAutoLock lock(sBrowserChildrenMutex);

  nsTArray<RefPtr<BrowserChild>> list;
  if (!sBrowserChildren) {
    return list;
  }

  for (auto iter = sBrowserChildren->Iter(); !iter.Done(); iter.Next()) {
    list.AppendElement(iter.Data());
  }

  return list;
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
  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  MOZ_ASSERT(lm);

  lm->DidComposite(aTransactionId, aCompositeStart, aCompositeEnd);
}

void BrowserChild::DidRequestComposite(const TimeStamp& aCompositeReqStart,
                                       const TimeStamp& aCompositeReqEnd) {
  nsCOMPtr<nsIDocShell> docShellComPtr = do_GetInterface(WebNavigation());
  if (!docShellComPtr) {
    return;
  }

  nsDocShell* docShell = static_cast<nsDocShell*>(docShellComPtr.get());
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();

  if (timelines && timelines->HasConsumer(docShell)) {
    // Since we're assuming that it's impossible for content JS to directly
    // trigger a synchronous paint, we can avoid capturing a stack trace here,
    // which means we won't run into JS engine reentrancy issues like bug
    // 1310014.
    timelines->AddMarkerForDocShell(
        docShell, "CompositeForwardTransaction", aCompositeReqStart,
        MarkerTracingType::START, MarkerStackRequest::NO_STACK);
    timelines->AddMarkerForDocShell(docShell, "CompositeForwardTransaction",
                                    aCompositeReqEnd, MarkerTracingType::END,
                                    MarkerStackRequest::NO_STACK);
  }
}

void BrowserChild::ClearCachedResources() {
  MOZ_ASSERT(mPuppetWidget);
  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  MOZ_ASSERT(lm);

  lm->ClearCachedResources();
}

void BrowserChild::InvalidateLayers() {
  MOZ_ASSERT(mPuppetWidget);
  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  MOZ_ASSERT(lm);

  FrameLayerBuilder::InvalidateAllLayers(lm);
}

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
  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  MOZ_ASSERT(lm);
  lm->SetLayersObserverEpoch(mLayersObserverEpoch);

  nsCOMPtr<Document> doc(GetTopLevelDocument());
  doc->NotifyLayerManagerRecreated();
}

void BrowserChild::ReinitRenderingForDeviceReset() {
  InvalidateLayers();

  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  if (WebRenderLayerManager* wlm = lm->AsWebRenderLayerManager()) {
    wlm->DoDestroy(/* aIsSync */ true);
  } else if (ClientLayerManager* clm = lm->AsClientLayerManager()) {
    if (ShadowLayerForwarder* fwd = clm->AsShadowForwarder()) {
      // Force the LayerTransactionChild to synchronously shutdown. It is
      // okay to do this early, we'll simply stop sending messages. This
      // step is necessary since otherwise the compositor will think we
      // are trying to attach two layer trees to the same ID.
      fwd->SynchronouslyShutdown();
    }
  } else {
    if (mLayersConnected.isNothing()) {
      return;
    }
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

mozilla::ipc::IPCResult BrowserChild::RecvRequestNotifyAfterRemotePaint() {
  // Get the CompositorBridgeChild instance for this content thread.
  CompositorBridgeChild* compositor = CompositorBridgeChild::Get();

  // Tell the CompositorBridgeChild that, when it gets a RemotePaintIsReady
  // message that it should forward it us so that we can bounce it to our
  // BrowserParent.
  compositor->RequestNotifyAfterRemotePaint(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvUIResolutionChanged(
    const float& aDpi, const int32_t& aRounding, const double& aScale) {
  nsCOMPtr<Document> document(GetTopLevelDocument());
  if (!document) {
    return IPC_OK();
  }

  ScreenIntSize oldScreenSize = GetInnerSize();
  if (aDpi > 0) {
    mPuppetWidget->UpdateBackingScaleCache(aDpi, aRounding, aScale);
  }
  RefPtr<nsPresContext> presContext = document->GetPresContext();
  if (presContext) {
    presContext->UIResolutionChangedSync();
  }

  ScreenIntSize screenSize = GetInnerSize();
  if (mHasValidInnerSize && oldScreenSize != screenSize) {
    ScreenIntRect screenRect = GetOuterRect();
    mPuppetWidget->Resize(screenRect.x + mClientOffset.x + mChromeOffset.x,
                          screenRect.y + mClientOffset.y + mChromeOffset.y,
                          screenSize.width, screenSize.height, true);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
    baseWin->SetPositionAndSize(0, 0, screenSize.width, screenSize.height,
                                nsIBaseWindow::eRepaint);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvThemeChanged(
    nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache) {
  nsCOMPtr<Document> document(GetTopLevelDocument());
  if (!document) {
    return IPC_OK();
  }

  LookAndFeel::SetIntCache(aLookAndFeelIntCache);
  RefPtr<nsPresContext> presContext = document->GetPresContext();
  if (presContext) {
    presContext->ThemeChanged();
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

mozilla::ipc::IPCResult BrowserChild::RecvAwaitLargeAlloc() {
  mAwaitingLA = true;
  return IPC_OK();
}

bool BrowserChild::IsAwaitingLargeAlloc() { return mAwaitingLA; }

bool BrowserChild::StopAwaitingLargeAlloc() {
  bool awaiting = mAwaitingLA;
  mAwaitingLA = false;
  return awaiting;
}

mozilla::ipc::IPCResult BrowserChild::RecvAllowScriptsToClose() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  if (window) {
    nsGlobalWindowOuter::Cast(window)->AllowScriptsToClose();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvSetOriginAttributes(
    const OriginAttributes& aOriginAttributes) {
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  nsDocShell::Cast(docShell)->SetOriginAttributes(aOriginAttributes);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserChild::RecvSetWidgetNativeData(
    const WindowsHandle& aWidgetNativeData) {
  mWidgetNativeData = aWidgetNativeData;
  return IPC_OK();
}

mozilla::plugins::PPluginWidgetChild* BrowserChild::AllocPPluginWidgetChild() {
#ifdef XP_WIN
  return new mozilla::plugins::PluginWidgetChild();
#else
  MOZ_ASSERT_UNREACHABLE("AllocPPluginWidgetChild only supports Windows");
  return nullptr;
#endif
}

bool BrowserChild::DeallocPPluginWidgetChild(
    mozilla::plugins::PPluginWidgetChild* aActor) {
  delete aActor;
  return true;
}

#ifdef XP_WIN
nsresult BrowserChild::CreatePluginWidget(nsIWidget* aParent,
                                          nsIWidget** aOut) {
  *aOut = nullptr;
  mozilla::plugins::PluginWidgetChild* child =
      static_cast<mozilla::plugins::PluginWidgetChild*>(
          SendPPluginWidgetConstructor());
  if (!child) {
    NS_ERROR("couldn't create PluginWidgetChild");
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<nsIWidget> pluginWidget =
      nsIWidget::CreatePluginProxyWidget(this, child);
  if (!pluginWidget) {
    NS_ERROR("couldn't create PluginWidgetProxy");
    return NS_ERROR_UNEXPECTED;
  }

  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_plugin_ipc_content;
  initData.mUnicode = false;
  initData.clipChildren = true;
  initData.clipSiblings = true;
  nsresult rv = pluginWidget->Create(
      aParent, nullptr, LayoutDeviceIntRect(0, 0, 0, 0), &initData);
  if (NS_FAILED(rv)) {
    NS_WARNING("Creating native plugin widget on the chrome side failed.");
  }
  pluginWidget.forget(aOut);
  return rv;
}
#endif  // XP_WIN

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

Maybe<LayoutDeviceIntRect> BrowserChild::GetVisibleRect() const {
  if (mIsTopLevel) {
    // We are conservative about visible rects for top-level browsers to avoid
    // artifacts when resizing
    return Nothing();
  }
  CSSRect visibleRectCSS = CSSPixel::FromAppUnits(mEffectsInfo.mVisibleRect);
  LayoutDeviceIntRect visibleRectLD =
      RoundedToInt(visibleRectCSS * mPuppetWidget->GetDefaultScale());
  return Some(visibleRectLD);
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
  if (!IPCOpen() || !mPuppetWidget || !mPuppetWidget->HasLayerManager()) {
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

  nsCOMPtr<nsISHistory> history = do_GetInterface(WebNavigation());
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

NS_IMETHODIMP BrowserChild::BeginSendingWebProgressEventsToParent() {
  mShouldSendWebProgressEventsToParent = true;
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

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (!docShell) {
    return NS_OK;
  }

  // We shouldn't need to notify the parent of redirect state changes, since
  // with DocumentChannel that only happens when we switch to the real channel,
  // and that's an implementation detail that we can hide.
  if (aStateFlags & nsIWebProgressListener::STATE_IS_REDIRECTED_DOCUMENT) {
    return NS_OK;
  }

  RefPtr<Document> document;
  if (nsCOMPtr<nsPIDOMWindowOuter> outerWindow = do_GetInterface(docShell)) {
    document = outerWindow->GetExtantDoc();
  } else {
    return NS_OK;
  }

  Maybe<WebProgressData> webProgressData;
  Maybe<WebProgressStateChangeData> stateChangeData;
  RequestData requestData;

  MOZ_TRY(PrepareProgressListenerData(aWebProgress, aRequest, webProgressData,
                                      requestData));

  /*
   * If
   * 1) this is a document,
   * 2) the document is top-level,
   * 3) the document is completely loaded (STATE_STOP), and
   * 4) this is the end of activity for the document
   *    (STATE_IS_WINDOW, STATE_IS_NETWORK),
   * then record the elapsed time that it took to load.
   */
  if (document && webProgressData->isTopLevel() &&
      (aStateFlags & nsIWebProgressListener::STATE_STOP) &&
      (aStateFlags & nsIWebProgressListener::STATE_IS_WINDOW) &&
      (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)) {
    RefPtr<nsDOMNavigationTiming> navigationTiming =
        document->GetNavigationTiming();
    if (navigationTiming) {
      TimeDuration elapsedLoadTimeMS =
          TimeStamp::Now() - navigationTiming->GetNavigationStartTimeStamp();
      requestData.elapsedLoadTimeMS() =
          Some(elapsedLoadTimeMS.ToMilliseconds());
    }
  }

  if (webProgressData->isTopLevel()) {
    stateChangeData.emplace();

    stateChangeData->isNavigating() = docShell->GetIsNavigating();
    stateChangeData->mayEnableCharacterEncodingMenu() =
        docShell->GetMayEnableCharacterEncodingMenu();
    stateChangeData->charsetAutodetected() = docShell->GetCharsetAutodetected();

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

  Maybe<WebProgressData> webProgressData;
  RequestData requestData;

  nsresult rv = PrepareProgressListenerData(aWebProgress, aRequest,
                                            webProgressData, requestData);
  NS_ENSURE_SUCCESS(rv, rv);

  Unused << SendOnProgressChange(webProgressData, requestData, aCurSelfProgress,
                                 aMaxSelfProgress, aCurTotalProgress,
                                 aMaxTotalProgress);

  return NS_OK;
}

NS_IMETHODIMP BrowserChild::OnLocationChange(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             nsIURI* aLocation,
                                             uint32_t aFlags) {
  if (!IPCOpen() || !mShouldSendWebProgressEventsToParent) {
    return NS_OK;
  }

  nsCOMPtr<nsIWebNavigation> webNav = WebNavigation();
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(webNav);
  if (!docShell) {
    return NS_OK;
  }

  RefPtr<Document> document;
  if (nsCOMPtr<nsPIDOMWindowOuter> outerWindow = do_GetInterface(docShell)) {
    document = outerWindow->GetExtantDoc();
  } else {
    return NS_OK;
  }

  if (!document) {
    return NS_OK;
  }

  Maybe<WebProgressData> webProgressData;
  RequestData requestData;

  MOZ_TRY(PrepareProgressListenerData(aWebProgress, aRequest, webProgressData,
                                      requestData));

  Maybe<WebProgressLocationChangeData> locationChangeData;

  bool canGoBack = false;
  bool canGoForward = false;

  MOZ_TRY(webNav->GetCanGoBack(&canGoBack));
  MOZ_TRY(webNav->GetCanGoForward(&canGoForward));

  if (aWebProgress && webProgressData->isTopLevel()) {
    locationChangeData.emplace();

    document->GetContentType(locationChangeData->contentType());
    locationChangeData->isNavigating() = docShell->GetIsNavigating();
    locationChangeData->documentURI() = document->GetDocumentURIObject();
    document->GetTitle(locationChangeData->title());
    document->GetCharacterSet(locationChangeData->charset());

    locationChangeData->mayEnableCharacterEncodingMenu() =
        docShell->GetMayEnableCharacterEncodingMenu();
    locationChangeData->charsetAutodetected() =
        docShell->GetCharsetAutodetected();

    locationChangeData->contentPrincipal() = document->NodePrincipal();
    locationChangeData->contentStoragePrincipal() =
        document->EffectiveStoragePrincipal();
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

      nsresult rv = NS_MutateURI(aLocation)
                        .SetUserPass(EmptyCString())
                        .Finalize(annotationURI);

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

  Maybe<WebProgressData> webProgressData;
  RequestData requestData;

  nsresult rv = PrepareProgressListenerData(aWebProgress, aRequest,
                                            webProgressData, requestData);

  NS_ENSURE_SUCCESS(rv, rv);

  const nsString message(aMessage);

  Unused << SendOnStatusChange(webProgressData, requestData, aStatus, message);

  return NS_OK;
}

NS_IMETHODIMP BrowserChild::OnSecurityChange(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             uint32_t aState) {
  if (!IPCOpen() || !mShouldSendWebProgressEventsToParent) {
    return NS_OK;
  }

  Maybe<WebProgressData> webProgressData;
  RequestData requestData;

  MOZ_TRY(PrepareProgressListenerData(aWebProgress, aRequest, webProgressData,
                                      requestData));

  Maybe<WebProgressSecurityChangeData> securityChangeData;

  if (aWebProgress && webProgressData->isTopLevel()) {
    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
    if (!docShell) {
      return NS_OK;
    }

    nsCOMPtr<nsITransportSecurityInfo> securityInfo;
    {
      nsCOMPtr<nsISecureBrowserUI> securityUI;
      MOZ_TRY(docShell->GetSecurityUI(getter_AddRefs(securityUI)));

      if (securityUI) {
        MOZ_TRY(securityUI->GetSecInfo(getter_AddRefs(securityInfo)));
      }
    }

    bool isSecureContext = false;
    {
      nsCOMPtr<nsPIDOMWindowOuter> outerWindow = do_GetInterface(docShell);
      if (!outerWindow) {
        return NS_OK;
      }

      if (nsPIDOMWindowInner* window = outerWindow->GetCurrentInnerWindow()) {
        isSecureContext = window->IsSecureContext();
      } else {
        return NS_OK;
      }
    }

    securityChangeData.emplace();
    securityChangeData->securityInfo() = ToRefPtr(std::move(securityInfo));
    securityChangeData->isSecureContext() = isSecureContext;
  }

  Unused << SendOnSecurityChange(webProgressData, requestData, aState,
                                 securityChangeData);

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
                                               int32_t aMillis, bool aSameURI,
                                               bool* aOut) {
  NS_ENSURE_ARG_POINTER(aOut);
  *aOut = true;

  return NS_OK;
}

NS_IMETHODIMP BrowserChild::NotifyNavigationFinished() {
  Unused << SendNavigationFinished();
  return NS_OK;
}

nsresult BrowserChild::PrepareProgressListenerData(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest,
    Maybe<WebProgressData>& aWebProgressData, RequestData& aRequestData) {
  if (aWebProgress) {
    aWebProgressData.emplace();

    bool isTopLevel = false;
    nsresult rv = aWebProgress->GetIsTopLevel(&isTopLevel);
    NS_ENSURE_SUCCESS(rv, rv);
    aWebProgressData->isTopLevel() = isTopLevel;

    bool isLoadingDocument = false;
    rv = aWebProgress->GetIsLoadingDocument(&isLoadingDocument);
    NS_ENSURE_SUCCESS(rv, rv);
    aWebProgressData->isLoadingDocument() = isLoadingDocument;

    uint32_t loadType = 0;
    rv = aWebProgress->GetLoadType(&loadType);
    NS_ENSURE_SUCCESS(rv, rv);
    aWebProgressData->loadType() = loadType;

    uint64_t outerDOMWindowID = 0;
    uint64_t innerDOMWindowID = 0;
    // The DOM Window ID getters here may throw if the inner or outer windows
    // aren't created yet or are destroyed at the time we're making this call
    // but that isn't fatal so ignore the exceptions here.
    Unused << aWebProgress->GetDOMWindowID(&outerDOMWindowID);
    aWebProgressData->outerDOMWindowID() = outerDOMWindowID;

    Unused << aWebProgress->GetInnerDOMWindowID(&innerDOMWindowID);
    aWebProgressData->innerDOMWindowID() = innerDOMWindowID;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (channel) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = channel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
    aRequestData.requestURI() = uri;

    rv = channel->GetOriginalURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
    aRequestData.originalRequestURI() = uri;

    nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
        do_QueryInterface(channel);
    if (classifiedChannel) {
      nsAutoCString matchedList;
      rv = classifiedChannel->GetMatchedList(matchedList);
      NS_ENSURE_SUCCESS(rv, rv);
      aRequestData.matchedList() = std::move(matchedList);
    }
  }
  return NS_OK;
}

bool BrowserChild::UpdateSessionStore(uint32_t aFlushId, bool aIsFinal) {
  if (!mSessionStoreListener) {
    return false;
  }
  RefPtr<ContentSessionStore> store = mSessionStoreListener->GetSessionStore();

  Maybe<nsCString> docShellCaps;
  if (store->IsDocCapChanged()) {
    docShellCaps.emplace(store->GetDocShellCaps());
  }

  Maybe<bool> privatedMode;
  if (store->IsPrivateChanged()) {
    privatedMode.emplace(store->GetPrivateModeEnabled());
  }

  nsTArray<int32_t> positionDescendants;
  nsTArray<nsCString> positions;
  if (store->IsScrollPositionChanged()) {
    store->GetScrollPositions(positions, positionDescendants);
  }

  nsTArray<InputFormData> inputs;
  nsTArray<CollectedInputDataValue> idVals, xPathVals;
  if (store->IsFormDataChanged()) {
    inputs = store->GetInputs(idVals, xPathVals);
  }

  nsTArray<nsCString> origins;
  nsTArray<nsString> keys, values;
  bool isFullStorage = false;
  if (store->IsStorageUpdated()) {
    isFullStorage = store->GetAndClearStorageChanges(origins, keys, values);
  }

  Unused << SendSessionStoreUpdate(
      docShellCaps, privatedMode, positions, positionDescendants, inputs,
      idVals, xPathVals, origins, keys, values, isFullStorage,
      store->GetAndClearSHistoryChanged(), aFlushId, aIsFinal,
      mSessionStoreListener->GetEpoch());
  return true;
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
          GetCurrentThreadSerialEventTarget(), __func__,
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
    const Maybe<mozilla::ContentBlockingNotifier::StorageAccessGrantedReason>&
        aReason) {
  if (!IPCOpen()) {
    return;
  }

  Maybe<WebProgressData> webProgressData;
  RequestData requestData;
  nsresult rv = PrepareProgressListenerData(nullptr, aChannel, webProgressData,
                                            requestData);
  NS_ENSURE_SUCCESS_VOID(rv);

  Unused << SendNotifyContentBlockingEvent(aEvent, requestData, aBlocked,
                                           PromiseFlatCString(aTrackingOrigin),
                                           aTrackingFullHashes, aReason);
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

uint64_t BrowserChildMessageManager::ChromeOuterWindowID() {
  if (!mBrowserChild) {
    return 0;
  }
  return mBrowserChild->ChromeOuterWindowID();
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
