/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BrowserParent.h"

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/DocAccessibleParent.h"
#  include "mozilla/a11y/Platform.h"
#  include "mozilla/a11y/RemoteAccessibleBase.h"
#  include "nsAccessibilityService.h"
#endif
#include "mozilla/Components.h"
#include "mozilla/dom/BrowserHost.h"
#include "mozilla/dom/BrowserSessionStore.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/CancelContentJSOptionsBinding.h"
#include "mozilla/dom/ChromeMessageSender.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DataTransferItemList.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/PaymentRequestParent.h"
#include "mozilla/dom/PointerEventHandler.h"
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/RemoteDragStartData.h"
#include "mozilla/dom/RemoteWebProgressRequest.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "mozilla/dom/SessionStoreParent.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/AsyncDragMetrics.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layout/RemoteLayerTreeOwner.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/NativeKeyBindingsType.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsFrameManager.h"
#include "nsIBaseWindow.h"
#include "nsIBrowser.h"
#include "nsIBrowserController.h"
#include "nsIContent.h"
#include "nsICookieJarSettings.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsImportModule.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadInfo.h"
#include "nsIPromptFactory.h"
#include "nsIURI.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebProtocolHandlerRegistrar.h"
#include "nsIWindowWatcher.h"
#include "nsIXPConnect.h"
#include "nsIXULBrowserWindow.h"
#include "nsIAppWindow.h"
#include "nsLayoutUtils.h"
#include "nsQueryActor.h"
#include "nsSHistory.h"
#include "nsViewManager.h"
#include "nsVariant.h"
#include "nsIWidget.h"
#include "nsNetUtil.h"
#ifndef XP_WIN
#  include "nsJARProtocolHandler.h"
#endif
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsQueryObject.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "PermissionMessageUtils.h"
#include "StructuredCloneData.h"
#include "ColorPickerParent.h"
#include "FilePickerParent.h"
#include "BrowserChild.h"
#include "nsNetCID.h"
#include "nsIAuthInformation.h"
#include "nsIAuthPromptCallback.h"
#include "nsAuthInformationHolder.h"
#include "nsICancelable.h"
#include "gfxUtils.h"
#include "nsILoginManagerAuthPrompter.h"
#include "nsPIWindowRoot.h"
#include "nsReadableUtils.h"
#include "nsIAuthPrompt2.h"
#include "gfxDrawable.h"
#include "ImageOps.h"
#include "UnitTransforms.h"
#include <algorithm>
#include "mozilla/NullPrincipal.h"
#include "mozilla/WebBrowserPersistDocumentParent.h"
#include "ProcessPriorityManager.h"
#include "nsString.h"
#include "IHistory.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/ProfilerLabels.h"
#include "MMPrinter.h"
#include "mozilla/dom/CrashReport.h"
#include "nsISecureBrowserUI.h"
#include "nsIXULRuntime.h"
#include "VsyncSource.h"
#include "nsSubDocumentFrame.h"

#ifdef XP_WIN
#  include "FxRWindowManager.h"
#endif

#if defined(XP_WIN) && defined(ACCESSIBILITY)
#  include "mozilla/a11y/AccessibleWrap.h"
#  include "mozilla/a11y/Compatibility.h"
#  include "mozilla/a11y/nsWinUtils.h"
#endif

#ifdef MOZ_ANDROID_HISTORY
#  include "GeckoViewHistory.h"
#endif

#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/widget/nsWindow.h"
#endif  // defined(MOZ_WIDGET_ANDROID)

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::services;
using namespace mozilla::widget;
using namespace mozilla::gfx;

using mozilla::LazyLogModule;

extern mozilla::LazyLogModule gSHIPBFCacheLog;

LazyLogModule gBrowserFocusLog("BrowserFocus");

#define LOGBROWSERFOCUS(args) \
  MOZ_LOG(gBrowserFocusLog, mozilla::LogLevel::Debug, args)

/* static */
BrowserParent* BrowserParent::sFocus = nullptr;
/* static */
BrowserParent* BrowserParent::sTopLevelWebFocus = nullptr;
/* static */
BrowserParent* BrowserParent::sLastMouseRemoteTarget = nullptr;

// The flags passed by the webProgress notifications are 16 bits shifted
// from the ones registered by webProgressListeners.
#define NOTIFY_FLAG_SHIFT 16

namespace mozilla::dom {

BrowserParent::LayerToBrowserParentTable*
    BrowserParent::sLayerToBrowserParentTable = nullptr;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowserParent)
  NS_INTERFACE_MAP_ENTRY(nsIAuthPromptProvider)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION_WEAK(BrowserParent, mFrameLoader, mBrowsingContext)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BrowserParent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BrowserParent)

BrowserParent::BrowserParent(ContentParent* aManager, const TabId& aTabId,
                             const TabContext& aContext,
                             CanonicalBrowsingContext* aBrowsingContext,
                             uint32_t aChromeFlags)
    : TabContext(aContext),
      mTabId(aTabId),
      mManager(aManager),
      mBrowsingContext(aBrowsingContext),
      mFrameElement(nullptr),
      mBrowserDOMWindow(nullptr),
      mFrameLoader(nullptr),
      mChromeFlags(aChromeFlags),
      mBrowserBridgeParent(nullptr),
      mBrowserHost(nullptr),
      mContentCache(*this),
      mRemoteLayerTreeOwner{},
      mLayerTreeEpoch{1},
      mChildToParentConversionMatrix{},
      mRect(0, 0, 0, 0),
      mDimensions(0, 0),
      mDPI(0),
      mRounding(0),
      mDefaultScale(0),
      mUpdatedDimensions(false),
      mSizeMode(nsSizeMode_Normal),
      mClientOffset{},
      mChromeOffset{},
      mCreatingWindow(false),
      mDelayedFrameScripts{},
      mVsyncParent(nullptr),
      mMarkedDestroying(false),
      mIsDestroyed(false),
      mRemoteTargetSetsCursor(false),
      mIsPreservingLayers(false),
      mRenderLayers(true),
      mPriorityHint(false),
      mHasLayers(false),
      mHasPresented(false),
      mIsReadyToHandleInputEvents(false),
      mIsMouseEnterIntoWidgetEventSuppressed(false),
      mLockedNativePointer(false),
      mShowingTooltip(false) {
  MOZ_ASSERT(aManager);
  // When the input event queue is disabled, we don't need to handle the case
  // that some input events are dispatched before PBrowserConstructor.
  mIsReadyToHandleInputEvents = !ContentParent::IsInputEventQueueSupported();

  // Make sure to compute our process priority if needed before the block of
  // code below. This makes sure the block below prioritizes our process if
  // needed.
  if (aBrowsingContext->IsTop()) {
    RecomputeProcessPriority();
  }

  // If we're in a BC tree that is active with respect to the priority manager,
  // ensure that this new BrowserParent is marked as active. This ensures that
  // the process will be prioritized in a cross-site iframe navigation in an
  // active tab, and also that the process is correctly prioritized if we got
  // created for a browsing context which was already active.
  if (aBrowsingContext->Top()->IsPriorityActive()) {
    ProcessPriorityManager::BrowserPriorityChanged(this, true);
  }
}

BrowserParent::~BrowserParent() = default;

/* static */
BrowserParent* BrowserParent::GetFocused() { return sFocus; }

/* static */
BrowserParent* BrowserParent::GetLastMouseRemoteTarget() {
  return sLastMouseRemoteTarget;
}

/*static*/
BrowserParent* BrowserParent::GetFrom(nsFrameLoader* aFrameLoader) {
  if (!aFrameLoader) {
    return nullptr;
  }
  return aFrameLoader->GetBrowserParent();
}

/*static*/
BrowserParent* BrowserParent::GetFrom(PBrowserParent* aBrowserParent) {
  return static_cast<BrowserParent*>(aBrowserParent);
}

/*static*/
BrowserParent* BrowserParent::GetFrom(nsIContent* aContent) {
  RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(aContent);
  if (!loaderOwner) {
    return nullptr;
  }
  RefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
  return GetFrom(frameLoader);
}

/* static */
BrowserParent* BrowserParent::GetBrowserParentFromLayersId(
    layers::LayersId aLayersId) {
  if (!sLayerToBrowserParentTable) {
    return nullptr;
  }
  return sLayerToBrowserParentTable->Get(uint64_t(aLayersId));
}

/*static*/
TabId BrowserParent::GetTabIdFrom(nsIDocShell* docShell) {
  nsCOMPtr<nsIBrowserChild> browserChild(BrowserChild::GetFrom(docShell));
  if (browserChild) {
    return static_cast<BrowserChild*>(browserChild.get())->GetTabId();
  }
  return TabId(0);
}

void BrowserParent::AddBrowserParentToTable(layers::LayersId aLayersId,
                                            BrowserParent* aBrowserParent) {
  if (!sLayerToBrowserParentTable) {
    sLayerToBrowserParentTable = new LayerToBrowserParentTable();
  }
  sLayerToBrowserParentTable->InsertOrUpdate(uint64_t(aLayersId),
                                             aBrowserParent);
}

void BrowserParent::RemoveBrowserParentFromTable(layers::LayersId aLayersId) {
  if (!sLayerToBrowserParentTable) {
    return;
  }
  sLayerToBrowserParentTable->Remove(uint64_t(aLayersId));
  if (sLayerToBrowserParentTable->Count() == 0) {
    delete sLayerToBrowserParentTable;
    sLayerToBrowserParentTable = nullptr;
  }
}

already_AddRefed<nsILoadContext> BrowserParent::GetLoadContext() {
  return do_AddRef(mBrowsingContext);
}

/**
 * Will return nullptr if there is no outer window available for the
 * document hosting the owner element of this BrowserParent. Also will return
 * nullptr if that outer window is in the process of closing.
 */
already_AddRefed<nsPIDOMWindowOuter> BrowserParent::GetParentWindowOuter() {
  nsCOMPtr<nsIContent> frame = GetOwnerElement();
  if (!frame) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parent = frame->OwnerDoc()->GetWindow();
  if (!parent || parent->Closed()) {
    return nullptr;
  }

  return parent.forget();
}

already_AddRefed<nsIWidget> BrowserParent::GetTopLevelWidget() {
  if (RefPtr<Element> element = mFrameElement) {
    if (PresShell* presShell = element->OwnerDoc()->GetPresShell()) {
      return presShell->GetViewManager()->GetRootWidget();
    }
  }
  return nullptr;
}

already_AddRefed<nsIWidget> BrowserParent::GetTextInputHandlingWidget() const {
  if (!mFrameElement) {
    return nullptr;
  }
  PresShell* presShell = mFrameElement->OwnerDoc()->GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  nsPresContext* presContext = presShell->GetPresContext();
  if (!presContext) {
    return nullptr;
  }
  nsCOMPtr<nsIWidget> widget = presContext->GetTextInputHandlingWidget();
  return widget.forget();
}

already_AddRefed<nsIWidget> BrowserParent::GetWidget() const {
  if (!mFrameElement) {
    return nullptr;
  }
  nsCOMPtr<nsIWidget> widget = nsContentUtils::WidgetForContent(mFrameElement);
  if (!widget) {
    widget = nsContentUtils::WidgetForDocument(mFrameElement->OwnerDoc());
  }
  return widget.forget();
}

already_AddRefed<nsIWidget> BrowserParent::GetDocWidget() const {
  if (!mFrameElement) {
    return nullptr;
  }
  return do_AddRef(
      nsContentUtils::WidgetForDocument(mFrameElement->OwnerDoc()));
}

nsIXULBrowserWindow* BrowserParent::GetXULBrowserWindow() {
  if (!mFrameElement) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = mFrameElement->OwnerDoc()->GetDocShell();
  if (!docShell) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShell->GetTreeOwner(getter_AddRefs(treeOwner));
  if (!treeOwner) {
    return nullptr;
  }

  nsCOMPtr<nsIAppWindow> window = do_GetInterface(treeOwner);
  if (!window) {
    return nullptr;
  }

  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow;
  window->GetXULBrowserWindow(getter_AddRefs(xulBrowserWindow));
  return xulBrowserWindow;
}

uint32_t BrowserParent::GetMaxTouchPoints(Element* aElement) {
  if (!aElement) {
    return 0;
  }

  if (StaticPrefs::dom_maxtouchpoints_testing_value() >= 0) {
    return StaticPrefs::dom_maxtouchpoints_testing_value();
  }

  nsIWidget* widget = nsContentUtils::WidgetForDocument(aElement->OwnerDoc());
  return widget ? widget->GetMaxTouchPoints() : 0;
}

a11y::DocAccessibleParent* BrowserParent::GetTopLevelDocAccessible() const {
#ifdef ACCESSIBILITY
  // XXX Consider managing non top level PDocAccessibles with their parent
  // document accessible.
  const ManagedContainer<PDocAccessibleParent>& docs =
      ManagedPDocAccessibleParent();
  for (auto* key : docs) {
    auto* doc = static_cast<a11y::DocAccessibleParent*>(key);
    // We want the document for this BrowserParent even if it's for an
    // embedded out-of-process iframe. Therefore, we use
    // IsTopLevelInContentProcess. In contrast, using IsToplevel would only
    // include documents that aren't embedded; e.g. tab documents.
    if (doc->IsTopLevelInContentProcess() && !doc->IsShutdown()) {
      return doc;
    }
  }
#endif
  return nullptr;
}

LayersId BrowserParent::GetLayersId() const {
  if (!mRemoteLayerTreeOwner.IsInitialized()) {
    return LayersId{};
  }
  return mRemoteLayerTreeOwner.GetLayersId();
}

BrowserBridgeParent* BrowserParent::GetBrowserBridgeParent() const {
  return mBrowserBridgeParent;
}

BrowserHost* BrowserParent::GetBrowserHost() const { return mBrowserHost; }

ParentShowInfo BrowserParent::GetShowInfo() {
  TryCacheDPIAndScale();
  if (mFrameElement) {
    nsAutoString name;
    mFrameElement->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
    bool isTransparent =
        nsContentUtils::IsChromeDoc(mFrameElement->OwnerDoc()) &&
        mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::transparent);
    return ParentShowInfo(name, false, isTransparent, mDPI, mRounding,
                          mDefaultScale.scale);
  }

  return ParentShowInfo(u""_ns, false, false, mDPI, mRounding,
                        mDefaultScale.scale);
}

already_AddRefed<nsIPrincipal> BrowserParent::GetContentPrincipal() const {
  nsCOMPtr<nsIBrowser> browser =
      mFrameElement ? mFrameElement->AsBrowser() : nullptr;
  NS_ENSURE_TRUE(browser, nullptr);

  RefPtr<nsIPrincipal> principal;

  nsresult rv;
  rv = browser->GetContentPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return principal.forget();
}

void BrowserParent::SetOwnerElement(Element* aElement) {
  // If we held previous content then unregister for its events.
  RemoveWindowListeners();

  // If we change top-level documents then we need to change our
  // registration with them.
  RefPtr<nsPIWindowRoot> curTopLevelWin, newTopLevelWin;
  if (mFrameElement) {
    curTopLevelWin = nsContentUtils::GetWindowRoot(mFrameElement->OwnerDoc());
  }
  if (aElement) {
    newTopLevelWin = nsContentUtils::GetWindowRoot(aElement->OwnerDoc());
  }
  bool isSameTopLevelWin = curTopLevelWin == newTopLevelWin;
  if (mBrowserHost && curTopLevelWin && !isSameTopLevelWin) {
    curTopLevelWin->RemoveBrowser(mBrowserHost);
  }

  // Update to the new content, and register to listen for events from it.
  mFrameElement = aElement;

  if (mBrowserHost && newTopLevelWin && !isSameTopLevelWin) {
    newTopLevelWin->AddBrowser(mBrowserHost);
  }

#if defined(XP_WIN) && defined(ACCESSIBILITY)
  if (!mIsDestroyed) {
    uintptr_t newWindowHandle = 0;
    if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
      newWindowHandle =
          reinterpret_cast<uintptr_t>(widget->GetNativeData(NS_NATIVE_WINDOW));
    }
    Unused << SendUpdateNativeWindowHandle(newWindowHandle);
    a11y::DocAccessibleParent* doc = GetTopLevelDocAccessible();
    if (doc) {
      HWND hWnd = reinterpret_cast<HWND>(doc->GetEmulatedWindowHandle());
      if (hWnd) {
        HWND parentHwnd = reinterpret_cast<HWND>(newWindowHandle);
        if (parentHwnd != ::GetParent(hWnd)) {
          ::SetParent(hWnd, parentHwnd);
        }
      }
    }
  }
#endif

  AddWindowListeners();
  TryCacheDPIAndScale();

  if (mRemoteLayerTreeOwner.IsInitialized()) {
    mRemoteLayerTreeOwner.OwnerContentChanged();
  }

  // Set our BrowsingContext's embedder if we're not embedded within a
  // BrowserBridgeParent.
  if (!GetBrowserBridgeParent() && mBrowsingContext && mFrameElement) {
    mBrowsingContext->SetEmbedderElement(mFrameElement);
  }

  UpdateVsyncParentVsyncDispatcher();

  VisitChildren([aElement](BrowserBridgeParent* aBrowser) {
    if (auto* browserParent = aBrowser->GetBrowserParent()) {
      browserParent->SetOwnerElement(aElement);
    }
  });
}

void BrowserParent::CacheFrameLoader(nsFrameLoader* aFrameLoader) {
  mFrameLoader = aFrameLoader;
}

void BrowserParent::AddWindowListeners() {
  if (mFrameElement) {
    if (nsCOMPtr<nsPIDOMWindowOuter> window =
            mFrameElement->OwnerDoc()->GetWindow()) {
      nsCOMPtr<EventTarget> eventTarget = window->GetTopWindowRoot();
      if (eventTarget) {
        eventTarget->AddEventListener(u"MozUpdateWindowPos"_ns, this, false,
                                      false);
        eventTarget->AddEventListener(u"fullscreenchange"_ns, this, false,
                                      false);
      }
    }
  }
}

