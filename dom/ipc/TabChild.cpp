/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "TabChild.h"

#include "gfxPrefs.h"
#ifdef ACCESSIBILITY
#include "mozilla/a11y/DocAccessibleChild.h"
#endif
#include "Layers.h"
#include "ContentChild.h"
#include "TabParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/BrowserElementParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/indexedDB/PIndexedDBPermissionRequestChild.h"
#include "mozilla/dom/PaymentRequestChild.h"
#include "mozilla/dom/TelemetryScrollProbe.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/ipc/DocumentRendererChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/layers/APZChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZCTreeManagerChild.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/ContentProcessController.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/DoubleTapToZoom.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layout/RenderFrameChild.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/plugins/PPluginWidgetChild.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Move.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsCSSFrameConstructor.h"
#include "nsDocShell.h"
#include "nsEmbedCID.h"
#include "nsGlobalWindow.h"
#include <algorithm>
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif
#include "nsFilePickerProxy.h"
#include "mozilla/dom/Element.h"
#include "nsGlobalWindow.h"
#include "nsIBaseWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDocumentInlines.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsFocusManager.h"
#include "EventStateManager.h"
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
#include "nsViewManager.h"
#include "nsWeakReference.h"
#include "nsWindowWatcher.h"
#include "PermissionMessageUtils.h"
#include "PuppetWidget.h"
#include "StructuredCloneData.h"
#include "nsViewportInfo.h"
#include "nsILoadContext.h"
#include "ipc/nsGUIEventIPC.h"
#include "mozilla/gfx/Matrix.h"
#include "UnitTransforms.h"
#include "ClientLayerManager.h"
#include "LayersLogging.h"
#include "nsDOMClassInfoID.h"
#include "nsColorPickerProxy.h"
#include "nsContentPermissionHelper.h"
#include "nsNetUtil.h"
#include "nsIPermissionManager.h"
#include "nsIURILoader.h"
#include "nsIScriptError.h"
#include "mozilla/EventForwards.h"
#include "nsDeviceContext.h"
#include "nsSandboxFlags.h"
#include "FrameLayerBuilder.h"
#include "VRManagerChild.h"
#include "nsICommandParams.h"
#include "nsISHistory.h"
#include "nsQueryObject.h"
#include "GroupedSHistory.h"
#include "nsIHttpChannel.h"
#include "mozilla/dom/DocGroup.h"
#include "nsString.h"
#include "nsISupportsPrimitives.h"
#include "mozilla/Telemetry.h"

#ifdef XP_WIN
#include "mozilla/plugins/PluginWidgetChild.h"
#endif

#ifdef NS_PRINTING
#include "nsIPrintSession.h"
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIWebBrowserPrint.h"
#endif

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
using mozilla::layers::GeckoContentController;

NS_IMPL_ISUPPORTS(ContentListener, nsIDOMEventListener)
NS_IMPL_ISUPPORTS(TabChildSHistoryListener,
                  nsISHistoryListener,
                  nsIPartialSHistoryListener,
                  nsISupportsWeakReference)

static const char BEFORE_FIRST_PAINT[] = "before-first-paint";

typedef nsDataHashtable<nsUint64HashKey, TabChild*> TabChildMap;
static TabChildMap* sTabChildren;
StaticMutex sTabChildrenMutex;

TabChildBase::TabChildBase()
  : mTabChildGlobal(nullptr)
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
  tmp->nsMessageManagerScriptExecutor::Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWebBrowserChrome)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TabChildBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTabChildGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWebBrowserChrome)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(TabChildBase)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TabChildBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TabChildBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TabChildBase)

already_AddRefed<nsIDocument>
TabChildBase::GetDocument() const
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  WebNavigation()->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  return doc.forget();
}

already_AddRefed<nsIPresShell>
TabChildBase::GetPresShell() const
{
  nsCOMPtr<nsIPresShell> result;
  if (nsCOMPtr<nsIDocument> doc = GetDocument()) {
    result = doc->GetShell();
  }
  return result.forget();
}

void
TabChildBase::DispatchMessageManagerMessage(const nsAString& aMessageName,
                                            const nsAString& aJSONData)
{
    AutoSafeJSContext cx;
    JS::Rooted<JS::Value> json(cx, JS::NullValue());
    dom::ipc::StructuredCloneData data;
    if (JS_ParseJSON(cx,
                      static_cast<const char16_t*>(aJSONData.BeginReading()),
                      aJSONData.Length(),
                      &json)) {
        ErrorResult rv;
        data.Write(cx, json, rv);
        if (NS_WARN_IF(rv.Failed())) {
            rv.SuppressException();
            return;
        }
    }

    JS::Rooted<JSObject*> kungFuDeathGrip(cx, GetGlobal());
    // Let the BrowserElementScrolling helper (if it exists) for this
    // content manipulate the frame state.
    RefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildGlobal), nullptr,
                       aMessageName, false, &data, nullptr, nullptr, nullptr);
}

bool
TabChildBase::UpdateFrameHandler(const FrameMetrics& aFrameMetrics)
{
  MOZ_ASSERT(aFrameMetrics.GetScrollId() != FrameMetrics::NULL_SCROLL_ID);

  if (aFrameMetrics.IsRootContent()) {
    if (nsCOMPtr<nsIPresShell> shell = GetPresShell()) {
      // Guard against stale updates (updates meant for a pres shell which
      // has since been torn down and destroyed).
      if (aFrameMetrics.GetPresShellId() == shell->GetPresShellId()) {
        ProcessUpdateFrame(aFrameMetrics);
        return true;
      }
    }
  } else {
    // aFrameMetrics.mIsRoot is false, so we are trying to update a subframe.
    // This requires special handling.
    FrameMetrics newSubFrameMetrics(aFrameMetrics);
    APZCCallbackHelper::UpdateSubFrame(newSubFrameMetrics);
    return true;
  }
  return true;
}

void
TabChildBase::ProcessUpdateFrame(const FrameMetrics& aFrameMetrics)
{
    if (!mGlobal || !mTabChildGlobal) {
        return;
    }

    FrameMetrics newMetrics = aFrameMetrics;
    APZCCallbackHelper::UpdateRootFrame(newMetrics);
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

class TabChild::DelayedDeleteRunnable final
  : public Runnable
{
    RefPtr<TabChild> mTabChild;

public:
    explicit DelayedDeleteRunnable(TabChild* aTabChild)
      : Runnable("TabChild::DelayedDeleteRunnable")
      , mTabChild(aTabChild)
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
    Run() override
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mTabChild);

        // Check in case ActorDestroy was called after RecvDestroy message.
        if (mTabChild->IPCOpen()) {
            Unused << PBrowserChild::Send__delete__(mTabChild);
        }

        mTabChild = nullptr;
        return NS_OK;
    }
};

namespace {
std::map<TabId, RefPtr<TabChild>>&
NestedTabChildMap()
{
  MOZ_ASSERT(NS_IsMainThread());
  static std::map<TabId, RefPtr<TabChild>> sNestedTabChildMap;
  return sNestedTabChildMap;
}
} // namespace

already_AddRefed<TabChild>
TabChild::FindTabChild(const TabId& aTabId)
{
  auto iter = NestedTabChildMap().find(aTabId);
  if (iter == NestedTabChildMap().end()) {
    return nullptr;
  }
  RefPtr<TabChild> tabChild = iter->second;
  return tabChild.forget();
}

/*static*/ already_AddRefed<TabChild>
TabChild::Create(nsIContentChild* aManager,
                 const TabId& aTabId,
                 const TabId& aSameTabGroupAs,
                 const TabContext &aContext,
                 uint32_t aChromeFlags)
{
  RefPtr<TabChild> groupChild = FindTabChild(aSameTabGroupAs);
  dom::TabGroup* group = groupChild ? groupChild->TabGroup() : nullptr;
  RefPtr<TabChild> iframe = new TabChild(aManager, aTabId, group,
                                         aContext, aChromeFlags);
  return iframe.forget();
}

TabChild::TabChild(nsIContentChild* aManager,
                   const TabId& aTabId,
                   dom::TabGroup* aTabGroup,
                   const TabContext& aContext,
                   uint32_t aChromeFlags)
  : TabContext(aContext)
  , mTabGroup(aTabGroup)
  , mRemoteFrame(nullptr)
  , mManager(aManager)
  , mChromeFlags(aChromeFlags)
  , mMaxTouchPoints(0)
  , mActiveSuppressDisplayport(0)
  , mLayersId(0)
  , mBeforeUnloadListeners(0)
  , mLayersConnected(true)
  , mDidFakeShow(false)
  , mNotified(false)
  , mTriedBrowserInit(false)
  , mOrientation(eScreenOrientation_PortraitPrimary)
  , mIgnoreKeyPressEvent(false)
  , mHasValidInnerSize(false)
  , mDestroyed(false)
  , mUniqueId(aTabId)
  , mDPI(0)
  , mRounding(0)
  , mDefaultScale(0)
  , mIsTransparent(false)
  , mIPCOpen(false)
  , mParentIsActive(false)
  , mDidSetRealShowInfo(false)
  , mDidLoadURLInit(false)
  , mAwaitingLA(false)
  , mSkipKeyPress(false)
  , mLayerObserverEpoch(0)
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  , mNativeWindowHandle(0)
#endif
#if defined(ACCESSIBILITY)
  , mTopLevelDocAccessibleChild(nullptr)
#endif
  , mPendingDocShellIsActive(false)
  , mPendingDocShellPreserveLayers(false)
  , mPendingDocShellReceivedMessage(false)
  , mPendingDocShellBlockers(0)
{
  nsWeakPtr weakPtrThis(do_GetWeakReference(static_cast<nsITabChild*>(this)));  // for capture by the lambda
  mSetAllowedTouchBehaviorCallback = [weakPtrThis](uint64_t aInputBlockId,
                                                   const nsTArray<TouchBehaviorFlags>& aFlags)
  {
    if (nsCOMPtr<nsITabChild> tabChild = do_QueryReferent(weakPtrThis)) {
      static_cast<TabChild*>(tabChild.get())->SetAllowedTouchBehavior(aInputBlockId, aFlags);
    }
  };

  // preloaded TabChild should not be added to child map
  if (mUniqueId) {
    MOZ_ASSERT(NestedTabChildMap().find(mUniqueId) == NestedTabChildMap().end());
    NestedTabChildMap()[mUniqueId] = this;
  }
}

