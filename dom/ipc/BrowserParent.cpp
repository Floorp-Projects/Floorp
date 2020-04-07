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
#  include "mozilla/a11y/ProxyAccessibleBase.h"
#  include "nsAccessibilityService.h"
#endif
#include "mozilla/BrowserElementParent.h"
#include "mozilla/dom/CancelContentJSOptionsBinding.h"
#include "mozilla/dom/ChromeMessageSender.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DataTransferItemList.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/PaymentRequestParent.h"
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/RemoteDragStartData.h"
#include "mozilla/dom/RemoteWebProgress.h"
#include "mozilla/dom/RemoteWebProgressRequest.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/SessionStoreUtilsBinding.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/Hal.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/layers/AsyncDragMetrics.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layout/RemoteLayerTreeOwner.h"
#include "mozilla/plugins/PPluginWidgetParent.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/StaticPrefs_dom.h"
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
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsImportModule.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadInfo.h"
#include "nsIPromptFactory.h"
#include "nsIURI.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebProtocolHandlerRegistrar.h"
#include "nsIXPConnect.h"
#include "nsIXULBrowserWindow.h"
#include "nsIAppWindow.h"
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
#include "LoadContext.h"
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
#include "MMPrinter.h"
#include "SessionStoreFunctions.h"
#include "mozilla/dom/CrashReport.h"

#ifdef XP_WIN
#  include "mozilla/plugins/PluginWidgetParent.h"
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

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::services;
using namespace mozilla::widget;
using namespace mozilla::jsipc;
using namespace mozilla::gfx;

using mozilla::LazyLogModule;
using mozilla::StaticAutoPtr;
using mozilla::Unused;

LazyLogModule gBrowserFocusLog("BrowserFocus");

#define LOGBROWSERFOCUS(args) \
  MOZ_LOG(gBrowserFocusLog, mozilla::LogLevel::Debug, args)

/* static */
BrowserParent* BrowserParent::sFocus = nullptr;
/* static */
BrowserParent* BrowserParent::sTopLevelWebFocus = nullptr;

// The flags passed by the webProgress notifications are 16 bits shifted
// from the ones registered by webProgressListeners.
#define NOTIFY_FLAG_SHIFT 16

namespace mozilla {
namespace dom {

BrowserParent::LayerToBrowserParentTable*
    BrowserParent::sLayerToBrowserParentTable = nullptr;

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowserParent)
  NS_INTERFACE_MAP_ENTRY(nsIAuthPromptProvider)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION_WEAK(BrowserParent, mLoadContext, mFrameLoader,
                              mBrowsingContext)
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
      mLoadContext(nullptr),
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
      mOrientation(0),
      mDPI(0),
      mRounding(0),
      mDefaultScale(0),
      mUpdatedDimensions(false),
      mSizeMode(nsSizeMode_Normal),
      mClientOffset{},
      mChromeOffset{},
      mCreatingWindow(false),
      mDelayedURL{},
      mDelayedFrameScripts{},
      mCursor(eCursorInvalid),
      mCustomCursor{},
      mCustomCursorHotspotX(0),
      mCustomCursorHotspotY(0),
      mVerifyDropLinks{},
      mDocShellIsActive(false),
      mMarkedDestroying(false),
      mIsDestroyed(false),
      mTabSetsCursor(false),
      mPreserveLayers(false),
      mRenderLayers(true),
      mActiveInPriorityManager(false),
      mHasLayers(false),
      mHasPresented(false),
      mIsReadyToHandleInputEvents(false),
      mIsMouseEnterIntoWidgetEventSuppressed(false),
      mSuspendedProgressEvents(false) {
  MOZ_ASSERT(aManager);
  // When the input event queue is disabled, we don't need to handle the case
  // that some input events are dispatched before PBrowserConstructor.
  mIsReadyToHandleInputEvents = !ContentParent::IsInputEventQueueSupported();
}

BrowserParent::~BrowserParent() = default;

/* static */
void BrowserParent::InitializeStatics() { MOZ_ASSERT(XRE_IsParentProcess()); }

/* static */
BrowserParent* BrowserParent::GetFocused() { return sFocus; }

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
  sLayerToBrowserParentTable->Put(uint64_t(aLayersId), aBrowserParent);
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
  nsCOMPtr<nsILoadContext> loadContext;
  if (mLoadContext) {
    loadContext = mLoadContext;
  } else {
    bool isPrivate = mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
    SetPrivateBrowsingAttributes(isPrivate);
    bool useTrackingProtection = false;
    if (mFrameElement) {
      nsCOMPtr<nsIDocShell> docShell = mFrameElement->OwnerDoc()->GetDocShell();
      if (docShell) {
        docShell->GetUseTrackingProtection(&useTrackingProtection);
      }
    }
    loadContext = new LoadContext(
        GetOwnerElement(), true /* aIsContent */, isPrivate,
        mChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW,
        mChromeFlags & nsIWebBrowserChrome::CHROME_FISSION_WINDOW,
        useTrackingProtection, OriginAttributesRef());
    mLoadContext = loadContext;
  }
  return loadContext.forget();
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
      nsViewManager* vm = presShell->GetViewManager();
      nsCOMPtr<nsIWidget> widget;
      vm->GetRootWidget(getter_AddRefs(widget));
      return widget.forget();
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
  for (auto iter = docs.ConstIter(); !iter.Done(); iter.Next()) {
    auto doc = static_cast<a11y::DocAccessibleParent*>(iter.Get()->GetKey());
    // We want the document for this BrowserParent even if it's for an
    // embedded out-of-process iframe. Therefore, we use
    // IsTopLevelInContentProcess. In contrast, using IsToplevel would only
    // include documents that aren't embedded; e.g. tab documents.
    if (doc->IsTopLevelInContentProcess()) {
      return doc;
    }
  }

  MOZ_ASSERT(docs.Count() == 0,
             "If there isn't a top level accessible doc "
             "there shouldn't be an accessible doc at all!");
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
    // FIXME(emilio, bug 1606660): allowfullscreen should probably move to
    // OwnerShowInfo.
    bool allowFullscreen =
        mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::allowfullscreen) ||
        mFrameElement->HasAttr(kNameSpaceID_None,
                               nsGkAtoms::mozallowfullscreen);
    bool isTransparent =
        nsContentUtils::IsChromeDoc(mFrameElement->OwnerDoc()) &&
        mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::transparent);
    return ParentShowInfo(name, allowFullscreen, false, isTransparent, mDPI,
                          mRounding, mDefaultScale.scale);
  }

  return ParentShowInfo(EmptyString(), false, false, false, mDPI, mRounding,
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

  if (mFrameElement) {
    bool useGlobalHistory = !mFrameElement->HasAttr(
        kNameSpaceID_None, nsGkAtoms::disableglobalhistory);
    Unused << SendSetUseGlobalHistory(useGlobalHistory);
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

  // Try to send down WidgetNativeData, now that this BrowserParent is
  // associated with a widget.
  nsCOMPtr<nsIWidget> widget = GetTopLevelWidget();
  if (widget) {
    WindowsHandle widgetNativeData = reinterpret_cast<WindowsHandle>(
        widget->GetNativeData(NS_NATIVE_SHAREABLE_WINDOW));
    if (widgetNativeData) {
      Unused << SendSetWidgetNativeData(widgetNativeData);
    }
  }

  if (mRemoteLayerTreeOwner.IsInitialized()) {
    mRemoteLayerTreeOwner.OwnerContentChanged();
  }

  // Set our BrowsingContext's embedder if we're not embedded within a
  // BrowserBridgeParent.
  if (!GetBrowserBridgeParent() && mBrowsingContext && mFrameElement) {
    mBrowsingContext->SetEmbedderElement(mFrameElement);
  }

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
        eventTarget->AddEventListener(NS_LITERAL_STRING("MozUpdateWindowPos"),
                                      this, false, false);
        eventTarget->AddEventListener(NS_LITERAL_STRING("fullscreenchange"),
                                      this, false, false);
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
      eventTarget->RemoveEventListener(NS_LITERAL_STRING("MozUpdateWindowPos"),
                                       this, false);
      eventTarget->RemoveEventListener(NS_LITERAL_STRING("fullscreenchange"),
                                       this, false);
    }
  }
}