void BrowserParent::RemoveWindowListeners() {
  if (mFrameElement && mFrameElement->OwnerDoc()->GetWindow()) {
    nsCOMPtr<nsPIDOMWindowOuter> window =
        mFrameElement->OwnerDoc()->GetWindow();
    nsCOMPtr<EventTarget> eventTarget = window->GetTopWindowRoot();
    if (eventTarget) {
      eventTarget->RemoveEventListener(u"MozUpdateWindowPos"_ns, this, false);
      eventTarget->RemoveEventListener(u"fullscreenchange"_ns, this, false);
    }
  }
}

void BrowserParent::Deactivated() {
  if (mShowingTooltip) {
    // Reuse the normal tooltip hiding method.
    mozilla::Unused << RecvHideTooltip();
  }
  UnlockNativePointer();
  UnsetTopLevelWebFocus(this);
  UnsetLastMouseRemoteTarget(this);
  PointerLockManager::ReleaseLockedRemoteTarget(this);
  PointerEventHandler::ReleasePointerCaptureRemoteTarget(this);
  PresShell::ReleaseCapturingRemoteTarget(this);
  ProcessPriorityManager::BrowserPriorityChanged(this, /* aPriority = */ false);
}

void BrowserParent::DestroyInternal() {
  Deactivated();

  RemoveWindowListeners();

#ifdef ACCESSIBILITY
  if (a11y::DocAccessibleParent* tabDoc = GetTopLevelDocAccessible()) {
#  if defined(ANDROID)
    MonitorAutoLock mal(nsAccessibilityService::GetAndroidMonitor());
#  endif
    tabDoc->Destroy();
  }
#endif

  // If this fails, it's most likely due to a content-process crash,
  // and auto-cleanup will kick in.  Otherwise, the child side will
  // destroy itself and send back __delete__().
  Unused << SendDestroy();
}

void BrowserParent::Destroy() {
  // Aggressively release the window to avoid leaking the world in shutdown
  // corner cases.
  mBrowserDOMWindow = nullptr;

  if (mIsDestroyed) {
    return;
  }

  DestroyInternal();

  mIsDestroyed = true;

  Manager()->NotifyTabDestroying();

  // This `AddKeepAlive` will be cleared if `mMarkedDestroying` is set in
  // `ActorDestroy`. Out of caution, we don't add the `KeepAlive` if our IPC
  // actor has somehow already been destroyed, as that would mean `ActorDestroy`
  // won't be called.
  if (CanRecv()) {
    mBrowsingContext->Group()->AddKeepAlive();
  }

  mMarkedDestroying = true;
}

mozilla::ipc::IPCResult BrowserParent::RecvDidUnsuppressPainting() {
  if (!mFrameElement) {
    return IPC_OK();
  }
  nsSubDocumentFrame* subdocFrame =
      do_QueryFrame(mFrameElement->GetPrimaryFrame());
  if (subdocFrame && subdocFrame->HasRetainedPaintData()) {
    subdocFrame->ClearRetainedPaintData();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvEnsureLayersConnected(
    CompositorOptions* aCompositorOptions) {
  if (mRemoteLayerTreeOwner.IsInitialized()) {
    mRemoteLayerTreeOwner.EnsureLayersConnected(aCompositorOptions);
  }
  return IPC_OK();
}

void BrowserParent::ActorDestroy(ActorDestroyReason why) {
  Manager()->NotifyTabDestroyed(mTabId, mMarkedDestroying);

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  if (cpm) {
    cpm->UnregisterRemoteFrame(mTabId);
  }

  if (mRemoteLayerTreeOwner.IsInitialized()) {
    auto layersId = mRemoteLayerTreeOwner.GetLayersId();
    if (mFrameElement) {
      nsSubDocumentFrame* f = do_QueryFrame(mFrameElement->GetPrimaryFrame());
      if (f && f->HasRetainedPaintData() &&
          f->GetRemotePaintData().mLayersId == layersId) {
        f->ClearRetainedPaintData();
      }
    }

    // It's important to unmap layers after the remote browser has been
    // destroyed, otherwise it may still send messages to the compositor which
    // will reject them, causing assertions.
    RemoveBrowserParentFromTable(layersId);
    mRemoteLayerTreeOwner.Destroy();
  }

  // Even though BrowserParent::Destroy calls this, we need to do it here too in
  // case of a crash.
  Deactivated();

  if (why == AbnormalShutdown) {
    // dom_reporting_header must also be enabled for the report to be sent.
    if (StaticPrefs::dom_reporting_crash_enabled()) {
      nsCOMPtr<nsIPrincipal> principal = GetContentPrincipal();

      if (principal) {
        nsAutoCString crash_reason;
        CrashReporter::GetAnnotation(OtherPid(),
                                     CrashReporter::Annotation::MozCrashReason,
                                     crash_reason);
        // FIXME(arenevier): Find a less fragile way to identify that a crash
        // was caused by OOM
        bool is_oom = false;
        if (crash_reason == "OOM" || crash_reason == "OOM!" ||
            StringBeginsWith(crash_reason, "[unhandlable oom]"_ns) ||
            StringBeginsWith(crash_reason, "Unhandlable OOM"_ns)) {
          is_oom = true;
        }

        CrashReport::Deliver(principal, is_oom);
      }
    }
  }

  // If we were shutting down normally, we held a reference to our
  // BrowsingContextGroup in `BrowserParent::Destroy`. Clear that reference
  // here.
  if (mMarkedDestroying) {
    mBrowsingContext->Group()->RemoveKeepAlive();
  }

  // Tell our embedder that the tab is now going away unless we're an
  // out-of-process iframe.
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader(true);
  if (frameLoader) {
    ReceiveMessage(CHILD_PROCESS_SHUTDOWN_MESSAGE, false, nullptr);

    if (mBrowsingContext->IsTop()) {
      // If this is a top-level BrowsingContext, tell the frameloader it's time
      // to go away. Otherwise, this is a subframe crash, and we can keep the
      // frameloader around.
      frameLoader->DestroyComplete();
    }

    // If this was a crash, tell our nsFrameLoader to fire crash events.
    if (why == AbnormalShutdown) {
      frameLoader->MaybeNotifyCrashed(mBrowsingContext, Manager()->ChildID(),
                                      GetIPCChannel());
    } else if (why == ManagedEndpointDropped) {
      // If we instead failed due to a constructor error, don't include process
      // information, as the process did not crash.
      frameLoader->MaybeNotifyCrashed(mBrowsingContext, ContentParentId{},
                                      nullptr);
    }
  }

  mFrameLoader = nullptr;

  // If we were destroyed due to our ManagedEndpoints being dropped, make a
  // point of showing the subframe crashed UI. We don't fire the full
  // `MaybeNotifyCrashed` codepath, as the entire process hasn't crashed on us,
  // and it may confuse the frontend.
  mBrowsingContext->BrowserParentDestroyed(
      this, why == AbnormalShutdown || why == ManagedEndpointDropped);
}

mozilla::ipc::IPCResult BrowserParent::RecvMoveFocus(
    const bool& aForward, const bool& aForDocumentNavigation) {
  LOGBROWSERFOCUS(("RecvMoveFocus %p, aForward: %d, aForDocumentNavigation: %d",
                   this, aForward, aForDocumentNavigation));
  BrowserBridgeParent* bridgeParent = GetBrowserBridgeParent();
  if (bridgeParent) {
    mozilla::Unused << bridgeParent->SendMoveFocus(aForward,
                                                   aForDocumentNavigation);
    return IPC_OK();
  }

  RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager();
  if (fm) {
    RefPtr<Element> dummy;

    uint32_t type =
        aForward
            ? (aForDocumentNavigation
                   ? static_cast<uint32_t>(
                         nsIFocusManager::MOVEFOCUS_FORWARDDOC)
                   : static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARD))
            : (aForDocumentNavigation
                   ? static_cast<uint32_t>(
                         nsIFocusManager::MOVEFOCUS_BACKWARDDOC)
                   : static_cast<uint32_t>(
                         nsIFocusManager::MOVEFOCUS_BACKWARD));
    fm->MoveFocus(nullptr, mFrameElement, type, nsIFocusManager::FLAG_BYKEY,
                  getter_AddRefs(dummy));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSizeShellTo(
    const uint32_t& aFlags, const int32_t& aWidth, const int32_t& aHeight,
    const int32_t& aShellItemWidth, const int32_t& aShellItemHeight) {
  NS_ENSURE_TRUE(mFrameElement, IPC_OK());

  nsCOMPtr<nsIDocShell> docShell = mFrameElement->OwnerDoc()->GetDocShell();
  NS_ENSURE_TRUE(docShell, IPC_OK());

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  nsresult rv = docShell->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  int32_t width = aWidth;
  int32_t height = aHeight;

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX) {
    width = mDimensions.width;
  }

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY) {
    height = mDimensions.height;
  }

  nsCOMPtr<nsIAppWindow> appWin(do_GetInterface(treeOwner));
  NS_ENSURE_TRUE(appWin, IPC_OK());
  appWin->SizeShellToWithLimit(width, height, aShellItemWidth,
                               aShellItemHeight);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvDropLinks(
    nsTArray<nsString>&& aLinks) {
  nsCOMPtr<nsIBrowser> browser =
      mFrameElement ? mFrameElement->AsBrowser() : nullptr;
  if (browser) {
    // Verify that links have not been modified by the child. If links have
    // not been modified then it's safe to load those links using the
    // SystemPrincipal. If they have been modified by web content, then
    // we use a NullPrincipal which still allows to load web links.
    bool loadUsingSystemPrincipal = true;
    if (aLinks.Length() != mVerifyDropLinks.Length()) {
      loadUsingSystemPrincipal = false;
    }
    for (uint32_t i = 0; i < aLinks.Length(); i++) {
      if (loadUsingSystemPrincipal) {
        if (!aLinks[i].Equals(mVerifyDropLinks[i])) {
          loadUsingSystemPrincipal = false;
        }
      }
    }
    mVerifyDropLinks.Clear();
    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    if (loadUsingSystemPrincipal) {
      triggeringPrincipal = nsContentUtils::GetSystemPrincipal();
    } else {
      triggeringPrincipal = NullPrincipal::CreateWithoutOriginAttributes();
    }
    browser->DropLinks(aLinks, triggeringPrincipal);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvEvent(const RemoteDOMEvent& aEvent) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  RefPtr<Event> event = aEvent.mEvent;
  NS_ENSURE_TRUE(event, IPC_OK());

  RefPtr<EventTarget> target = mFrameElement;
  NS_ENSURE_TRUE(target, IPC_OK());

  event->SetOwner(target);

  target->DispatchEvent(*event);
  return IPC_OK();
}

bool BrowserParent::SendLoadRemoteScript(const nsString& aURL,
                                         const bool& aRunInGlobalScope) {
  if (mCreatingWindow) {
    mDelayedFrameScripts.AppendElement(
        FrameScriptInfo(aURL, aRunInGlobalScope));
    return true;
  }

  MOZ_ASSERT(mDelayedFrameScripts.IsEmpty());
  return PBrowserParent::SendLoadRemoteScript(aURL, aRunInGlobalScope);
}

void BrowserParent::LoadURL(nsDocShellLoadState* aLoadState) {
  MOZ_ASSERT(aLoadState);
  MOZ_ASSERT(aLoadState->URI());
  if (mIsDestroyed) {
    return;
  }

  if (mCreatingWindow) {
    // Don't send the message if the child wants to load its own URL.
    return;
  }

  Unused << SendLoadURL(aLoadState, GetShowInfo());
}

void BrowserParent::ResumeLoad(uint64_t aPendingSwitchID) {
  MOZ_ASSERT(aPendingSwitchID != 0);

  if (NS_WARN_IF(mIsDestroyed)) {
    return;
  }

  Unused << SendResumeLoad(aPendingSwitchID, GetShowInfo());
}

void BrowserParent::InitRendering() {
  if (mRemoteLayerTreeOwner.IsInitialized()) {
    return;
  }
  mRemoteLayerTreeOwner.Initialize(this);

  layers::LayersId layersId = mRemoteLayerTreeOwner.GetLayersId();
  AddBrowserParentToTable(layersId, this);

  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (frameLoader) {
    nsIFrame* frame = frameLoader->GetPrimaryFrameOfOwningContent();
    if (frame) {
      frame->InvalidateFrame();
    }
  }

  TextureFactoryIdentifier textureFactoryIdentifier;
  mRemoteLayerTreeOwner.GetTextureFactoryIdentifier(&textureFactoryIdentifier);
  Unused << SendInitRendering(textureFactoryIdentifier, layersId,
                              mRemoteLayerTreeOwner.GetCompositorOptions(),
                              mRemoteLayerTreeOwner.IsLayersConnected());

  RefPtr<nsIWidget> widget = GetTopLevelWidget();
  if (widget) {
    ScreenIntMargin safeAreaInsets = widget->GetSafeAreaInsets();
    Unused << SendSafeAreaInsetsChanged(safeAreaInsets);
  }

#if defined(MOZ_WIDGET_ANDROID)
  MOZ_ASSERT(widget);

  if (GetBrowsingContext()->IsTopContent()) {
    Unused << SendDynamicToolbarMaxHeightChanged(
        widget->GetDynamicToolbarMaxHeight());
  }
#endif
}

bool BrowserParent::AttachWindowRenderer() {
  return mRemoteLayerTreeOwner.AttachWindowRenderer();
}

void BrowserParent::MaybeShowFrame() {
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    return;
  }
  frameLoader->MaybeShowFrame();
}

bool BrowserParent::Show(const OwnerShowInfo& aOwnerInfo) {
  mDimensions = aOwnerInfo.size();
  if (mIsDestroyed) {
    return false;
  }

  MOZ_ASSERT(mRemoteLayerTreeOwner.IsInitialized());
  if (!mRemoteLayerTreeOwner.AttachWindowRenderer()) {
    return false;
  }

  mSizeMode = aOwnerInfo.sizeMode();
  Unused << SendShow(GetShowInfo(), aOwnerInfo);
  return true;
}

mozilla::ipc::IPCResult BrowserParent::RecvSetDimensions(
    const uint32_t& aFlags, const int32_t& aX, const int32_t& aY,
    const int32_t& aCx, const int32_t& aCy, const double& aScale) {
  MOZ_ASSERT(!(aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER),
             "We should never see DIM_FLAGS_SIZE_INNER here!");

  NS_ENSURE_TRUE(mFrameElement, IPC_OK());
  nsCOMPtr<nsIDocShell> docShell = mFrameElement->OwnerDoc()->GetDocShell();
  NS_ENSURE_TRUE(docShell, IPC_OK());
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShell->GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = do_QueryInterface(treeOwner);
  NS_ENSURE_TRUE(treeOwnerAsWin, IPC_OK());

  // We only care about the parameters that actually changed, see more details
  // in `BrowserChild::SetDimensions()`.
  // Note that `BrowserChild::SetDimensions()` may be called before receiving
  // our `SendUIResolutionChanged()` call.  Therefore, if given each cordinate
  // shouldn't be ignored, we need to recompute it if DPI has been changed.
  // And also note that don't use `mDefaultScale.scale` here since it may be
  // different from the result of `GetUnscaledDevicePixelsPerCSSPixel()`.
  double currentScale;
  treeOwnerAsWin->GetUnscaledDevicePixelsPerCSSPixel(&currentScale);

  int32_t x = aX;
  int32_t y = aY;
  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_X) {
      int32_t unused;
      treeOwnerAsWin->GetPosition(&x, &unused);
    } else if (aScale != currentScale) {
      x = x * currentScale / aScale;
    }

    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_Y) {
      int32_t unused;
      treeOwnerAsWin->GetPosition(&unused, &y);
    } else if (aScale != currentScale) {
      y = y * currentScale / aScale;
    }
  }

  int32_t cx = aCx;
  int32_t cy = aCy;
  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER) {
    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX) {
      int32_t unused;
      treeOwnerAsWin->GetSize(&cx, &unused);
    } else if (aScale != currentScale) {
      cx = cx * currentScale / aScale;
    }

    if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY) {
      int32_t unused;
      treeOwnerAsWin->GetSize(&unused, &cy);
    } else if (aScale != currentScale) {
      cy = cy * currentScale / aScale;
    }
  }

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
      aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER) {
    treeOwnerAsWin->SetPositionAndSize(x, y, cx, cy, nsIBaseWindow::eRepaint);
    return IPC_OK();
  }

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    treeOwnerAsWin->SetPosition(x, y);
    mUpdatedDimensions = false;
    UpdatePosition();
    return IPC_OK();
  }

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER) {
    treeOwnerAsWin->SetSize(cx, cy, true);
    return IPC_OK();
  }

  MOZ_ASSERT(false, "Unknown flags!");
  return IPC_FAIL_NO_REASON(this);
}

nsresult BrowserParent::UpdatePosition() {
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    return NS_OK;
  }
  nsIntRect windowDims;
  NS_ENSURE_SUCCESS(frameLoader->GetWindowDimensions(windowDims),
                    NS_ERROR_FAILURE);
  UpdateDimensions(windowDims, mDimensions);
  return NS_OK;
}

void BrowserParent::NotifyPositionUpdatedForContentsInPopup() {
  if (CanonicalBrowsingContext* bc = GetBrowsingContext()) {
    bc->PreOrderWalk([](BrowsingContext* aContext) {
      if (WindowGlobalParent* windowGlobalParent =
              aContext->Canonical()->GetCurrentWindowGlobal()) {
        if (RefPtr<BrowserParent> browserParent =
                windowGlobalParent->GetBrowserParent()) {
          browserParent->UpdatePosition();
        }
      }
    });
  }
}