bool
TabChild::AsyncPanZoomEnabled() const
{
  // By the time anybody calls this, we must have had InitRenderingState called
  // already, and so mCompositorOptions should be populated.
  MOZ_RELEASE_ASSERT(mCompositorOptions);
  return mCompositorOptions->UseAPZ();
}

NS_IMETHODIMP
TabChild::Observe(nsISupports *aSubject,
                  const char *aTopic,
                  const char16_t *aData)
{
  if (!strcmp(aTopic, BEFORE_FIRST_PAINT)) {
    if (AsyncPanZoomEnabled()) {
      nsCOMPtr<nsIDocument> subject(do_QueryInterface(aSubject));
      nsCOMPtr<nsIDocument> doc(GetDocument());

      if (SameCOMIdentity(subject, doc)) {
        nsCOMPtr<nsIPresShell> shell(doc->GetShell());
        if (shell) {
          shell->SetIsFirstPaint(true);
        }

        APZCCallbackHelper::InitializeRootDisplayport(shell);
      }
    }
  }

  return NS_OK;
}

void
TabChild::ContentReceivedInputBlock(const ScrollableLayerGuid& aGuid,
                                    uint64_t aInputBlockId,
                                    bool aPreventDefault) const
{
  if (mApzcTreeManager) {
    mApzcTreeManager->ContentReceivedInputBlock(aInputBlockId, aPreventDefault);
  }
}

void
TabChild::SetTargetAPZC(uint64_t aInputBlockId,
                        const nsTArray<ScrollableLayerGuid>& aTargets) const
{
  if (mApzcTreeManager) {
    mApzcTreeManager->SetTargetAPZC(aInputBlockId, aTargets);
  }
}

void
TabChild::SetAllowedTouchBehavior(uint64_t aInputBlockId,
                                  const nsTArray<TouchBehaviorFlags>& aTargets) const
{
  if (mApzcTreeManager) {
    mApzcTreeManager->SetAllowedTouchBehavior(aInputBlockId, aTargets);
  }
}

bool
TabChild::DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                                  const ViewID& aViewId,
                                  const Maybe<ZoomConstraints>& aConstraints)
{
  if (!mApzcTreeManager) {
    return false;
  }

  ScrollableLayerGuid guid = ScrollableLayerGuid(mLayersId, aPresShellId, aViewId);

  mApzcTreeManager->UpdateZoomConstraints(guid, aConstraints);
  return true;
}

nsresult
TabChild::Init()
{
  if (!mTabGroup) {
    mTabGroup = TabGroup::GetFromActor(this);
  }

  nsCOMPtr<nsIWebBrowser> webBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  if (!webBrowser) {
    NS_ERROR("Couldn't create a nsWebBrowser?");
    return NS_ERROR_FAILURE;
  }

  webBrowser->SetContainerWindow(this);
  webBrowser->SetOriginAttributes(OriginAttributesRef());
  mWebNav = do_QueryInterface(webBrowser);
  NS_ASSERTION(mWebNav, "nsWebBrowser doesn't implement nsIWebNavigation?");

  nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(WebNavigation()));
  docShellItem->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
  if (!baseWindow) {
    NS_ERROR("mWebNav doesn't QI to nsIBaseWindow");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(this);
  mPuppetWidget = static_cast<PuppetWidget*>(widget.get());
  if (!mPuppetWidget) {
    NS_ERROR("couldn't create fake widget");
    return NS_ERROR_FAILURE;
  }
  mPuppetWidget->InfallibleCreate(
    nullptr, 0,              // no parents
    LayoutDeviceIntRect(0, 0, 0, 0),
    nullptr                  // HandleWidgetEvent
  );

  baseWindow->InitWindow(0, mPuppetWidget, 0, 0, 0, 0);
  baseWindow->Create();

  // Set the tab context attributes then pass to docShell
  NotifyTabContextUpdated(false);

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
  loadContext->SetPrivateBrowsing(OriginAttributesRef().mPrivateBrowsingId > 0);
  loadContext->SetRemoteTabs(
      mChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW);

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
  nsCOMPtr<EventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  docShell->SetChromeEventHandler(chromeHandler);

  if (window->GetCurrentInnerWindow()) {
    window->SetKeyboardIndicators(ShowAccelerators(), ShowFocusRings());
  } else {
    // Skip ShouldShowFocusRing check if no inner window is available
    window->SetInitialKeyboardIndicators(ShowAccelerators(), ShowFocusRings());
  }

  // Set prerender flag if necessary.
  if (mIsPrerendered) {
    docShell->SetIsPrerendered();
  }

  nsContentUtils::SetScrollbarsVisibility(window->GetDocShell(),
    !!(mChromeFlags & nsIWebBrowserChrome::CHROME_SCROLLBARS));

  nsWeakPtr weakPtrThis = do_GetWeakReference(static_cast<nsITabChild*>(this));  // for capture by the lambda
  ContentReceivedInputBlockCallback callback(
      [weakPtrThis](const ScrollableLayerGuid& aGuid,
                    uint64_t aInputBlockId,
                    bool aPreventDefault)
      {
        if (nsCOMPtr<nsITabChild> tabChild = do_QueryReferent(weakPtrThis)) {
          static_cast<TabChild*>(tabChild.get())->ContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
        }
      });
  mAPZEventState = new APZEventState(mPuppetWidget, Move(callback));

  mIPCOpen = true;

  if (GroupedSHistory::GroupedHistoryEnabled()) {
    // Set session history listener.
    nsCOMPtr<nsISHistory> shistory = GetRelatedSHistory();
    if (!shistory) {
      return NS_ERROR_FAILURE;
    }
    mHistoryListener = new TabChildSHistoryListener(this);
    nsCOMPtr<nsISHistoryListener> listener(do_QueryObject(mHistoryListener));
    shistory->AddSHistoryListener(listener);
    nsCOMPtr<nsIPartialSHistoryListener> partialListener(do_QueryObject(mHistoryListener));
    shistory->SetPartialSHistoryListener(partialListener);
  }

  return NS_OK;
}

void
TabChild::NotifyTabContextUpdated(bool aIsPreallocated)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  if (!docShell) {
    return;
  }

  UpdateFrameType();

  if (aIsPreallocated)  {
    nsDocShell::Cast(docShell)->SetOriginAttributes(OriginAttributesRef());
  }

  // Set SANDBOXED_AUXILIARY_NAVIGATION flag if this is a receiver page.
  if (!PresentationURL().IsEmpty()) {
    docShell->SetSandboxFlags(SANDBOXED_AUXILIARY_NAVIGATION);
  }
}