void BrowserParent::DestroyInternal() {
  UnsetTopLevelWebFocus(this);

  RemoveWindowListeners();

#ifdef ACCESSIBILITY
  if (a11y::DocAccessibleParent* tabDoc = GetTopLevelDocAccessible()) {
    tabDoc->Destroy();
  }
#endif

  // If this fails, it's most likely due to a content-process crash,
  // and auto-cleanup will kick in.  Otherwise, the child side will
  // destroy itself and send back __delete__().
  Unused << SendDestroy();

#ifdef XP_WIN
  // Let all PluginWidgets know we are tearing down. Prevents
  // these objects from sending async events after the child side
  // is shut down.
  const ManagedContainer<PPluginWidgetParent>& kids =
      ManagedPPluginWidgetParent();
  for (auto iter = kids.ConstIter(); !iter.Done(); iter.Next()) {
    static_cast<mozilla::plugins::PluginWidgetParent*>(iter.Get()->GetKey())
        ->ParentDestroy();
  }
#endif
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

  mMarkedDestroying = true;
}

mozilla::ipc::IPCResult BrowserParent::RecvEnsureLayersConnected(
    CompositorOptions* aCompositorOptions) {
  if (mRemoteLayerTreeOwner.IsInitialized()) {
    mRemoteLayerTreeOwner.EnsureLayersConnected(aCompositorOptions);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::Recv__delete__() {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  Manager()->NotifyTabDestroyed(mTabId, mMarkedDestroying);
  return IPC_OK();
}

void BrowserParent::ActorDestroy(ActorDestroyReason why) {
  ContentProcessManager::GetSingleton()->UnregisterRemoteFrame(mTabId);

  if (mRemoteLayerTreeOwner.IsInitialized()) {
    // It's important to unmap layers after the remote browser has been
    // destroyed, otherwise it may still send messages to the compositor which
    // will reject them, causing assertions.
    RemoveBrowserParentFromTable(mRemoteLayerTreeOwner.GetLayersId());
    mRemoteLayerTreeOwner.Destroy();
  }

  // Even though BrowserParent::Destroy calls this, we need to do it here too in
  // case of a crash.
  BrowserParent::UnsetTopLevelWebFocus(this);

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
            StringBeginsWith(crash_reason,
                             NS_LITERAL_CSTRING("[unhandlable oom]")) ||
            StringBeginsWith(crash_reason,
                             NS_LITERAL_CSTRING("Unhandlable OOM"))) {
          is_oom = true;
        }

        CrashReport::Deliver(principal, is_oom);
      }
    }
  }

  // Prevent executing ContentParent::NotifyTabDestroying in
  // BrowserParent::Destroy() called by frameLoader->DestroyComplete() below
  // when tab crashes in contentprocess because ContentParent::ActorDestroy()
  // in main process will be triggered before this function
  // and remove the process information that
  // ContentParent::NotifyTabDestroying need from mContentParentMap.

  // When tab crashes in content process,
  // there is no need to call ContentParent::NotifyTabDestroying
  // because the jobs in ContentParent::NotifyTabDestroying
  // will be done by ContentParent::ActorDestroy.
  if (XRE_IsContentProcess() && why == AbnormalShutdown && !mIsDestroyed) {
    DestroyInternal();
    mIsDestroyed = true;
  }

  // Tell our embedder that the tab is now going away unless we're an
  // out-of-process iframe.
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader(true);
  if (frameLoader) {
    ReceiveMessage(CHILD_PROCESS_SHUTDOWN_MESSAGE, false, nullptr, nullptr,
                   nullptr);

    if (!mBrowsingContext->GetParent()) {
      // If this is a top-level BrowsingContext, tell the frameloader it's time
      // to go away. Otherwise, this is a subframe crash, and we can keep the
      // frameloader around.
      frameLoader->DestroyComplete();
    }

    // If this was a crash, tell our nsFrameLoader to fire crash events.
    if (why == AbnormalShutdown) {
      frameLoader->MaybeNotifyCrashed(mBrowsingContext, GetIPCChannel());

      if (!mBrowsingContext->IsTopContent()) {
        OnSubFrameCrashed();
      }
    }
  }

  mFrameLoader = nullptr;
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

  nsCOMPtr<nsIFocusManager> fm = nsFocusManager::GetFocusManager();
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

void BrowserParent::LoadURL(nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  if (mIsDestroyed) {
    return;
  }

  nsCString spec;
  aURI->GetSpec(spec);

  if (mCreatingWindow) {
    // Don't send the message if the child wants to load its own URL.
    MOZ_ASSERT(mDelayedURL.IsEmpty());
    mDelayedURL = spec;
    return;
  }

  Unused << SendLoadURL(spec, GetShowInfo());
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
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(widget);

    Unused << SendDynamicToolbarMaxHeightChanged(
        widget->GetDynamicToolbarMaxHeight());
  }
#endif
}