void BrowserParent::UpdateDimensions(const nsIntRect& rect,
                                     const ScreenIntSize& size) {
  if (mIsDestroyed) {
    return;
  }
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    NS_WARNING("No widget found in BrowserParent::UpdateDimensions");
    return;
  }

  LayoutDeviceIntPoint clientOffset = GetClientOffset();
  LayoutDeviceIntPoint chromeOffset = !GetBrowserBridgeParent()
                                          ? -GetChildProcessOffset()
                                          : LayoutDeviceIntPoint();

  if (!mUpdatedDimensions || mDimensions != size || !mRect.IsEqualEdges(rect) ||
      clientOffset != mClientOffset || chromeOffset != mChromeOffset) {
    mUpdatedDimensions = true;
    mRect = rect;
    mDimensions = size;
    mClientOffset = clientOffset;
    mChromeOffset = chromeOffset;

    Unused << SendUpdateDimensions(GetDimensionInfo());
    UpdateNativePointerLockCenter(widget);
  }
}

DimensionInfo BrowserParent::GetDimensionInfo() {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  MOZ_ASSERT(widget);
  CSSToLayoutDeviceScale widgetScale = widget->GetDefaultScale();

  LayoutDeviceIntRect devicePixelRect = ViewAs<LayoutDevicePixel>(
      mRect, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
  LayoutDeviceIntSize devicePixelSize = ViewAs<LayoutDevicePixel>(
      mDimensions, PixelCastJustification::LayoutDeviceIsScreenForTabDims);

  CSSRect unscaledRect = devicePixelRect / widgetScale;
  CSSSize unscaledSize = devicePixelSize / widgetScale;
  DimensionInfo di(unscaledRect, unscaledSize, mClientOffset, mChromeOffset);
  return di;
}

void BrowserParent::UpdateNativePointerLockCenter(nsIWidget* aWidget) {
  if (!mLockedNativePointer) {
    return;
  }
  LayoutDeviceIntRect dims(
      {0, 0},
      ViewAs<LayoutDevicePixel>(
          mDimensions, PixelCastJustification::LayoutDeviceIsScreenForTabDims));
  aWidget->SetNativePointerLockCenter((dims + mChromeOffset).Center());
}

void BrowserParent::SizeModeChanged(const nsSizeMode& aSizeMode) {
  if (!mIsDestroyed && aSizeMode != mSizeMode) {
    mSizeMode = aSizeMode;
    Unused << SendSizeModeChanged(aSizeMode);
  }
}

#if defined(MOZ_WIDGET_ANDROID)
void BrowserParent::DynamicToolbarMaxHeightChanged(ScreenIntCoord aHeight) {
  if (!mIsDestroyed) {
    Unused << SendDynamicToolbarMaxHeightChanged(aHeight);
  }
}

void BrowserParent::DynamicToolbarOffsetChanged(ScreenIntCoord aOffset) {
  if (!mIsDestroyed) {
    Unused << SendDynamicToolbarOffsetChanged(aOffset);
  }
}
#endif

void BrowserParent::HandleAccessKey(const WidgetKeyboardEvent& aEvent,
                                    nsTArray<uint32_t>& aCharCodes) {
  if (!mIsDestroyed) {
    // Note that we don't need to mark aEvent is posted to a remote process
    // because the event may be dispatched to it as normal keyboard event.
    // Therefore, we should use local copy to send it.
    WidgetKeyboardEvent localEvent(aEvent);
    Unused << SendHandleAccessKey(localEvent, aCharCodes);
  }
}

void BrowserParent::Activate(uint64_t aActionId) {
  LOGBROWSERFOCUS(("Activate %p actionid: %" PRIu64, this, aActionId));
  if (!mIsDestroyed) {
    SetTopLevelWebFocus(this);  // Intentionally inside "if"
    Unused << SendActivate(aActionId);
  }
}

void BrowserParent::Deactivate(bool aWindowLowering, uint64_t aActionId) {
  LOGBROWSERFOCUS(("Deactivate %p actionid: %" PRIu64, this, aActionId));
  if (!aWindowLowering) {
    UnsetTopLevelWebFocus(this);  // Intentionally outside the next "if"
  }
  if (!mIsDestroyed) {
    Unused << SendDeactivate(aActionId);
  }
}

#ifdef ACCESSIBILITY
a11y::PDocAccessibleParent* BrowserParent::AllocPDocAccessibleParent(
    PDocAccessibleParent* aParent, const uint64_t&,
    const MaybeDiscardedBrowsingContext&, const uint32_t&,
    const IAccessibleHolder&) {
  // Reference freed in DeallocPDocAccessibleParent.
  return do_AddRef(new a11y::DocAccessibleParent()).take();
}

bool BrowserParent::DeallocPDocAccessibleParent(PDocAccessibleParent* aParent) {
  // Free reference from AllocPDocAccessibleParent.
  static_cast<a11y::DocAccessibleParent*>(aParent)->Release();
  return true;
}

mozilla::ipc::IPCResult BrowserParent::RecvPDocAccessibleConstructor(
    PDocAccessibleParent* aDoc, PDocAccessibleParent* aParentDoc,
    const uint64_t& aParentID,
    const MaybeDiscardedBrowsingContext& aBrowsingContext,
    const uint32_t& aMsaaID, const IAccessibleHolder& aDocCOMProxy) {
#  if defined(ANDROID)
  MonitorAutoLock mal(nsAccessibilityService::GetAndroidMonitor());
#  endif
  auto doc = static_cast<a11y::DocAccessibleParent*>(aDoc);

  // If this tab is already shutting down just mark the new actor as shutdown
  // and ignore it.  When the tab actor is destroyed it will be too.
  if (mIsDestroyed) {
    doc->MarkAsShutdown();
    return IPC_OK();
  }

  if (aBrowsingContext) {
    doc->SetBrowsingContext(aBrowsingContext.get_canonical());
  }

  if (aParentDoc) {
    // Iframe document rendered in the same process as its embedder.
    // A document should never directly be the parent of another document.
    // There should always be an outer doc accessible child of the outer
    // document containing the child.
    MOZ_ASSERT(aParentID);
    if (!aParentID) {
      return IPC_FAIL_NO_REASON(this);
    }

    auto parentDoc = static_cast<a11y::DocAccessibleParent*>(aParentDoc);
    if (parentDoc->IsShutdown()) {
      // This can happen if parentDoc is an OOP iframe, but its embedder has
      // been destroyed. (DocAccessibleParent::Destroy destroys any child
      // documents.) The OOP iframe (and anything it embeds) will die soon
      // anyway, so mark this document as shutdown and ignore it.
      doc->MarkAsShutdown();
      return IPC_OK();
    }

    mozilla::ipc::IPCResult added = parentDoc->AddChildDoc(doc, aParentID);
    if (!added) {
#  ifdef DEBUG
      return added;
#  else
      return IPC_OK();
#  endif
    }

#  ifdef XP_WIN
    MOZ_ASSERT(aDocCOMProxy.IsNull());
    if (!StaticPrefs::accessibility_cache_enabled_AtStartup()) {
      a11y::MsaaAccessible::GetFrom(doc)->SetID(aMsaaID);
    }
    if (a11y::nsWinUtils::IsWindowEmulationStarted()) {
      doc->SetEmulatedWindowHandle(parentDoc->GetEmulatedWindowHandle());
    }
#  else
    Unused << aDoc->SendConstructedInParentProcess();
#  endif

    return IPC_OK();
  }

  if (auto* bridge = GetBrowserBridgeParent()) {
    // Iframe document rendered in a different process to its embedder.
    // In this case, we don't get aParentDoc and aParentID.
    MOZ_ASSERT(!aParentDoc && !aParentID);
    doc->SetTopLevelInContentProcess();
#  ifdef XP_WIN
    if (!StaticPrefs::accessibility_cache_enabled_AtStartup()) {
      MOZ_ASSERT(!aDocCOMProxy.IsNull());
      RefPtr<IAccessible> proxy(aDocCOMProxy.Get());
      doc->SetCOMInterface(proxy);
    }
#  endif
    a11y::ProxyCreated(doc);
#  ifdef XP_WIN
    if (!StaticPrefs::accessibility_cache_enabled_AtStartup()) {
      // This *must* be called after ProxyCreated because
      // MsaaAccessible::GetFrom will fail before that.
      a11y::MsaaAccessible* msaa = a11y::MsaaAccessible::GetFrom(doc);
      MOZ_ASSERT(msaa);
      msaa->SetID(aMsaaID);
    }
#  endif
    // It's possible the embedder accessible hasn't been set yet; e.g.
    // a hidden iframe. In that case, embedderDoc will be null and this will
    // be handled when the embedder is set.
    if (a11y::DocAccessibleParent* embedderDoc =
            bridge->GetEmbedderAccessibleDoc()) {
      mozilla::ipc::IPCResult added = embedderDoc->AddChildDoc(bridge);
      if (!added) {
#  ifdef DEBUG
        return added;
#  else
        return IPC_OK();
#  endif
      }
    }
    return IPC_OK();
  } else {
    // null aParentDoc means this document is at the top level in the child
    // process.  That means it makes no sense to get an id for an accessible
    // that is its parent.
    MOZ_ASSERT(!aParentID);
    if (aParentID) {
      return IPC_FAIL_NO_REASON(this);
    }

    doc->SetTopLevel();
    a11y::DocManager::RemoteDocAdded(doc);
#  ifdef XP_WIN
    if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
      doc->MaybeInitWindowEmulation();
    } else {
      a11y::MsaaAccessible::GetFrom(doc)->SetID(aMsaaID);
      MOZ_ASSERT(!aDocCOMProxy.IsNull());

      RefPtr<IAccessible> proxy(aDocCOMProxy.Get());
      doc->SetCOMInterface(proxy);
      doc->MaybeInitWindowEmulation();
      if (a11y::LocalAccessible* outerDoc = doc->OuterDocOfRemoteBrowser()) {
        doc->SendParentCOMProxy(outerDoc);
      }
    }
#  endif
  }
  return IPC_OK();
}
#endif

PFilePickerParent* BrowserParent::AllocPFilePickerParent(const nsString& aTitle,
                                                         const int16_t& aMode) {
  return new FilePickerParent(aTitle, aMode);
}

bool BrowserParent::DeallocPFilePickerParent(PFilePickerParent* actor) {
  delete actor;
  return true;
}

IPCResult BrowserParent::RecvIndexedDBPermissionRequest(
    nsIPrincipal* aPrincipal, IndexedDBPermissionRequestResolver&& aResolve) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal(aPrincipal);
  if (!principal) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!mFrameElement)) {
    return IPC_FAIL_NO_REASON(this);
  }

  RefPtr<indexedDB::PermissionRequestHelper> actor =
      new indexedDB::PermissionRequestHelper(mFrameElement, principal,
                                             aResolve);

  mozilla::Result permissionOrErr = actor->PromptIfNeeded();
  if (permissionOrErr.isErr()) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (permissionOrErr.inspect() !=
      indexedDB::PermissionRequestBase::kPermissionPrompt) {
    aResolve(permissionOrErr.inspect());
  }

  return IPC_OK();
}

already_AddRefed<PSessionStoreParent>
BrowserParent::AllocPSessionStoreParent() {
  RefPtr<BrowserSessionStore> sessionStore =
      BrowserSessionStore::GetOrCreate(mBrowsingContext->Top());
  if (!sessionStore) {
    return nullptr;
  }

  return do_AddRef(new SessionStoreParent(mBrowsingContext, sessionStore));
}

IPCResult BrowserParent::RecvNewWindowGlobal(
    ManagedEndpoint<PWindowGlobalParent>&& aEndpoint,
    const WindowGlobalInit& aInit) {
  RefPtr<CanonicalBrowsingContext> browsingContext =
      CanonicalBrowsingContext::Get(aInit.context().mBrowsingContextId);
  if (!browsingContext) {
    return IPC_FAIL(this, "Cannot create for missing BrowsingContext");
  }
  if (!aInit.principal()) {
    return IPC_FAIL(this, "Cannot create without valid principal");
  }

  // Ensure we never load a document with a content principal in
  // the wrong type of webIsolated process
  EnumSet<ContentParent::ValidatePrincipalOptions> validationOptions = {};
  nsCOMPtr<nsIURI> docURI = aInit.documentURI();
  if (docURI->SchemeIs("about") || docURI->SchemeIs("blob") ||
      docURI->SchemeIs("chrome")) {
    // XXXckerschb TODO - Do not use SystemPrincipal for:
    // Bug 1700639: about:plugins
    // Bug 1699385: Remove allowSystem for blobs
    // Bug 1698087: chrome://devtools/content/shared/webextension-fallback.html
    // chrome reftests, e.g.
    //   * chrome://reftest/content/writing-mode/ua-style-sheet-button-1a-ref.html
    //   * chrome://reftest/content/xul-document-load/test003.xhtml
    //   * chrome://reftest/content/forms/input/text/centering-1.xhtml
    validationOptions = {ContentParent::ValidatePrincipalOptions::AllowSystem};
  }

  if (!mManager->ValidatePrincipal(aInit.principal(), validationOptions)) {
    ContentParent::LogAndAssertFailedPrincipalValidationInfo(aInit.principal(),
                                                             __func__);
  }

  // Construct our new WindowGlobalParent, bind, and initialize it.
  RefPtr<WindowGlobalParent> wgp =
      WindowGlobalParent::CreateDisconnected(aInit);
  BindPWindowGlobalEndpoint(std::move(aEndpoint), wgp);
  wgp->Init();
  return IPC_OK();
}

PVsyncParent* BrowserParent::AllocPVsyncParent() {
  MOZ_ASSERT(!mVsyncParent);
  mVsyncParent = new VsyncParent();
  UpdateVsyncParentVsyncDispatcher();
  return mVsyncParent.get();
}

bool BrowserParent::DeallocPVsyncParent(PVsyncParent* aActor) {
  MOZ_ASSERT(aActor);
  mVsyncParent = nullptr;
  return true;
}

void BrowserParent::UpdateVsyncParentVsyncDispatcher() {
  if (!mVsyncParent) {
    return;
  }

  if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
    RefPtr<VsyncDispatcher> vsyncDispatcher = widget->GetVsyncDispatcher();
    if (!vsyncDispatcher) {
      vsyncDispatcher = gfxPlatform::GetPlatform()->GetGlobalVsyncDispatcher();
    }
    mVsyncParent->UpdateVsyncDispatcher(vsyncDispatcher);
  }
}

void BrowserParent::MouseEnterIntoWidget() {
  if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
    // When we mouseenter the remote target, the remote target's cursor should
    // become the current cursor.  When we mouseexit, we stop.
    mRemoteTargetSetsCursor = true;
    widget->SetCursor(mCursor);
  }

  // Mark that we have missed a mouse enter event, so that
  // the next mouse event will create a replacement mouse
  // enter event and send it to the child.
  mIsMouseEnterIntoWidgetEventSuppressed = true;
}