void
TabChild::UpdateFrameType()
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  // TODO: Bug 1252794 - remove frameType from nsIDocShell.idl
  docShell->SetFrameType(IsMozBrowserElement() ? nsIDocShell::FRAME_TYPE_BROWSER :
                           nsIDocShell::FRAME_TYPE_REGULAR);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TabChild)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
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
  NS_WARNING("TabChild::GetWebBrowser not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
  NS_WARNING("TabChild::SetWebBrowser not supported in TabChild");

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
  NS_WARNING("trying to SetChromeFlags from content process?");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::DestroyBrowserWindow()
{
  NS_WARNING("TabChild::DestroyBrowserWindow not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::RemoteSizeShellTo(int32_t aWidth, int32_t aHeight,
                            int32_t aShellItemWidth, int32_t aShellItemHeight)
{
  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(ourDocShell));
  int32_t width, height;
  docShellAsWin->GetSize(&width, &height);

  uint32_t flags = 0;
  if (width == aWidth) {
    flags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX;
  }

  if (height == aHeight) {
    flags |= nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY;
  }

  bool sent = SendSizeShellTo(flags, aWidth, aHeight, aShellItemWidth, aShellItemHeight);

  return sent ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TabChild::RemoteDropLinks(uint32_t aLinksCount,
                          nsIDroppedLinkItem** aLinks)
{
  nsTArray<nsString> linksArray;
  nsresult rv = NS_OK;
  for (uint32_t i = 0; i < aLinksCount; i++) {
    nsString tmp;
    rv = aLinks[i]->GetUrl(tmp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    linksArray.AppendElement(tmp);

    rv = aLinks[i]->GetName(tmp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    linksArray.AppendElement(tmp);

    rv = aLinks[i]->GetType(tmp);
    if (NS_FAILED(rv)) {
      return rv;
    }
    linksArray.AppendElement(tmp);
  }
  bool sent = SendDropLinks(linksArray);

  return sent ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TabChild::SizeBrowserTo(int32_t aWidth, int32_t aHeight)
{
  NS_WARNING("TabChild::SizeBrowserTo not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::ShowAsModal()
{
  NS_WARNING("TabChild::ShowAsModal not supported in TabChild");

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
  NS_WARNING("TabChild::ExitModalEventLoop not supported in TabChild");

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

  Unused << SendSetDimensions(aFlags, aX, aY, aCx, aCy);

  return NS_OK;
}

NS_IMETHODIMP
TabChild::GetDimensions(uint32_t aFlags, int32_t* aX,
                             int32_t* aY, int32_t* aCx, int32_t* aCy)
{
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
TabChild::SetFocus()
{
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
  NS_WARNING("TabChild::GetTitle not supported in TabChild");

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
  NS_WARNING("TabChild::GetSiteWindow not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::Blur()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::FocusNextElement(bool aForDocumentNavigation)
{
  SendMoveFocus(true, aForDocumentNavigation);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::FocusPrevElement(bool aForDocumentNavigation)
{
  SendMoveFocus(false, aForDocumentNavigation);
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
TabChild::ProvideWindow(mozIDOMWindowProxy* aParent,
                        uint32_t aChromeFlags,
                        bool aCalledFromJS,
                        bool aPositionSpecified, bool aSizeSpecified,
                        nsIURI* aURI, const nsAString& aName,
                        const nsACString& aFeatures, bool aForceNoOpener,
                        bool* aWindowIsNew, mozIDOMWindowProxy** aReturn)
{
    *aReturn = nullptr;

    // If aParent is inside an <iframe mozbrowser> and this isn't a request to
    // open a modal-type window, we're going to create a new <iframe mozbrowser>
    // and return its window here.
    nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
    bool iframeMoz = (docshell && docshell->GetIsInMozBrowser() &&
                      !(aChromeFlags & (nsIWebBrowserChrome::CHROME_MODAL |
                                        nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                                        nsIWebBrowserChrome::CHROME_OPENAS_CHROME)));

    if (!iframeMoz) {
      int32_t openLocation =
        nsWindowWatcher::GetWindowOpenLocation(nsPIDOMWindowOuter::From(aParent),
                                               aChromeFlags, aCalledFromJS,
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
    ContentChild* cc = ContentChild::GetSingleton();
    return cc->ProvideWindowCommon(this,
                                   aParent,
                                   iframeMoz,
                                   aChromeFlags,
                                   aCalledFromJS,
                                   aPositionSpecified,
                                   aSizeSpecified,
                                   aURI,
                                   aName,
                                   aFeatures,
                                   aForceNoOpener,
                                   aWindowIsNew,
                                   aReturn);
}

void
TabChild::DestroyWindow()
{
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
    if (baseWindow)
        baseWindow->Destroy();

    // NB: the order of mPuppetWidget->Destroy() and mRemoteFrame->Destroy()
    // is important: we want to kill off remote layers before their
    // frames
    if (mPuppetWidget) {
        mPuppetWidget->Destroy();
    }

    if (mRemoteFrame) {
        mRemoteFrame->Destroy();
        mRemoteFrame = nullptr;
    }


    if (mLayersId != 0) {
      StaticMutexAutoLock lock(sTabChildrenMutex);

      MOZ_ASSERT(sTabChildren);
      sTabChildren->Remove(mLayersId);
      if (!sTabChildren->Count()) {
        delete sTabChildren;
        sTabChildren = nullptr;
      }
      mLayersId = 0;
    }
}

void
TabChild::ActorDestroy(ActorDestroyReason why)
{
  mIPCOpen = false;

  DestroyWindow();

  if (mTabChildGlobal) {
    // We should have a message manager if the global is alive, but it
    // seems sometimes we don't.  Assert in aurora/nightly, but don't
    // crash in release builds.
    MOZ_DIAGNOSTIC_ASSERT(mTabChildGlobal->mMessageManager);
    if (mTabChildGlobal->mMessageManager) {
      // The messageManager relays messages via the TabChild which
      // no longer exists.
      static_cast<nsFrameMessageManager*>
        (mTabChildGlobal->mMessageManager.get())->Disconnect();
      mTabChildGlobal->mMessageManager = nullptr;
    }
  }

  CompositorBridgeChild* compositorChild = static_cast<CompositorBridgeChild*>(CompositorBridgeChild::Get());
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

  if (mHistoryListener) {
    mHistoryListener->ClearTabChild();
  }
}

mozilla::ipc::IPCResult
TabChild::RecvLoadURL(const nsCString& aURI,
                      const ShowInfo& aInfo)
{
  if (!mDidLoadURLInit) {
    mDidLoadURLInit = true;
    if (!InitTabChildGlobal()) {
      return IPC_FAIL_NO_REASON(this);
    }

    ApplyShowInfo(aInfo);
  }

  nsresult rv =
    WebNavigation()->LoadURI(NS_ConvertUTF8toUTF16(aURI).get(),
                             nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
                             nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL,
                             nullptr, nullptr, nullptr, nsContentUtils::GetSystemPrincipal());
  if (NS_FAILED(rv)) {
      NS_WARNING("WebNavigation()->LoadURI failed. Eating exception, what else can I do?");
  }

#ifdef MOZ_CRASHREPORTER
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("URL"), aURI);
#endif

  return IPC_OK();
}

void
TabChild::DoFakeShow(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                     const uint64_t& aLayersId,
                     const CompositorOptions& aCompositorOptions,
                     PRenderFrameChild* aRenderFrame, const ShowInfo& aShowInfo)
{
  InitRenderingState(aTextureFactoryIdentifier, aLayersId, aCompositorOptions, aRenderFrame);
  RecvShow(ScreenIntSize(0, 0), aShowInfo, mParentIsActive, nsSizeMode_Normal);
  mDidFakeShow = true;
}

void
TabChild::ApplyShowInfo(const ShowInfo& aInfo)
{
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
    nsCOMPtr<nsIDocShellTreeItem> item = do_GetInterface(docShell);
    if (IsMozBrowser()) {
      // B2G allows window.name to be set by changing the name attribute on the
      // <iframe mozbrowser> element. window.open calls cause this attribute to
      // be set to the correct value. A normal <xul:browser> element has no such
      // attribute. The data we get here comes from reading the attribute, so we
      // shouldn't trust it for <xul:browser> elements.
      item->SetName(aInfo.name());
    }
    docShell->SetFullscreenAllowed(aInfo.fullscreenAllowed());
    if (aInfo.isPrivate()) {
      nsCOMPtr<nsILoadContext> context = do_GetInterface(docShell);
      // No need to re-set private browsing mode.
      if (!context->UsePrivateBrowsing()) {
        if (docShell->GetHasLoadedNonBlankURI()) {
          nsContentUtils::ReportToConsoleNonLocalized(
            NS_LITERAL_STRING("We should not switch to Private Browsing after loading a document."),
            nsIScriptError::warningFlag,
            NS_LITERAL_CSTRING("mozprivatebrowsing"),
            nullptr);
        } else {
          OriginAttributes attrs(nsDocShell::Cast(docShell)->GetOriginAttributes());
          attrs.SyncAttributesWithPrivateBrowsing(true);
          nsDocShell::Cast(docShell)->SetOriginAttributes(attrs);
        }
      }
    }
  }
  mDPI = aInfo.dpi();
  mRounding = aInfo.widgetRounding();
  mDefaultScale = aInfo.defaultScale();
  mIsTransparent = aInfo.isTransparent();
}

mozilla::ipc::IPCResult
TabChild::RecvShow(const ScreenIntSize& aSize,
                   const ShowInfo& aInfo,
                   const bool& aParentIsActive,
                   const nsSizeMode& aSizeMode)
{
  bool res = true;

  mPuppetWidget->SetSizeMode(aSizeMode);
  if (!mDidFakeShow) {
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(WebNavigation());
    if (!baseWindow) {
        NS_ERROR("WebNavigation() doesn't QI to nsIBaseWindow");
        return IPC_FAIL_NO_REASON(this);
    }

    baseWindow->SetVisibility(true);
    res = InitTabChildGlobal();
  }

  ApplyShowInfo(aInfo);
  RecvParentActivated(aParentIsActive);

  if (!res) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvInitRendering(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                            const uint64_t& aLayersId,
                            const CompositorOptions& aCompositorOptions,
                            const bool& aLayersConnected,
                            PRenderFrameChild* aRenderFrame)
{
  MOZ_ASSERT((!mDidFakeShow && aRenderFrame) || (mDidFakeShow && !aRenderFrame));

  mLayersConnected = aLayersConnected;
  InitRenderingState(aTextureFactoryIdentifier, aLayersId, aCompositorOptions, aRenderFrame);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvUpdateDimensions(const DimensionInfo& aDimensionInfo)
{
    if (!mRemoteFrame) {
        return IPC_OK();
    }

    mUnscaledOuterRect = aDimensionInfo.rect();
    mClientOffset = aDimensionInfo.clientOffset();
    mChromeDisp = aDimensionInfo.chromeDisp();

    mOrientation = aDimensionInfo.orientation();
    SetUnscaledInnerSize(aDimensionInfo.size());
    if (!mHasValidInnerSize &&
        aDimensionInfo.size().width != 0 &&
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

    mPuppetWidget->Resize(screenRect.x + mClientOffset.x + mChromeDisp.x,
                          screenRect.y + mClientOffset.y + mChromeDisp.y,
                          screenSize.width, screenSize.height, true);

    return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSizeModeChanged(const nsSizeMode& aSizeMode)
{
  mPuppetWidget->SetSizeMode(aSizeMode);
  if (!mPuppetWidget->IsVisible()) {
    return IPC_OK();
  }
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (presShell) {
    nsPresContext* presContext = presShell->GetPresContext();
    if (presContext) {
      presContext->SizeModeChanged(aSizeMode);
    }
  }
  return IPC_OK();
}

bool
TabChild::UpdateFrame(const FrameMetrics& aFrameMetrics)
{
  return TabChildBase::UpdateFrameHandler(aFrameMetrics);
}

mozilla::ipc::IPCResult
TabChild::RecvSuppressDisplayport(const bool& aEnabled)
{
  if (aEnabled) {
    mActiveSuppressDisplayport++;
  } else {
    mActiveSuppressDisplayport--;
  }

  MOZ_ASSERT(mActiveSuppressDisplayport >= 0);
  APZCCallbackHelper::SuppressDisplayport(aEnabled, GetPresShell());
  return IPC_OK();
}

void
TabChild::HandleDoubleTap(const CSSPoint& aPoint, const Modifiers& aModifiers,
                          const ScrollableLayerGuid& aGuid)
{
  TABC_LOG("Handling double tap at %s with %p %p\n",
    Stringify(aPoint).c_str(), mGlobal.get(), mTabChildGlobal.get());

  if (!mGlobal || !mTabChildGlobal) {
    return;
  }

  // Note: there is nothing to do with the modifiers here, as we are not
  // synthesizing any sort of mouse event.
  nsCOMPtr<nsIDocument> document = GetDocument();
  CSSRect zoomToRect = CalculateRectToZoomTo(document, aPoint);
  // The double-tap can be dispatched by any scroll frame (so |aGuid| could be
  // the guid of any scroll frame), but the zoom-to-rect operation must be
  // performed by the root content scroll frame, so query its identifiers
  // for the SendZoomToRect() call rather than using the ones from |aGuid|.
  uint32_t presShellId;
  ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(
      document->GetDocumentElement(), &presShellId, &viewId) && mApzcTreeManager) {
    ScrollableLayerGuid guid(mLayersId, presShellId, viewId);

    mApzcTreeManager->ZoomToRect(guid, zoomToRect, DEFAULT_BEHAVIOR);
  }
}

mozilla::ipc::IPCResult
TabChild::RecvHandleTap(const GeckoContentController::TapType& aType,
                        const LayoutDevicePoint& aPoint,
                        const Modifiers& aModifiers,
                        const ScrollableLayerGuid& aGuid,
                        const uint64_t& aInputBlockId)
{
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell) {
    return IPC_OK();
  }
  if (!presShell->GetPresContext()) {
    return IPC_OK();
  }
  CSSToLayoutDeviceScale scale(presShell->GetPresContext()->CSSToDevPixelScale());
  CSSPoint point = APZCCallbackHelper::ApplyCallbackTransform(aPoint / scale, aGuid);

  switch (aType) {
  case GeckoContentController::TapType::eSingleTap:
    if (mGlobal && mTabChildGlobal) {
      mAPZEventState->ProcessSingleTap(point, scale, aModifiers, aGuid, 1);
    }
    break;
  case GeckoContentController::TapType::eDoubleTap:
    HandleDoubleTap(point, aModifiers, aGuid);
    break;
  case GeckoContentController::TapType::eSecondTap:
    if (mGlobal && mTabChildGlobal) {
      mAPZEventState->ProcessSingleTap(point, scale, aModifiers, aGuid, 2);
    }
    break;
  case GeckoContentController::TapType::eLongTap:
    if (mGlobal && mTabChildGlobal) {
      mAPZEventState->ProcessLongTap(presShell, point, scale, aModifiers, aGuid,
          aInputBlockId);
    }
    break;
  case GeckoContentController::TapType::eLongTapUp:
    if (mGlobal && mTabChildGlobal) {
      mAPZEventState->ProcessLongTapUp(presShell, point, scale, aModifiers);
    }
    break;
  case GeckoContentController::TapType::eSentinel:
    // Should never happen, but we need to handle this case to make the compiler
    // happy.
    MOZ_ASSERT(false);
    break;
  }
  return IPC_OK();
}

bool
TabChild::NotifyAPZStateChange(const ViewID& aViewId,
                               const layers::GeckoContentController::APZStateChange& aChange,
                               const int& aArg)
{
  mAPZEventState->ProcessAPZStateChange(aViewId, aChange, aArg);
  if (aChange == layers::GeckoContentController::APZStateChange::eTransformEnd) {
    // This is used by tests to determine when the APZ is done doing whatever
    // it's doing. XXX generify this as needed when writing additional tests.
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    observerService->NotifyObservers(nullptr, "APZ:TransformEnd", nullptr);
  }
  return true;
}

void
TabChild::StartScrollbarDrag(const layers::AsyncDragMetrics& aDragMetrics)
{
  ScrollableLayerGuid guid(mLayersId, aDragMetrics.mPresShellId,
                           aDragMetrics.mViewId);

  if (mApzcTreeManager) {
    mApzcTreeManager->StartScrollbarDrag(guid, aDragMetrics);
  }
}

void
TabChild::ZoomToRect(const uint32_t& aPresShellId,
                     const FrameMetrics::ViewID& aViewId,
                     const CSSRect& aRect,
                     const uint32_t& aFlags)
{
  ScrollableLayerGuid guid(mLayersId, aPresShellId, aViewId);

  if (mApzcTreeManager) {
    mApzcTreeManager->ZoomToRect(guid, aRect, aFlags);
  }
}

mozilla::ipc::IPCResult
TabChild::RecvActivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(WebNavigation());
  browser->Activate();
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvDeactivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(WebNavigation());
  browser->Deactivate();
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvParentActivated(const bool& aActivated)
{
  mParentIsActive = aActivated;

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, IPC_OK());

  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  fm->ParentActivated(window, aActivated);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSetKeyboardIndicators(const UIStateChangeType& aShowAccelerators,
                                    const UIStateChangeType& aShowFocusRings)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, IPC_OK());

  window->SetKeyboardIndicators(aShowAccelerators, aShowFocusRings);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvStopIMEStateManagement()
{
  IMEStateManager::StopIMEStateManagement();
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvMenuKeyboardListenerInstalled(const bool& aInstalled)
{
  IMEStateManager::OnInstalledMenuKeyboardListener(aInstalled);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNotifyAttachGroupedSHistory(const uint32_t& aOffset)
{
  // nsISHistory uses int32_t
  if (NS_WARN_IF(aOffset > INT32_MAX)) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsCOMPtr<nsISHistory> shistory = GetRelatedSHistory();
  NS_ENSURE_TRUE(shistory, IPC_FAIL_NO_REASON(this));

  if (NS_FAILED(shistory->OnAttachGroupedSHistory(aOffset))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNotifyPartialSHistoryActive(const uint32_t& aGlobalLength,
                                          const uint32_t& aTargetLocalIndex)
{
  // nsISHistory uses int32_t
  if (NS_WARN_IF(aGlobalLength > INT32_MAX || aTargetLocalIndex > INT32_MAX)) {
    return IPC_FAIL_NO_REASON(this);
  }

  nsCOMPtr<nsISHistory> shistory = GetRelatedSHistory();
  NS_ENSURE_TRUE(shistory, IPC_FAIL_NO_REASON(this));

  if (NS_FAILED(shistory->OnPartialSHistoryActive(aGlobalLength,
                                                  aTargetLocalIndex))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNotifyPartialSHistoryDeactive()
{
  nsCOMPtr<nsISHistory> shistory = GetRelatedSHistory();
  NS_ENSURE_TRUE(shistory, IPC_FAIL_NO_REASON(this));

  if (NS_FAILED(shistory->OnPartialSHistoryDeactive())) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvMouseEvent(const nsString& aType,
                         const float&    aX,
                         const float&    aY,
                         const int32_t&  aButton,
                         const int32_t&  aClickCount,
                         const int32_t&  aModifiers,
                         const bool&     aIgnoreRootScrollFrame)
{
  APZCCallbackHelper::DispatchMouseEvent(GetPresShell(), aType,
                                         CSSPoint(aX, aY), aButton, aClickCount,
                                         aModifiers, aIgnoreRootScrollFrame,
                                         nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN,
                                         0 /* Use the default value here. */);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvRealMouseMoveEvent(const WidgetMouseEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 const uint64_t& aInputBlockId)
{
  if (!RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSynthMouseMoveEvent(const WidgetMouseEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId)
{
  if (!RecvRealMouseButtonEvent(aEvent, aGuid, aInputBlockId)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvRealMouseButtonEvent(const WidgetMouseEvent& aEvent,
                                   const ScrollableLayerGuid& aGuid,
                                   const uint64_t& aInputBlockId)
{
  // Mouse events like eMouseEnterIntoWidget, that are created in the parent
  // process EventStateManager code, have an input block id which they get from
  // the InputAPZContext in the parent process stack. However, they did not
  // actually go through the APZ code and so their mHandledByAPZ flag is false.
  // Since thos events didn't go through APZ, we don't need to send
  // notifications for them.
  bool pendingLayerization = false;
  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    nsCOMPtr<nsIDocument> document(GetDocument());
    pendingLayerization =
      APZCCallbackHelper::SendSetTargetAPZCNotification(mPuppetWidget, document,
                                                        aEvent, aGuid,
                                                        aInputBlockId);
  }

  nsEventStatus unused;
  InputAPZContext context(aGuid, aInputBlockId, unused);
  if (pendingLayerization) {
    context.SetPendingLayerization();
  }

  WidgetMouseEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
      mPuppetWidget->GetDefaultScale());
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    mAPZEventState->ProcessMouseEvent(aEvent, aGuid, aInputBlockId);
  }
  return IPC_OK();
}


// In case handling repeated mouse wheel takes much time, we skip firing current
// wheel event if it may be coalesced to the next one.
bool
TabChild::MaybeCoalesceWheelEvent(const WidgetWheelEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId,
                                  bool* aIsNextWheelEvent)
{
  MOZ_ASSERT(aIsNextWheelEvent);
  if (aEvent.mMessage == eWheel) {
    GetIPCChannel()->PeekMessages(
        [aIsNextWheelEvent](const IPC::Message& aMsg) -> bool {
          if (aMsg.type() == mozilla::dom::PBrowser::Msg_MouseWheelEvent__ID) {
            *aIsNextWheelEvent = true;
          }
          return false; // Stop peeking.
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
    if (!mLastWheelProcessedTimeFromParent.IsNull() &&
        *aIsNextWheelEvent &&
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

void
TabChild::MaybeDispatchCoalescedWheelEvent()
{
  if (mCoalescedWheelData.IsEmpty()) {
    return;
  }
  const WidgetWheelEvent* wheelEvent =
    mCoalescedWheelData.GetCoalescedWheelEvent();
  MOZ_ASSERT(wheelEvent);
  DispatchWheelEvent(*wheelEvent,
                     mCoalescedWheelData.GetScrollableLayerGuid(),
                     mCoalescedWheelData.GetInputBlockId());
  mCoalescedWheelData.Reset();
}

void
TabChild::DispatchWheelEvent(const WidgetWheelEvent& aEvent,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t& aInputBlockId)
{
  WidgetWheelEvent localEvent(aEvent);
  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    nsCOMPtr<nsIDocument> document(GetDocument());
    APZCCallbackHelper::SendSetTargetAPZCNotification(
      mPuppetWidget, document, aEvent, aGuid, aInputBlockId);
  }

  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
                                             mPuppetWidget->GetDefaultScale());
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);

  if (localEvent.mCanTriggerSwipe) {
    SendRespondStartSwipeEvent(aInputBlockId, localEvent.TriggersSwipe());
  }

  if (aInputBlockId && aEvent.mFlags.mHandledByAPZ) {
    mAPZEventState->ProcessWheelEvent(localEvent, aGuid, aInputBlockId);
  }
}

mozilla::ipc::IPCResult
TabChild::RecvMouseWheelEvent(const WidgetWheelEvent& aEvent,
                              const ScrollableLayerGuid& aGuid,
                              const uint64_t& aInputBlockId)
{
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

mozilla::ipc::IPCResult
TabChild::RecvRealTouchEvent(const WidgetTouchEvent& aEvent,
                             const ScrollableLayerGuid& aGuid,
                             const uint64_t& aInputBlockId,
                             const nsEventStatus& aApzResponse)
{
  TABC_LOG("Receiving touch event of type %d\n", aEvent.mMessage);

  WidgetTouchEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  APZCCallbackHelper::ApplyCallbackTransform(localEvent, aGuid,
                                             mPuppetWidget->GetDefaultScale());

  if (localEvent.mMessage == eTouchStart && AsyncPanZoomEnabled()) {
    nsCOMPtr<nsIDocument> document = GetDocument();
    if (gfxPrefs::TouchActionEnabled()) {
      APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(
        mPuppetWidget, document, localEvent, aInputBlockId,
        mSetAllowedTouchBehaviorCallback);
    }
    APZCCallbackHelper::SendSetTargetAPZCNotification(mPuppetWidget, document,
                                                      localEvent, aGuid,
                                                      aInputBlockId);
  }

  // Dispatch event to content (potentially a long-running operation)
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);

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

mozilla::ipc::IPCResult
TabChild::RecvRealTouchMoveEvent(const WidgetTouchEvent& aEvent,
                                 const ScrollableLayerGuid& aGuid,
                                 const uint64_t& aInputBlockId,
                                 const nsEventStatus& aApzResponse)
{
  if (!RecvRealTouchEvent(aEvent, aGuid, aInputBlockId, aApzResponse)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvRealDragEvent(const WidgetDragEvent& aEvent,
                            const uint32_t& aDragAction,
                            const uint32_t& aDropEffect)
{
  WidgetDragEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (dragSession) {
    dragSession->SetDragAction(aDragAction);
    nsCOMPtr<nsIDOMDataTransfer> initialDataTransfer;
    dragSession->GetDataTransfer(getter_AddRefs(initialDataTransfer));
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

  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvPluginEvent(const WidgetPluginEvent& aEvent)
{
  WidgetPluginEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  if (status != nsEventStatus_eConsumeNoDefault) {
    // If not consumed, we should call default action
    SendDefaultProcOfPluginEvent(aEvent);
  }
  return IPC_OK();
}

void
TabChild::RequestEditCommands(nsIWidget::NativeKeyBindingsType aType,
                              const WidgetKeyboardEvent& aEvent,
                              nsTArray<CommandInt>& aCommands)
{
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

  SendRequestNativeKeyBindings(aType, aEvent, &aCommands);
}

mozilla::ipc::IPCResult
TabChild::RecvNativeSynthesisResponse(const uint64_t& aObserverId,
                                      const nsCString& aResponse)
{
  mozilla::widget::AutoObserverNotifier::NotifySavedObserver(aObserverId,
                                                             aResponse.get());
  return IPC_OK();
}

// In case handling repeated keys takes much time, we skip firing new ones.
bool
TabChild::SkipRepeatedKeyEvent(const WidgetKeyboardEvent& aEvent)
{
  if (mRepeatedKeyEventTime.IsNull() ||
      !aEvent.mIsRepeat ||
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

void
TabChild::UpdateRepeatedKeyEventEndTime(const WidgetKeyboardEvent& aEvent)
{
  if (aEvent.mIsRepeat &&
      (aEvent.mMessage == eKeyDown || aEvent.mMessage == eKeyPress)) {
    mRepeatedKeyEventTime = TimeStamp::Now();
  }
}

mozilla::ipc::IPCResult
TabChild::RecvRealKeyEvent(const WidgetKeyboardEvent& aEvent)
{
  if (SkipRepeatedKeyEvent(aEvent)) {
    return IPC_OK();
  }

  MOZ_ASSERT(aEvent.mMessage != eKeyPress ||
             aEvent.AreAllEditCommandsInitialized(),
    "eKeyPress event should have native key binding information");

  // If content code called preventDefault() on a keydown event, then we don't
  // want to process any following keypress events.
  if (aEvent.mMessage == eKeyPress && mIgnoreKeyPressEvent) {
    return IPC_OK();
  }

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  localEvent.mUniqueId = aEvent.mUniqueId;
  nsEventStatus status = APZCCallbackHelper::DispatchWidgetEvent(localEvent);

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
  // If mAccessKeyForwardedToChild is set, then don't resend the key event yet
  // as RecvHandleAccessKey will do this.
  if (localEvent.mFlags.mWantReplyFromContentProcess) {
    SendReplyKeyEvent(localEvent);
  }

  if (localEvent.mAccessKeyForwardedToChild) {
    SendAccessKeyNotHandled(localEvent);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvKeyEvent(const nsString& aType,
                       const int32_t& aKeyCode,
                       const int32_t& aCharCode,
                       const int32_t& aModifiers,
                       const bool& aPreventDefault)
{
  bool ignored = false;
  nsContentUtils::SendKeyEvent(mPuppetWidget, aType, aKeyCode, aCharCode,
                               aModifiers, aPreventDefault, &ignored);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvCompositionEvent(const WidgetCompositionEvent& aEvent)
{
  WidgetCompositionEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  Unused << SendOnEventNeedingAckHandled(aEvent.mMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSelectionEvent(const WidgetSelectionEvent& aEvent)
{
  WidgetSelectionEvent localEvent(aEvent);
  localEvent.mWidget = mPuppetWidget;
  APZCCallbackHelper::DispatchWidgetEvent(localEvent);
  Unused << SendOnEventNeedingAckHandled(aEvent.mMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvPasteTransferable(const IPCDataTransfer& aDataTransfer,
                                const bool& aIsPrivateData,
                                const IPC::Principal& aRequestingPrincipal)
{
  nsresult rv;
  nsCOMPtr<nsITransferable> trans =
    do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());
  trans->Init(nullptr);

  rv = nsContentUtils::IPCTransferableToTransferable(aDataTransfer,
                                                     aIsPrivateData,
                                                     aRequestingPrincipal,
                                                     trans, nullptr, this);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  if (NS_WARN_IF(!ourDocShell)) {
    return IPC_OK();
  }

  nsCOMPtr<nsICommandParams> params =
    do_CreateInstance("@mozilla.org/embedcomp/command-params;1", &rv);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  rv = params->SetISupportsValue("transferable", trans);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  ourDocShell->DoCommandWithParams("cmd_pasteTransferable", params);
  return IPC_OK();
}


a11y::PDocAccessibleChild*
TabChild::AllocPDocAccessibleChild(PDocAccessibleChild*, const uint64_t&,
                                   const uint32_t&, const IAccessibleHolder&)
{
  MOZ_ASSERT(false, "should never call this!");
  return nullptr;
}

bool
TabChild::DeallocPDocAccessibleChild(a11y::PDocAccessibleChild* aChild)
{
#ifdef ACCESSIBILITY
  delete static_cast<mozilla::a11y::DocAccessibleChild*>(aChild);
#endif
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

mozilla::ipc::IPCResult
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
        return IPC_OK(); // silently ignore
    nsCOMPtr<mozIDOMWindowProxy> window;
    if (NS_FAILED(browser->GetContentDOMWindow(getter_AddRefs(window))) ||
        !window)
    {
        return IPC_OK(); // silently ignore
    }

    nsCString data;
    bool ret = render->RenderDocument(nsPIDOMWindowOuter::From(window),
                                      documentRect, transform,
                                      bgcolor,
                                      renderFlags, flushLayout,
                                      renderSize, data);
    if (!ret)
        return IPC_OK(); // silently ignore

    if (!PDocumentRendererChild::Send__delete__(actor, renderSize, data)) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
}

PColorPickerChild*
TabChild::AllocPColorPickerChild(const nsString&, const nsString&)
{
  MOZ_CRASH("unused");
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
  MOZ_CRASH("unused");
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

mozilla::ipc::IPCResult
TabChild::RecvActivateFrameEvent(const nsString& aType, const bool& capture)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
  NS_ENSURE_TRUE(window, IPC_OK());
  nsCOMPtr<EventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, IPC_OK());
  RefPtr<ContentListener> listener = new ContentListener(this);
  chromeHandler->AddEventListener(aType, listener, capture);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvLoadRemoteScript(const nsString& aURL, const bool& aRunInGlobalScope)
{
  if (!mGlobal && !InitTabChildGlobal())
    // This can happen if we're half-destroyed.  It's not a fatal
    // error.
    return IPC_OK();

  LoadScriptInternal(aURL, aRunInGlobalScope);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvAsyncMessage(const nsString& aMessage,
                           InfallibleTArray<CpowEntry>&& aCpows,
                           const IPC::Principal& aPrincipal,
                           const ClonedMessageData& aData)
{
  NS_LossyConvertUTF16toASCII messageNameCStr(aMessage);
  PROFILER_LABEL_DYNAMIC("TabChild", "RecvAsyncMessage",
                         js::ProfileEntry::Category::EVENTS,
                         messageNameCStr.get());

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  if (!mTabChildGlobal) {
    return IPC_OK();
  }

  // We should have a message manager if the global is alive, but it
  // seems sometimes we don't.  Assert in aurora/nightly, but don't
  // crash in release builds.
  MOZ_DIAGNOSTIC_ASSERT(mTabChildGlobal->mMessageManager);
  if (!mTabChildGlobal->mMessageManager) {
    return IPC_OK();
  }

  JS::Rooted<JSObject*> kungFuDeathGrip(dom::RootingCx(), GetGlobal());
  StructuredCloneData data;
  UnpackClonedMessageDataForChild(aData, data);
  RefPtr<nsFrameMessageManager> mm =
    static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
  mm->ReceiveMessage(static_cast<EventTarget*>(mTabChildGlobal), nullptr,
                     aMessage, false, &data, &cpows, aPrincipal, nullptr);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSwappedWithOtherRemoteLoader(const IPCTabContext& aContext)
{
  nsCOMPtr<nsIDocShell> ourDocShell = do_GetInterface(WebNavigation());
  if (NS_WARN_IF(!ourDocShell)) {
    return IPC_OK();
  }

  nsCOMPtr<nsPIDOMWindowOuter> ourWindow = ourDocShell->GetWindow();
  if (NS_WARN_IF(!ourWindow)) {
    return IPC_OK();
  }

  RefPtr<nsDocShell> docShell = static_cast<nsDocShell*>(ourDocShell.get());

  nsCOMPtr<EventTarget> ourEventTarget = ourWindow->GetParentTarget();

  docShell->SetInFrameSwap(true);

  nsContentUtils::FirePageShowEvent(ourDocShell, ourEventTarget, false);
  nsContentUtils::FirePageHideEvent(ourDocShell, ourEventTarget);

  // Owner content type may have changed, so store the possibly updated context
  // and notify others.
  MaybeInvalidTabContext maybeContext(aContext);
  if (!maybeContext.IsValid()) {
    NS_ERROR(nsPrintfCString("Received an invalid TabContext from "
                             "the parent process. (%s)",
                             maybeContext.GetInvalidReason()).get());
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
  if (IsMozBrowser()) {
    RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
  }

  nsContentUtils::FirePageShowEvent(ourDocShell, ourEventTarget, true);

  docShell->SetInFrameSwap(false);

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvHandleAccessKey(const WidgetKeyboardEvent& aEvent,
                              nsTArray<uint32_t>&& aCharCodes,
                              const int32_t& aModifierMask)
{
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (presShell) {
    nsPresContext* pc = presShell->GetPresContext();
    if (pc) {
      if (!pc->EventStateManager()->
                 HandleAccessKey(&(const_cast<WidgetKeyboardEvent&>(aEvent)),
                                 pc, aCharCodes,
                                 aModifierMask, true)) {
        // If no accesskey was found, inform the parent so that accesskeys on
        // menus can be handled.
        WidgetKeyboardEvent localEvent(aEvent);
        localEvent.mWidget = mPuppetWidget;
        SendAccessKeyNotHandled(localEvent);
      }
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSetUseGlobalHistory(const bool& aUse)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  MOZ_ASSERT(docShell);

  nsresult rv = docShell->SetUseGlobalHistory(aUse);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to set UseGlobalHistory on TabChild docShell");
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvPrint(const uint64_t& aOuterWindowID, const PrintData& aPrintData)
{
#ifdef NS_PRINTING
  nsGlobalWindow* outerWindow =
    nsGlobalWindow::GetOuterWindowWithId(aOuterWindowID);
  if (NS_WARN_IF(!outerWindow)) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint =
    do_GetInterface(outerWindow->AsOuter());
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

  nsCOMPtr<nsIPrintSession>  printSession =
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

mozilla::ipc::IPCResult
TabChild::RecvUpdateNativeWindowHandle(const uintptr_t& aNewHandle)
{
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  mNativeWindowHandle = aNewHandle;
  return IPC_OK();
#else
  return IPC_FAIL_NO_REASON(this);
#endif
}

mozilla::ipc::IPCResult
TabChild::RecvDestroy()
{
  MOZ_ASSERT(mDestroyed == false);
  mDestroyed = true;

  nsTArray<PContentPermissionRequestChild*> childArray =
      nsContentPermissionUtils::GetContentPermissionRequestChildById(GetTabId());

  // Need to close undeleted ContentPermissionRequestChilds before tab is closed.
  for (auto& permissionRequestChild : childArray) {
      auto child = static_cast<RemotePermissionRequest*>(permissionRequestChild);
      child->Destroy();
  }

  while (mActiveSuppressDisplayport > 0) {
    APZCCallbackHelper::SuppressDisplayport(false, nullptr);
    mActiveSuppressDisplayport--;
  }

  if (mTabChildGlobal) {
    // Message handlers are called from the event loop, so it better be safe to
    // run script.
    MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());
    mTabChildGlobal->DispatchTrustedEvent(NS_LITERAL_STRING("unload"));
  }

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  observerService->RemoveObserver(this, BEFORE_FIRST_PAINT);

  // XXX what other code in ~TabChild() should we be running here?
  DestroyWindow();

  // Bounce through the event loop once to allow any delayed teardown runnables
  // that were just generated to have a chance to run.
  nsCOMPtr<nsIRunnable> deleteRunnable = new DelayedDeleteRunnable(this);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(deleteRunnable));

  return IPC_OK();
}

void
TabChild::AddPendingDocShellBlocker()
{
  mPendingDocShellBlockers++;
}

void
TabChild::RemovePendingDocShellBlocker()
{
  mPendingDocShellBlockers--;
  if (!mPendingDocShellBlockers && mPendingDocShellReceivedMessage) {
    mPendingDocShellReceivedMessage = false;
    InternalSetDocShellIsActive(mPendingDocShellIsActive,
                                mPendingDocShellPreserveLayers);
  }
}

void
TabChild::InternalSetDocShellIsActive(bool aIsActive, bool aPreserveLayers)
{
  auto clearForcePaint = MakeScopeExit([&] {
    // We might force a paint, or we might already have painted and this is a
    // no-op. In either case, once we exit this scope, we need to alert the
    // ProcessHangMonitor that we've finished responding to what might have
    // been a request to force paint. This is so that the BackgroundHangMonitor
    // for force painting can be made to wait again.
    if (aIsActive) {
      ProcessHangMonitor::ClearForcePaint();
    }
  });

  if (mCompositorOptions) {
    // Note that |GetLayerManager()| has side-effects in that it creates a layer
    // manager if one doesn't exist already. Calling it inside a debug-only
    // assertion is generally bad but in this case we call it unconditionally
    // just below so it's ok.
    MOZ_ASSERT(mPuppetWidget);
    MOZ_ASSERT(mPuppetWidget->GetLayerManager());
    MOZ_ASSERT(mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT
            || mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_WR
            || (gfxPlatform::IsHeadless() && mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC));

    // We send the current layer observer epoch to the compositor so that
    // TabParent knows whether a layer update notification corresponds to the
    // latest SetDocShellIsActive request that was made.
    mPuppetWidget->GetLayerManager()->SetLayerObserverEpoch(mLayerObserverEpoch);
  }

  // docshell is consider prerendered only if not active yet
  mIsPrerendered &= !aIsActive;
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  if (docShell) {
    bool wasActive;
    docShell->GetIsActive(&wasActive);
    if (aIsActive && wasActive) {
      // This request is a no-op. In this case, we still want a MozLayerTreeReady
      // notification to fire in the parent (so that it knows that the child has
      // updated its epoch). ForcePaintNoOp does that.
      if (IPCOpen()) {
        Unused << SendForcePaintNoOp(mLayerObserverEpoch);
        return;
      }
    }

    docShell->SetIsActive(aIsActive);
  }

  if (aIsActive) {
    MakeVisible();

    // We don't use TabChildBase::GetPresShell() here because that would create
    // a content viewer if one doesn't exist yet. Creating a content viewer can
    // cause JS to run, which we want to avoid. nsIDocShell::GetPresShell
    // returns null if no content viewer exists yet.
    if (nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell()) {
      if (nsIFrame* root = presShell->FrameConstructor()->GetRootFrame()) {
        FrameLayerBuilder::InvalidateAllLayersForFrame(
          nsLayoutUtils::GetDisplayRootFrame(root));
        root->SchedulePaint();
      }

      Telemetry::AutoTimer<Telemetry::TABCHILD_PAINT_TIME> timer;
      // If we need to repaint, let's do that right away. No sense waiting until
      // we get back to the event loop again. We suppress the display port so that
      // we only paint what's visible. This ensures that the tab we're switching
      // to paints as quickly as possible.
      APZCCallbackHelper::SuppressDisplayport(true, presShell);
      if (nsContentUtils::IsSafeToRunScript()) {
        WebWidget()->PaintNowIfNeeded();
      } else {
        RefPtr<nsViewManager> vm = presShell->GetViewManager();
        if (nsView* view = vm->GetRootView()) {
          presShell->Paint(view, view->GetBounds(),
                           nsIPresShell::PAINT_LAYERS);
        }
      }
      APZCCallbackHelper::SuppressDisplayport(false, presShell);
    }
  } else if (!aPreserveLayers) {
    MakeHidden();
  }
}

mozilla::ipc::IPCResult
TabChild::RecvSetDocShellIsActive(const bool& aIsActive,
                                  const bool& aPreserveLayers,
                                  const uint64_t& aLayerObserverEpoch)
{
  // Since requests to change the active docshell come in from both the hang
  // monitor channel and the PContent channel, we have an ordering problem. This
  // code ensures that we respect the order in which the requests were made and
  // ignore stale requests.
  if (mLayerObserverEpoch >= aLayerObserverEpoch) {
    return IPC_OK();
  }
  mLayerObserverEpoch = aLayerObserverEpoch;

  // If we're currently waiting for window opening to complete, we need to hold
  // off on setting the docshell active. We queue up the values we're receiving
  // in the mWindowOpenDocShellActiveStatus.
  if (mPendingDocShellBlockers > 0) {
    mPendingDocShellReceivedMessage = true;
    mPendingDocShellIsActive = aIsActive;
    mPendingDocShellPreserveLayers = aPreserveLayers;
    return IPC_OK();
  }

  InternalSetDocShellIsActive(aIsActive, aPreserveLayers);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvNavigateByKey(const bool& aForward, const bool& aForDocumentNavigation)
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    nsCOMPtr<nsIDOMElement> result;
    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());

    // Move to the first or last document.
    uint32_t type = aForward ?
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FIRSTDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_ROOT)) :
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LASTDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_LAST));
    fm->MoveFocus(window, nullptr, type,
                  nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));

    // No valid root element was found, so move to the first focusable element.
    if (!result && aForward && !aForDocumentNavigation) {
      fm->MoveFocus(window, nullptr, nsIFocusManager::MOVEFOCUS_FIRST,
                  nsIFocusManager::FLAG_BYKEY, getter_AddRefs(result));
    }

    SendRequestFocus(false);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvHandledWindowedPluginKeyEvent(
            const NativeEventData& aKeyEventData,
            const bool& aIsConsumed)
{
  if (NS_WARN_IF(!mPuppetWidget)) {
    return IPC_OK();
  }
  mPuppetWidget->HandledWindowedPluginKeyEvent(aKeyEventData, aIsConsumed);
  return IPC_OK();
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
TabChild::InitTabChildGlobal()
{
  if (!mGlobal && !mTabChildGlobal) {
    nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(WebNavigation());
    NS_ENSURE_TRUE(window, false);
    nsCOMPtr<EventTarget> chromeHandler =
      do_QueryInterface(window->GetChromeEventHandler());
    NS_ENSURE_TRUE(chromeHandler, false);

    RefPtr<TabChildGlobal> scope = new TabChildGlobal(this);

    nsISupports* scopeSupports = NS_ISUPPORTS_CAST(EventTarget*, scope);

    NS_NAMED_LITERAL_CSTRING(globalId, "outOfProcessTabChildGlobal");
    NS_ENSURE_TRUE(InitChildGlobalInternal(scopeSupports, globalId), false);

    scope->Init();

    nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
    NS_ENSURE_TRUE(root, false);
    root->SetParentTarget(scope);

    mTabChildGlobal = scope.forget();;
  }

  if (!mTriedBrowserInit) {
    mTriedBrowserInit = true;
    // Initialize the child side of the browser element machinery,
    // if appropriate.
    if (IsMozBrowser()) {
      RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT, true);
    }
  }

  return true;
}

void
TabChild::InitRenderingState(const TextureFactoryIdentifier& aTextureFactoryIdentifier,
                             const uint64_t& aLayersId,
                             const CompositorOptions& aCompositorOptions,
                             PRenderFrameChild* aRenderFrame)
{
    mPuppetWidget->InitIMEState();

    if (!aRenderFrame) {
      NS_WARNING("failed to construct RenderFrame");
      return;
    }

    MOZ_ASSERT(aLayersId != 0);
    mTextureFactoryIdentifier = aTextureFactoryIdentifier;

    // Pushing layers transactions directly to a separate
    // compositor context.
    PCompositorBridgeChild* compositorChild = CompositorBridgeChild::Get();
    if (!compositorChild) {
      NS_WARNING("failed to get CompositorBridgeChild instance");
      return;
    }

    mCompositorOptions = Some(aCompositorOptions);

    mRemoteFrame = static_cast<RenderFrameChild*>(aRenderFrame);
    if (aLayersId != 0) {
      StaticMutexAutoLock lock(sTabChildrenMutex);

      if (!sTabChildren) {
        sTabChildren = new TabChildMap;
      }
      MOZ_ASSERT(!sTabChildren->Get(aLayersId));
      sTabChildren->Put(aLayersId, this);
      mLayersId = aLayersId;
    }

    ShadowLayerForwarder* lf =
        mPuppetWidget->GetLayerManager(
            nullptr, mTextureFactoryIdentifier.mParentBackend)
                ->AsShadowForwarder();

    LayerManager* lm = mPuppetWidget->GetLayerManager();
    if (lm->AsWebRenderLayerManager()) {
      lm->AsWebRenderLayerManager()->Initialize(compositorChild,
                                                wr::AsPipelineId(aLayersId),
                                                &mTextureFactoryIdentifier);
      ImageBridgeChild::IdentifyCompositorTextureHost(mTextureFactoryIdentifier);
      gfx::VRManagerChild::IdentifyTextureHost(mTextureFactoryIdentifier);
      InitAPZState();
    }

    if (lf) {
      nsTArray<LayersBackend> backends;
      backends.AppendElement(mTextureFactoryIdentifier.mParentBackend);
      PLayerTransactionChild* shadowManager =
          compositorChild->SendPLayerTransactionConstructor(backends, aLayersId);
      if (shadowManager) {
        lf->SetShadowManager(shadowManager);
        lf->IdentifyTextureHost(mTextureFactoryIdentifier);
        ImageBridgeChild::IdentifyCompositorTextureHost(mTextureFactoryIdentifier);
        gfx::VRManagerChild::IdentifyTextureHost(mTextureFactoryIdentifier);
        InitAPZState();
      }
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    if (observerService) {
        observerService->AddObserver(this,
                                     BEFORE_FIRST_PAINT,
                                     false);
    }
}

void
TabChild::InitAPZState()
{
  if (!mCompositorOptions->UseAPZ()) {
    return;
  }
  auto cbc = CompositorBridgeChild::Get();

  // Initialize the ApzcTreeManager. This takes multiple casts because of ugly multiple inheritance.
  PAPZCTreeManagerChild* baseProtocol = cbc->SendPAPZCTreeManagerConstructor(mLayersId);
  APZCTreeManagerChild* derivedProtocol = static_cast<APZCTreeManagerChild*>(baseProtocol);

  mApzcTreeManager = RefPtr<IAPZCTreeManager>(derivedProtocol);

  // Initialize the GeckoContentController for this tab. We don't hold a reference because we don't need it.
  // The ContentProcessController will hold a reference to the tab, and will be destroyed by the compositor or ipdl
  // during destruction.
  RefPtr<GeckoContentController> contentController = new ContentProcessController(this);
  APZChild* apzChild = new APZChild(contentController);
  cbc->SetEventTargetForActor(
    apzChild, TabGroup()->EventTargetFor(TaskCategory::Other));
  MOZ_ASSERT(apzChild->GetActorEventTarget());
  cbc->SendPAPZConstructor(apzChild, mLayersId);
}

void
TabChild::GetDPI(float* aDPI)
{
    *aDPI = -1.0;
    if (!(mDidFakeShow || mDidSetRealShowInfo)) {
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
    if (!(mDidFakeShow || mDidSetRealShowInfo)) {
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
TabChild::GetWidgetRounding(int32_t* aRounding)
{
  *aRounding = 1;
  if (!(mDidFakeShow || mDidSetRealShowInfo)) {
    return;
  }
  if (mRounding > 0) {
    *aRounding = mRounding;
    return;
  }

  // Fallback to a sync call if needed.
  SendGetWidgetRounding(aRounding);
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
  if (mPuppetWidget && mPuppetWidget->IsVisible()) {
    return;
  }

  if (mPuppetWidget) {
    mPuppetWidget->Show(true);
  }
}

void
TabChild::MakeHidden()
{
  if (mPuppetWidget && !mPuppetWidget->IsVisible()) {
    return;
  }

  ClearCachedResources();

  // Hide all plugins in this tab.
  if (nsCOMPtr<nsIPresShell> shell = GetPresShell()) {
    if (nsPresContext* presContext = shell->GetPresContext()) {
      nsRootPresContext* rootPresContext = presContext->GetRootPresContext();
      nsIFrame* rootFrame = shell->FrameConstructor()->GetRootFrame();
      rootPresContext->ComputePluginGeometryUpdates(rootFrame, nullptr, nullptr);
      rootPresContext->ApplyPluginGeometryUpdates();
    }
  }

  if (mPuppetWidget) {
    mPuppetWidget->Show(false);
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
TabChild::SendGetTabCount(uint32_t* tabCount)
{
  PBrowserChild::SendGetTabCount(tabCount);
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
                                StructuredCloneData& aData,
                                JS::Handle<JSObject *> aCpows,
                                nsIPrincipal* aPrincipal,
                                nsTArray<StructuredCloneData>* aRetVal,
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
                           Principal(aPrincipal), aRetVal);
  }

  return SendRpcMessage(PromiseFlatString(aMessage), data, cpows,
                        Principal(aPrincipal), aRetVal);
}

nsresult
TabChild::DoSendAsyncMessage(JSContext* aCx,
                             const nsAString& aMessage,
                             StructuredCloneData& aData,
                             JS::Handle<JSObject *> aCpows,
                             nsIPrincipal* aPrincipal)
{
  ClonedMessageData data;
  if (!BuildClonedMessageDataForChild(Manager(), aData, data)) {
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }
  InfallibleTArray<CpowEntry> cpows;
  if (aCpows && !Manager()->GetCPOWManager()->Wrap(aCx, aCpows, &cpows)) {
    return NS_ERROR_UNEXPECTED;
  }
  if (!SendAsyncMessage(PromiseFlatString(aMessage), cpows,
                        Principal(aPrincipal), data)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

/* static */ nsTArray<RefPtr<TabChild>>
TabChild::GetAll()
{
  StaticMutexAutoLock lock(sTabChildrenMutex);

  nsTArray<RefPtr<TabChild>> list;
  if (!sTabChildren) {
    return list;
  }

  for (auto iter = sTabChildren->Iter(); !iter.Done(); iter.Next()) {
    list.AppendElement(iter.Data());
  }

  return list;
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
  StaticMutexAutoLock lock(sTabChildrenMutex);
  if (!sTabChildren) {
    return nullptr;
  }
  return sTabChildren->Get(aLayersId);
}

void
TabChild::DidComposite(uint64_t aTransactionId,
                       const TimeStamp& aCompositeStart,
                       const TimeStamp& aCompositeEnd)
{
  MOZ_ASSERT(mPuppetWidget);
  MOZ_ASSERT(mPuppetWidget->GetLayerManager());
  MOZ_ASSERT(mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT
             || mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_WR
             || (gfxPlatform::IsHeadless() && mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC));

  mPuppetWidget->GetLayerManager()->DidComposite(aTransactionId, aCompositeStart, aCompositeEnd);
}

void
TabChild::DidRequestComposite(const TimeStamp& aCompositeReqStart,
                              const TimeStamp& aCompositeReqEnd)
{
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
    timelines->AddMarkerForDocShell(docShell,
      "CompositeForwardTransaction", aCompositeReqStart,
      MarkerTracingType::START, MarkerStackRequest::NO_STACK);
    timelines->AddMarkerForDocShell(docShell,
      "CompositeForwardTransaction", aCompositeReqEnd,
      MarkerTracingType::END, MarkerStackRequest::NO_STACK);
  }
}

void
TabChild::ClearCachedResources()
{
  MOZ_ASSERT(mPuppetWidget);
  MOZ_ASSERT(mPuppetWidget->GetLayerManager());
  MOZ_ASSERT(mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT
             || mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_WR
             || (gfxPlatform::IsHeadless() && mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC));

  mPuppetWidget->GetLayerManager()->ClearCachedResources();
}

void
TabChild::InvalidateLayers()
{
  MOZ_ASSERT(mPuppetWidget);
  MOZ_ASSERT(mPuppetWidget->GetLayerManager());
  MOZ_ASSERT(mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT
             || mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_WR
             || (gfxPlatform::IsHeadless() && mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC));

  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  FrameLayerBuilder::InvalidateAllLayers(lm);
}

void
TabChild::ReinitRendering()
{
  MOZ_ASSERT(mLayersId);

  // Before we establish a new PLayerTransaction, we must connect our layer tree
  // id, CompositorBridge, and the widget compositor all together again.
  // Normally this happens in TabParent before TabChild is given rendering
  // information.
  //
  // In this case, we will send a sync message to our TabParent, which in turn
  // will send a sync message to the Compositor of the widget owning this tab.
  // This guarantees the correct association is in place before our
  // PLayerTransaction constructor message arrives on the cross-process
  // compositor bridge.
  CompositorOptions options;
  SendEnsureLayersConnected(&options);
  mCompositorOptions = Some(options);

  RefPtr<CompositorBridgeChild> cb = CompositorBridgeChild::Get();
  if (gfxVars::UseWebRender()) {
    RefPtr<LayerManager> lm = mPuppetWidget->RecreateLayerManager(nullptr);
    MOZ_ASSERT(lm->AsWebRenderLayerManager());
    lm->AsWebRenderLayerManager()->Initialize(cb,
                                              wr::AsPipelineId(mLayersId),
                                              &mTextureFactoryIdentifier);
  } else {
    bool success = false;
    nsTArray<LayersBackend> ignored;
    PLayerTransactionChild* shadowManager = cb->SendPLayerTransactionConstructor(ignored, LayersId());
    if (shadowManager &&
        shadowManager->SendGetTextureFactoryIdentifier(&mTextureFactoryIdentifier) &&
        mTextureFactoryIdentifier.mParentBackend != LayersBackend::LAYERS_NONE)
    {
      success = true;
    }
    if (!success) {
      NS_WARNING("failed to re-allocate layer transaction");
      return;
    }

    RefPtr<LayerManager> lm = mPuppetWidget->RecreateLayerManager(shadowManager);
    ShadowLayerForwarder* lf = lm->AsShadowForwarder();
    lf->IdentifyTextureHost(mTextureFactoryIdentifier);
  }

  ImageBridgeChild::IdentifyCompositorTextureHost(mTextureFactoryIdentifier);
  gfx::VRManagerChild::IdentifyTextureHost(mTextureFactoryIdentifier);

  InitAPZState();

  nsCOMPtr<nsIDocument> doc(GetDocument());
  doc->NotifyLayerManagerRecreated();
}

void
TabChild::ReinitRenderingForDeviceReset()
{
  InvalidateLayers();

  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();
  ClientLayerManager* clm = lm->AsClientLayerManager();
  if (!clm) {
    return;
  }

  if (ShadowLayerForwarder* fwd = clm->AsShadowForwarder()) {
    // Force the LayerTransactionChild to synchronously shutdown. It is
    // okay to do this early, we'll simply stop sending messages. This
    // step is necessary since otherwise the compositor will think we
    // are trying to attach two layer trees to the same ID.
    fwd->SynchronouslyShutdown();
  }

  // Proceed with destroying and recreating the layer manager.
  ReinitRendering();
}

void
TabChild::CompositorUpdated(const TextureFactoryIdentifier& aNewIdentifier,
                            uint64_t aDeviceResetSeqNo)
{
  MOZ_ASSERT(mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT
             || mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_WR
             || (gfxPlatform::IsHeadless() && mPuppetWidget->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC));

  RefPtr<LayerManager> lm = mPuppetWidget->GetLayerManager();

  mTextureFactoryIdentifier = aNewIdentifier;
  lm->UpdateTextureFactoryIdentifier(aNewIdentifier, aDeviceResetSeqNo);
  FrameLayerBuilder::InvalidateAllLayers(lm);
}

NS_IMETHODIMP
TabChild::OnShowTooltip(int32_t aXCoords, int32_t aYCoords, const char16_t *aTipText,
                        const char16_t *aTipDir)
{
    nsString str(aTipText);
    nsString dir(aTipDir);
    SendShowTooltip(aXCoords, aYCoords, str, dir);
    return NS_OK;
}

NS_IMETHODIMP
TabChild::OnHideTooltip()
{
    SendHideTooltip();
    return NS_OK;
}

mozilla::ipc::IPCResult
TabChild::RecvRequestNotifyAfterRemotePaint()
{
  // Get the CompositorBridgeChild instance for this content thread.
  CompositorBridgeChild* compositor = CompositorBridgeChild::Get();

  // Tell the CompositorBridgeChild that, when it gets a RemotePaintIsReady
  // message that it should forward it us so that we can bounce it to our
  // RenderFrameParent.
  compositor->RequestNotifyAfterRemotePaint(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvUIResolutionChanged(const float& aDpi,
                                  const int32_t& aRounding,
                                  const double& aScale)
{
  ScreenIntSize oldScreenSize = GetInnerSize();
  mDPI = 0;
  mRounding = 0;
  mDefaultScale = 0;
  static_cast<PuppetWidget*>(mPuppetWidget.get())->UpdateBackingScaleCache(aDpi, aRounding, aScale);
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (presShell) {
    RefPtr<nsPresContext> presContext = presShell->GetPresContext();
    if (presContext) {
      presContext->UIResolutionChangedSync();
    }
  }

  ScreenIntSize screenSize = GetInnerSize();
  if (mHasValidInnerSize && oldScreenSize != screenSize) {
    ScreenIntRect screenRect = GetOuterRect();
    mPuppetWidget->Resize(screenRect.x + mClientOffset.x + mChromeDisp.x,
                          screenRect.y + mClientOffset.y + mChromeDisp.y,
                          screenSize.width, screenSize.height, true);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(WebNavigation());
    baseWin->SetPositionAndSize(0, 0, screenSize.width, screenSize.height,
                                nsIBaseWindow::eRepaint);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvThemeChanged(nsTArray<LookAndFeelInt>&& aLookAndFeelIntCache)
{
  LookAndFeel::SetIntCache(aLookAndFeelIntCache);
  nsCOMPtr<nsIDocument> document(GetDocument());
  nsCOMPtr<nsIPresShell> presShell = document->GetShell();
  if (presShell) {
    RefPtr<nsPresContext> presContext = presShell->GetPresContext();
    if (presContext) {
      presContext->ThemeChanged();
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvAwaitLargeAlloc()
{
  mAwaitingLA = true;
  return IPC_OK();
}

bool
TabChild::IsAwaitingLargeAlloc()
{
  return mAwaitingLA;
}

bool
TabChild::StopAwaitingLargeAlloc()
{
  bool awaiting = mAwaitingLA;
  mAwaitingLA = false;
  return awaiting;
}

mozilla::ipc::IPCResult
TabChild::RecvSetWindowName(const nsString& aName)
{
  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(WebNavigation());
  if (item) {
    item->SetName(aName);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabChild::RecvSetOriginAttributes(const OriginAttributes& aOriginAttributes)
{
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(WebNavigation());
  nsDocShell::Cast(docShell)->SetOriginAttributes(aOriginAttributes);

  return IPC_OK();
}

mozilla::plugins::PPluginWidgetChild*
TabChild::AllocPPluginWidgetChild()
{
#ifdef XP_WIN
  return new mozilla::plugins::PluginWidgetChild();
#else
  MOZ_ASSERT_UNREACHABLE();
  return nullptr;
#endif
}

bool
TabChild::DeallocPPluginWidgetChild(mozilla::plugins::PPluginWidgetChild* aActor)
{
  delete aActor;
  return true;
}

#ifdef XP_WIN
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
  nsresult rv = pluginWidget->Create(aParent, nullptr,
                                     LayoutDeviceIntRect(0, 0, 0, 0),
                                     &initData);
  if (NS_FAILED(rv)) {
    NS_WARNING("Creating native plugin widget on the chrome side failed.");
  }
  pluginWidget.forget(aOut);
  return rv;
}
#endif // XP_WIN

PPaymentRequestChild*
TabChild::AllocPPaymentRequestChild()
{
  MOZ_CRASH("We should never be manually allocating PPaymentRequestChild actors");
  return nullptr;
}

bool
TabChild::DeallocPPaymentRequestChild(PPaymentRequestChild* actor)
{
  return true;
}

ScreenIntSize
TabChild::GetInnerSize()
{
  LayoutDeviceIntSize innerSize =
    RoundedToInt(mUnscaledInnerSize * mPuppetWidget->GetDefaultScale());
  return ViewAs<ScreenPixel>(innerSize, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
};

ScreenIntRect
TabChild::GetOuterRect()
{
  LayoutDeviceIntRect outerRect =
    RoundedToInt(mUnscaledOuterRect * mPuppetWidget->GetDefaultScale());
  return ViewAs<ScreenPixel>(outerRect, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
}

void
TabChild::ForcePaint(uint64_t aLayerObserverEpoch)
{
  if (!IPCOpen()) {
    // Don't bother doing anything now. Better to wait until we receive the
    // message on the PContent channel.
    return;
  }

  nsAutoScriptBlocker scriptBlocker;
  RecvSetDocShellIsActive(true, false, aLayerObserverEpoch);
}

void
TabChild::BeforeUnloadAdded()
{
  // Don't bother notifying the parent if we don't have an IPC link open.
  if (mBeforeUnloadListeners == 0 && IPCOpen()) {
    SendSetHasBeforeUnload(true);
  }

  mBeforeUnloadListeners++;
  MOZ_ASSERT(mBeforeUnloadListeners >= 0);
}

void
TabChild::BeforeUnloadRemoved()
{
  mBeforeUnloadListeners--;
  MOZ_ASSERT(mBeforeUnloadListeners >= 0);

  // Don't bother notifying the parent if we don't have an IPC link open.
  if (mBeforeUnloadListeners == 0 && IPCOpen()) {
    SendSetHasBeforeUnload(false);
  }
}

already_AddRefed<nsISHistory>
TabChild::GetRelatedSHistory()
{
  nsCOMPtr<nsISHistory> shistory;
  mWebNav->GetSessionHistory(getter_AddRefs(shistory));
  return shistory.forget();
}

nsresult
TabChildSHistoryListener::SHistoryDidUpdate(bool aTruncate /* = false */)
{
  RefPtr<TabChild> tabChild(mTabChild);
  if (NS_WARN_IF(!tabChild)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISHistory> shistory = tabChild->GetRelatedSHistory();
  NS_ENSURE_TRUE(shistory, NS_ERROR_FAILURE);

  int32_t index, count;
  nsresult rv = shistory->GetIndex(&index);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = shistory->GetCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX: It would be nice if we could batch these updates like SessionStore
  // does, and provide a form of `Flush` command which would allow us to trigger
  // an update, and wait for the state to become consistent.
  NS_ENSURE_TRUE(tabChild->SendSHistoryUpdate(count, index, aTruncate), NS_ERROR_FAILURE);
  return NS_OK;
}

mozilla::dom::TabGroup*
TabChild::TabGroup()
{
  return mTabGroup;
}

/*******************************************************************************
 * nsISHistoryListener
 ******************************************************************************/

NS_IMETHODIMP
TabChildSHistoryListener::OnHistoryNewEntry(nsIURI *aNewURI, int32_t aOldIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChildSHistoryListener::OnHistoryGoBack(nsIURI *aBackURI, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChildSHistoryListener::OnHistoryGoForward(nsIURI *aForwardURI, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChildSHistoryListener::OnHistoryReload(nsIURI *aReloadURI, uint32_t aReloadFlags, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChildSHistoryListener::OnHistoryGotoIndex(int32_t aIndex, nsIURI *aGotoURI, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChildSHistoryListener::OnHistoryPurge(int32_t aNumEntries, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChildSHistoryListener::OnHistoryReplaceEntry(int32_t aIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChildSHistoryListener::OnLengthChanged(int32_t aCount)
{
  return SHistoryDidUpdate(/* aTruncate = */ true);
}

NS_IMETHODIMP
TabChildSHistoryListener::OnIndexChanged(int32_t aIndex)
{
  return SHistoryDidUpdate(/* aTruncate = */ false);
}

NS_IMETHODIMP
TabChildSHistoryListener::OnRequestCrossBrowserNavigation(uint32_t aIndex)
{
  RefPtr<TabChild> tabChild(mTabChild);
  if (!tabChild) {
    return NS_ERROR_FAILURE;
  }

  return tabChild->SendRequestCrossBrowserNavigation(aIndex) ?
           NS_OK : NS_ERROR_FAILURE;
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

  TelemetryScrollProbe::Create(this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TabChildGlobal)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TabChildGlobal,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTabChild);
  tmp->UnlinkHostObjectURIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TabChildGlobal,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTabChild)
  tmp->TraverseHostObjectURIs(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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

// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
TabChildGlobal::MarkForCC()
{
  if (mTabChild) {
    mTabChild->MarkScopesForCC();
  }
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->MarkForCC();
  }
  return mMessageManager ? mMessageManager->MarkForCC() : false;
}

NS_IMETHODIMP
TabChildGlobal::GetContent(mozIDOMWindowProxy** aContent)
{
  *aContent = nullptr;
  if (!mTabChild)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsPIDOMWindowOuter> window = do_GetInterface(mTabChild->WebNavigation());
  window.forget(aContent);
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
  return mTabChild->GetGlobal();
}