bool BrowserParent::AttachLayerManager() {
  return !!mRemoteLayerTreeOwner.AttachLayerManager();
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
  if (!mRemoteLayerTreeOwner.AttachLayerManager()) {
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

  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  hal::ScreenOrientation orientation = config.orientation();
  LayoutDeviceIntPoint clientOffset = GetClientOffset();
  LayoutDeviceIntPoint chromeOffset = -GetChildProcessOffset();

  if (!mUpdatedDimensions || mOrientation != orientation ||
      mDimensions != size || !mRect.IsEqualEdges(rect) ||
      clientOffset != mClientOffset || chromeOffset != mChromeOffset) {
    mUpdatedDimensions = true;
    mRect = rect;
    mDimensions = size;
    mOrientation = orientation;
    mClientOffset = clientOffset;
    mChromeOffset = chromeOffset;

    Unused << SendUpdateDimensions(GetDimensionInfo());
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
  DimensionInfo di(unscaledRect, unscaledSize, mOrientation, mClientOffset,
                   mChromeOffset);
  return di;
}

void BrowserParent::SizeModeChanged(const nsSizeMode& aSizeMode) {
  if (!mIsDestroyed && aSizeMode != mSizeMode) {
    mSizeMode = aSizeMode;
    Unused << SendSizeModeChanged(aSizeMode);
  }
}

void BrowserParent::ThemeChanged() {
  if (!mIsDestroyed) {
    // The theme has changed, and any cached values we had sent down
    // to the child have been invalidated. When this method is called,
    // LookAndFeel should have the up-to-date values, which we now
    // send down to the child. We do this for every remote tab for now,
    // but bug 1156934 has been filed to do it once per content process.
    Unused << SendThemeChanged(LookAndFeel::GetIntCache());
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

void BrowserParent::Activate() {
  LOGBROWSERFOCUS(("Activate %p", this));
  if (!mIsDestroyed) {
    SetTopLevelWebFocus(this);  // Intentionally inside "if"
    Unused << Manager()->SendActivate(this);
  }
}

void BrowserParent::Deactivate(bool aWindowLowering) {
  LOGBROWSERFOCUS(("Deactivate %p", this));
  if (!aWindowLowering) {
    UnsetTopLevelWebFocus(this);  // Intentionally outside the next "if"
  }
  if (!mIsDestroyed) {
    Unused << Manager()->SendDeactivate(this);
  }
}

#ifdef ACCESSIBILITY
a11y::PDocAccessibleParent* BrowserParent::AllocPDocAccessibleParent(
    PDocAccessibleParent* aParent, const uint64_t&, const uint32_t&,
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
    const uint64_t& aParentID, const uint32_t& aMsaaID,
    const IAccessibleHolder& aDocCOMProxy) {
  auto doc = static_cast<a11y::DocAccessibleParent*>(aDoc);

  // If this tab is already shutting down just mark the new actor as shutdown
  // and ignore it.  When the tab actor is destroyed it will be too.
  if (mIsDestroyed) {
    doc->MarkAsShutdown();
    return IPC_OK();
  }

  if (aParentDoc) {
    // A document should never directly be the parent of another document.
    // There should always be an outer doc accessible child of the outer
    // document containing the child.
    MOZ_ASSERT(aParentID);
    if (!aParentID) {
      return IPC_FAIL_NO_REASON(this);
    }

    auto parentDoc = static_cast<a11y::DocAccessibleParent*>(aParentDoc);
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
    a11y::WrapperFor(doc)->SetID(aMsaaID);
    if (a11y::nsWinUtils::IsWindowEmulationStarted()) {
      doc->SetEmulatedWindowHandle(parentDoc->GetEmulatedWindowHandle());
    }
#  endif

    return IPC_OK();
  }

  if (GetBrowserBridgeParent()) {
    // Iframe document rendered in a different process to its embedder.
    // In this case, we don't get aParentDoc and aParentID.
    MOZ_ASSERT(!aParentDoc && !aParentID);
    doc->SetTopLevelInContentProcess();
#  ifdef XP_WIN
    MOZ_ASSERT(!aDocCOMProxy.IsNull());
    RefPtr<IAccessible> proxy(aDocCOMProxy.Get());
    doc->SetCOMInterface(proxy);
#  endif
    a11y::ProxyCreated(
        doc, a11y::Interfaces::DOCUMENT | a11y::Interfaces::HYPERTEXT);
#  ifdef XP_WIN
    // This *must* be called after ProxyCreated because WrapperFor will fail
    // before that.
    a11y::AccessibleWrap* wrapper = a11y::WrapperFor(doc);
    MOZ_ASSERT(wrapper);
    wrapper->SetID(aMsaaID);
#  endif
    a11y::DocAccessibleParent* embedderDoc;
    uint64_t embedderID;
    Tie(embedderDoc, embedderID) = doc->GetRemoteEmbedder();
    // It's possible the embedder accessible hasn't been set yet; e.g.
    // a hidden iframe. In that case, embedderDoc will be null and this will
    // be handled when the embedder is set.
    if (embedderDoc) {
      MOZ_ASSERT(embedderID);
      mozilla::ipc::IPCResult added =
          embedderDoc->AddChildDoc(doc, embedderID,
                                   /* aCreating */ false);
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
    a11y::WrapperFor(doc)->SetID(aMsaaID);
    MOZ_ASSERT(!aDocCOMProxy.IsNull());

    RefPtr<IAccessible> proxy(aDocCOMProxy.Get());
    doc->SetCOMInterface(proxy);
    doc->MaybeInitWindowEmulation();
    if (a11y::Accessible* outerDoc = doc->OuterDocOfRemoteBrowser()) {
      doc->SendParentCOMProxy(outerDoc);
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

  indexedDB::PermissionRequestBase::PermissionValue permission;
  nsresult rv = actor->PromptIfNeeded(&permission);
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }

  if (permission != indexedDB::PermissionRequestBase::kPermissionPrompt) {
    aResolve(permission);
  }

  return IPC_OK();
}

IPCResult BrowserParent::RecvNewWindowGlobal(
    ManagedEndpoint<PWindowGlobalParent>&& aEndpoint,
    const WindowGlobalInit& aInit) {
  if (!aInit.browsingContext().GetMaybeDiscarded()) {
    return IPC_FAIL(this, "Cannot create for missing BrowsingContext");
  }
  if (!aInit.principal()) {
    return IPC_FAIL(this, "Cannot create without valid principal");
  }

  // Construct our new WindowGlobalParent, bind, and initialize it.
  auto wgp = MakeRefPtr<WindowGlobalParent>(aInit, /* inproc */ false);

  BindPWindowGlobalEndpoint(std::move(aEndpoint), wgp);
  wgp->Init(aInit);
  return IPC_OK();
}

void BrowserParent::SendMouseEvent(const nsAString& aType, float aX, float aY,
                                   int32_t aButton, int32_t aClickCount,
                                   int32_t aModifiers,
                                   bool aIgnoreRootScrollFrame) {
  if (!mIsDestroyed) {
    Unused << PBrowserParent::SendMouseEvent(nsString(aType), aX, aY, aButton,
                                             aClickCount, aModifiers,
                                             aIgnoreRootScrollFrame);
  }
}

void BrowserParent::MouseEnterIntoWidget() {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    // When we mouseenter the tab, the tab's cursor should
    // become the current cursor.  When we mouseexit, we stop.
    mTabSetsCursor = true;
    if (mCursor != eCursorInvalid) {
      widget->SetCursor(mCursor, mCustomCursor, mCustomCursorHotspotX,
                        mCustomCursorHotspotY);
    }
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
  aEvent.mRefPoint = TransformParentToChild(aEvent.mRefPoint);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    // When we mouseenter the tab, the tab's cursor should
    // become the current cursor.  When we mouseexit, we stop.
    if (eMouseEnterIntoWidget == aEvent.mMessage) {
      mTabSetsCursor = true;
      if (mCursor != eCursorInvalid) {
        widget->SetCursor(mCursor, mCustomCursor, mCustomCursorHotspotX,
                          mCustomCursorHotspotY);
      }
    } else if (eMouseExitFromWidget == aEvent.mMessage) {
      mTabSetsCursor = false;
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
            ? SendRealMouseButtonEvent(localEvent, guid, blockId)
            : SendNormalPriorityRealMouseButtonEvent(localEvent, guid, blockId);
    NS_WARNING_ASSERTION(
        ret, "SendRealMouseButtonEvent(eMouseEnterIntoWidget) failed");
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

mozilla::ipc::IPCResult BrowserParent::RecvRequestNativeKeyBindings(
    const uint32_t& aType, const WidgetKeyboardEvent& aEvent,
    nsTArray<CommandInt>* aCommands) {
  MOZ_ASSERT(aCommands);
  MOZ_ASSERT(aCommands->IsEmpty());

  nsIWidget::NativeKeyBindingsType keyBindingsType =
      static_cast<nsIWidget::NativeKeyBindingsType>(aType);
  switch (keyBindingsType) {
    case nsIWidget::NativeKeyBindingsForSingleLineEditor:
    case nsIWidget::NativeKeyBindingsForMultiLineEditor:
    case nsIWidget::NativeKeyBindingsForRichTextEditor:
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

  if (localEvent.InitEditCommandsFor(keyBindingsType)) {
    *aCommands = localEvent.EditCommandsConstRef(keyBindingsType);
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
    const uint32_t& aModifierFlags, const uint64_t& aObserverId) {
  AutoSynthesizedEventResponder responder(this, aObserverId, "mouseevent");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeMouseEvent(aPoint, aNativeMessage, aModifierFlags,
                                       responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeMouseMove(
    const LayoutDeviceIntPoint& aPoint, const uint64_t& aObserverId) {
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
  AutoSynthesizedEventResponder responder(this, aObserverId, "touchpoint");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchPoint(aPointerId, aPointerState, aPoint,
                                       aPointerPressure, aPointerOrientation,
                                       responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSynthesizeNativeTouchTap(
    const LayoutDeviceIntPoint& aPoint, const bool& aLongTap,
    const uint64_t& aObserverId) {
  AutoSynthesizedEventResponder responder(this, aObserverId, "touchtap");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchTap(aPoint, aLongTap, responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvClearNativeTouchSequence(
    const uint64_t& aObserverId) {
  AutoSynthesizedEventResponder responder(this, aObserverId, "cleartouch");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->ClearNativeTouchSequence(responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
BrowserParent::RecvSetPrefersReducedMotionOverrideForTest(const bool& aValue) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SetPrefersReducedMotionOverrideForTest(aValue);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
BrowserParent::RecvResetPrefersReducedMotionOverrideForTest() {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->ResetPrefersReducedMotionOverrideForTest();
  }
  return IPC_OK();
}

void BrowserParent::SendRealKeyEvent(WidgetKeyboardEvent& aEvent) {
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return;
  }
  aEvent.mRefPoint = TransformParentToChild(aEvent.mRefPoint);

  if (aEvent.mMessage == eKeyPress) {
    // XXX Should we do this only when input context indicates an editor having
    //     focus and the key event won't cause inputting text?
    aEvent.InitAllEditCommands();
  } else {
    aEvent.PreventNativeKeyBindings();
  }
  DebugOnly<bool> ret =
      Manager()->IsInputPriorityEventEnabled()
          ? PBrowserParent::SendRealKeyEvent(aEvent)
          : PBrowserParent::SendNormalPriorityRealKeyEvent(aEvent);

  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendRealKeyEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
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
    for (int i = aEvent.mTouches.Length() - 1; i >= 0; i--) {
      if (!aEvent.mTouches[i]->mChanged) {
        aEvent.mTouches.RemoveElementAt(i);
      }
    }
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

void BrowserParent::SendPluginEvent(WidgetPluginEvent& aEvent) {
  DebugOnly<bool> ret = PBrowserParent::SendPluginEvent(aEvent);
  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendPluginEvent() failed");
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
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
      if (frameLoader) {
        RefPtr<Element> element = frameLoader->GetOwnerContent();
        if (element) {
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
    nsTArray<CpowEntry>&& aCpows, nsIPrincipal* aPrincipal,
    nsTArray<StructuredCloneData>* aRetVal) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("BrowserParent::RecvSyncMessage",
                                             OTHER, aMessage);
  MMPrinter::Print("BrowserParent::RecvSyncMessage", aMessage, aData);

  StructuredCloneData data;
  ipc::UnpackClonedMessageDataForParent(aData, data);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  if (!ReceiveMessage(aMessage, true, &data, &cpows, aPrincipal, aRetVal)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvRpcMessage(
    const nsString& aMessage, const ClonedMessageData& aData,
    nsTArray<CpowEntry>&& aCpows, nsIPrincipal* aPrincipal,
    nsTArray<StructuredCloneData>* aRetVal) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("BrowserParent::RecvRpcMessage",
                                             OTHER, aMessage);
  MMPrinter::Print("BrowserParent::RecvRpcMessage", aMessage, aData);

  StructuredCloneData data;
  ipc::UnpackClonedMessageDataForParent(aData, data);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  if (!ReceiveMessage(aMessage, true, &data, &cpows, aPrincipal, aRetVal)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvAsyncMessage(
    const nsString& aMessage, nsTArray<CpowEntry>&& aCpows,
    nsIPrincipal* aPrincipal, const ClonedMessageData& aData) {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING("BrowserParent::RecvAsyncMessage",
                                             OTHER, aMessage);
  MMPrinter::Print("BrowserParent::RecvAsyncMessage", aMessage, aData);

  StructuredCloneData data;
  ipc::UnpackClonedMessageDataForParent(aData, data);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  if (!ReceiveMessage(aMessage, false, &data, &cpows, aPrincipal, nullptr)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSetCursor(
    const nsCursor& aCursor, const bool& aHasCustomCursor,
    const nsCString& aCursorData, const uint32_t& aWidth,
    const uint32_t& aHeight, const uint32_t& aStride,
    const gfx::SurfaceFormat& aFormat, const uint32_t& aHotspotX,
    const uint32_t& aHotspotY, const bool& aForce) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  if (aForce) {
    widget->ClearCachedCursor();
  }

  if (!mTabSetsCursor) {
    return IPC_OK();
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

  widget->SetCursor(aCursor, cursorImage, aHotspotX, aHotspotY);
  mCursor = aCursor;
  mCustomCursor = cursorImage;
  mCustomCursorHotspotX = aHotspotX;
  mCustomCursorHotspotY = aHotspotY;

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

  xulBrowserWindow->ShowTooltip(aX, aY, aTooltip, aDirection, el);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvHideTooltip() {
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

void BrowserParent::HandledWindowedPluginKeyEvent(
    const NativeEventData& aKeyEventData, bool aIsConsumed) {
  DebugOnly<bool> ok =
      SendHandledWindowedPluginKeyEvent(aKeyEventData, aIsConsumed);
  NS_WARNING_ASSERTION(ok, "SendHandledWindowedPluginKeyEvent failed");
}

mozilla::ipc::IPCResult BrowserParent::RecvOnWindowedPluginKeyEvent(
    const NativeEventData& aKeyEventData) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (NS_WARN_IF(!widget)) {
    // Notifies the plugin process of the key event being not consumed by us.
    HandledWindowedPluginKeyEvent(aKeyEventData, false);
    return IPC_OK();
  }
  nsresult rv = widget->OnWindowedPluginKeyEvent(aKeyEventData, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Notifies the plugin process of the key event being not consumed by us.
    HandledWindowedPluginKeyEvent(aKeyEventData, false);
    return IPC_OK();
  }

  // If the key event is posted to another process, we need to wait a call
  // of HandledWindowedPluginKeyEvent().  So, nothing to do here in this case.
  if (rv == NS_SUCCESS_EVENT_HANDLED_ASYNCHRONOUSLY) {
    return IPC_OK();
  }

  // Otherwise, the key event is handled synchronously.  Let's notify the
  // plugin process of the key event's result.
  bool consumed = (rv == NS_SUCCESS_EVENT_CONSUMED);
  HandledWindowedPluginKeyEvent(aKeyEventData, consumed);

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

mozilla::ipc::IPCResult BrowserParent::RecvEnableDisableCommands(
    const MaybeDiscarded<BrowsingContext>& aContext, const nsString& aAction,
    nsTArray<nsCString>&& aEnabledCommands,
    nsTArray<nsCString>&& aDisabledCommands) {
  if (aContext.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  nsCOMPtr<nsIBrowserController> browserController =
      do_QueryActor(u"Controllers", aContext.get_canonical());
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
  mChildToParentConversionMatrix = aMatrix;
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

  nsPoint pt = targetFrame->GetOffsetTo(rootFrame);
  return -nsLayoutUtils::TranslateViewToWidget(presContext, rootView, pt,
                                               widget);
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
    const WidgetKeyboardEvent& aEvent) {
  NS_ENSURE_TRUE(mFrameElement, IPC_OK());

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.MarkAsHandledInRemoteProcess();

  // Here we convert the WidgetEvent that we received to an Event
  // to be able to dispatch it to the <browser> element as the target element.
  Document* doc = mFrameElement->OwnerDoc();
  nsPresContext* presContext = doc->GetPresContext();
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

  EventDispatcher::Dispatch(mFrameElement, presContext, &localEvent, nullptr,
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
    nsPresContext* presContext = presShell->GetPresContext();
    NS_ENSURE_TRUE(presContext, IPC_OK());

    EventDispatcher::Dispatch(mFrameElement, presContext, &localEvent);
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
    const Maybe<WebProgressData>& aWebProgressData,
    const RequestData& aRequestData, const uint32_t aStateFlags,
    const nsresult aStatus,
    const Maybe<WebProgressStateChangeData>& aStateChangeData) {
  if (mSuspendedProgressEvents) {
    nsCOMPtr<nsIURI> uri = aRequestData.requestURI();
    const uint32_t startDocumentFlags =
        nsIWebProgressListener::STATE_START |
        nsIWebProgressListener::STATE_IS_DOCUMENT |
        nsIWebProgressListener::STATE_IS_REQUEST |
        nsIWebProgressListener::STATE_IS_WINDOW |
        nsIWebProgressListener::STATE_IS_NETWORK;
    // Once we get a load start from something that isn't the initial
    // about:blank, we should stop blocking future state changes.
    if ((aStateFlags & startDocumentFlags) == startDocumentFlags &&
        (aWebProgressData && aWebProgressData->isTopLevel()) &&
        (!uri || !NS_IsAboutBlank(uri))) {
      mSuspendedProgressEvents = false;
    }

    return IPC_OK();
  }

  nsCOMPtr<nsIBrowser> browser;
  nsCOMPtr<nsIWebProgress> manager;
  nsCOMPtr<nsIWebProgressListener> managerAsListener;
  if (!GetWebProgressListener(getter_AddRefs(browser), getter_AddRefs(manager),
                              getter_AddRefs(managerAsListener))) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWebProgress> webProgress;
  nsCOMPtr<nsIRequest> request;
  ReconstructWebProgressAndRequest(manager, aWebProgressData, aRequestData,
                                   getter_AddRefs(webProgress),
                                   getter_AddRefs(request));

  if (aWebProgressData && aWebProgressData->isTopLevel() &&
      aStateChangeData.isSome()) {
    Unused << browser->SetIsNavigating(aStateChangeData->isNavigating());
    Unused << browser->SetMayEnableCharacterEncodingMenu(
        aStateChangeData->mayEnableCharacterEncodingMenu());
    Unused << browser->SetCharsetAutodetected(
        aStateChangeData->charsetAutodetected());
    Unused << browser->UpdateForStateChange(aStateChangeData->charset(),
                                            aStateChangeData->documentURI(),
                                            aStateChangeData->contentType());
  } else if (aStateChangeData.isSome()) {
    return IPC_FAIL(
        this,
        "Unexpected WebProgressStateChangeData for non-top-level WebProgress");
  }

  Unused << managerAsListener->OnStateChange(webProgress, request, aStateFlags,
                                             aStatus);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvOnProgressChange(
    const Maybe<WebProgressData>& aWebProgressData,
    const RequestData& aRequestData, const int32_t aCurSelfProgress,
    const int32_t aMaxSelfProgress, const int32_t aCurTotalProgress,
    const int32_t aMaxTotalProgress) {
  if (mSuspendedProgressEvents) {
    return IPC_OK();
  }

  nsCOMPtr<nsIBrowser> browser;
  nsCOMPtr<nsIWebProgress> manager;
  nsCOMPtr<nsIWebProgressListener> managerAsListener;
  if (!GetWebProgressListener(getter_AddRefs(browser), getter_AddRefs(manager),
                              getter_AddRefs(managerAsListener))) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWebProgress> webProgress;
  nsCOMPtr<nsIRequest> request;
  ReconstructWebProgressAndRequest(manager, aWebProgressData, aRequestData,
                                   getter_AddRefs(webProgress),
                                   getter_AddRefs(request));

  Unused << managerAsListener->OnProgressChange(
      webProgress, request, aCurSelfProgress, aMaxSelfProgress,
      aCurTotalProgress, aMaxTotalProgress);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvOnLocationChange(
    const Maybe<WebProgressData>& aWebProgressData,
    const RequestData& aRequestData, nsIURI* aLocation, const uint32_t aFlags,
    const bool aCanGoBack, const bool aCanGoForward,
    const Maybe<WebProgressLocationChangeData>& aLocationChangeData) {
  if (mSuspendedProgressEvents) {
    return IPC_OK();
  }

  nsCOMPtr<nsIBrowser> browser;
  nsCOMPtr<nsIWebProgress> manager;
  nsCOMPtr<nsIWebProgressListener> managerAsListener;
  if (!GetWebProgressListener(getter_AddRefs(browser), getter_AddRefs(manager),
                              getter_AddRefs(managerAsListener))) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWebProgress> webProgress;
  nsCOMPtr<nsIRequest> request;
  ReconstructWebProgressAndRequest(manager, aWebProgressData, aRequestData,
                                   getter_AddRefs(webProgress),
                                   getter_AddRefs(request));

  Unused << browser->UpdateWebNavigationForLocationChange(aCanGoBack,
                                                          aCanGoForward);

  if (aWebProgressData && aWebProgressData->isTopLevel() &&
      aLocationChangeData.isSome()) {
    nsCOMPtr<nsIPrincipal> contentBlockingAllowListPrincipal;
    Unused << browser->SetIsNavigating(aLocationChangeData->isNavigating());
    Unused << browser->UpdateForLocationChange(
        aLocation, aLocationChangeData->charset(),
        aLocationChangeData->mayEnableCharacterEncodingMenu(),
        aLocationChangeData->charsetAutodetected(),
        aLocationChangeData->documentURI(), aLocationChangeData->title(),
        aLocationChangeData->contentPrincipal(),
        aLocationChangeData->contentStoragePrincipal(),
        aLocationChangeData->csp(), aLocationChangeData->referrerInfo(),
        aLocationChangeData->isSyntheticDocument(),
        aWebProgressData->innerDOMWindowID(),
        aLocationChangeData->requestContextID().isSome(),
        aLocationChangeData->requestContextID().valueOr(0),
        aLocationChangeData->contentType());
  }

  Unused << managerAsListener->OnLocationChange(webProgress, request, aLocation,
                                                aFlags);

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvOnStatusChange(
    const Maybe<WebProgressData>& aWebProgressData,
    const RequestData& aRequestData, const nsresult aStatus,
    const nsString& aMessage) {
  if (mSuspendedProgressEvents) {
    return IPC_OK();
  }

  nsCOMPtr<nsIBrowser> browser;
  nsCOMPtr<nsIWebProgress> manager;
  nsCOMPtr<nsIWebProgressListener> managerAsListener;
  if (!GetWebProgressListener(getter_AddRefs(browser), getter_AddRefs(manager),
                              getter_AddRefs(managerAsListener))) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWebProgress> webProgress;
  nsCOMPtr<nsIRequest> request;
  ReconstructWebProgressAndRequest(manager, aWebProgressData, aRequestData,
                                   getter_AddRefs(webProgress),
                                   getter_AddRefs(request));

  Unused << managerAsListener->OnStatusChange(webProgress, request, aStatus,
                                              aMessage.get());

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvOnSecurityChange(
    const Maybe<WebProgressData>& aWebProgressData,
    const RequestData& aRequestData, const uint32_t aState,
    const Maybe<WebProgressSecurityChangeData>& aSecurityChangeData) {
  nsCOMPtr<nsIBrowser> browser;
  nsCOMPtr<nsIWebProgress> manager;
  nsCOMPtr<nsIWebProgressListener> managerAsListener;
  if (!GetWebProgressListener(getter_AddRefs(browser), getter_AddRefs(manager),
                              getter_AddRefs(managerAsListener))) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWebProgress> webProgress;
  nsCOMPtr<nsIRequest> request;
  ReconstructWebProgressAndRequest(manager, aWebProgressData, aRequestData,
                                   getter_AddRefs(webProgress),
                                   getter_AddRefs(request));

  if (aWebProgressData && aWebProgressData->isTopLevel() &&
      aSecurityChangeData.isSome()) {
    Unused << browser->UpdateSecurityUIForSecurityChange(
        aSecurityChangeData->securityInfo(), aState,
        aSecurityChangeData->isSecureContext());
  }

  Unused << managerAsListener->OnSecurityChange(webProgress, request, aState);

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
    const Maybe<mozilla::ContentBlockingNotifier::StorageAccessGrantedReason>&
        aReason) {
  MOZ_ASSERT(aRequestData.elapsedLoadTimeMS().isNothing());

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
      aRequestData.matchedList(), aRequestData.elapsedLoadTimeMS());

  wgp->NotifyContentBlockingEvent(aEvent, request, aBlocked, aTrackingOrigin,
                                  aTrackingFullHashes, aReason);

  return IPC_OK();
}

bool BrowserParent::GetWebProgressListener(
    nsIBrowser** aOutBrowser, nsIWebProgress** aOutManager,
    nsIWebProgressListener** aOutListener) {
  MOZ_ASSERT(aOutBrowser);
  MOZ_ASSERT(aOutManager);
  MOZ_ASSERT(aOutListener);

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

  if (!browser) {
    return false;
  }

  nsCOMPtr<nsIWebProgress> manager;
  nsresult rv = browser->GetRemoteWebProgressManager(getter_AddRefs(manager));
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(manager);
  if (!listener) {
    // We are no longer remote so we cannot forward this event.
    return false;
  }

  browser.forget(aOutBrowser);
  manager.forget(aOutManager);
  listener.forget(aOutListener);

  return true;
}

void BrowserParent::ReconstructWebProgressAndRequest(
    nsIWebProgress* aManager, const Maybe<WebProgressData>& aWebProgressData,
    const RequestData& aRequestData, nsIWebProgress** aOutWebProgress,
    nsIRequest** aOutRequest) {
  MOZ_DIAGNOSTIC_ASSERT(aOutWebProgress,
                        "aOutWebProgress should never be null");
  MOZ_DIAGNOSTIC_ASSERT(aOutRequest, "aOutRequest should never be null");

  nsCOMPtr<nsIWebProgress> webProgress;
  if (aWebProgressData) {
    webProgress = new RemoteWebProgress(
        aManager, aWebProgressData->outerDOMWindowID(),
        aWebProgressData->innerDOMWindowID(), aWebProgressData->loadType(),
        aWebProgressData->isLoadingDocument(), aWebProgressData->isTopLevel());
  } else {
    webProgress = new RemoteWebProgress(aManager, 0, 0, 0, false, false);
  }
  webProgress.forget(aOutWebProgress);

  if (aRequestData.requestURI()) {
    nsCOMPtr<nsIRequest> request = MakeAndAddRef<RemoteWebProgressRequest>(
        aRequestData.requestURI(), aRequestData.originalRequestURI(),
        aRequestData.matchedList(), aRequestData.elapsedLoadTimeMS());
    request.forget(aOutRequest);
  } else {
    *aOutRequest = nullptr;
  }
}

mozilla::ipc::IPCResult BrowserParent::RecvSessionStoreUpdate(
    const Maybe<nsCString>& aDocShellCaps, const Maybe<bool>& aPrivatedMode,
    nsTArray<nsCString>&& aPositions, nsTArray<int32_t>&& aPositionDescendants,
    const nsTArray<InputFormData>& aInputs,
    const nsTArray<CollectedInputDataValue>& aIdVals,
    const nsTArray<CollectedInputDataValue>& aXPathVals,
    nsTArray<nsCString>&& aOrigins, nsTArray<nsString>&& aKeys,
    nsTArray<nsString>&& aValues, const bool aIsFullStorage,
    const bool aNeedCollectSHistory, const uint32_t& aFlushId,
    const bool& aIsFinal, const uint32_t& aEpoch) {
  UpdateSessionStoreData data;
  if (aDocShellCaps.isSome()) {
    data.mDocShellCaps.Construct() = aDocShellCaps.value();
  }
  if (aPrivatedMode.isSome()) {
    data.mIsPrivate.Construct() = aPrivatedMode.value();
  }
  if (aPositions.Length() != 0) {
    data.mPositions.Construct(std::move(aPositions));
    data.mPositionDescendants.Construct(std::move(aPositionDescendants));
  }
  if (aIdVals.Length() != 0) {
    SessionStoreUtils::ComposeInputData(aIdVals, data.mId.Construct());
  }
  if (aXPathVals.Length() != 0) {
    SessionStoreUtils::ComposeInputData(aXPathVals, data.mXpath.Construct());
  }
  if (aInputs.Length() != 0) {
    nsTArray<int> descendants, numId, numXPath;
    nsTArray<nsString> innerHTML;
    nsTArray<nsCString> url;
    for (const InputFormData& input : aInputs) {
      descendants.AppendElement(input.descendants);
      numId.AppendElement(input.numId);
      numXPath.AppendElement(input.numXPath);
      innerHTML.AppendElement(input.innerHTML);
      url.AppendElement(input.url);
    }

    data.mInputDescendants.Construct(std::move(descendants));
    data.mNumId.Construct(std::move(numId));
    data.mNumXPath.Construct(std::move(numXPath));
    data.mInnerHTML.Construct(std::move(innerHTML));
    data.mUrl.Construct(std::move(url));
  }
  // In normal case, we only update the storage when needed.
  // However, we need to reset the session storage(aOrigins.Length() will be 0)
  //   if the usage is over the "browser_sessionstore_dom_storage_limit".
  // In this case, aIsFullStorage is true.
  if (aOrigins.Length() != 0 || aIsFullStorage) {
    data.mStorageOrigins.Construct(std::move(aOrigins));
    data.mStorageKeys.Construct(std::move(aKeys));
    data.mStorageValues.Construct(std::move(aValues));
    data.mIsFullStorage.Construct() = aIsFullStorage;
  }

  nsCOMPtr<nsISessionStoreFunctions> funcs =
      do_ImportModule("resource://gre/modules/SessionStoreFunctions.jsm");
  NS_ENSURE_TRUE(funcs, IPC_OK());
  nsCOMPtr<nsIXPConnectWrappedJS> wrapped = do_QueryInterface(funcs);
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(wrapped->GetJSObjectGlobal()));
  JS::Rooted<JS::Value> dataVal(jsapi.cx());
  bool ok = ToJSValue(jsapi.cx(), data, &dataVal);
  NS_ENSURE_TRUE(ok, IPC_OK());

  nsresult rv = funcs->UpdateSessionStore(
      mFrameElement, aFlushId, aIsFinal, aEpoch, dataVal, aNeedCollectSHistory);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  return IPC_OK();
}

bool BrowserParent::HandleQueryContentEvent(WidgetQueryContentEvent& aEvent) {
  nsCOMPtr<nsIWidget> textInputHandlingWidget = GetTextInputHandlingWidget();
  if (!textInputHandlingWidget) {
    return true;
  }
  if (NS_WARN_IF(!mContentCache.HandleQueryContentEvent(
          aEvent, textInputHandlingWidget)) ||
      NS_WARN_IF(!aEvent.mSucceeded)) {
    return true;
  }
  switch (aEvent.mMessage) {
    case eQueryTextRect:
    case eQueryCaretRect:
    case eQueryEditorRect: {
      nsCOMPtr<nsIWidget> browserWidget = GetWidget();
      if (browserWidget != textInputHandlingWidget) {
        aEvent.mReply.mRect += nsLayoutUtils::WidgetToWidgetOffset(
            browserWidget, textInputHandlingWidget);
      }
      aEvent.mReply.mRect = TransformChildToParent(aEvent.mReply.mRect);
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

bool BrowserParent::SendPasteTransferable(const IPCDataTransfer& aDataTransfer,
                                          const bool& aIsPrivateData,
                                          nsIPrincipal* aRequestingPrincipal,
                                          const uint32_t& aContentPolicyType) {
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

mozilla::ipc::IPCResult BrowserParent::RecvStartPluginIME(
    const WidgetKeyboardEvent& aKeyboardEvent, const int32_t& aPanelX,
    const int32_t& aPanelY, nsString* aCommitted) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }
  Unused << widget->StartPluginIME(aKeyboardEvent, (int32_t&)aPanelX,
                                   (int32_t&)aPanelY, *aCommitted);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSetPluginFocused(
    const bool& aFocused) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }
  widget->SetPluginFocused((bool&)aFocused);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvSetCandidateWindowForPlugin(
    const CandidateWindowPosition& aPosition) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  widget->SetCandidateWindowForPlugin(aPosition);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvEnableIMEForPlugin(
    const bool& aEnable) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }
  widget->EnableIMEForPlugin(aEnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvDefaultProcOfPluginEvent(
    const WidgetPluginEvent& aEvent) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  widget->DefaultProcOfPluginEvent(aEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvGetInputContext(
    widget::IMEState* aState) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    *aState = widget::IMEState(IMEState::DISABLED,
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

mozilla::ipc::IPCResult BrowserParent::RecvSetNativeChildOfShareableWindow(
    const uintptr_t& aChildWindow) {
#if defined(XP_WIN)
  nsCOMPtr<nsIWidget> widget = GetTopLevelWidget();
  if (widget) {
    // Note that this call will probably cause a sync native message to the
    // process that owns the child window.
    widget->SetNativeData(NS_NATIVE_CHILD_OF_SHAREABLE_WINDOW, aChildWindow);
  }
  return IPC_OK();
#else
  MOZ_ASSERT_UNREACHABLE(
      "BrowserParent::RecvSetNativeChildOfShareableWindow not implemented!");
  return IPC_FAIL_NO_REASON(this);
#endif
}

mozilla::ipc::IPCResult BrowserParent::RecvDispatchFocusToTopLevelWindow() {
  if (nsCOMPtr<nsIWidget> widget = GetTopLevelWidget()) {
    widget->SetFocus(nsIWidget::Raise::No, CallerType::System);
  }
  return IPC_OK();
}

bool BrowserParent::ReceiveMessage(const nsString& aMessage, bool aSync,
                                   StructuredCloneData* aData,
                                   CpowHolder* aCpows, nsIPrincipal* aPrincipal,
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
                            aCpows, aPrincipal, aRetVal, IgnoreErrors());
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

mozilla::ipc::IPCResult BrowserParent::RecvBrowserFrameOpenWindow(
    PBrowserParent* aOpener, const nsString& aURL, const nsString& aName,
    bool aForceNoReferrer, const nsString& aFeatures,
    BrowserFrameOpenWindowResolver&& aResolve) {
  CreatedWindowInfo cwi;
  cwi.rv() = NS_OK;
  cwi.maxTouchPoints() = 0;

  BrowserElementParent::OpenWindowResult opened =
      BrowserElementParent::OpenWindowOOP(BrowserParent::GetFrom(aOpener), this,
                                          aURL, aName, aForceNoReferrer,
                                          aFeatures);
  cwi.windowOpened() = (opened == BrowserElementParent::OPEN_WINDOW_ADDED);
  cwi.maxTouchPoints() = GetMaxTouchPoints();

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    cwi.dimensions() = GetDimensionInfo();
  }

  // Resolve the request with the information we collected.
  aResolve(cwi);

  if (!cwi.windowOpened()) {
    Destroy();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvRespondStartSwipeEvent(
    const uint64_t& aInputBlockId, const bool& aStartSwipe) {
  if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
    widget->ReportSwipeStarted(aInputBlockId, aStartSwipe);
  }
  return IPC_OK();
}

bool BrowserParent::GetDocShellIsActive() { return mDocShellIsActive; }

void BrowserParent::SetDocShellIsActive(bool isActive) {
  mDocShellIsActive = isActive;
  SetRenderLayers(isActive);
  Unused << SendSetDocShellIsActive(isActive);

  // update active accessible documents on windows
#if defined(XP_WIN) && defined(ACCESSIBILITY)
  if (a11y::Compatibility::IsDolphin()) {
    if (a11y::DocAccessibleParent* tabDoc = GetTopLevelDocAccessible()) {
      HWND window = tabDoc->GetEmulatedWindowHandle();
      MOZ_ASSERT(window);
      if (window) {
        if (isActive) {
          a11y::nsWinUtils::ShowNativeWindow(window);
        } else {
          a11y::nsWinUtils::HideNativeWindow(window);
        }
      }
    }
  }
#endif
}

bool BrowserParent::GetHasPresented() { return mHasPresented; }

bool BrowserParent::GetHasLayers() { return mHasLayers; }

bool BrowserParent::GetRenderLayers() { return mRenderLayers; }

void BrowserParent::SetRenderLayers(bool aEnabled) {
  if (mActiveInPriorityManager != aEnabled) {
    mActiveInPriorityManager = aEnabled;
    // Let's inform the priority manager. This operation can end up with the
    // changing of the process priority.
    ProcessPriorityManager::TabActivityChanged(this, aEnabled);
  }

  if (aEnabled == mRenderLayers) {
    if (aEnabled && mHasLayers && mPreserveLayers) {
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
  if (!aEnabled && mPreserveLayers) {
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

void BrowserParent::PreserveLayers(bool aPreserveLayers) {
  mPreserveLayers = aPreserveLayers;
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

void BrowserParent::Deprioritize() {
  if (mActiveInPriorityManager) {
    ProcessPriorityManager::TabActivityChanged(this, false);
    mActiveInPriorityManager = false;
  }
}

bool BrowserParent::StartApzAutoscroll(float aAnchorX, float aAnchorY,
                                       nsViewID aScrollId,
                                       uint32_t aPresShellId) {
  if (!AsyncPanZoomEnabled()) {
    return false;
  }

  bool success = false;
  if (mRemoteLayerTreeOwner.IsInitialized()) {
    layers::LayersId layersId = mRemoteLayerTreeOwner.GetLayersId();
    if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
      ScrollableLayerGuid guid(layersId, aPresShellId, aScrollId);

      // The anchor coordinates that are passed in are relative to the origin
      // of the screen, but we are sending them to APZ which only knows about
      // coordinates relative to the widget, so convert them accordingly.
      CSSPoint anchorCss{aAnchorX, aAnchorY};
      LayoutDeviceIntPoint anchor =
          RoundedToInt(anchorCss * widget->GetDefaultScale());
      anchor -= widget->WidgetToScreenOffset();

      success = widget->StartAsyncAutoscroll(
          ViewAs<ScreenPixel>(
              anchor, PixelCastJustification::LayoutDeviceIsScreenForBounds),
          guid);
    }
  }
  return success;
}

void BrowserParent::StopApzAutoscroll(nsViewID aScrollId,
                                      uint32_t aPresShellId) {
  if (!AsyncPanZoomEnabled()) {
    return;
  }

  if (mRemoteLayerTreeOwner.IsInitialized()) {
    layers::LayersId layersId = mRemoteLayerTreeOwner.GetLayersId();
    if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
      ScrollableLayerGuid guid(layersId, aPresShellId, aScrollId);

      widget->StopAsyncAutoscroll(guid);
    }
  }
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

  RefPtr<EventTarget> target = mFrameElement;
  if (!target) {
    NS_WARNING("Could not locate target for layer tree message.");
    return;
  }

  mHasLayers = aActive;

  RefPtr<Event> event = NS_NewDOMEvent(mFrameElement, nullptr, nullptr);
  if (aActive) {
    mHasPresented = true;
    event->InitEvent(NS_LITERAL_STRING("MozLayerTreeReady"), true, false);
  } else {
    event->InitEvent(NS_LITERAL_STRING("MozLayerTreeCleared"), true, false);
  }
  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
  mFrameElement->DispatchEvent(*event);
}

mozilla::ipc::IPCResult BrowserParent::RecvPaintWhileInterruptingJSNoOp(
    const LayersObserverEpoch& aEpoch) {
  // We sent a PaintWhileInterruptingJS message when layers were already
  // visible. In this case, we should act as if an update occurred even though
  // we already have the layers.
  LayerTreeUpdate(aEpoch, true);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvRemotePaintIsReady() {
  RefPtr<EventTarget> target = mFrameElement;
  if (!target) {
    NS_WARNING("Could not locate target for MozAfterRemotePaint message.");
    return IPC_OK();
  }

  RefPtr<Event> event = NS_NewDOMEvent(mFrameElement, nullptr, nullptr);
  event->InitEvent(NS_LITERAL_STRING("MozAfterRemotePaint"), false, false);
  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
  mFrameElement->DispatchEvent(*event);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvNotifyCompositorTransaction() {
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();

  if (!frameLoader) {
    return IPC_OK();
  }

  nsIFrame* docFrame = frameLoader->GetPrimaryFrameOfOwningContent();

  if (!docFrame) {
    // Bad, but nothing we can do about it (XXX/cjones: or is there?
    // maybe bug 589337?).  When the new frame is created, we'll
    // probably still be the current render frame and will get to draw
    // our content then.  Or, we're shutting down and this update goes
    // to /dev/null.
    return IPC_OK();
  }

  docFrame->InvalidateLayer(DisplayItemType::TYPE_REMOTE);
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

mozilla::plugins::PPluginWidgetParent*
BrowserParent::AllocPPluginWidgetParent() {
#ifdef XP_WIN
  return new mozilla::plugins::PluginWidgetParent();
#else
  MOZ_ASSERT_UNREACHABLE("AllocPPluginWidgetParent only supports Windows");
  return nullptr;
#endif
}

bool BrowserParent::DeallocPPluginWidgetParent(
    mozilla::plugins::PPluginWidgetParent* aActor) {
  delete aActor;
  return true;
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

class FakeChannel final : public nsIChannel,
                          public nsIAuthPromptCallback,
                          public nsIInterfaceRequestor,
                          public nsILoadContext {
 public:
  FakeChannel(const nsCString& aUri, uint64_t aCallbackId, Element* aElement)
      : mCallbackId(aCallbackId), mElement(aElement) {
    NS_NewURI(getter_AddRefs(mUri), aUri);
  }

  NS_DECL_ISUPPORTS

#define NO_IMPL \
  override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetName(nsACString&) NO_IMPL;
  NS_IMETHOD IsPending(bool*) NO_IMPL;
  NS_IMETHOD GetStatus(nsresult*) NO_IMPL;
  NS_IMETHOD Cancel(nsresult) NO_IMPL;
  NS_IMETHOD GetCanceled(bool* aCanceled) NO_IMPL;
  NS_IMETHOD Suspend() NO_IMPL;
  NS_IMETHOD Resume() NO_IMPL;
  NS_IMETHOD GetLoadGroup(nsILoadGroup**) NO_IMPL;
  NS_IMETHOD SetLoadGroup(nsILoadGroup*) NO_IMPL;
  NS_IMETHOD SetLoadFlags(nsLoadFlags) NO_IMPL;
  NS_IMETHOD GetLoadFlags(nsLoadFlags*) NO_IMPL;
  NS_IMETHOD GetTRRMode(nsIRequest::TRRMode* aTRRMode) NO_IMPL;
  NS_IMETHOD SetTRRMode(nsIRequest::TRRMode aMode) NO_IMPL;
  NS_IMETHOD GetIsDocument(bool*) NO_IMPL;
  NS_IMETHOD GetOriginalURI(nsIURI**) NO_IMPL;
  NS_IMETHOD SetOriginalURI(nsIURI*) NO_IMPL;
  NS_IMETHOD GetURI(nsIURI** aUri) override {
    nsCOMPtr<nsIURI> copy = mUri;
    copy.forget(aUri);
    return NS_OK;
  }
  NS_IMETHOD GetOwner(nsISupports**) NO_IMPL;
  NS_IMETHOD SetOwner(nsISupports*) NO_IMPL;
  NS_IMETHOD GetLoadInfo(nsILoadInfo** aLoadInfo) override {
    nsCOMPtr<nsILoadInfo> copy = mLoadInfo;
    copy.forget(aLoadInfo);
    return NS_OK;
  }
  NS_IMETHOD SetLoadInfo(nsILoadInfo* aLoadInfo) override {
    mLoadInfo = aLoadInfo;
    return NS_OK;
  }
  NS_IMETHOD GetNotificationCallbacks(
      nsIInterfaceRequestor** aRequestor) override {
    NS_ADDREF(*aRequestor = this);
    return NS_OK;
  }
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor*) NO_IMPL;
  NS_IMETHOD GetSecurityInfo(nsISupports**) NO_IMPL;
  NS_IMETHOD GetContentType(nsACString&) NO_IMPL;
  NS_IMETHOD SetContentType(const nsACString&) NO_IMPL;
  NS_IMETHOD GetContentCharset(nsACString&) NO_IMPL;
  NS_IMETHOD SetContentCharset(const nsACString&) NO_IMPL;
  NS_IMETHOD GetContentLength(int64_t*) NO_IMPL;
  NS_IMETHOD SetContentLength(int64_t) NO_IMPL;
  NS_IMETHOD Open(nsIInputStream**) NO_IMPL;
  NS_IMETHOD AsyncOpen(nsIStreamListener*) NO_IMPL;
  NS_IMETHOD GetContentDisposition(uint32_t*) NO_IMPL;
  NS_IMETHOD SetContentDisposition(uint32_t) NO_IMPL;
  NS_IMETHOD GetContentDispositionFilename(nsAString&) NO_IMPL;
  NS_IMETHOD SetContentDispositionFilename(const nsAString&) NO_IMPL;
  NS_IMETHOD GetContentDispositionHeader(nsACString&) NO_IMPL;
  NS_IMETHOD OnAuthAvailable(nsISupports* aContext,
                             nsIAuthInformation* aAuthInfo) override;
  NS_IMETHOD OnAuthCancelled(nsISupports* aContext, bool userCancel) override;
  NS_IMETHOD GetInterface(const nsIID& uuid, void** result) override {
    return QueryInterface(uuid, result);
  }
  NS_IMETHOD GetAssociatedWindow(mozIDOMWindowProxy**) NO_IMPL;
  NS_IMETHOD GetTopWindow(mozIDOMWindowProxy**) NO_IMPL;
  NS_IMETHOD GetTopFrameElement(Element** aElement) override {
    RefPtr<Element> elem = mElement;
    elem.forget(aElement);
    return NS_OK;
  }
  NS_IMETHOD GetNestedFrameId(uint64_t*) NO_IMPL;
  NS_IMETHOD GetIsContent(bool*) NO_IMPL;
  NS_IMETHOD GetUsePrivateBrowsing(bool*) NO_IMPL;
  NS_IMETHOD SetUsePrivateBrowsing(bool) NO_IMPL;
  NS_IMETHOD SetPrivateBrowsing(bool) NO_IMPL;
  NS_IMETHOD GetScriptableOriginAttributes(JSContext*,
                                           JS::MutableHandleValue) NO_IMPL;
  NS_IMETHOD_(void)
  GetOriginAttributes(mozilla::OriginAttributes& aAttrs) override {}
  NS_IMETHOD GetUseRemoteTabs(bool*) NO_IMPL;
  NS_IMETHOD SetRemoteTabs(bool) NO_IMPL;
  NS_IMETHOD GetUseRemoteSubframes(bool*) NO_IMPL;
  NS_IMETHOD SetRemoteSubframes(bool) NO_IMPL;
  NS_IMETHOD GetUseTrackingProtection(bool*) NO_IMPL;
  NS_IMETHOD SetUseTrackingProtection(bool) NO_IMPL;
#undef NO_IMPL

 protected:
  ~FakeChannel() = default;

  nsCOMPtr<nsIURI> mUri;
  uint64_t mCallbackId;
  RefPtr<Element> mElement;
  nsCOMPtr<nsILoadInfo> mLoadInfo;
};

NS_IMPL_ISUPPORTS(FakeChannel, nsIChannel, nsIAuthPromptCallback, nsIRequest,
                  nsIInterfaceRequestor, nsILoadContext);

mozilla::ipc::IPCResult BrowserParent::RecvAsyncAuthPrompt(
    const nsCString& aUri, const nsString& aRealm,
    const uint64_t& aCallbackId) {
  nsCOMPtr<nsIAuthPrompt2> authPrompt;
  GetAuthPrompt(nsIAuthPromptProvider::PROMPT_NORMAL,
                NS_GET_IID(nsIAuthPrompt2), getter_AddRefs(authPrompt));
  RefPtr<FakeChannel> channel =
      new FakeChannel(aUri, aCallbackId, mFrameElement);
  uint32_t promptFlags = nsIAuthInformation::AUTH_HOST;

  RefPtr<nsAuthInformationHolder> holder =
      new nsAuthInformationHolder(promptFlags, aRealm, EmptyCString());

  uint32_t level = nsIAuthPrompt2::LEVEL_NONE;
  nsCOMPtr<nsICancelable> dummy;
  nsresult rv = authPrompt->AsyncPromptAuth(channel, channel, nullptr, level,
                                            holder, getter_AddRefs(dummy));

  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvInvokeDragSession(
    nsTArray<IPCDataTransfer>&& aTransfers, const uint32_t& aAction,
    Maybe<Shmem>&& aVisualDnDData, const uint32_t& aStride,
    const gfx::SurfaceFormat& aFormat, const LayoutDeviceIntRect& aDragRect,
    nsIPrincipal* aPrincipal, nsIContentSecurityPolicy* aCsp) {
  PresShell* presShell = mFrameElement->OwnerDoc()->GetPresShell();
  if (!presShell) {
    Unused << Manager()->SendEndDragSession(true, true, LayoutDeviceIntPoint(),
                                            0);
    // Continue sending input events with input priority when stopping the dnd
    // session.
    Manager()->SetInputPriorityEventEnabled(true);
    return IPC_OK();
  }

  RefPtr<RemoteDragStartData> dragStartData = new RemoteDragStartData(
      this, std::move(aTransfers), aDragRect, aPrincipal, aCsp);

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
    uint64_t aOuterWindowID, nsIWebBrowserPersistDocumentReceiver* aRecv,
    ErrorResult& aRv) {
  auto* actor = new WebBrowserPersistDocumentParent();
  actor->SetOnReady(aRecv);
  bool ok = Manager()->SendPWebBrowserPersistDocumentConstructor(
      actor, this, aOuterWindowID);
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
  nsCOMPtr<IHistory> history = services::GetHistoryService();
  if (history) {
    Unused << history->VisitURI(widget, aURI, aLastVisitedURI, aFlags);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserParent::RecvQueryVisitedState(
    const nsTArray<RefPtr<nsIURI>>&& aURIs) {
#ifdef MOZ_ANDROID_HISTORY
  nsCOMPtr<IHistory> history = services::GetHistoryService();
  if (NS_WARN_IF(!history)) {
    return IPC_OK();
  }
  RefPtr<nsIWidget> widget = GetWidget();
  if (NS_WARN_IF(!widget)) {
    return IPC_OK();
  }

  for (size_t i = 0; i < aURIs.Length(); ++i) {
    if (!aURIs[i]) {
      return IPC_FAIL(this, "Received null URI");
    }
  }

  GeckoViewHistory* gvHistory = static_cast<GeckoViewHistory*>(history.get());
  gvHistory->QueryVisitedState(widget, std::move(aURIs));

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
    bool aFireLoadAtEmbeddingElement) {
  BrowserBridgeParent* bridge = GetBrowserBridgeParent();
  if (!bridge) {
    NS_WARNING("Received `load` event on unbridged BrowserParent!");
    return IPC_OK();
  }

  Unused << bridge->SendMaybeFireEmbedderLoadEvents(
      aFireLoadAtEmbeddingElement);
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

NS_IMETHODIMP
FakeChannel::OnAuthAvailable(nsISupports* aContext,
                             nsIAuthInformation* aAuthInfo) {
  nsAuthInformationHolder* holder =
      static_cast<nsAuthInformationHolder*>(aAuthInfo);

  if (!net::gNeckoChild->SendOnAuthAvailable(
          mCallbackId, holder->User(), holder->Password(), holder->Domain())) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
FakeChannel::OnAuthCancelled(nsISupports* aContext, bool userCancel) {
  if (!net::gNeckoChild->SendOnAuthCancelled(mCallbackId, userCancel)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void BrowserParent::OnSubFrameCrashed() {
  if (mBrowsingContext->IsDiscarded()) {
    return;
  }

  auto processId = Manager()->ChildID();
  BrowsingContext* parent = mBrowsingContext->GetParent();
  ContentParent* embedderProcess = parent->Canonical()->GetContentParent();
  if (!embedderProcess) {
    return;
  }

  ContentParent* manager = Manager();
  // Set the owner process of a browsing context belonging to a
  // crashed process to the parent context's process, since
  // we'll be showing the crashed page in that process.
  mBrowsingContext->SetOwnerProcessId(embedderProcess->ChildID());

  // Find all same process sub tree nodes and detach them, cache all
  // other nodes in the sub tree.
  mBrowsingContext->PostOrderWalk([&](auto* aContext) {
    // By iterating in reverse we can deal with detach removing the child that
    // we're currently on
    for (auto it = aContext->GetChildren().rbegin();
         it != aContext->GetChildren().rend(); it++) {
      RefPtr<BrowsingContext> context = *it;
      if (context->Canonical()->IsOwnedByProcess(processId)) {
        // Hold a reference to `context` until the response comes back to
        // ensure it doesn't die while messages relating to this context are
        // in-flight.
        auto resolve = [context](bool) {};
        auto reject = [context](ResponseRejectReason) {};
        context->Group()->EachOtherParent(manager, [&](auto* aParent) {
          aParent->SendDetachBrowsingContext(context->Id(), resolve, reject);
        });

        context->Detach(/* aFromIPC */ true);
      }
    }

    // Cache all the children not owned by crashing process. Note that
    // all remaining children are out of process, which makes it ok to
    // just cache.
    aContext->Group()->EachOtherParent(manager, [&](auto* aParent) {
      Unused << aParent->SendCacheBrowsingContextChildren(aContext);
    });
    aContext->CacheChildren(/* aFromIPC */ true);
  });

  MOZ_DIAGNOSTIC_ASSERT(!mBrowsingContext->GetChildren().Length());
  // Tell the browser bridge to show the subframe crashed page.
  if (GetBrowserBridgeParent()) {
    Unused << GetBrowserBridgeParent()->SendSubFrameCrashed(mBrowsingContext);
  }
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

}  // namespace dom
}  // namespace mozilla