void BrowserParent::SendRealMouseEvent(WidgetMouseEvent& aEvent) {
  if (mIsDestroyed) {
    return;
  }

  // XXXedgar, if the synthesized mouse events could deliver to the correct
  // process directly (see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1549355), we probably don't
  // need to check mReason then.
  if (aEvent.mReason == WidgetMouseEvent::eReal) {
    if (aEvent.mMessage == eMouseExitFromWidget) {
      // Since we are leaving this remote target, so don't need to update
      // sLastMouseRemoteTarget, and if we are sLastMouseRemoteTarget, reset it
      // to null.
      BrowserParent::UnsetLastMouseRemoteTarget(this);
    } else {
      // Last remote target should not be changed without eMouseExitFromWidget.
      MOZ_ASSERT_IF(sLastMouseRemoteTarget, sLastMouseRemoteTarget == this);
      sLastMouseRemoteTarget = this;
    }
  }

  aEvent.mRefPoint = TransformParentToChild(aEvent.mRefPoint);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    // When we mouseenter the remote target, the remote target's cursor should
    // become the current cursor.  When we mouseexit, we stop.
    if (eMouseEnterIntoWidget == aEvent.mMessage) {
      mRemoteTargetSetsCursor = true;
      widget->SetCursor(mCursor);
    } else if (eMouseExitFromWidget == aEvent.mMessage) {
      mRemoteTargetSetsCursor = false;
    }
  }
  if (!mIsReadyToHandleInputEvents) {
    if (eMouseEnterIntoWidget == aEvent.mMessage) {
      mIsMouseEnterIntoWidgetEventSuppressed = true;
    } else if (eMouseExitFromWidget == aEvent.mMessage) {
      mIsMouseEnterIntoWidgetEventSuppressed = false;
    }
    return;
  }

  ScrollableLayerGuid guid;
  uint64_t blockId;
  ApzAwareEventRoutingToChild(&guid, &blockId, nullptr);

  bool isInputPriorityEventEnabled = Manager()->IsInputPriorityEventEnabled();

  if (mIsMouseEnterIntoWidgetEventSuppressed) {
    // In the case that the BrowserParent suppressed the eMouseEnterWidget event
    // due to its corresponding BrowserChild wasn't ready to handle it, we have
    // to resend it when the BrowserChild is ready.
    mIsMouseEnterIntoWidgetEventSuppressed = false;
    WidgetMouseEvent localEvent(aEvent);
    localEvent.mMessage = eMouseEnterIntoWidget;
    DebugOnly<bool> ret =
        isInputPriorityEventEnabled
            ? SendRealMouseEnterExitWidgetEvent(localEvent, guid, blockId)
            : SendNormalPriorityRealMouseEnterExitWidgetEvent(localEvent, guid,
                                                              blockId);
    NS_WARNING_ASSERTION(ret, "SendRealMouseEnterExitWidgetEvent() failed");
    MOZ_ASSERT(!ret || localEvent.HasBeenPostedToRemoteProcess());
  }

  if (eMouseMove == aEvent.mMessage) {
    if (aEvent.mReason == WidgetMouseEvent::eSynthesized) {
      DebugOnly<bool> ret =
          isInputPriorityEventEnabled
              ? SendSynthMouseMoveEvent(aEvent, guid, blockId)
              : SendNormalPrioritySynthMouseMoveEvent(aEvent, guid, blockId);
      NS_WARNING_ASSERTION(ret, "SendSynthMouseMoveEvent() failed");
      MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
      return;
    }

    if (!aEvent.mFlags.mIsSynthesizedForTests) {
      DebugOnly<bool> ret =
          isInputPriorityEventEnabled
              ? SendRealMouseMoveEvent(aEvent, guid, blockId)
              : SendNormalPriorityRealMouseMoveEvent(aEvent, guid, blockId);
      NS_WARNING_ASSERTION(ret, "SendRealMouseMoveEvent() failed");
      MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
      return;
    }

    DebugOnly<bool> ret =
        isInputPriorityEventEnabled
            ? SendRealMouseMoveEventForTests(aEvent, guid, blockId)
            : SendNormalPriorityRealMouseMoveEventForTests(aEvent, guid,
                                                           blockId);
    NS_WARNING_ASSERTION(ret, "SendRealMouseMoveEventForTests() failed");
    MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
    return;
  }

  if (eMouseEnterIntoWidget == aEvent.mMessage ||
      eMouseExitFromWidget == aEvent.mMessage) {
    DebugOnly<bool> ret =
        isInputPriorityEventEnabled
            ? SendRealMouseEnterExitWidgetEvent(aEvent, guid, blockId)
            : SendNormalPriorityRealMouseEnterExitWidgetEvent(aEvent, guid,
                                                              blockId);
    NS_WARNING_ASSERTION(ret, "SendRealMouseEnterExitWidgetEvent() failed");
    MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
    return;
  }

  DebugOnly<bool> ret =
      isInputPriorityEventEnabled
          ? SendRealMouseButtonEvent(aEvent, guid, blockId)
          : SendNormalPriorityRealMouseButtonEvent(aEvent, guid, blockId);
  NS_WARNING_ASSERTION(ret, "SendRealMouseButtonEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

LayoutDeviceToCSSScale BrowserParent::GetLayoutDeviceToCSSScale() {
  Document* doc = (mFrameElement ? mFrameElement->OwnerDoc() : nullptr);
  nsPresContext* ctx = (doc ? doc->GetPresContext() : nullptr);
  return LayoutDeviceToCSSScale(
      ctx ? (float)ctx->AppUnitsPerDevPixel() / AppUnitsPerCSSPixel() : 0.0f);
}

bool BrowserParent::QueryDropLinksForVerification() {
  // Before sending the dragEvent, we query the links being dragged and
  // store them on the parent, to make sure the child can not modify links.
  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (!dragSession) {
    NS_WARNING("No dragSession to query links for verification");
    return false;
  }

  RefPtr<DataTransfer> initialDataTransfer = dragSession->GetDataTransfer();
  if (!initialDataTransfer) {
    NS_WARNING("No initialDataTransfer to query links for verification");
    return false;
  }

  nsCOMPtr<nsIDroppedLinkHandler> dropHandler =
      do_GetService("@mozilla.org/content/dropped-link-handler;1");
  if (!dropHandler) {
    NS_WARNING("No dropHandler to query links for verification");
    return false;
  }

  // No more than one drop event can happen simultaneously; reset the link
  // verification array and store all links that are being dragged.
  mVerifyDropLinks.Clear();

  nsTArray<RefPtr<nsIDroppedLinkItem>> droppedLinkItems;
  dropHandler->QueryLinks(initialDataTransfer, droppedLinkItems);

  // Since the entire event is cancelled if one of the links is invalid,
  // we can store all links on the parent side without any prior
  // validation checks.
  nsresult rv = NS_OK;
  for (nsIDroppedLinkItem* item : droppedLinkItems) {
    nsString tmp;
    rv = item->GetUrl(tmp);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to query url for verification");
      break;
    }
    mVerifyDropLinks.AppendElement(tmp);

    rv = item->GetName(tmp);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to query name for verification");
      break;
    }
    mVerifyDropLinks.AppendElement(tmp);

    rv = item->GetType(tmp);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to query type for verification");
      break;
    }
    mVerifyDropLinks.AppendElement(tmp);
  }
  if (NS_FAILED(rv)) {
    mVerifyDropLinks.Clear();
    return false;
  }
  return true;
}

void BrowserParent::SendRealDragEvent(WidgetDragEvent& aEvent,
                                      uint32_t aDragAction,
                                      uint32_t aDropEffect,
                                      nsIPrincipal* aPrincipal,
                                      nsIContentSecurityPolicy* aCsp) {
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return;
  }
  MOZ_ASSERT(!Manager()->IsInputPriorityEventEnabled());
  aEvent.mRefPoint = TransformParentToChild(aEvent.mRefPoint);
  if (aEvent.mMessage == eDrop) {
    if (!QueryDropLinksForVerification()) {
      return;
    }
  }
  DebugOnly<bool> ret = PBrowserParent::SendRealDragEvent(
      aEvent, aDragAction, aDropEffect, aPrincipal, aCsp);
  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendRealDragEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

void BrowserParent::SendMouseWheelEvent(WidgetWheelEvent& aEvent) {
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return;
  }

  ScrollableLayerGuid guid;
  uint64_t blockId;
  ApzAwareEventRoutingToChild(&guid, &blockId, nullptr);
  aEvent.mRefPoint = TransformParentToChild(aEvent.mRefPoint);
  DebugOnly<bool> ret =
      Manager()->IsInputPriorityEventEnabled()
          ? PBrowserParent::SendMouseWheelEvent(aEvent, guid, blockId)
          : PBrowserParent::SendNormalPriorityMouseWheelEvent(aEvent, guid,
                                                              blockId);

  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendMouseWheelEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

mozilla::ipc::IPCResult BrowserParent::RecvDispatchWheelEvent(
    const mozilla::WidgetWheelEvent& aEvent) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  WidgetWheelEvent localEvent(aEvent);
  localEvent.mWidget = widget;
  localEvent.mRefPoint = TransformChildToParent(localEvent.mRefPoint);

  widget->DispatchInputEvent(&localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvDispatchMouseEvent(
    const mozilla::WidgetMouseEvent& aEvent) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  WidgetMouseEvent localEvent(aEvent);
  localEvent.mWidget = widget;
  localEvent.mRefPoint = TransformChildToParent(localEvent.mRefPoint);

  widget->DispatchInputEvent(&localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvDispatchKeyboardEvent(
    const mozilla::WidgetKeyboardEvent& aEvent) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.mWidget = widget;
  localEvent.mRefPoint = TransformChildToParent(localEvent.mRefPoint);

  widget->DispatchInputEvent(&localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvDispatchTouchEvent(
    const mozilla::WidgetTouchEvent& aEvent) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  WidgetTouchEvent localEvent(aEvent);
  localEvent.mWidget = widget;

  for (uint32_t i = 0; i < localEvent.mTouches.Length(); i++) {
    localEvent.mTouches[i]->mRefPoint =
        TransformChildToParent(localEvent.mTouches[i]->mRefPoint);
  }

  widget->DispatchInputEvent(&localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvRequestNativeKeyBindings(
    const uint32_t& aType, const WidgetKeyboardEvent& aEvent,
    nsTArray<CommandInt>* aCommands) {
  MOZ_ASSERT(aCommands);
  MOZ_ASSERT(aCommands->IsEmpty());

  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  NativeKeyBindingsType keyBindingsType =
      static_cast<NativeKeyBindingsType>(aType);
  switch (keyBindingsType) {
    case NativeKeyBindingsType::SingleLineEditor:
    case NativeKeyBindingsType::MultiLineEditor:
    case NativeKeyBindingsType::RichTextEditor:
      break;
    default:
      return IPC_FAIL(this, "Invalid aType value");
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.mWidget = widget;

  if (NS_FAILED(widget->AttachNativeKeyEvent(localEvent))) {
    return IPC_OK();
  }

  Maybe<WritingMode> writingMode;
  if (RefPtr<widget::TextEventDispatcher> dispatcher =
          widget->GetTextEventDispatcher()) {
    writingMode = dispatcher->MaybeQueryWritingModeAtSelection();
  }
  if (localEvent.InitEditCommandsFor(keyBindingsType, writingMode)) {
    *aCommands = localEvent.EditCommandsConstRef(keyBindingsType).Clone();
  }

  return IPC_OK();
}

class SynthesizedEventObserver : public nsIObserver {
  NS_DECL_ISUPPORTS

 public:
  SynthesizedEventObserver(BrowserParent* aBrowserParent,
                           const uint64_t& aObserverId)
      : mBrowserParent(aBrowserParent), mObserverId(aObserverId) {
    MOZ_ASSERT(mBrowserParent);
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    if (!mBrowserParent || !mObserverId) {
      // We already sent the notification, or we don't actually need to
      // send any notification at all.
      return NS_OK;
    }

    if (mBrowserParent->IsDestroyed()) {
      // If this happens it's probably a bug in the test that's triggering this.
      NS_WARNING(
          "BrowserParent was unexpectedly destroyed during event "
          "synthesization!");
    } else if (!mBrowserParent->SendNativeSynthesisResponse(
                   mObserverId, nsCString(aTopic))) {
      NS_WARNING("Unable to send native event synthesization response!");
    }
    // Null out browserParent to indicate we already sent the response
    mBrowserParent = nullptr;
    return NS_OK;
  }

 private:
  virtual ~SynthesizedEventObserver() = default;

  RefPtr<BrowserParent> mBrowserParent;
  uint64_t mObserverId;
};

NS_IMPL_ISUPPORTS(SynthesizedEventObserver, nsIObserver)

class MOZ_STACK_CLASS AutoSynthesizedEventResponder {
 public:
  AutoSynthesizedEventResponder(BrowserParent* aBrowserParent,
                                const uint64_t& aObserverId, const char* aTopic)
      : mObserver(new SynthesizedEventObserver(aBrowserParent, aObserverId)),
        mTopic(aTopic) {}

  ~AutoSynthesizedEventResponder() {
    // This may be a no-op if the observer already sent a response.
    mObserver->Observe(nullptr, mTopic, nullptr);
  }

  nsIObserver* GetObserver() { return mObserver; }

 private:
  nsCOMPtr<nsIObserver> mObserver;
  const char* mTopic;
};

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeKeyEvent(
    const int32_t& aNativeKeyboardLayout, const int32_t& aNativeKeyCode,
    const uint32_t& aModifierFlags, const nsString& aCharacters,
    const nsString& aUnmodifiedCharacters, const uint64_t& aObserverId) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  AutoSynthesizedEventResponder responder(this, aObserverId, "keyevent");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeKeyEvent(
        aNativeKeyboardLayout, aNativeKeyCode, aModifierFlags, aCharacters,
        aUnmodifiedCharacters, responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeMouseEvent(
    const LayoutDeviceIntPoint& aPoint, const uint32_t& aNativeMessage,
    const int16_t& aButton, const uint32_t& aModifierFlags,
    const uint64_t& aObserverId) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  const uint32_t last =
      static_cast<uint32_t>(nsIWidget::NativeMouseMessage::LeaveWindow);
  NS_ENSURE_TRUE(aNativeMessage <= last, IPC_FAIL(this, "Bogus message"));
  AutoSynthesizedEventResponder responder(this, aObserverId, "mouseevent");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeMouseEvent(
        aPoint, static_cast<nsIWidget::NativeMouseMessage>(aNativeMessage),
        static_cast<mozilla::MouseButton>(aButton),
        static_cast<nsIWidget::Modifiers>(aModifierFlags),
        responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeMouseMove(
    const LayoutDeviceIntPoint& aPoint, const uint64_t& aObserverId) {
  // This is used by pointer lock API.  So, even if it's not in the automation
  // mode, we need to accept the request.
  AutoSynthesizedEventResponder responder(this, aObserverId, "mousemove");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeMouseMove(aPoint, responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeMouseScrollEvent(
    const LayoutDeviceIntPoint& aPoint, const uint32_t& aNativeMessage,
    const double& aDeltaX, const double& aDeltaY, const double& aDeltaZ,
    const uint32_t& aModifierFlags, const uint32_t& aAdditionalFlags,
    const uint64_t& aObserverId) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  AutoSynthesizedEventResponder responder(this, aObserverId,
                                          "mousescrollevent");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeMouseScrollEvent(
        aPoint, aNativeMessage, aDeltaX, aDeltaY, aDeltaZ, aModifierFlags,
        aAdditionalFlags, responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeTouchPoint(
    const uint32_t& aPointerId, const TouchPointerState& aPointerState,
    const LayoutDeviceIntPoint& aPoint, const double& aPointerPressure,
    const uint32_t& aPointerOrientation, const uint64_t& aObserverId) {
  // This is used by DevTools to emulate touch events from mouse events in the
  // responsive design mode.  Therefore, we should accept the IPC messages even
  // if it's not in the automation mode but the browsing context is in RDM pane.
  // And the IPC message could be just delayed after closing the responsive
  // design mode.  Therefore, we shouldn't return IPC_FAIL since doing it makes
  // the tab crash.
  if (!xpc::IsInAutomation()) {
    NS_ENSURE_TRUE(mBrowsingContext, IPC_OK());
    NS_ENSURE_TRUE(mBrowsingContext->Top()->GetInRDMPane(), IPC_OK());
  }

  AutoSynthesizedEventResponder responder(this, aObserverId, "touchpoint");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchPoint(aPointerId, aPointerState, aPoint,
                                       aPointerPressure, aPointerOrientation,
                                       responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeTouchPadPinch(
    const TouchpadGesturePhase& aEventPhase, const float& aScale,
    const LayoutDeviceIntPoint& aPoint, const int32_t& aModifierFlags) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchPadPinch(aEventPhase, aScale, aPoint,
                                          aModifierFlags);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeTouchTap(
    const LayoutDeviceIntPoint& aPoint, const bool& aLongTap,
    const uint64_t& aObserverId) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  AutoSynthesizedEventResponder responder(this, aObserverId, "touchtap");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchTap(aPoint, aLongTap, responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvClearNativeTouchSequence(
    const uint64_t& aObserverId) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  AutoSynthesizedEventResponder responder(this, aObserverId, "cleartouch");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->ClearNativeTouchSequence(responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativePenInput(
    const uint32_t& aPointerId, const TouchPointerState& aPointerState,
    const LayoutDeviceIntPoint& aPoint, const double& aPressure,
    const uint32_t& aRotation, const int32_t& aTiltX, const int32_t& aTiltY,
    const int32_t& aButton, const uint64_t& aObserverId) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  AutoSynthesizedEventResponder responder(this, aObserverId, "peninput");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativePenInput(aPointerId, aPointerState, aPoint,
                                     aPressure, aRotation, aTiltX, aTiltY,
                                     aButton, responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeTouchpadDoubleTap(
    const LayoutDeviceIntPoint& aPoint, const uint32_t& aModifierFlags) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchpadDoubleTap(aPoint, aModifierFlags);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeTouchpadPan(
    const TouchpadGesturePhase& aEventPhase, const LayoutDeviceIntPoint& aPoint,
    const double& aDeltaX, const double& aDeltaY, const int32_t& aModifierFlags,
    const uint64_t& aObserverId) {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), IPC_FAIL(this, "Unexpected event"));

  AutoSynthesizedEventResponder responder(this, aObserverId,
                                          "touchpadpanevent");

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchpadPan(aEventPhase, aPoint, aDeltaX, aDeltaY,
                                        aModifierFlags,
                                        responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvLockNativePointer() {
  if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
    mLockedNativePointer = true;  // do before updating the center
    UpdateNativePointerLockCenter(widget);
    widget->LockNativePointer();
  }
  return IPC_OK();
}

void BrowserParent::UnlockNativePointer() {
  if (!mLockedNativePointer) {
    return;
  }
  if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
    widget->UnlockNativePointer();
    mLockedNativePointer = false;
  }
}

mozilla::ipc::IPCResult BrowserParent::RecvUnlockNativePointer() {
  UnlockNativePointer();
  return IPC_OK();
}

void BrowserParent::SendRealKeyEvent(WidgetKeyboardEvent& aEvent) {
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return;
  }
  aEvent.mRefPoint = TransformParentToChild(aEvent.mRefPoint);

  // NOTE: If you call `InitAllEditCommands()` for the other messages too,
  //       you also need to update
  //       TextEventDispatcher::DispatchKeyboardEventInternal().
  if (aEvent.mMessage == eKeyPress) {
    // If current input context is editable, the edit commands are initialized
    // by TextEventDispatcher::DispatchKeyboardEventInternal().  Otherwise,
    // we need to do it here (they are not necessary for the parent process,
    // therefore, we need to do it here for saving the runtime cost).
    if (!aEvent.AreAllEditCommandsInitialized()) {
      // XXX Is it good thing that the keypress event will be handled in an
      //     editor even though the user pressed the key combination before the
      //     focus change has not been completed in the parent process yet or
      //     focus change will happen?  If no, we can stop doing this.
      Maybe<WritingMode> writingMode;
      if (aEvent.mWidget) {
        if (RefPtr<widget::TextEventDispatcher> dispatcher =
                aEvent.mWidget->GetTextEventDispatcher()) {
          writingMode = dispatcher->MaybeQueryWritingModeAtSelection();
        }
      }
      aEvent.InitAllEditCommands(writingMode);
    }
  } else {
    aEvent.PreventNativeKeyBindings();
  }
  SentKeyEventData sendKeyEventData{
      aEvent.mKeyCode,      aEvent.mCharCode,      aEvent.mPseudoCharCode,
      aEvent.mKeyNameIndex, aEvent.mCodeNameIndex, aEvent.mModifiers,
      nsID::GenerateUUID()};
  const bool ok =
      Manager()->IsInputPriorityEventEnabled()
          ? PBrowserParent::SendRealKeyEvent(aEvent, sendKeyEventData.mUUID)
          : PBrowserParent::SendNormalPriorityRealKeyEvent(
                aEvent, sendKeyEventData.mUUID);

  NS_WARNING_ASSERTION(ok, "PBrowserParent::SendRealKeyEvent() failed");
  MOZ_ASSERT(!ok || aEvent.HasBeenPostedToRemoteProcess());
  if (ok && aEvent.IsWaitingReplyFromRemoteProcess()) {
    mWaitingReplyKeyboardEvents.AppendElement(sendKeyEventData);
  }
}

void BrowserParent::SendRealTouchEvent(WidgetTouchEvent& aEvent) {
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return;
  }

  // PresShell::HandleEventInternal adds touches on touch end/cancel.  This
  // confuses remote content and the panning and zooming logic into thinking
  // that the added touches are part of the touchend/cancel, when actually
  // they're not.
  if (aEvent.mMessage == eTouchEnd || aEvent.mMessage == eTouchCancel) {
    aEvent.mTouches.RemoveElementsBy(
        [](const auto& touch) { return !touch->mChanged; });
  }

  APZData apzData;
  ApzAwareEventRoutingToChild(&apzData.guid, &apzData.blockId,
                              &apzData.apzResponse);

  if (mIsDestroyed) {
    return;
  }

  for (uint32_t i = 0; i < aEvent.mTouches.Length(); i++) {
    aEvent.mTouches[i]->mRefPoint =
        TransformParentToChild(aEvent.mTouches[i]->mRefPoint);
  }

  static uint32_t sConsecutiveTouchMoveCount = 0;
  if (aEvent.mMessage == eTouchMove) {
    ++sConsecutiveTouchMoveCount;
    SendRealTouchMoveEvent(aEvent, apzData, sConsecutiveTouchMoveCount);
    return;
  }

  sConsecutiveTouchMoveCount = 0;
  DebugOnly<bool> ret =
      Manager()->IsInputPriorityEventEnabled()
          ? PBrowserParent::SendRealTouchEvent(
                aEvent, apzData.guid, apzData.blockId, apzData.apzResponse)
          : PBrowserParent::SendNormalPriorityRealTouchEvent(
                aEvent, apzData.guid, apzData.blockId, apzData.apzResponse);

  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendRealTouchEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

void BrowserParent::SendRealTouchMoveEvent(
    WidgetTouchEvent& aEvent, APZData& aAPZData,
    uint32_t aConsecutiveTouchMoveCount) {
  // Touchmove handling is complicated, since IPC compression should be used
  // only when there are consecutive touch objects for the same touch on the
  // same BrowserParent. IPC compression can be disabled by switching to
  // different IPC message.
  static bool sIPCMessageType1 = true;
  static TabId sLastTargetBrowserParent(0);
  static Maybe<APZData> sPreviousAPZData;
  // Artificially limit max touch points to 10. That should be in practise
  // more than enough.
  const uint32_t kMaxTouchMoveIdentifiers = 10;
  static Maybe<int32_t> sLastTouchMoveIdentifiers[kMaxTouchMoveIdentifiers];

  // Returns true if aIdentifiers contains all the touches in
  // sLastTouchMoveIdentifiers.
  auto LastTouchMoveIdentifiersContainedIn =
      [&](const nsTArray<int32_t>& aIdentifiers) -> bool {
    for (Maybe<int32_t>& entry : sLastTouchMoveIdentifiers) {
      if (entry.isSome() && !aIdentifiers.Contains(entry.value())) {
        return false;
      }
    }
    return true;
  };

  // Cache touch identifiers in sLastTouchMoveIdentifiers array to be used
  // when checking whether compression can be done for the next touchmove.
  auto SetLastTouchMoveIdentifiers =
      [&](const nsTArray<int32_t>& aIdentifiers) {
        for (Maybe<int32_t>& entry : sLastTouchMoveIdentifiers) {
          entry.reset();
        }

        MOZ_ASSERT(aIdentifiers.Length() <= kMaxTouchMoveIdentifiers);
        for (uint32_t j = 0; j < aIdentifiers.Length(); ++j) {
          sLastTouchMoveIdentifiers[j].emplace(aIdentifiers[j]);
        }
      };

  AutoTArray<int32_t, kMaxTouchMoveIdentifiers> changedTouches;
  bool preventCompression = !StaticPrefs::dom_events_compress_touchmove() ||
                            // Ensure the very first touchmove isn't overridden
                            // by the second one, so that web pages can get
                            // accurate coordinates for the first touchmove.
                            aConsecutiveTouchMoveCount < 3 ||
                            sPreviousAPZData.isNothing() ||
                            sPreviousAPZData.value() != aAPZData ||
                            sLastTargetBrowserParent != GetTabId() ||
                            aEvent.mTouches.Length() > kMaxTouchMoveIdentifiers;

  if (!preventCompression) {
    for (RefPtr<Touch>& touch : aEvent.mTouches) {
      if (touch->mChanged) {
        changedTouches.AppendElement(touch->mIdentifier);
      }
    }

    // Prevent compression if the new event has fewer or different touches
    // than the old one.
    preventCompression = !LastTouchMoveIdentifiersContainedIn(changedTouches);
  }

  if (preventCompression) {
    sIPCMessageType1 = !sIPCMessageType1;
  }

  // Update the last touch move identifiers always, so that when the next
  // event comes in, the new identifiers can be compared to the old ones.
  // If the pref is disabled, this just does a quick small loop.
  SetLastTouchMoveIdentifiers(changedTouches);
  sPreviousAPZData.reset();
  sPreviousAPZData.emplace(aAPZData);
  sLastTargetBrowserParent = GetTabId();

  DebugOnly<bool> ret = true;
  if (sIPCMessageType1) {
    ret =
        Manager()->IsInputPriorityEventEnabled()
            ? PBrowserParent::SendRealTouchMoveEvent(
                  aEvent, aAPZData.guid, aAPZData.blockId, aAPZData.apzResponse)
            : PBrowserParent::SendNormalPriorityRealTouchMoveEvent(
                  aEvent, aAPZData.guid, aAPZData.blockId,
                  aAPZData.apzResponse);
  } else {
    ret =
        Manager()->IsInputPriorityEventEnabled()
            ? PBrowserParent::SendRealTouchMoveEvent2(
                  aEvent, aAPZData.guid, aAPZData.blockId, aAPZData.apzResponse)
            : PBrowserParent::SendNormalPriorityRealTouchMoveEvent2(
                  aEvent, aAPZData.guid, aAPZData.blockId,
                  aAPZData.apzResponse);
  }

  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendRealTouchMoveEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

bool BrowserParent::SendHandleTap(TapType aType,
                                  const LayoutDevicePoint& aPoint,
                                  Modifiers aModifiers,
                                  const ScrollableLayerGuid& aGuid,
                                  uint64_t aInputBlockId) {
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return false;
  }
  if ((aType == TapType::eSingleTap || aType == TapType::eSecondTap)) {
    if (RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager()) {
      if (RefPtr<nsFrameLoader> frameLoader = GetFrameLoader()) {
        if (RefPtr<Element> element = frameLoader->GetOwnerContent()) {
          fm->SetFocus(element, nsIFocusManager::FLAG_BYMOUSE |
                                    nsIFocusManager::FLAG_BYTOUCH |
                                    nsIFocusManager::FLAG_NOSCROLL);
        }
      }
    }
  }
  return Manager()->IsInputPriorityEventEnabled()
             ? PBrowserParent::SendHandleTap(aType,
                                             TransformParentToChild(aPoint),
                                             aModifiers, aGuid, aInputBlockId)
             : PBrowserParent::SendNormalPriorityHandleTap(
                   aType, TransformParentToChild(aPoint), aModifiers, aGuid,
                   aInputBlockId);
}

mozilla::ipc::IPCResult BrowserParent::RecvSyncMessage(
    const nsString& aMessage, const ClonedMessageData& aData,
    nsTArray<StructuredCloneData>* aRetVal) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("BrowserParent::RecvSyncMessage",
                                             OTHER, aMessage);
  MMPrinter::Print("BrowserParent::RecvSyncMessage", aMessage, aData);

  StructuredCloneData data;
  ipc::UnpackClonedMessageData(aData, data);

  if (!ReceiveMessage(aMessage, true, &data, aRetVal)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvAsyncMessage(
    const nsString& aMessage, const ClonedMessageData& aData) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("BrowserParent::RecvAsyncMessage",
                                             OTHER, aMessage);
  MMPrinter::Print("BrowserParent::RecvAsyncMessage", aMessage, aData);

  StructuredCloneData data;
  ipc::UnpackClonedMessageData(aData, data);

  if (!ReceiveMessage(aMessage, false, &data, nullptr)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSetCursor(
    const nsCursor& aCursor, const bool& aHasCustomCursor,
    const nsCString& aCursorData, const uint32_t& aWidth,
    const uint32_t& aHeight, const float& aResolutionX,
    const float& aResolutionY, const uint32_t& aStride,
    const gfx::SurfaceFormat& aFormat, const uint32_t& aHotspotX,
    const uint32_t& aHotspotY, const bool& aForce) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  if (aForce) {
    widget->ClearCachedCursor();
  }

  nsCOMPtr<imgIContainer> cursorImage;
  if (aHasCustomCursor) {
    if (aHeight * aStride != aCursorData.Length() ||
        aStride < aWidth * gfx::BytesPerPixel(aFormat)) {
      return IPC_FAIL(this, "Invalid custom cursor data");
    }
    const gfx::IntSize size(aWidth, aHeight);
    RefPtr<gfx::DataSourceSurface> customCursor =
        gfx::CreateDataSourceSurfaceFromData(
            size, aFormat,
            reinterpret_cast<const uint8_t*>(aCursorData.BeginReading()),
            aStride);

    RefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(customCursor, size);
    cursorImage = image::ImageOps::CreateFromDrawable(drawable);
  }

  mCursor = nsIWidget::Cursor{aCursor,
                              std::move(cursorImage),
                              aHotspotX,
                              aHotspotY,
                              {aResolutionX, aResolutionY}};
  if (!mRemoteTargetSetsCursor) {
    return IPC_OK();
  }

  widget->SetCursor(mCursor);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSetLinkStatus(
    const nsString& aStatus) {
  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  if (!xulBrowserWindow) {
    return IPC_OK();
  }

  xulBrowserWindow->SetOverLink(aStatus);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvShowTooltip(
    const uint32_t& aX, const uint32_t& aY, const nsString& aTooltip,
    const nsString& aDirection) {
  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  if (!xulBrowserWindow) {
    return IPC_OK();
  }

  // ShowTooltip will end up accessing XULElement properties in JS (specifically
  // BoxObject). However, to get it to JS, we need to make sure we're a
  // nsFrameLoaderOwner, which implies we're a XULFrameElement. We can then
  // safely pass Element into JS.
  RefPtr<nsFrameLoaderOwner> flo = do_QueryObject(mFrameElement);
  if (!flo) return IPC_OK();

  nsCOMPtr<Element> el = do_QueryInterface(flo);
  if (!el) return IPC_OK();

  if (NS_SUCCEEDED(
          xulBrowserWindow->ShowTooltip(aX, aY, aTooltip, aDirection, el))) {
    mShowingTooltip = true;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvHideTooltip() {
  mShowingTooltip = false;

  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  if (!xulBrowserWindow) {
    return IPC_OK();
  }

  xulBrowserWindow->HideTooltip();
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvNotifyIMEFocus(
    const ContentCache& aContentCache, const IMENotification& aIMENotification,
    NotifyIMEFocusResolver&& aResolve) {
  if (mIsDestroyed) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWidget> widget = GetTextInputHandlingWidget();
  if (!widget) {
    aResolve(IMENotificationRequests());
    return IPC_OK();
  }

  mContentCache.AssignContent(aContentCache, widget, &aIMENotification);
  IMEStateManager::NotifyIME(aIMENotification, widget, this);

  IMENotificationRequests requests;
  if (aIMENotification.mMessage == NOTIFY_IME_OF_FOCUS) {
    requests = widget->IMENotificationRequestsRef();
  }
  aResolve(requests);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvNotifyIMETextChange(
    const ContentCache& aContentCache,
    const IMENotification& aIMENotification) {
  nsCOMPtr<nsIWidget> widget = GetTextInputHandlingWidget();
  if (!widget || !IMEStateManager::DoesBrowserParentHaveIMEFocus(this)) {
    return IPC_OK();
  }
  mContentCache.AssignContent(aContentCache, widget, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvNotifyIMECompositionUpdate(
    const ContentCache& aContentCache,
    const IMENotification& aIMENotification) {
  nsCOMPtr<nsIWidget> widget = GetTextInputHandlingWidget();
  if (!widget || !IMEStateManager::DoesBrowserParentHaveIMEFocus(this)) {
    return IPC_OK();
  }
  mContentCache.AssignContent(aContentCache, widget, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvNotifyIMESelection(
    const ContentCache& aContentCache,
    const IMENotification& aIMENotification) {
  nsCOMPtr<nsIWidget> widget = GetTextInputHandlingWidget();
  if (!widget || !IMEStateManager::DoesBrowserParentHaveIMEFocus(this)) {
    return IPC_OK();
  }
  mContentCache.AssignContent(aContentCache, widget, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvUpdateContentCache(
    const ContentCache& aContentCache) {
  nsCOMPtr<nsIWidget> widget = GetTextInputHandlingWidget();
  if (!widget || !IMEStateManager::DoesBrowserParentHaveIMEFocus(this)) {
    return IPC_OK();
  }

  mContentCache.AssignContent(aContentCache, widget);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvNotifyIMEMouseButtonEvent(
    const IMENotification& aIMENotification, bool* aConsumedByIME) {
  nsCOMPtr<nsIWidget> widget = GetTextInputHandlingWidget();
  if (!widget || !IMEStateManager::DoesBrowserParentHaveIMEFocus(this)) {
    *aConsumedByIME = false;
    return IPC_OK();
  }
  nsresult rv = IMEStateManager::NotifyIME(aIMENotification, widget, this);
  *aConsumedByIME = rv == NS_SUCCESS_EVENT_CONSUMED;
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvNotifyIMEPositionChange(
    const ContentCache& aContentCache,
    const IMENotification& aIMENotification) {
  nsCOMPtr<nsIWidget> widget = GetTextInputHandlingWidget();
  if (!widget || !IMEStateManager::DoesBrowserParentHaveIMEFocus(this)) {
    return IPC_OK();
  }
  mContentCache.AssignContent(aContentCache, widget, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvOnEventNeedingAckHandled(
    const EventMessage& aMessage) {
  // This is called when the child process receives WidgetCompositionEvent or
  // WidgetSelectionEvent.
  // FYI: Don't check if widget is nullptr here because it's more important to
  //      notify mContentCahce of this than handling something in it.
  nsCOMPtr<nsIWidget> widget = GetTextInputHandlingWidget();

  // While calling OnEventNeedingAckHandled(), BrowserParent *might* be
  // destroyed since it may send notifications to IME.
  RefPtr<BrowserParent> kungFuDeathGrip(this);
  mContentCache.OnEventNeedingAckHandled(widget, aMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvRequestFocus(
    const bool& aCanRaise, const CallerType aCallerType) {
  LOGBROWSERFOCUS(("RecvRequestFocus %p, aCanRaise: %d", this, aCanRaise));
  if (BrowserBridgeParent* bridgeParent = GetBrowserBridgeParent()) {
    mozilla::Unused << bridgeParent->SendRequestFocus(aCanRaise, aCallerType);
    return IPC_OK();
  }

  if (!mFrameElement) {
    return IPC_OK();
  }

  nsContentUtils::RequestFrameFocus(*mFrameElement, aCanRaise, aCallerType);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvWheelZoomChange(bool aIncrease) {
  RefPtr<BrowsingContext> bc = GetBrowsingContext();
  if (!bc) {
    return IPC_OK();
  }

  bc->Canonical()->DispatchWheelZoomChange(aIncrease);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvEnableDisableCommands(
    const MaybeDiscarded<BrowsingContext>& aContext, const nsString& aAction,
    nsTArray<nsCString>&& aEnabledCommands,
    nsTArray<nsCString>&& aDisabledCommands) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  nsCOMPtr<nsIBrowserController> browserController = do_QueryActor(
      "Controllers", aContext.get_canonical()->GetCurrentWindowGlobal());
  if (browserController) {
    browserController->EnableDisableCommands(aAction, aEnabledCommands,
                                             aDisabledCommands);
  }

  return IPC_OK();
}

LayoutDeviceIntPoint BrowserParent::TransformPoint(
    const LayoutDeviceIntPoint& aPoint,
    const LayoutDeviceToLayoutDeviceMatrix4x4& aMatrix) {
  LayoutDevicePoint floatPoint(aPoint);
  LayoutDevicePoint floatTransformed = TransformPoint(floatPoint, aMatrix);
  // The next line loses precision if an out-of-process iframe
  // has been scaled or rotated.
  return RoundedToInt(floatTransformed);
}

LayoutDevicePoint BrowserParent::TransformPoint(
    const LayoutDevicePoint& aPoint,
    const LayoutDeviceToLayoutDeviceMatrix4x4& aMatrix) {
  return aMatrix.TransformPoint(aPoint);
}

LayoutDeviceIntPoint BrowserParent::TransformParentToChild(
    const LayoutDeviceIntPoint& aPoint) {
  LayoutDeviceToLayoutDeviceMatrix4x4 matrix =
      GetChildToParentConversionMatrix();
  if (!matrix.Invert()) {
    return LayoutDeviceIntPoint(0, 0);
  }
  return TransformPoint(aPoint, matrix);
}

LayoutDevicePoint BrowserParent::TransformParentToChild(
    const LayoutDevicePoint& aPoint) {
  LayoutDeviceToLayoutDeviceMatrix4x4 matrix =
      GetChildToParentConversionMatrix();
  if (!matrix.Invert()) {
    return LayoutDevicePoint(0.0, 0.0);
  }
  return TransformPoint(aPoint, matrix);
}

LayoutDeviceIntPoint BrowserParent::TransformChildToParent(
    const LayoutDeviceIntPoint& aPoint) {
  return TransformPoint(aPoint, GetChildToParentConversionMatrix());
}

LayoutDevicePoint BrowserParent::TransformChildToParent(
    const LayoutDevicePoint& aPoint) {
  return TransformPoint(aPoint, GetChildToParentConversionMatrix());
}

LayoutDeviceIntRect BrowserParent::TransformChildToParent(
    const LayoutDeviceIntRect& aRect) {
  LayoutDeviceToLayoutDeviceMatrix4x4 matrix =
      GetChildToParentConversionMatrix();
  LayoutDeviceRect floatRect(aRect);
  // The outcome is not ideal if an out-of-process iframe has been rotated
  LayoutDeviceRect floatTransformed = matrix.TransformBounds(floatRect);
  // The next line loses precision if an out-of-process iframe
  // has been scaled or rotated.
  return RoundedToInt(floatTransformed);
}

LayoutDeviceToLayoutDeviceMatrix4x4
BrowserParent::GetChildToParentConversionMatrix() {
  if (mChildToParentConversionMatrix) {
    return *mChildToParentConversionMatrix;
  }
  LayoutDevicePoint offset(-GetChildProcessOffset());
  return LayoutDeviceToLayoutDeviceMatrix4x4::Translation(offset);
}

void BrowserParent::SetChildToParentConversionMatrix(
    const Maybe<LayoutDeviceToLayoutDeviceMatrix4x4>& aMatrix,
    const ScreenRect& aRemoteDocumentRect) {
  if (mChildToParentConversionMatrix == aMatrix &&
      mRemoteDocumentRect.isSome() &&
      mRemoteDocumentRect.value() == aRemoteDocumentRect) {
    return;
  }

  mChildToParentConversionMatrix = aMatrix;
  mRemoteDocumentRect = Some(aRemoteDocumentRect);
  if (mIsDestroyed) {
    return;
  }
  mozilla::Unused << SendChildToParentMatrix(ToUnknownMatrix(aMatrix),
                                             aRemoteDocumentRect);
}

LayoutDeviceIntPoint BrowserParent::GetChildProcessOffset() {
  // The "toplevel widget" in child processes is always at position
  // 0,0.  Map the event coordinates to match that.

  LayoutDeviceIntPoint offset(0, 0);
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    return offset;
  }
  nsIFrame* targetFrame = frameLoader->GetPrimaryFrameOfOwningContent();
  if (!targetFrame) {
    return offset;
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return offset;
  }

  nsPresContext* presContext = targetFrame->PresContext();
  nsIFrame* rootFrame = presContext->PresShell()->GetRootFrame();
  nsView* rootView = rootFrame ? rootFrame->GetView() : nullptr;
  if (!rootView) {
    return offset;
  }

  // Note that we don't want to take into account transforms here:
#if 0
  nsPoint pt(0, 0);
  nsLayoutUtils::TransformPoint(targetFrame, rootFrame, pt);
#endif
  // In practice, when transforms are applied to this frameLoader, we currently
  // get the wrong results whether we take transforms into account here or not.
  // But applying transforms here gives us the wrong results in all
  // circumstances when transforms are applied, unless they're purely
  // translational. It also gives us the wrong results whenever CSS transitions
  // are used to apply transforms, since the offeets aren't updated as the
  // transition is animated.
  //
  // What we actually need to do is apply the transforms to the coordinates of
  // any events we send to the child, and reverse them for any screen
  // coordinates that we retrieve from the child.

  // TODO: Once we take into account transforms here, set viewportType
  // correctly. For now we use Visual as this means we don't apply
  // the layout-to-visual transform in TranslateViewToWidget().
  ViewportType viewportType = ViewportType::Visual;

  nsPoint pt = targetFrame->GetOffsetTo(rootFrame);
  return -nsLayoutUtils::TranslateViewToWidget(presContext, rootView, pt,
                                               viewportType, widget);
}

LayoutDeviceIntPoint BrowserParent::GetClientOffset() {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  nsCOMPtr<nsIWidget> docWidget = GetDocWidget();

  if (widget == docWidget) {
    return widget->GetClientOffset();
  }

  return (docWidget->GetClientOffset() +
          nsLayoutUtils::WidgetToWidgetOffset(widget, docWidget));
}

void BrowserParent::StopIMEStateManagement() {
  if (mIsDestroyed) {
    return;
  }
  Unused << SendStopIMEStateManagement();
}

mozilla::ipc::IPCResult BrowserParent::RecvReplyKeyEvent(
    const WidgetKeyboardEvent& aEvent, const nsID& aUUID) {
  NS_ENSURE_TRUE(mFrameElement, IPC_OK());

  // First, verify aEvent is what we've sent to a remote process.
  Maybe<size_t> index = [&]() -> Maybe<size_t> {
    for (const size_t i : IntegerRange(mWaitingReplyKeyboardEvents.Length())) {
      const SentKeyEventData& data = mWaitingReplyKeyboardEvents[i];
      if (data.mUUID.Equals(aUUID)) {
        if (NS_WARN_IF(data.mKeyCode != aEvent.mKeyCode) ||
            NS_WARN_IF(data.mCharCode != aEvent.mCharCode) ||
            NS_WARN_IF(data.mPseudoCharCode != aEvent.mPseudoCharCode) ||
            NS_WARN_IF(data.mKeyNameIndex != aEvent.mKeyNameIndex) ||
            NS_WARN_IF(data.mCodeNameIndex != aEvent.mCodeNameIndex) ||
            NS_WARN_IF(data.mModifiers != aEvent.mModifiers)) {
          // Got different event data from what we stored before dispatching an
          // event with the ID.
          return Nothing();
        }
        return Some(i);
      }
    }
    // No entry found.
    return Nothing();
  }();
  if (MOZ_UNLIKELY(index.isNothing())) {
    return IPC_FAIL(this, "Bogus reply keyboard event");
  }
  // Don't discard the older keyboard events because the order may be changed if
  // the remote process has a event listener which takes too long time and while
  // the freezing, user may switch the tab, or if the remote process sends
  // synchronous XMLHttpRequest.
  mWaitingReplyKeyboardEvents.RemoveElementAt(*index);

  // If the event propagation was stopped by the child, it means that the event
  // was ignored in the child.  In the case, we should ignore it too because the
  // focused web app didn't have a chance to prevent its default.
  if (aEvent.PropagationStopped()) {
    return IPC_OK();
  }

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.MarkAsHandledInRemoteProcess();

  // Here we convert the WidgetEvent that we received to an Event
  // to be able to dispatch it to the <browser> element as the target element.
  RefPtr<nsPresContext> presContext =
      mFrameElement->OwnerDoc()->GetPresContext();
  NS_ENSURE_TRUE(presContext, IPC_OK());

  AutoHandlingUserInputStatePusher userInpStatePusher(localEvent.IsTrusted(),
                                                      &localEvent);

  nsEventStatus status = nsEventStatus_eIgnore;

  // Handle access key in this process before dispatching reply event because
  // ESM handles it before dispatching the event to the DOM tree.
  if (localEvent.mMessage == eKeyPress &&
      (localEvent.ModifiersMatchWithAccessKey(AccessKeyType::eChrome) ||
       localEvent.ModifiersMatchWithAccessKey(AccessKeyType::eContent))) {
    RefPtr<EventStateManager> esm = presContext->EventStateManager();
    AutoTArray<uint32_t, 10> accessCharCodes;
    localEvent.GetAccessKeyCandidates(accessCharCodes);
    if (esm->HandleAccessKey(&localEvent, presContext, accessCharCodes)) {
      status = nsEventStatus_eConsumeNoDefault;
    }
  }

  RefPtr<Element> frameElement = mFrameElement;
  EventDispatcher::Dispatch(frameElement, presContext, &localEvent, nullptr,
                            &status);

  if (!localEvent.DefaultPrevented() &&
      !localEvent.mFlags.mIsSynthesizedForTests) {
    nsCOMPtr<nsIWidget> widget = GetWidget();
    if (widget) {
      widget->PostHandleKeyEvent(&localEvent);
      localEvent.StopPropagation();
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvAccessKeyNotHandled(
    const WidgetKeyboardEvent& aEvent) {
  NS_ENSURE_TRUE(mFrameElement, IPC_OK());

  // This is called only when this process had focus and HandleAccessKey
  // message was posted to all remote process and each remote process didn't
  // execute any content access keys.
  // XXX If there were two or more remote processes, this may be called
  //     twice or more for a keyboard event, that must be a bug.  But how to
  //     detect if received event has already been handled?

  MOZ_ASSERT(aEvent.mMessage == eKeyPress);
  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.MarkAsHandledInRemoteProcess();
  localEvent.mMessage = eAccessKeyNotFound;

  // Here we convert the WidgetEvent that we received to an Event
  // to be able to dispatch it to the <browser> element as the target element.
  Document* doc = mFrameElement->OwnerDoc();
  PresShell* presShell = doc->GetPresShell();
  NS_ENSURE_TRUE(presShell, IPC_OK());

  if (presShell->CanDispatchEvent()) {
    RefPtr<nsPresContext> presContext = presShell->GetPresContext();
    NS_ENSURE_TRUE(presContext, IPC_OK());

    RefPtr<Element> frameElement = mFrameElement;
    EventDispatcher::Dispatch(frameElement, presContext, &localEvent);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvRegisterProtocolHandler(
    const nsString& aScheme, nsIURI* aHandlerURI, const nsString& aTitle,
    nsIURI* aDocURI) {
  nsCOMPtr<nsIWebProtocolHandlerRegistrar> registrar =
      do_GetService(NS_WEBPROTOCOLHANDLERREGISTRAR_CONTRACTID);
  if (registrar) {
    registrar->RegisterProtocolHandler(aScheme, aHandlerURI, aTitle, aDocURI,
                                       mFrameElement);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvOnStateChange(
    const WebProgressData& aWebProgressData, const RequestData& aRequestData,
    const uint32_t aStateFlags, const nsresult aStatus,
    const Maybe<WebProgressStateChangeData>& aStateChangeData) {
  RefPtr<CanonicalBrowsingContext> browsingContext =
      BrowsingContextForWebProgress(aWebProgressData);
  if (!browsingContext) {
    return IPC_OK();
  }

  nsCOMPtr<nsIRequest> request;
  if (aRequestData.requestURI()) {
    request = MakeAndAddRef<RemoteWebProgressRequest>(
        aRequestData.requestURI(), aRequestData.originalRequestURI(),
        aRequestData.matchedList());
  }

  if (aStateChangeData.isSome()) {
    if (!browsingContext->IsTopContent()) {
      return IPC_FAIL(
          this,
          "Unexpected WebProgressStateChangeData for non toplevel webProgress");
    }

    if (nsCOMPtr<nsIBrowser> browser = GetBrowser()) {
      Unused << browser->SetIsNavigating(aStateChangeData->isNavigating());
      Unused << browser->SetMayEnableCharacterEncodingMenu(
          aStateChangeData->mayEnableCharacterEncodingMenu());
      Unused << browser->UpdateForStateChange(aStateChangeData->charset(),
                                              aStateChangeData->documentURI(),
                                              aStateChangeData->contentType());
    }
  }

  if (auto* listener = browsingContext->GetWebProgress()) {
    listener->OnStateChange(listener, request, aStateFlags, aStatus);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvOnProgressChange(
    const int32_t aCurTotalProgress, const int32_t aMaxTotalProgress) {
  // We only collect progress change notifications for the toplevel
  // BrowserParent.
  // FIXME: In the future, consider merging in progress change information from
  // oop subframes.
  if (!GetBrowsingContext()->IsTopContent() ||
      !GetBrowsingContext()->GetWebProgress()) {
    return IPC_OK();
  }

  GetBrowsingContext()->GetWebProgress()->OnProgressChange(
      nullptr, nullptr, 0, 0, aCurTotalProgress, aMaxTotalProgress);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvOnLocationChange(
    const WebProgressData& aWebProgressData, const RequestData& aRequestData,
    nsIURI* aLocation, const uint32_t aFlags, const bool aCanGoBack,
    const bool aCanGoForward,
    const Maybe<WebProgressLocationChangeData>& aLocationChangeData) {
  RefPtr<CanonicalBrowsingContext> browsingContext =
      BrowsingContextForWebProgress(aWebProgressData);
  if (!browsingContext) {
    return IPC_OK();
  }

  nsCOMPtr<nsIRequest> request;
  if (aRequestData.requestURI()) {
    request = MakeAndAddRef<RemoteWebProgressRequest>(
        aRequestData.requestURI(), aRequestData.originalRequestURI(),
        aRequestData.matchedList());
  }

  browsingContext->SetCurrentRemoteURI(aLocation);

  nsCOMPtr<nsIBrowser> browser = GetBrowser();
  if (!mozilla::SessionHistoryInParent() && browser) {
    Unused << browser->UpdateWebNavigationForLocationChange(aCanGoBack,
                                                            aCanGoForward);
  }

  if (aLocationChangeData.isSome()) {
    if (!browsingContext->IsTopContent()) {
      return IPC_FAIL(this,
                      "Unexpected WebProgressLocationChangeData for non "
                      "toplevel webProgress");
    }

    if (browser) {
      Unused << browser->SetIsNavigating(aLocationChangeData->isNavigating());
      Unused << browser->UpdateForLocationChange(
          aLocation, aLocationChangeData->charset(),
          aLocationChangeData->mayEnableCharacterEncodingMenu(),
          aLocationChangeData->documentURI(), aLocationChangeData->title(),
          aLocationChangeData->contentPrincipal(),
          aLocationChangeData->contentPartitionedPrincipal(),
          aLocationChangeData->csp(), aLocationChangeData->referrerInfo(),
          aLocationChangeData->isSyntheticDocument(),
          aLocationChangeData->requestContextID().isSome(),
          aLocationChangeData->requestContextID().valueOr(0),
          aLocationChangeData->contentType());
    }
  }

  if (auto* listener = browsingContext->GetWebProgress()) {
    listener->OnLocationChange(listener, request, aLocation, aFlags);
  }

  // Since we've now changed Documents, notify the BrowsingContext that we've
  // changed. Ideally we'd just let the BrowsingContext do this when it changes
  // the current window global, but that happens before this and we have a lot
  // of tests that depend on the specific ordering of messages.
  if (browsingContext->IsTopContent() &&
      !(aFlags & nsIWebProgressListener::LOCATION_CHANGE_SAME_DOCUMENT)) {
    browsingContext->UpdateSecurityState();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvOnStatusChange(
    const nsString& aMessage) {
  // We only collect status change notifications for the toplevel
  // BrowserParent.
  // FIXME: In the future, consider merging in status change information from
  // oop subframes.
  if (!GetBrowsingContext()->IsTopContent() ||
      !GetBrowsingContext()->GetWebProgress()) {
    return IPC_OK();
  }

  GetBrowsingContext()->GetWebProgress()->OnStatusChange(nullptr, nullptr,
                                                         NS_OK, aMessage.get());

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvNavigationFinished() {
  nsCOMPtr<nsIBrowser> browser =
      mFrameElement ? mFrameElement->AsBrowser() : nullptr;

  if (browser) {
    browser->SetIsNavigating(false);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvNotifyContentBlockingEvent(
    const uint32_t& aEvent, const RequestData& aRequestData,
    const bool aBlocked, const nsACString& aTrackingOrigin,
    nsTArray<nsCString>&& aTrackingFullHashes,
    const Maybe<
        mozilla::ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
        aReason) {
  RefPtr<BrowsingContext> bc = GetBrowsingContext();

  if (!bc || bc->IsDiscarded()) {
    return IPC_OK();
  }

  // Get the top-level browsing context.
  bc = bc->Top();
  RefPtr<dom::WindowGlobalParent> wgp =
      bc->Canonical()->GetCurrentWindowGlobal();

  // The WindowGlobalParent would be null while running the test
  // browser_339445.js. This is unexpected and we will address this in a
  // following bug. For now, we first workaround this issue.
  if (!wgp) {
    return IPC_OK();
  }

  nsCOMPtr<nsIRequest> request = MakeAndAddRef<RemoteWebProgressRequest>(
      aRequestData.requestURI(), aRequestData.originalRequestURI(),
      aRequestData.matchedList());

  wgp->NotifyContentBlockingEvent(aEvent, request, aBlocked, aTrackingOrigin,
                                  aTrackingFullHashes, aReason);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSetAllowDeprecatedTls(bool value) {
  Preferences::SetBool("security.tls.version.enable-deprecated", value);
  return IPC_OK();
}

already_AddRefed<nsIBrowser> BrowserParent::GetBrowser() {
  nsCOMPtr<nsIBrowser> browser;
  RefPtr<Element> currentElement = mFrameElement;

  // In Responsive Design Mode, mFrameElement will be the <iframe mozbrowser>,
  // but we want the <xul:browser> that it is embedded in.
  while (currentElement) {
    browser = currentElement->AsBrowser();
    if (browser) {
      break;
    }

    BrowsingContext* browsingContext =
        currentElement->OwnerDoc()->GetBrowsingContext();
    currentElement =
        browsingContext ? browsingContext->GetEmbedderElement() : nullptr;
  }

  return browser.forget();
}

already_AddRefed<CanonicalBrowsingContext>
BrowserParent::BrowsingContextForWebProgress(
    const WebProgressData& aWebProgressData) {
  // Look up the BrowsingContext which this notification was fired for.
  if (aWebProgressData.browsingContext().IsNullOrDiscarded()) {
    NS_WARNING("WebProgress Ignored: BrowsingContext is null or discarded");
    return nullptr;
  }
  RefPtr<CanonicalBrowsingContext> browsingContext =
      aWebProgressData.browsingContext().get_canonical();

  // Double-check that we actually manage this BrowsingContext, and are not
  // receiving a malformed or out-of-date request. browsingContext should either
  // be the toplevel one managed by this BrowserParent, or embedded within a
  // WindowGlobalParent managed by this BrowserParent.
  if (browsingContext != mBrowsingContext) {
    WindowGlobalParent* embedder = browsingContext->GetParentWindowContext();
    if (!embedder || embedder->GetBrowserParent() != this) {
      NS_WARNING("WebProgress Ignored: wrong embedder process");
      return nullptr;
    }
  }

  // The current process for this BrowsingContext may have changed since the
  // notification was fired. Don't fire events for it anymore, as ownership of
  // the BrowsingContext has been moved elsewhere.
  if (RefPtr<WindowGlobalParent> current =
          browsingContext->GetCurrentWindowGlobal();
      current && current->GetBrowserParent() != this) {
    NS_WARNING("WebProgress Ignored: no longer current window global");
    return nullptr;
  }

  if (RefPtr<BrowsingContextWebProgress> progress =
          browsingContext->GetWebProgress()) {
    progress->SetLoadType(aWebProgressData.loadType());
  }

  return browsingContext.forget();
}

mozilla::ipc::IPCResult BrowserParent::RecvIntrinsicSizeOrRatioChanged(
    const Maybe<IntrinsicSize>& aIntrinsicSize,
    const Maybe<AspectRatio>& aIntrinsicRatio) {
  BrowserBridgeParent* bridge = GetBrowserBridgeParent();
  if (!bridge || !bridge->CanSend()) {
    return IPC_OK();
  }

  Unused << bridge->SendIntrinsicSizeOrRatioChanged(aIntrinsicSize,
                                                    aIntrinsicRatio);

  return IPC_OK();
}

bool BrowserParent::HandleQueryContentEvent(WidgetQueryContentEvent& aEvent) {
  nsCOMPtr<nsIWidget> textInputHandlingWidget = GetTextInputHandlingWidget();
  if (!textInputHandlingWidget) {
    return true;
  }
  if (!mContentCache.HandleQueryContentEvent(aEvent, textInputHandlingWidget) ||
      NS_WARN_IF(aEvent.Failed())) {
    return true;
  }
  switch (aEvent.mMessage) {
    case eQueryTextRect:
    case eQueryCaretRect:
    case eQueryEditorRect: {
      nsCOMPtr<nsIWidget> browserWidget = GetWidget();
      if (browserWidget != textInputHandlingWidget) {
        aEvent.mReply->mRect += nsLayoutUtils::WidgetToWidgetOffset(
            browserWidget, textInputHandlingWidget);
      }
      aEvent.mReply->mRect = TransformChildToParent(aEvent.mReply->mRect);
      break;
    }
    default:
      break;
  }
  return true;
}

bool BrowserParent::SendCompositionEvent(WidgetCompositionEvent& aEvent) {
  if (mIsDestroyed) {
    return false;
  }

  if (!mContentCache.OnCompositionEvent(aEvent)) {
    return true;
  }

  bool ret = Manager()->IsInputPriorityEventEnabled()
                 ? PBrowserParent::SendCompositionEvent(aEvent)
                 : PBrowserParent::SendNormalPriorityCompositionEvent(aEvent);
  if (NS_WARN_IF(!ret)) {
    return false;
  }
  MOZ_ASSERT(aEvent.HasBeenPostedToRemoteProcess());
  return true;
}

bool BrowserParent::SendSelectionEvent(WidgetSelectionEvent& aEvent) {
  if (mIsDestroyed) {
    return false;
  }
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }
  mContentCache.OnSelectionEvent(aEvent);
  bool ret = Manager()->IsInputPriorityEventEnabled()
                 ? PBrowserParent::SendSelectionEvent(aEvent)
                 : PBrowserParent::SendNormalPrioritySelectionEvent(aEvent);
  if (NS_WARN_IF(!ret)) {
    return false;
  }
  MOZ_ASSERT(aEvent.HasBeenPostedToRemoteProcess());
  aEvent.mSucceeded = true;
  return true;
}

bool BrowserParent::SendInsertText(const nsString& aStringToInsert) {
  if (mIsDestroyed) {
    return false;
  }
  return Manager()->IsInputPriorityEventEnabled()
             ? PBrowserParent::SendInsertText(aStringToInsert)
             : PBrowserParent::SendNormalPriorityInsertText(aStringToInsert);
}

bool BrowserParent::SendPasteTransferable(
    const IPCDataTransfer& aDataTransfer, const bool& aIsPrivateData,
    nsIPrincipal* aRequestingPrincipal,
    const nsContentPolicyType& aContentPolicyType) {
  return PBrowserParent::SendPasteTransferable(
      aDataTransfer, aIsPrivateData, aRequestingPrincipal, aContentPolicyType);
}

/* static */
void BrowserParent::SetTopLevelWebFocus(BrowserParent* aBrowserParent) {
  BrowserParent* old = GetFocused();
  if (aBrowserParent && !aBrowserParent->GetBrowserBridgeParent()) {
    // top-level Web content
    sTopLevelWebFocus = aBrowserParent;
    BrowserParent* bp = UpdateFocus();
    if (old != bp) {
      LOGBROWSERFOCUS(
          ("SetTopLevelWebFocus updated focus; old: %p, new: %p", old, bp));
      IMEStateManager::OnFocusMovedBetweenBrowsers(old, bp);
    }
  }
}

/* static */
void BrowserParent::UnsetTopLevelWebFocus(BrowserParent* aBrowserParent) {
  BrowserParent* old = GetFocused();
  if (sTopLevelWebFocus == aBrowserParent) {
    // top-level Web content
    sTopLevelWebFocus = nullptr;
    sFocus = nullptr;
    if (old) {
      LOGBROWSERFOCUS(
          ("UnsetTopLevelWebFocus moved focus to chrome; old: %p", old));
      IMEStateManager::OnFocusMovedBetweenBrowsers(old, nullptr);
    }
  }
}

/* static */
void BrowserParent::UpdateFocusFromBrowsingContext() {
  BrowserParent* old = GetFocused();
  BrowserParent* bp = UpdateFocus();
  if (old != bp) {
    LOGBROWSERFOCUS(
        ("UpdateFocusFromBrowsingContext updated focus; old: %p, new: %p", old,
         bp));
    IMEStateManager::OnFocusMovedBetweenBrowsers(old, bp);
  }
}

/* static */
BrowserParent* BrowserParent::UpdateFocus() {
  if (!sTopLevelWebFocus) {
    sFocus = nullptr;
    return nullptr;
  }
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    BrowsingContext* bc = fm->GetFocusedBrowsingContextInChrome();
    if (bc) {
      BrowsingContext* top = bc->Top();
      MOZ_ASSERT(top, "Should always have a top BrowsingContext.");
      CanonicalBrowsingContext* canonicalTop = top->Canonical();
      MOZ_ASSERT(canonicalTop,
                 "Casting to canonical should always be possible in the parent "
                 "process (top case).");
      WindowGlobalParent* globalTop = canonicalTop->GetCurrentWindowGlobal();
      if (globalTop) {
        RefPtr<BrowserParent> globalTopParent = globalTop->GetBrowserParent();
        if (sTopLevelWebFocus == globalTopParent) {
          CanonicalBrowsingContext* canonical = bc->Canonical();
          MOZ_ASSERT(
              canonical,
              "Casting to canonical should always be possible in the parent "
              "process.");
          WindowGlobalParent* global = canonical->GetCurrentWindowGlobal();
          if (global) {
            RefPtr<BrowserParent> parent = global->GetBrowserParent();
            sFocus = parent;
            return sFocus;
          }
          LOGBROWSERFOCUS(
              ("Focused BrowsingContext did not have WindowGlobalParent."));
        }
      } else {
        LOGBROWSERFOCUS(
            ("Top-level BrowsingContext did not have WindowGlobalParent."));
      }
    }
  }
  sFocus = sTopLevelWebFocus;
  return sFocus;
}

/* static */
void BrowserParent::UnsetTopLevelWebFocusAll() {
  if (sTopLevelWebFocus) {
    UnsetTopLevelWebFocus(sTopLevelWebFocus);
  }
}

/* static */
void BrowserParent::UnsetLastMouseRemoteTarget(BrowserParent* aBrowserParent) {
  if (sLastMouseRemoteTarget == aBrowserParent) {
    sLastMouseRemoteTarget = nullptr;
  }
}

mozilla::ipc::IPCResult BrowserParent::RecvRequestIMEToCommitComposition(
    const bool& aCancel, bool* aIsCommitted, nsString* aCommittedString) {
  nsCOMPtr<nsIWidget> widget = GetTextInputHandlingWidget();
  if (!widget) {
    *aIsCommitted = false;
    return IPC_OK();
  }

  *aIsCommitted = mContentCache.RequestIMEToCommitComposition(
      widget, aCancel, *aCommittedString);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvGetInputContext(
    widget::IMEState* aState) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    *aState = widget::IMEState(IMEEnabled::Disabled,
                               IMEState::OPEN_STATE_NOT_SUPPORTED);
    return IPC_OK();
  }

  *aState = widget->GetInputContext().mIMEState;
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSetInputContext(
    const InputContext& aContext, const InputContextAction& aAction) {
  IMEStateManager::SetInputContextForChildProcess(this, aContext, aAction);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvDispatchFocusToTopLevelWindow() {
  if (nsCOMPtr<nsIWidget> widget = GetTopLevelWidget()) {
    widget->SetFocus(nsIWidget::Raise::No, CallerType::System);
  }
  return IPC_OK();
}

bool BrowserParent::ReceiveMessage(const nsString& aMessage, bool aSync,
                                   StructuredCloneData* aData,
                                   nsTArray<StructuredCloneData>* aRetVal) {
  // If we're for an oop iframe, don't deliver messages to the wrong place.
  if (mBrowserBridgeParent) {
    return true;
  }

  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader(true);
  if (frameLoader && frameLoader->GetFrameMessageManager()) {
    RefPtr<nsFrameMessageManager> manager =
        frameLoader->GetFrameMessageManager();

    manager->ReceiveMessage(mFrameElement, frameLoader, aMessage, aSync, aData,
                            aRetVal, IgnoreErrors());
  }
  return true;
}

// nsIAuthPromptProvider

// This method is largely copied from nsDocShell::GetAuthPrompt
NS_IMETHODIMP
BrowserParent::GetAuthPrompt(uint32_t aPromptReason, const nsIID& iid,
                             void** aResult) {
  // we're either allowing auth, or it's a proxy request
  nsresult rv;
  nsCOMPtr<nsIPromptFactory> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsPIDOMWindowOuter> window;
  RefPtr<Element> frame = mFrameElement;
  if (frame) window = frame->OwnerDoc()->GetWindow();

  // Get an auth prompter for our window so that the parenting
  // of the dialogs works as it should when using tabs.
  nsCOMPtr<nsISupports> prompt;
  rv = wwatch->GetPrompt(window, iid, getter_AddRefs(prompt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoginManagerAuthPrompter> prompter = do_QueryInterface(prompt);
  if (prompter) {
    prompter->SetBrowser(mFrameElement);
  }

  *aResult = prompt.forget().take();
  return NS_OK;
}

PColorPickerParent* BrowserParent::AllocPColorPickerParent(
    const nsString& aTitle, const nsString& aInitialColor) {
  return new ColorPickerParent(aTitle, aInitialColor);
}

bool BrowserParent::DeallocPColorPickerParent(PColorPickerParent* actor) {
  delete actor;
  return true;
}

already_AddRefed<nsFrameLoader> BrowserParent::GetFrameLoader(
    bool aUseCachedFrameLoaderAfterDestroy) const {
  if (mIsDestroyed && !aUseCachedFrameLoaderAfterDestroy) {
    return nullptr;
  }

  if (mFrameLoader) {
    RefPtr<nsFrameLoader> fl = mFrameLoader;
    return fl.forget();
  }
  RefPtr<Element> frameElement(mFrameElement);
  RefPtr<nsFrameLoaderOwner> frameLoaderOwner = do_QueryObject(frameElement);
  return frameLoaderOwner ? frameLoaderOwner->GetFrameLoader() : nullptr;
}

void BrowserParent::TryCacheDPIAndScale() {
  if (mDPI > 0) {
    return;
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();

  if (widget) {
    mDPI = widget->GetDPI();
    mRounding = widget->RoundsWidgetCoordinatesTo();
    mDefaultScale = widget->GetDefaultScale();
  }
}

void BrowserParent::ApzAwareEventRoutingToChild(
    ScrollableLayerGuid* aOutTargetGuid, uint64_t* aOutInputBlockId,
    nsEventStatus* aOutApzResponse) {
  // Let the widget know that the event will be sent to the child process,
  // which will (hopefully) send a confirmation notice back to APZ.
  // Do this even if APZ is off since we need it for swipe gesture support on
  // OS X without APZ.
  InputAPZContext::SetRoutedToChildProcess();

  if (AsyncPanZoomEnabled()) {
    if (aOutTargetGuid) {
      *aOutTargetGuid = InputAPZContext::GetTargetLayerGuid();

      // There may be cases where the APZ hit-testing code came to a different
      // conclusion than the main-thread hit-testing code as to where the event
      // is destined. In such cases the layersId of the APZ result may not match
      // the layersId of this RemoteLayerTreeOwner. In such cases the
      // main-thread hit- testing code "wins" so we need to update the guid to
      // reflect this.
      if (mRemoteLayerTreeOwner.IsInitialized()) {
        if (aOutTargetGuid->mLayersId != mRemoteLayerTreeOwner.GetLayersId()) {
          *aOutTargetGuid =
              ScrollableLayerGuid(mRemoteLayerTreeOwner.GetLayersId(), 0,
                                  ScrollableLayerGuid::NULL_SCROLL_ID);
        }
      }
    }
    if (aOutInputBlockId) {
      *aOutInputBlockId = InputAPZContext::GetInputBlockId();
    }
    if (aOutApzResponse) {
      *aOutApzResponse = InputAPZContext::GetApzResponse();

      // We can get here without there being an InputAPZContext on the stack
      // if a non-native event synthesization function (such as
      // nsIDOMWindowUtils.sendTouchEvent()) was used in the parent process to
      // synthesize an event that's targeting a content process. Such events do
      // not go through APZ. Without an InputAPZContext on the stack we pick up
      // the default value "eSentinel" which cannot be sent over IPC, so replace
      // it with "eIgnore" instead, which what APZ uses when it ignores an
      // event. If a caller needs the ability to synthesize a event with a
      // different APZ response, a native event synthesization function (such as
      // sendNativeTouchPoint()) can be used.
      if (*aOutApzResponse == nsEventStatus_eSentinel) {
        *aOutApzResponse = nsEventStatus_eIgnore;
      }
    }
  } else {
    if (aOutInputBlockId) {
      *aOutInputBlockId = 0;
    }
    if (aOutApzResponse) {
      *aOutApzResponse = nsEventStatus_eIgnore;
    }
  }
}

mozilla::ipc::IPCResult BrowserParent::RecvRespondStartSwipeEvent(
    const uint64_t& aInputBlockId, const bool& aStartSwipe) {
  if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
    widget->ReportSwipeStarted(aInputBlockId, aStartSwipe);
  }
  return IPC_OK();
}

bool BrowserParent::GetDocShellIsActive() {
  return mBrowsingContext && mBrowsingContext->IsActive();
}

bool BrowserParent::GetHasPresented() { return mHasPresented; }

bool BrowserParent::GetHasLayers() { return mHasLayers; }

bool BrowserParent::GetRenderLayers() { return mRenderLayers; }

void BrowserParent::SetRenderLayers(bool aEnabled) {
  if (aEnabled == mRenderLayers) {
    if (aEnabled && mHasLayers && mIsPreservingLayers) {
      // RenderLayers might be called when we've been preserving layers,
      // and already had layers uploaded. In that case, the MozLayerTreeReady
      // event will not naturally arrive, which can confuse the front-end
      // layer. So we fire the event here.
      RefPtr<BrowserParent> self = this;
      LayersObserverEpoch epoch = mLayerTreeEpoch;
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "dom::BrowserParent::RenderLayers", [self, epoch]() {
            MOZ_ASSERT(NS_IsMainThread());
            self->LayerTreeUpdate(epoch, true);
          }));
    }

    return;
  }

  // Preserve layers means that attempts to stop rendering layers
  // will be ignored.
  if (!aEnabled && mIsPreservingLayers) {
    return;
  }

  mRenderLayers = aEnabled;

  SetRenderLayersInternal(aEnabled);
}

void BrowserParent::SetRenderLayersInternal(bool aEnabled) {
  // Increment the epoch so that layer tree updates from previous
  // RenderLayers requests are ignored.
  mLayerTreeEpoch = mLayerTreeEpoch.Next();

  Unused << SendRenderLayers(aEnabled, mLayerTreeEpoch);

  // Ask the child to repaint using the PHangMonitor channel/thread (which may
  // be less congested).
  if (aEnabled) {
    Manager()->PaintTabWhileInterruptingJS(this, mLayerTreeEpoch);
  }
}

bool BrowserParent::GetPriorityHint() { return mPriorityHint; }

void BrowserParent::SetPriorityHint(bool aPriorityHint) {
  mPriorityHint = aPriorityHint;
  RecomputeProcessPriority();
}

void BrowserParent::RecomputeProcessPriority() {
  auto* bc = GetBrowsingContext();
  ProcessPriorityManager::BrowserPriorityChanged(
      bc, bc->IsActive() || mPriorityHint);
}

void BrowserParent::PreserveLayers(bool aPreserveLayers) {
  if (mIsPreservingLayers == aPreserveLayers) {
    return;
  }
  mIsPreservingLayers = aPreserveLayers;
  Unused << SendPreserveLayers(aPreserveLayers);
}

void BrowserParent::NotifyResolutionChanged() {
  if (!mIsDestroyed) {
    // TryCacheDPIAndScale()'s cache is keyed off of
    // mDPI being greater than 0, so this invalidates it.
    mDPI = -1;
    TryCacheDPIAndScale();
    // If mDPI was set to -1 to invalidate it and then TryCacheDPIAndScale
    // fails to cache the values, then mDefaultScale.scale might be invalid.
    // We don't want to send that value to content. Just send -1 for it too in
    // that case.
    Unused << SendUIResolutionChanged(mDPI, mRounding,
                                      mDPI < 0 ? -1.0 : mDefaultScale.scale);
  }
}

bool BrowserParent::CanCancelContentJS(
    nsIRemoteTab::NavigationType aNavigationType, int32_t aNavigationIndex,
    nsIURI* aNavigationURI) const {
  // Pre-checking if we can cancel content js in the parent is only
  // supported when session history in the parent is enabled.
  if (!mozilla::SessionHistoryInParent()) {
    // If session history in the parent isn't enabled, this check will
    // be fully done in BrowserChild::CanCancelContentJS
    return true;
  }

  nsCOMPtr<nsISHistory> history = mBrowsingContext->GetSessionHistory();

  if (!history) {
    // If there is no history we can't possibly know if it's ok to
    // cancel content js.
    return false;
  }

  int32_t current;
  NS_ENSURE_SUCCESS(history->GetIndex(&current), false);

  if (current == -1) {
    // This tab has no history! Just return.
    return false;
  }

  nsCOMPtr<nsISHEntry> entry;
  NS_ENSURE_SUCCESS(history->GetEntryAtIndex(current, getter_AddRefs(entry)),
                    false);

  nsCOMPtr<nsIURI> currentURI = entry->GetURI();
  if (!currentURI->SchemeIs("http") && !currentURI->SchemeIs("https") &&
      !currentURI->SchemeIs("file")) {
    // Only cancel content JS for http(s) and file URIs. Other URIs are probably
    // internal and we should just let them run to completion.
    return false;
  }

  if (aNavigationType == nsIRemoteTab::NAVIGATE_BACK) {
    aNavigationIndex = current - 1;
  } else if (aNavigationType == nsIRemoteTab::NAVIGATE_FORWARD) {
    aNavigationIndex = current + 1;
  } else if (aNavigationType == nsIRemoteTab::NAVIGATE_URL) {
    if (!aNavigationURI) {
      return false;
    }

    if (aNavigationURI->SchemeIs("javascript")) {
      // "javascript:" URIs don't (necessarily) trigger navigation to a
      // different page, so don't allow the current page's JS to terminate.
      return false;
    }

    // If navigating directly to a URL (e.g. via hitting Enter in the location
    // bar), then we can cancel anytime the next URL is different from the
    // current, *excluding* the ref ("#").
    bool equals;
    NS_ENSURE_SUCCESS(currentURI->EqualsExceptRef(aNavigationURI, &equals),
                      false);
    return !equals;
  }
  // Note: aNavigationType may also be NAVIGATE_INDEX, in which case we don't
  // need to do anything special.

  int32_t delta = aNavigationIndex > current ? 1 : -1;
  for (int32_t i = current + delta; i != aNavigationIndex + delta; i += delta) {
    nsCOMPtr<nsISHEntry> nextEntry;
    // If `i` happens to be negative, this call will fail (which is what we
    // would want to happen).
    NS_ENSURE_SUCCESS(history->GetEntryAtIndex(i, getter_AddRefs(nextEntry)),
                      false);

    nsCOMPtr<nsISHEntry> laterEntry = delta == 1 ? nextEntry : entry;
    nsCOMPtr<nsIURI> thisURI = entry->GetURI();
    nsCOMPtr<nsIURI> nextURI = nextEntry->GetURI();

    // If we changed origin and the load wasn't in a subframe, we know it was
    // a full document load, so we can cancel the content JS safely.
    if (!laterEntry->GetIsSubFrame()) {
      nsAutoCString thisHost;
      NS_ENSURE_SUCCESS(thisURI->GetPrePath(thisHost), false);

      nsAutoCString nextHost;
      NS_ENSURE_SUCCESS(nextURI->GetPrePath(nextHost), false);

      if (!thisHost.Equals(nextHost)) {
        return true;
      }
    }

    entry = nextEntry;
  }

  return false;
}

void BrowserParent::SuppressDisplayport(bool aEnabled) {
  if (IsDestroyed()) {
    return;
  }

#ifdef DEBUG
  if (aEnabled) {
    mActiveSupressDisplayportCount++;
  } else {
    mActiveSupressDisplayportCount--;
  }
  MOZ_ASSERT(mActiveSupressDisplayportCount >= 0);
#endif

  Unused << SendSuppressDisplayport(aEnabled);
}

void BrowserParent::NavigateByKey(bool aForward, bool aForDocumentNavigation) {
  Unused << SendNavigateByKey(aForward, aForDocumentNavigation);
}

void BrowserParent::LayerTreeUpdate(const LayersObserverEpoch& aEpoch,
                                    bool aActive) {
  // Ignore updates if we're an out-of-process iframe. For oop iframes, our
  // |mFrameElement| is that of the top-level document, and so AsyncTabSwitcher
  // will treat MozLayerTreeReady / MozLayerTreeCleared events as if they came
  // from the top-level tab, which is wrong.
  //
  // XXX: Should we still be updating |mHasLayers|?
  if (GetBrowserBridgeParent()) {
    return;
  }

  // Ignore updates from old epochs. They might tell us that layers are
  // available when we've already sent a message to clear them. We can't trust
  // the update in that case since layers could disappear anytime after that.
  if (aEpoch != mLayerTreeEpoch || mIsDestroyed) {
    return;
  }

  RefPtr<Element> frameElement = mFrameElement;
  if (!frameElement) {
    NS_WARNING("Could not locate target for layer tree message.");
    return;
  }

  mHasLayers = aActive;

  RefPtr<Event> event = NS_NewDOMEvent(frameElement, nullptr, nullptr);
  if (aActive) {
    mHasPresented = true;
    event->InitEvent(u"MozLayerTreeReady"_ns, true, false);
  } else {
    event->InitEvent(u"MozLayerTreeCleared"_ns, true, false);
  }
  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
  frameElement->DispatchEvent(*event);
}

mozilla::ipc::IPCResult BrowserParent::RecvPaintWhileInterruptingJSNoOp(
    const LayersObserverEpoch& aEpoch) {
  // We sent a PaintWhileInterruptingJS message when layers were already
  // visible. In this case, we should act as if an update occurred even though
  // we already have the layers.
  LayerTreeUpdate(aEpoch, true);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvRemoteIsReadyToHandleInputEvents() {
  // When enabling input event prioritization, input events may preempt other
  // normal priority IPC messages. To prevent the input events preempt
  // PBrowserConstructor, we use an IPC 'RemoteIsReadyToHandleInputEvents' to
  // notify the parent that BrowserChild is created and ready to handle input
  // events.
  SetReadyToHandleInputEvents();
  return IPC_OK();
}

PPaymentRequestParent* BrowserParent::AllocPPaymentRequestParent() {
  RefPtr<PaymentRequestParent> actor = new PaymentRequestParent();
  return actor.forget().take();
}

bool BrowserParent::DeallocPPaymentRequestParent(
    PPaymentRequestParent* aActor) {
  RefPtr<PaymentRequestParent> actor =
      dont_AddRef(static_cast<PaymentRequestParent*>(aActor));
  return true;
}

nsresult BrowserParent::HandleEvent(Event* aEvent) {
  if (mIsDestroyed) {
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("MozUpdateWindowPos") ||
      eventType.EqualsLiteral("fullscreenchange")) {
    // Events that signify the window moving are used to update the position
    // and notify the BrowserChild.
    return UpdatePosition();
  }
  return NS_OK;
}

mozilla::ipc::IPCResult BrowserParent::RecvInvokeDragSession(
    nsTArray<IPCDataTransfer>&& aTransfers, const uint32_t& aAction,
    Maybe<Shmem>&& aVisualDnDData, const uint32_t& aStride,
    const gfx::SurfaceFormat& aFormat, const LayoutDeviceIntRect& aDragRect,
    nsIPrincipal* aPrincipal, nsIContentSecurityPolicy* aCsp,
    const CookieJarSettingsArgs& aCookieJarSettingsArgs,
    const MaybeDiscarded<WindowContext>& aSourceWindowContext) {
  PresShell* presShell = mFrameElement->OwnerDoc()->GetPresShell();
  if (!presShell) {
    Unused << Manager()->SendEndDragSession(true, true, LayoutDeviceIntPoint(),
                                            0);
    // Continue sending input events with input priority when stopping the dnd
    // session.
    Manager()->SetInputPriorityEventEnabled(true);
    return IPC_OK();
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  net::CookieJarSettings::Deserialize(aCookieJarSettingsArgs,
                                      getter_AddRefs(cookieJarSettings));

  RefPtr<RemoteDragStartData> dragStartData = new RemoteDragStartData(
      this, std::move(aTransfers), aDragRect, aPrincipal, aCsp,
      cookieJarSettings, aSourceWindowContext.GetMaybeDiscarded());

  if (!aVisualDnDData.isNothing() && aVisualDnDData.ref().IsReadable() &&
      aVisualDnDData.ref().Size<char>() >= aDragRect.height * aStride) {
    dragStartData->SetVisualization(gfx::CreateDataSourceSurfaceFromData(
        gfx::IntSize(aDragRect.width, aDragRect.height), aFormat,
        aVisualDnDData.ref().get<uint8_t>(), aStride));
  }

  nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
  if (dragService) {
    dragService->MaybeAddChildProcess(Manager());
  }

  presShell->GetPresContext()
      ->EventStateManager()
      ->BeginTrackingRemoteDragGesture(mFrameElement, dragStartData);

  if (aVisualDnDData.isSome()) {
    Unused << DeallocShmem(aVisualDnDData.ref());
  }

  return IPC_OK();
}

bool BrowserParent::AsyncPanZoomEnabled() const {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  return widget && widget->AsyncPanZoomEnabled();
}

void BrowserParent::StartPersistence(
    CanonicalBrowsingContext* aContext,
    nsIWebBrowserPersistDocumentReceiver* aRecv, ErrorResult& aRv) {
  auto* actor = new WebBrowserPersistDocumentParent();
  actor->SetOnReady(aRecv);
  bool ok = Manager()->SendPWebBrowserPersistDocumentConstructor(actor, this,
                                                                 aContext);
  if (!ok) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
  // (The actor will be destroyed on constructor failure.)
}

mozilla::ipc::IPCResult BrowserParent::RecvLookUpDictionary(
    const nsString& aText, nsTArray<FontRange>&& aFontRangeArray,
    const bool& aIsVertical, const LayoutDeviceIntPoint& aPoint) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  widget->LookUpDictionary(aText, aFontRangeArray, aIsVertical,
                           TransformChildToParent(aPoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvShowCanvasPermissionPrompt(
    const nsCString& aOrigin, const bool& aHideDoorHanger) {
  nsCOMPtr<nsIBrowser> browser =
      mFrameElement ? mFrameElement->AsBrowser() : nullptr;
  if (!browser) {
    // If the tab is being closed, the browser may not be available.
    // In this case we can ignore the request.
    return IPC_OK();
  }
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (!os) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsresult rv = os->NotifyObservers(
      browser,
      aHideDoorHanger ? "canvas-permissions-prompt-hide-doorhanger"
                      : "canvas-permissions-prompt",
      NS_ConvertUTF8toUTF16(aOrigin).get());
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvVisitURI(nsIURI* aURI,
                                                    nsIURI* aLastVisitedURI,
                                                    const uint32_t& aFlags) {
  if (!aURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  RefPtr<nsIWidget> widget = GetWidget();
  if (NS_WARN_IF(!widget)) {
    return IPC_OK();
  }
  nsCOMPtr<IHistory> history = components::History::Service();
  if (history) {
    Unused << history->VisitURI(widget, aURI, aLastVisitedURI, aFlags);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvQueryVisitedState(
    nsTArray<RefPtr<nsIURI>>&& aURIs) {
#ifdef MOZ_ANDROID_HISTORY
  nsCOMPtr<IHistory> history = components::History::Service();
  if (NS_WARN_IF(!history)) {
    return IPC_OK();
  }
  RefPtr<nsIWidget> widget = GetWidget();
  if (NS_WARN_IF(!widget)) {
    return IPC_OK();
  }

  // FIXME(emilio): Is this check really needed?
  for (nsIURI* uri : aURIs) {
    if (!uri) {
      return IPC_FAIL(this, "Received null URI");
    }
  }

  auto* gvHistory = static_cast<GeckoViewHistory*>(history.get());
  gvHistory->QueryVisitedState(widget, mManager, std::move(aURIs));
  return IPC_OK();
#else
  return IPC_FAIL(this, "QueryVisitedState is Android-only");
#endif
}

void BrowserParent::LiveResizeStarted() { SuppressDisplayport(true); }

void BrowserParent::LiveResizeStopped() { SuppressDisplayport(false); }

void BrowserParent::SetBrowserBridgeParent(BrowserBridgeParent* aBrowser) {
  // We should either be clearing out our reference to a browser bridge, or not
  // have either a browser bridge, browser host, or owner content yet.
  MOZ_ASSERT(!aBrowser ||
             (!mBrowserBridgeParent && !mBrowserHost && !mFrameElement));
  mBrowserBridgeParent = aBrowser;
}

void BrowserParent::SetBrowserHost(BrowserHost* aBrowser) {
  // We should either be clearing out our reference to a browser host, or not
  // have either a browser bridge, browser host, or owner content yet.
  MOZ_ASSERT(!aBrowser ||
             (!mBrowserBridgeParent && !mBrowserHost && !mFrameElement));
  mBrowserHost = aBrowser;
}

mozilla::ipc::IPCResult BrowserParent::RecvSetSystemFont(
    const nsCString& aFontName) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SetSystemFont(aFontName);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvGetSystemFont(nsCString* aFontName) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->GetSystemFont(*aFontName);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvMaybeFireEmbedderLoadEvents(
    EmbedderElementEventType aFireEventAtEmbeddingElement) {
  BrowserBridgeParent* bridge = GetBrowserBridgeParent();
  if (!bridge) {
    NS_WARNING("Received `load` event on unbridged BrowserParent!");
    return IPC_OK();
  }

  Unused << bridge->SendMaybeFireEmbedderLoadEvents(
      aFireEventAtEmbeddingElement);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvScrollRectIntoView(
    const nsRect& aRect, const ScrollAxis& aVertical,
    const ScrollAxis& aHorizontal, const ScrollFlags& aScrollFlags,
    const int32_t& aAppUnitsPerDevPixel) {
  BrowserBridgeParent* bridge = GetBrowserBridgeParent();
  if (!bridge || !bridge->CanSend()) {
    return IPC_OK();
  }

  Unused << bridge->SendScrollRectIntoView(aRect, aVertical, aHorizontal,
                                           aScrollFlags, aAppUnitsPerDevPixel);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvIsWindowSupportingProtectedMedia(
    const uint64_t& aOuterWindowID,
    IsWindowSupportingProtectedMediaResolver&& aResolve) {
#ifdef XP_WIN
  bool isFxrWindow =
      FxRWindowManager::GetInstance()->IsFxRWindow(aOuterWindowID);
  aResolve(!isFxrWindow);
#else
  MOZ_CRASH("Should only be called on Windows");
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvIsWindowSupportingWebVR(
    const uint64_t& aOuterWindowID,
    IsWindowSupportingWebVRResolver&& aResolve) {
#ifdef XP_WIN
  bool isFxrWindow =
      FxRWindowManager::GetInstance()->IsFxRWindow(aOuterWindowID);
  aResolve(!isFxrWindow);
#else
  aResolve(true);
#endif

  return IPC_OK();
}

static BrowserParent* GetTopLevelBrowserParent(BrowserParent* aBrowserParent) {
  MOZ_ASSERT(aBrowserParent);
  BrowserParent* parent = aBrowserParent;
  while (BrowserBridgeParent* bridge = parent->GetBrowserBridgeParent()) {
    parent = bridge->Manager();
  }
  return parent;
}

mozilla::ipc::IPCResult BrowserParent::RecvRequestPointerLock(
    RequestPointerLockResolver&& aResolve) {
  nsCString error;
  if (sTopLevelWebFocus != GetTopLevelBrowserParent(this)) {
    error = "PointerLockDeniedNotFocused";
  } else if (!PointerLockManager::SetLockedRemoteTarget(this)) {
    error = "PointerLockDeniedInUse";
  } else {
    PointerEventHandler::ReleaseAllPointerCaptureRemoteTarget();
  }
  aResolve(error);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvReleasePointerLock() {
  MOZ_ASSERT_IF(PointerLockManager::GetLockedRemoteTarget(),
                PointerLockManager::GetLockedRemoteTarget() == this);
  PointerLockManager::ReleaseLockedRemoteTarget(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvRequestPointerCapture(
    const uint32_t& aPointerId, RequestPointerCaptureResolver&& aResolve) {
  aResolve(
      PointerEventHandler::SetPointerCaptureRemoteTarget(aPointerId, this));
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvReleasePointerCapture(
    const uint32_t& aPointerId) {
  PointerEventHandler::ReleasePointerCaptureRemoteTarget(aPointerId);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvShowDynamicToolbar() {
#if defined(MOZ_WIDGET_ANDROID)
  nsCOMPtr<nsIWidget> widget = GetTopLevelWidget();
  if (!widget) {
    return IPC_OK();
  }

  RefPtr<nsWindow> window = nsWindow::From(widget);
  if (!window) {
    return IPC_OK();
  }

  window->ShowDynamicToolbar();
#endif  // defined(MOZ_WIDGET_ANDROID)
  return IPC_OK();
}

}  // namespace mozilla::dom
