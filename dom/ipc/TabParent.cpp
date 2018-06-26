/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "TabParent.h"

#ifdef ACCESSIBILITY
#include "mozilla/a11y/DocAccessibleParent.h"
#include "nsAccessibilityService.h"
#endif
#include "mozilla/BrowserElementParent.h"
#include "mozilla/dom/ChromeMessageSender.h"
#include "mozilla/dom/ContentBridgeParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DataTransferItemList.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/PaymentRequestParent.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/Hal.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/layers/AsyncDragMetrics.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/plugins/PPluginWidgetParent.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsContentAreaDragDrop.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsFrameManager.h"
#include "nsIBaseWindow.h"
#include "nsIBrowser.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadInfo.h"
#include "nsIPromptFactory.h"
#include "nsIURI.h"
#include "nsIWindowWatcher.h"
#include "nsIWebBrowserChrome.h"
#include "nsIXULBrowserWindow.h"
#include "nsIXULWindow.h"
#include "nsIRemoteBrowser.h"
#include "nsViewManager.h"
#include "nsVariant.h"
#include "nsIWidget.h"
#include "nsNetUtil.h"
#ifndef XP_WIN
#include "nsJARProtocolHandler.h"
#endif
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "PermissionMessageUtils.h"
#include "StructuredCloneData.h"
#include "ColorPickerParent.h"
#include "FilePickerParent.h"
#include "TabChild.h"
#include "LoadContext.h"
#include "nsNetCID.h"
#include "nsIAuthInformation.h"
#include "nsIAuthPromptCallback.h"
#include "nsAuthInformationHolder.h"
#include "nsICancelable.h"
#include "gfxPrefs.h"
#include "nsILoginManagerPrompter.h"
#include "nsPIWindowRoot.h"
#include "nsIAuthPrompt2.h"
#include "gfxDrawable.h"
#include "ImageOps.h"
#include "UnitTransforms.h"
#include <algorithm>
#include "mozilla/WebBrowserPersistDocumentParent.h"
#include "ProcessPriorityManager.h"
#include "nsString.h"
#include "NullPrincipal.h"

#ifdef XP_WIN
#include "mozilla/plugins/PluginWidgetParent.h"
#endif

#if defined(XP_WIN) && defined(ACCESSIBILITY)
#include "mozilla/a11y/AccessibleWrap.h"
#include "mozilla/a11y/Compatibility.h"
#include "mozilla/a11y/nsWinUtils.h"
#endif

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::services;
using namespace mozilla::widget;
using namespace mozilla::jsipc;
using namespace mozilla::gfx;

using mozilla::Unused;

// The flags passed by the webProgress notifications are 16 bits shifted
// from the ones registered by webProgressListeners.
#define NOTIFY_FLAG_SHIFT 16

namespace mozilla {
namespace dom {

TabParent::LayerToTabParentTable* TabParent::sLayerToTabParentTable = nullptr;

NS_IMPL_ISUPPORTS(TabParent,
                  nsITabParent,
                  nsIAuthPromptProvider,
                  nsISecureBrowserUI,
                  nsISupportsWeakReference)

TabParent::TabParent(nsIContentParent* aManager,
                     const TabId& aTabId,
                     const TabContext& aContext,
                     uint32_t aChromeFlags)
  : TabContext(aContext)
  , mFrameElement(nullptr)
  , mContentCache(*this)
  , mRect(0, 0, 0, 0)
  , mDimensions(0, 0)
  , mOrientation(0)
  , mDPI(0)
  , mRounding(0)
  , mDefaultScale(0)
  , mUpdatedDimensions(false)
  , mSizeMode(nsSizeMode_Normal)
  , mManager(aManager)
  , mDocShellIsActive(false)
  , mMarkedDestroying(false)
  , mIsDestroyed(false)
  , mChromeFlags(aChromeFlags)
  , mDragValid(false)
  , mInitedByParent(false)
  , mTabId(aTabId)
  , mCreatingWindow(false)
  , mCursor(eCursorInvalid)
  , mTabSetsCursor(false)
  , mHasContentOpener(false)
#ifdef DEBUG
  , mActiveSupressDisplayportCount(0)
#endif
  , mLayerTreeEpoch(1)
  , mPreserveLayers(false)
  , mRenderLayers(true)
  , mHasLayers(false)
  , mHasPresented(false)
  , mHasBeforeUnload(false)
  , mIsMouseEnterIntoWidgetEventSuppressed(false)
{
  MOZ_ASSERT(aManager);
  // When the input event queue is disabled, we don't need to handle the case
  // that some input events are dispatched before PBrowserConstructor.
  mIsReadyToHandleInputEvents = !ContentParent::IsInputEventQueueSupported();
}

TabParent::~TabParent()
{
}

TabParent*
TabParent::GetTabParentFromLayersId(layers::LayersId aLayersId)
{
  if (!sLayerToTabParentTable) {
    return nullptr;
  }
  return sLayerToTabParentTable->Get(uint64_t(aLayersId));
}

void
TabParent::AddTabParentToTable(layers::LayersId aLayersId, TabParent* aTabParent)
{
  if (!sLayerToTabParentTable) {
    sLayerToTabParentTable = new LayerToTabParentTable();
  }
  sLayerToTabParentTable->Put(uint64_t(aLayersId), aTabParent);
}

void
TabParent::RemoveTabParentFromTable(layers::LayersId aLayersId)
{
  if (!sLayerToTabParentTable) {
    return;
  }
  sLayerToTabParentTable->Remove(uint64_t(aLayersId));
  if (sLayerToTabParentTable->Count() == 0) {
    delete sLayerToTabParentTable;
    sLayerToTabParentTable = nullptr;
  }
}

void
TabParent::CacheFrameLoader(nsFrameLoader* aFrameLoader)
{
  mFrameLoader = aFrameLoader;
}

/**
 * Will return nullptr if there is no outer window available for the
 * document hosting the owner element of this TabParent. Also will return
 * nullptr if that outer window is in the process of closing.
 */
already_AddRefed<nsPIDOMWindowOuter>
TabParent::GetParentWindowOuter()
{
  nsCOMPtr<nsIContent> frame = do_QueryInterface(GetOwnerElement());
  if (!frame) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parent = frame->OwnerDoc()->GetWindow();
  if (!parent || parent->Closed()) {
    return nullptr;
  }

  return parent.forget();
}

void
TabParent::SetOwnerElement(Element* aElement)
{
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
  if (curTopLevelWin && !isSameTopLevelWin) {
    curTopLevelWin->RemoveBrowser(this);
  }

  // Update to the new content, and register to listen for events from it.
  mFrameElement = aElement;

  if (newTopLevelWin && !isSameTopLevelWin) {
    newTopLevelWin->AddBrowser(this);
  }

  if (mFrameElement) {
    bool useGlobalHistory =
      !mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::disableglobalhistory);
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

  // Try to send down WidgetNativeData, now that this TabParent is associated
  // with a widget.
  nsCOMPtr<nsIWidget> widget = GetTopLevelWidget();
  if (widget) {
    WindowsHandle widgetNativeData = reinterpret_cast<WindowsHandle>(
      widget->GetNativeData(NS_NATIVE_SHAREABLE_WINDOW));
    if (widgetNativeData) {
      Unused << SendSetWidgetNativeData(widgetNativeData);
    }
  }
}

void
TabParent::AddWindowListeners()
{
  if (mFrameElement) {
    if (nsCOMPtr<nsPIDOMWindowOuter> window = mFrameElement->OwnerDoc()->GetWindow()) {
      nsCOMPtr<EventTarget> eventTarget = window->GetTopWindowRoot();
      if (eventTarget) {
        eventTarget->AddEventListener(NS_LITERAL_STRING("MozUpdateWindowPos"),
                                      this, false, false);
      }
    }
  }
}

void
TabParent::RemoveWindowListeners()
{
  if (mFrameElement && mFrameElement->OwnerDoc()->GetWindow()) {
    nsCOMPtr<nsPIDOMWindowOuter> window = mFrameElement->OwnerDoc()->GetWindow();
    nsCOMPtr<EventTarget> eventTarget = window->GetTopWindowRoot();
    if (eventTarget) {
      eventTarget->RemoveEventListener(NS_LITERAL_STRING("MozUpdateWindowPos"),
                                       this, false);
    }
  }
}

void
TabParent::DestroyInternal()
{
  IMEStateManager::OnTabParentDestroying(this);

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

  if (RenderFrameParent* frame = GetRenderFrame()) {
    RemoveTabParentFromTable(frame->GetLayersId());
    frame->Destroy();
  }

#ifdef XP_WIN
  // Let all PluginWidgets know we are tearing down. Prevents
  // these objects from sending async events after the child side
  // is shut down.
  const ManagedContainer<PPluginWidgetParent>& kids =
    ManagedPPluginWidgetParent();
  for (auto iter = kids.ConstIter(); !iter.Done(); iter.Next()) {
    static_cast<mozilla::plugins::PluginWidgetParent*>(
       iter.Get()->GetKey())->ParentDestroy();
  }
#endif
}

void
TabParent::Destroy()
{
  // Aggressively release the window to avoid leaking the world in shutdown
  // corner cases.
  mBrowserDOMWindow = nullptr;

  if (mIsDestroyed) {
    return;
  }

  DestroyInternal();

  mIsDestroyed = true;

  if (XRE_IsParentProcess()) {
    ContentParent::NotifyTabDestroying(this->GetTabId(), Manager()->AsContentParent()->ChildID());
  } else {
    ContentParent::NotifyTabDestroying(this->GetTabId(), Manager()->ChildID());
  }

  mMarkedDestroying = true;
}

mozilla::ipc::IPCResult
TabParent::RecvEnsureLayersConnected(CompositorOptions* aCompositorOptions)
{
  if (RenderFrameParent* frame = GetRenderFrame()) {
    frame->EnsureLayersConnected(aCompositorOptions);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::Recv__delete__()
{
  if (XRE_IsParentProcess()) {
    ContentParent::UnregisterRemoteFrame(mTabId,
                                         Manager()->AsContentParent()->ChildID(),
                                         mMarkedDestroying);
  }
  else {
    Manager()->AsContentBridgeParent()->NotifyTabDestroyed();
    ContentParent::UnregisterRemoteFrame(mTabId,
                                         Manager()->ChildID(),
                                         mMarkedDestroying);
  }

  return IPC_OK();
}

void
TabParent::ActorDestroy(ActorDestroyReason why)
{
  // Even though TabParent::Destroy calls this, we need to do it here too in
  // case of a crash.
  IMEStateManager::OnTabParentDestroying(this);

  // Prevent executing ContentParent::NotifyTabDestroying in
  // TabParent::Destroy() called by frameLoader->DestroyComplete() below
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

  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader(true);
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (frameLoader) {
    nsCOMPtr<Element> frameElement(mFrameElement);
    ReceiveMessage(CHILD_PROCESS_SHUTDOWN_MESSAGE, false, nullptr, nullptr,
                   nullptr);
    frameLoader->DestroyComplete();

    if (why == AbnormalShutdown && os) {
      os->NotifyObservers(ToSupports(frameLoader),
                          "oop-frameloader-crashed", nullptr);
      nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface(frameElement);
      if (owner) {
        RefPtr<nsFrameLoader> currentFrameLoader = owner->GetFrameLoader();
        // It's possible that the frameloader owner has already moved on
        // and created a new frameloader. If so, we don't fire the event,
        // since the frameloader owner has clearly moved on.
        if (currentFrameLoader == frameLoader) {
          MessageChannel* channel = GetIPCChannel();
          if (channel && !channel->DoBuildIDsMatch()) {
            nsContentUtils::DispatchTrustedEvent(
              frameElement->OwnerDoc(), frameElement,
              NS_LITERAL_STRING("oop-browser-buildid-mismatch"), true, true);
          } else {
            nsContentUtils::DispatchTrustedEvent(
              frameElement->OwnerDoc(), frameElement,
              NS_LITERAL_STRING("oop-browser-crashed"), true, true);
          }
        }
      }
    }

    mFrameLoader = nullptr;
  }

  if (os) {
    os->NotifyObservers(NS_ISUPPORTS_CAST(nsITabParent*, this), "ipc:browser-destroyed", nullptr);
  }
}

mozilla::ipc::IPCResult
TabParent::RecvMoveFocus(const bool& aForward, const bool& aForDocumentNavigation)
{
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm) {
    RefPtr<Element> dummy;

    uint32_t type = aForward ?
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARDDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARD)) :
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARDDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARD));
    fm->MoveFocus(nullptr, mFrameElement, type, nsIFocusManager::FLAG_BYKEY,
                  getter_AddRefs(dummy));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSizeShellTo(const uint32_t& aFlags, const int32_t& aWidth, const int32_t& aHeight,
                           const int32_t& aShellItemWidth, const int32_t& aShellItemHeight)
{
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

  nsCOMPtr<nsIXULWindow> xulWin(do_GetInterface(treeOwner));
  NS_ENSURE_TRUE(xulWin, IPC_OK());
  xulWin->SizeShellToWithLimit(width, height, aShellItemWidth, aShellItemHeight);

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvDropLinks(nsTArray<nsString>&& aLinks)
{
  nsCOMPtr<nsIBrowser> browser = do_QueryInterface(mFrameElement);
  if (browser) {
    // Verify that links have not been modified by the child. If links have
    // not been modified then it's safe to load those links using the
    // SystemPrincipal. If they have been modified by web content, then
    // we use a NullPrincipal which still allows to load web links.
    bool loadUsingSystemPrincipal = true;
    if (aLinks.Length() != mVerifyDropLinks.Length()) {
      loadUsingSystemPrincipal = false;
    }
    UniquePtr<const char16_t*[]> links;
    links = MakeUnique<const char16_t*[]>(aLinks.Length());
    for (uint32_t i = 0; i < aLinks.Length(); i++) {
      if (loadUsingSystemPrincipal) {
        if (!aLinks[i].Equals(mVerifyDropLinks[i])) {
          loadUsingSystemPrincipal = false;
        }
      }
      links[i] = aLinks[i].get();
    }
    mVerifyDropLinks.Clear();
    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    if (loadUsingSystemPrincipal) {
      triggeringPrincipal = nsContentUtils::GetSystemPrincipal();
    } else {
      triggeringPrincipal = NullPrincipal::CreateWithoutOriginAttributes();
    }
    browser->DropLinks(aLinks.Length(), links.get(), triggeringPrincipal);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvEvent(const RemoteDOMEvent& aEvent)
{
  RefPtr<Event> event = aEvent.mEvent;
  NS_ENSURE_TRUE(event, IPC_OK());

  nsCOMPtr<mozilla::dom::EventTarget> target = do_QueryInterface(mFrameElement);
  NS_ENSURE_TRUE(target, IPC_OK());

  event->SetOwner(target);

  target->DispatchEvent(*event);
  return IPC_OK();
}

bool
TabParent::SendLoadRemoteScript(const nsString& aURL,
                                const bool& aRunInGlobalScope)
{
  if (mCreatingWindow) {
    mDelayedFrameScripts.AppendElement(FrameScriptInfo(aURL, aRunInGlobalScope));
    return true;
  }

  MOZ_ASSERT(mDelayedFrameScripts.IsEmpty());
  return PBrowserParent::SendLoadRemoteScript(aURL, aRunInGlobalScope);
}

void
TabParent::LoadURL(nsIURI* aURI)
{
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

void
TabParent::InitRenderFrame()
{
  if (IsInitedByParent()) {
    // If TabParent is initialized by parent side then the RenderFrame must also
    // be created here. If TabParent is initialized by child side,
    // child side will create RenderFrame.
    MOZ_ASSERT(!GetRenderFrame());
    RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
    MOZ_ASSERT(frameLoader);
    if (frameLoader) {
      RenderFrameParent* renderFrame = new RenderFrameParent(frameLoader);
      MOZ_ASSERT(renderFrame->IsInitted());
      layers::LayersId layersId = renderFrame->GetLayersId();
      AddTabParentToTable(layersId, this);
      if (!SendPRenderFrameConstructor(renderFrame)) {
        return;
      }

      TextureFactoryIdentifier textureFactoryIdentifier;
      renderFrame->GetTextureFactoryIdentifier(&textureFactoryIdentifier);
      Unused << SendInitRendering(textureFactoryIdentifier, layersId,
        renderFrame->GetCompositorOptions(),
        renderFrame->IsLayersConnected(), renderFrame);
    }
  } else {
    // Otherwise, the child should have constructed the RenderFrame,
    // and we should already know about it.
    MOZ_ASSERT(GetRenderFrame());
  }
}

void
TabParent::Show(const ScreenIntSize& size, bool aParentIsActive)
{
    mDimensions = size;
    if (mIsDestroyed) {
        return;
    }

    MOZ_ASSERT(GetRenderFrame());

    nsCOMPtr<nsISupports> container = mFrameElement->OwnerDoc()->GetContainer();
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container);
    nsCOMPtr<nsIWidget> mainWidget;
    baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
    mSizeMode = mainWidget ? mainWidget->SizeMode() : nsSizeMode_Normal;

    Unused << SendShow(size, GetShowInfo(), aParentIsActive, mSizeMode);
}

mozilla::ipc::IPCResult
TabParent::RecvSetDimensions(const uint32_t& aFlags,
                             const int32_t& aX, const int32_t& aY,
                             const int32_t& aCx, const int32_t& aCy)
{
  MOZ_ASSERT(!(aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER),
             "We should never see DIM_FLAGS_SIZE_INNER here!");

  NS_ENSURE_TRUE(mFrameElement, IPC_OK());
  nsCOMPtr<nsIDocShell> docShell = mFrameElement->OwnerDoc()->GetDocShell();
  NS_ENSURE_TRUE(docShell, IPC_OK());
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShell->GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = do_QueryInterface(treeOwner);
  NS_ENSURE_TRUE(treeOwnerAsWin, IPC_OK());

  // We only care about the parameters that actually changed, see more
  // details in TabChild::SetDimensions.
  int32_t unused;
  int32_t x = aX;
  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_X) {
    treeOwnerAsWin->GetPosition(&x, &unused);
  }

  int32_t y = aY;
  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_Y) {
    treeOwnerAsWin->GetPosition(&unused, &y);
  }

  int32_t cx = aCx;
  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CX) {
    treeOwnerAsWin->GetSize(&cx, &unused);
  }

  int32_t cy = aCy;
  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_IGNORE_CY) {
    treeOwnerAsWin->GetSize(&unused, &cy);
  }

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
      aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER) {
    treeOwnerAsWin->SetPositionAndSize(x, y, cx, cy,
                                       nsIBaseWindow::eRepaint);
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

nsresult
TabParent::UpdatePosition()
{
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    return NS_OK;
  }
  nsIntRect windowDims;
  NS_ENSURE_SUCCESS(frameLoader->GetWindowDimensions(windowDims), NS_ERROR_FAILURE);
  UpdateDimensions(windowDims, mDimensions);
  return NS_OK;
}

void
TabParent::UpdateDimensions(const nsIntRect& rect, const ScreenIntSize& size)
{
  if (mIsDestroyed) {
    return;
  }
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    NS_WARNING("No widget found in TabParent::UpdateDimensions");
    return;
  }

  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  ScreenOrientationInternal orientation = config.orientation();
  LayoutDeviceIntPoint clientOffset = GetClientOffset();
  LayoutDeviceIntPoint chromeOffset = -GetChildProcessOffset();

  if (!mUpdatedDimensions || mOrientation != orientation ||
      mDimensions != size || !mRect.IsEqualEdges(rect) ||
      clientOffset != mClientOffset ||
      chromeOffset != mChromeOffset) {

    mUpdatedDimensions = true;
    mRect = rect;
    mDimensions = size;
    mOrientation = orientation;
    mClientOffset = clientOffset;
    mChromeOffset = chromeOffset;

    Unused << SendUpdateDimensions(GetDimensionInfo());
  }
}

DimensionInfo
TabParent::GetDimensionInfo()
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  MOZ_ASSERT(widget);
  CSSToLayoutDeviceScale widgetScale = widget->GetDefaultScale();

  LayoutDeviceIntRect devicePixelRect =
    ViewAs<LayoutDevicePixel>(mRect,
                              PixelCastJustification::LayoutDeviceIsScreenForTabDims);
  LayoutDeviceIntSize devicePixelSize =
    ViewAs<LayoutDevicePixel>(mDimensions,
                              PixelCastJustification::LayoutDeviceIsScreenForTabDims);

  CSSRect unscaledRect = devicePixelRect / widgetScale;
  CSSSize unscaledSize = devicePixelSize / widgetScale;
  DimensionInfo di(unscaledRect, unscaledSize, mOrientation,
                   mClientOffset, mChromeOffset);
  return di;
}

void
TabParent::SizeModeChanged(const nsSizeMode& aSizeMode)
{
  if (!mIsDestroyed && aSizeMode != mSizeMode) {
    mSizeMode = aSizeMode;
    Unused << SendSizeModeChanged(aSizeMode);
  }
}

void
TabParent::UIResolutionChanged()
{
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

void
TabParent::ThemeChanged()
{
  if (!mIsDestroyed) {
    // The theme has changed, and any cached values we had sent down
    // to the child have been invalidated. When this method is called,
    // LookAndFeel should have the up-to-date values, which we now
    // send down to the child. We do this for every remote tab for now,
    // but bug 1156934 has been filed to do it once per content process.
    Unused << SendThemeChanged(LookAndFeel::GetIntCache());
  }
}

void
TabParent::HandleAccessKey(const WidgetKeyboardEvent& aEvent,
                           nsTArray<uint32_t>& aCharCodes)
{
  if (!mIsDestroyed) {
    // Note that we don't need to mark aEvent is posted to a remote process
    // because the event may be dispatched to it as normal keyboard event.
    // Therefore, we should use local copy to send it.
    WidgetKeyboardEvent localEvent(aEvent);
    Unused << SendHandleAccessKey(localEvent, aCharCodes);
  }
}

void
TabParent::Activate()
{
  if (!mIsDestroyed) {
    Unused << Manager()->SendActivate(this);
  }
}

void
TabParent::Deactivate()
{
  if (!mIsDestroyed) {
    Unused << Manager()->SendDeactivate(this);
  }
}

NS_IMETHODIMP
TabParent::Init(mozIDOMWindowProxy *window)
{
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetState(uint32_t *aState)
{
  NS_ENSURE_ARG(aState);
  NS_WARNING("SecurityState not valid here");
  *aState = 0;
  return NS_OK;
}

NS_IMETHODIMP
TabParent::SetDocShell(nsIDocShell *aDocShell)
{
  NS_ENSURE_ARG(aDocShell);
  NS_WARNING("No mDocShell member in TabParent so there is no docShell to set");
  return NS_OK;
}

  a11y::PDocAccessibleParent*
TabParent::AllocPDocAccessibleParent(PDocAccessibleParent* aParent,
                                     const uint64_t&, const uint32_t&,
                                     const IAccessibleHolder&)
{
#ifdef ACCESSIBILITY
  return new a11y::DocAccessibleParent();
#else
  return nullptr;
#endif
}

bool
TabParent::DeallocPDocAccessibleParent(PDocAccessibleParent* aParent)
{
#ifdef ACCESSIBILITY
  delete static_cast<a11y::DocAccessibleParent*>(aParent);
#endif
  return true;
}

mozilla::ipc::IPCResult
TabParent::RecvPDocAccessibleConstructor(PDocAccessibleParent* aDoc,
                                         PDocAccessibleParent* aParentDoc,
                                         const uint64_t& aParentID,
                                         const uint32_t& aMsaaID,
                                         const IAccessibleHolder& aDocCOMProxy)
{
#ifdef ACCESSIBILITY
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
#ifdef DEBUG
      return added;
#else
      return IPC_OK();
#endif
    }

#ifdef XP_WIN
    MOZ_ASSERT(aDocCOMProxy.IsNull());
    a11y::WrapperFor(doc)->SetID(aMsaaID);
    if (a11y::nsWinUtils::IsWindowEmulationStarted()) {
      doc->SetEmulatedWindowHandle(parentDoc->GetEmulatedWindowHandle());
    }
#endif

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
#ifdef XP_WIN
    a11y::WrapperFor(doc)->SetID(aMsaaID);
    MOZ_ASSERT(!aDocCOMProxy.IsNull());

    RefPtr<IAccessible> proxy(aDocCOMProxy.Get());
    doc->SetCOMInterface(proxy);
    doc->MaybeInitWindowEmulation();
    doc->SendParentCOMProxy();
#endif
  }
#endif
  return IPC_OK();
}

a11y::DocAccessibleParent*
TabParent::GetTopLevelDocAccessible() const
{
#ifdef ACCESSIBILITY
  // XXX Consider managing non top level PDocAccessibles with their parent
  // document accessible.
  const ManagedContainer<PDocAccessibleParent>& docs = ManagedPDocAccessibleParent();
  for (auto iter = docs.ConstIter(); !iter.Done(); iter.Next()) {
    auto doc = static_cast<a11y::DocAccessibleParent*>(iter.Get()->GetKey());
    if (doc->IsTopLevel()) {
      return doc;
    }
  }

  MOZ_ASSERT(docs.Count() == 0, "If there isn't a top level accessible doc "
                                "there shouldn't be an accessible doc at all!");
#endif
  return nullptr;
}

PFilePickerParent*
TabParent::AllocPFilePickerParent(const nsString& aTitle, const int16_t& aMode)
{
  return new FilePickerParent(aTitle, aMode);
}

bool
TabParent::DeallocPFilePickerParent(PFilePickerParent* actor)
{
  delete actor;
  return true;
}

auto
TabParent::AllocPIndexedDBPermissionRequestParent(const Principal& aPrincipal)
  -> PIndexedDBPermissionRequestParent*
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal(aPrincipal);
  if (!principal) {
    return nullptr;
  }

  nsCOMPtr<nsIContentParent> manager = Manager();
  if (!manager->IsContentParent()) {
    MOZ_CRASH("Figure out security checks for bridged content!");
  }

  if (NS_WARN_IF(!mFrameElement)) {
    return nullptr;
  }

  return
    mozilla::dom::indexedDB::AllocPIndexedDBPermissionRequestParent(mFrameElement,
                                                                    principal);
}

mozilla::ipc::IPCResult
TabParent::RecvPIndexedDBPermissionRequestConstructor(
                                      PIndexedDBPermissionRequestParent* aActor,
                                      const Principal& aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  if (!mozilla::dom::indexedDB::RecvPIndexedDBPermissionRequestConstructor(aActor)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

bool
TabParent::DeallocPIndexedDBPermissionRequestParent(
                                      PIndexedDBPermissionRequestParent* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  return
    mozilla::dom::indexedDB::DeallocPIndexedDBPermissionRequestParent(aActor);
}

void
TabParent::SendMouseEvent(const nsAString& aType, float aX, float aY,
                          int32_t aButton, int32_t aClickCount,
                          int32_t aModifiers, bool aIgnoreRootScrollFrame)
{
  if (!mIsDestroyed) {
    Unused << PBrowserParent::SendMouseEvent(nsString(aType), aX, aY,
                                             aButton, aClickCount,
                                             aModifiers,
                                             aIgnoreRootScrollFrame);
  }
}

void
TabParent::SendRealMouseEvent(WidgetMouseEvent& aEvent)
{
  if (mIsDestroyed) {
    return;
  }
  aEvent.mRefPoint += GetChildProcessOffset();

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    // When we mouseenter the tab, the tab's cursor should
    // become the current cursor.  When we mouseexit, we stop.
    if (eMouseEnterIntoWidget == aEvent.mMessage) {
      mTabSetsCursor = true;
      if (mCustomCursor) {
        widget->SetCursor(mCustomCursor,
                          mCustomCursorHotspotX, mCustomCursorHotspotY);
      } else if (mCursor != eCursorInvalid) {
        widget->SetCursor(mCursor);
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

  bool isInputPriorityEventEnabled =
    Manager()->AsContentParent()->IsInputPriorityEventEnabled();

  if (mIsMouseEnterIntoWidgetEventSuppressed) {
    // In the case that the TabParent suppressed the eMouseEnterWidget event due
    // to its corresponding TabChild wasn't ready to handle it, we have to
    // resend it when the TabChild is ready.
    mIsMouseEnterIntoWidgetEventSuppressed = false;
    WidgetMouseEvent localEvent(aEvent);
    localEvent.mMessage = eMouseEnterIntoWidget;
    DebugOnly<bool> ret = isInputPriorityEventEnabled
      ? SendRealMouseButtonEvent(localEvent, guid, blockId)
      : SendNormalPriorityRealMouseButtonEvent(localEvent, guid, blockId);
    NS_WARNING_ASSERTION(ret, "SendRealMouseButtonEvent(eMouseEnterIntoWidget) failed");
    MOZ_ASSERT(!ret || localEvent.HasBeenPostedToRemoteProcess());
  }

  if (eMouseMove == aEvent.mMessage) {
    if (aEvent.mReason == WidgetMouseEvent::eSynthesized) {
      DebugOnly<bool> ret = isInputPriorityEventEnabled
        ? SendSynthMouseMoveEvent(aEvent, guid, blockId)
        : SendNormalPrioritySynthMouseMoveEvent(aEvent, guid, blockId);
      NS_WARNING_ASSERTION(ret, "SendSynthMouseMoveEvent() failed");
      MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
      return;
    }
    DebugOnly<bool> ret = isInputPriorityEventEnabled
      ? SendRealMouseMoveEvent(aEvent, guid, blockId)
      : SendNormalPriorityRealMouseMoveEvent(aEvent, guid, blockId);
    NS_WARNING_ASSERTION(ret, "SendRealMouseMoveEvent() failed");
    MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
    return;
  }

  DebugOnly<bool> ret = isInputPriorityEventEnabled
    ? SendRealMouseButtonEvent(aEvent, guid, blockId)
    : SendNormalPriorityRealMouseButtonEvent(aEvent, guid, blockId);
  NS_WARNING_ASSERTION(ret, "SendRealMouseButtonEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

LayoutDeviceToCSSScale
TabParent::GetLayoutDeviceToCSSScale()
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mFrameElement);
  nsIDocument* doc = (content ? content->OwnerDoc() : nullptr);
  nsPresContext* ctx = (doc ? doc->GetPresContext() : nullptr);
  return LayoutDeviceToCSSScale(ctx
    ? (float)ctx->AppUnitsPerDevPixel() / nsPresContext::AppUnitsPerCSSPixel()
    : 0.0f);
}

bool
TabParent::QueryDropLinksForVerification()
{
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

  uint32_t linksCount = 0;
  nsIDroppedLinkItem** droppedLinkedItems = nullptr;
  dropHandler->QueryLinks(initialDataTransfer,
                          &linksCount, &droppedLinkedItems);

  // Since the entire event is cancelled if one of the links is invalid,
  // we can store all links on the parent side without any prior
  // validation checks.
  nsresult rv = NS_OK;
  for (uint32_t i = 0; i < linksCount; i++) {
    nsString tmp;
    rv = droppedLinkedItems[i]->GetUrl(tmp);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to query url for verification");
      break;
    }
    mVerifyDropLinks.AppendElement(tmp);

    rv = droppedLinkedItems[i]->GetName(tmp);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to query name for verification");
      break;
    }
    mVerifyDropLinks.AppendElement(tmp);

    rv = droppedLinkedItems[i]->GetType(tmp);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to query type for verification");
      break;
    }
    mVerifyDropLinks.AppendElement(tmp);
  }
  for (uint32_t i = 0; i < linksCount; i++) {
    NS_IF_RELEASE(droppedLinkedItems[i]);
  }
  free(droppedLinkedItems);
  if (NS_FAILED(rv)) {
    mVerifyDropLinks.Clear();
    return false;
  }
  return true;
}

void
TabParent::SendRealDragEvent(WidgetDragEvent& aEvent, uint32_t aDragAction,
                             uint32_t aDropEffect,
                             const nsCString& aPrincipalURISpec)
{
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return;
  }
  MOZ_ASSERT(!Manager()->AsContentParent()->IsInputPriorityEventEnabled());
  aEvent.mRefPoint += GetChildProcessOffset();
  if (aEvent.mMessage == eDrop) {
    if (!QueryDropLinksForVerification()) {
      return;
    }
  }
  DebugOnly<bool> ret =
    PBrowserParent::SendRealDragEvent(aEvent, aDragAction, aDropEffect,
                                      aPrincipalURISpec);
  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendRealDragEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

LayoutDevicePoint
TabParent::AdjustTapToChildWidget(const LayoutDevicePoint& aPoint)
{
  return aPoint + LayoutDevicePoint(GetChildProcessOffset());
}

void
TabParent::SendMouseWheelEvent(WidgetWheelEvent& aEvent)
{
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return;
  }

  ScrollableLayerGuid guid;
  uint64_t blockId;
  ApzAwareEventRoutingToChild(&guid, &blockId, nullptr);
  aEvent.mRefPoint += GetChildProcessOffset();
  DebugOnly<bool> ret =
    Manager()->AsContentParent()->IsInputPriorityEventEnabled()
      ? PBrowserParent::SendMouseWheelEvent(aEvent, guid, blockId)
      : PBrowserParent::SendNormalPriorityMouseWheelEvent(aEvent, guid, blockId);

  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendMouseWheelEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

mozilla::ipc::IPCResult
TabParent::RecvDispatchWheelEvent(const mozilla::WidgetWheelEvent& aEvent)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  WidgetWheelEvent localEvent(aEvent);
  localEvent.mWidget = widget;
  localEvent.mRefPoint -= GetChildProcessOffset();

  widget->DispatchInputEvent(&localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvDispatchMouseEvent(const mozilla::WidgetMouseEvent& aEvent)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  WidgetMouseEvent localEvent(aEvent);
  localEvent.mWidget = widget;
  localEvent.mRefPoint -= GetChildProcessOffset();

  widget->DispatchInputEvent(&localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvDispatchKeyboardEvent(const mozilla::WidgetKeyboardEvent& aEvent)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.mWidget = widget;
  localEvent.mRefPoint -= GetChildProcessOffset();

  widget->DispatchInputEvent(&localEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvRequestNativeKeyBindings(const uint32_t& aType,
                                        const WidgetKeyboardEvent& aEvent,
                                        nsTArray<CommandInt>* aCommands)
{
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

  localEvent.InitEditCommandsFor(keyBindingsType);
  *aCommands = localEvent.EditCommandsConstRef(keyBindingsType);

  return IPC_OK();
}

class SynthesizedEventObserver : public nsIObserver
{
  NS_DECL_ISUPPORTS

public:
  SynthesizedEventObserver(TabParent* aTabParent, const uint64_t& aObserverId)
    : mTabParent(aTabParent)
    , mObserverId(aObserverId)
  {
    MOZ_ASSERT(mTabParent);
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const char16_t* aData) override
  {
    if (!mTabParent || !mObserverId) {
      // We already sent the notification, or we don't actually need to
      // send any notification at all.
      return NS_OK;
    }

    if (!mTabParent->SendNativeSynthesisResponse(mObserverId,
                                                 nsCString(aTopic))) {
      NS_WARNING("Unable to send native event synthesization response!");
    }
    // Null out tabparent to indicate we already sent the response
    mTabParent = nullptr;
    return NS_OK;
  }

private:
  virtual ~SynthesizedEventObserver() { }

  RefPtr<TabParent> mTabParent;
  uint64_t mObserverId;
};

NS_IMPL_ISUPPORTS(SynthesizedEventObserver, nsIObserver)

class MOZ_STACK_CLASS AutoSynthesizedEventResponder
{
public:
  AutoSynthesizedEventResponder(TabParent* aTabParent,
                                const uint64_t& aObserverId,
                                const char* aTopic)
    : mObserver(new SynthesizedEventObserver(aTabParent, aObserverId))
    , mTopic(aTopic)
  { }

  ~AutoSynthesizedEventResponder()
  {
    // This may be a no-op if the observer already sent a response.
    mObserver->Observe(nullptr, mTopic, nullptr);
  }

  nsIObserver* GetObserver()
  {
    return mObserver;
  }

private:
  nsCOMPtr<nsIObserver> mObserver;
  const char* mTopic;
};

mozilla::ipc::IPCResult
TabParent::RecvSynthesizeNativeKeyEvent(const int32_t& aNativeKeyboardLayout,
                                        const int32_t& aNativeKeyCode,
                                        const uint32_t& aModifierFlags,
                                        const nsString& aCharacters,
                                        const nsString& aUnmodifiedCharacters,
                                        const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "keyevent");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeKeyEvent(aNativeKeyboardLayout, aNativeKeyCode,
                                     aModifierFlags, aCharacters,
                                     aUnmodifiedCharacters,
                                     responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSynthesizeNativeMouseEvent(const LayoutDeviceIntPoint& aPoint,
                                          const uint32_t& aNativeMessage,
                                          const uint32_t& aModifierFlags,
                                          const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "mouseevent");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeMouseEvent(aPoint, aNativeMessage, aModifierFlags,
                                       responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSynthesizeNativeMouseMove(const LayoutDeviceIntPoint& aPoint,
                                         const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "mousemove");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeMouseMove(aPoint, responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSynthesizeNativeMouseScrollEvent(
             const LayoutDeviceIntPoint& aPoint,
             const uint32_t& aNativeMessage,
             const double& aDeltaX,
             const double& aDeltaY,
             const double& aDeltaZ,
             const uint32_t& aModifierFlags,
             const uint32_t& aAdditionalFlags,
             const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "mousescrollevent");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeMouseScrollEvent(aPoint, aNativeMessage,
                                             aDeltaX, aDeltaY, aDeltaZ,
                                             aModifierFlags, aAdditionalFlags,
                                             responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSynthesizeNativeTouchPoint(
             const uint32_t& aPointerId,
             const TouchPointerState& aPointerState,
             const LayoutDeviceIntPoint& aPoint,
             const double& aPointerPressure,
             const uint32_t& aPointerOrientation,
             const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "touchpoint");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchPoint(aPointerId, aPointerState, aPoint,
                                       aPointerPressure, aPointerOrientation,
                                       responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSynthesizeNativeTouchTap(const LayoutDeviceIntPoint& aPoint,
                                        const bool& aLongTap,
                                        const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "touchtap");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchTap(aPoint, aLongTap, responder.GetObserver());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvClearNativeTouchSequence(const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "cleartouch");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->ClearNativeTouchSequence(responder.GetObserver());
  }
  return IPC_OK();
}

void
TabParent::SendRealKeyEvent(WidgetKeyboardEvent& aEvent)
{
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return;
  }
  aEvent.mRefPoint += GetChildProcessOffset();

  if (aEvent.mMessage == eKeyPress) {
    // XXX Should we do this only when input context indicates an editor having
    //     focus and the key event won't cause inputting text?
    aEvent.InitAllEditCommands();
  } else {
    aEvent.PreventNativeKeyBindings();
  }
  DebugOnly<bool> ret =
    Manager()->AsContentParent()->IsInputPriorityEventEnabled()
      ? PBrowserParent::SendRealKeyEvent(aEvent)
      : PBrowserParent::SendNormalPriorityRealKeyEvent(aEvent);

  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendRealKeyEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

void
TabParent::SendRealTouchEvent(WidgetTouchEvent& aEvent)
{
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

  ScrollableLayerGuid guid;
  uint64_t blockId;
  nsEventStatus apzResponse;
  ApzAwareEventRoutingToChild(&guid, &blockId, &apzResponse);

  if (mIsDestroyed) {
    return;
  }

  LayoutDeviceIntPoint offset = GetChildProcessOffset();
  for (uint32_t i = 0; i < aEvent.mTouches.Length(); i++) {
    aEvent.mTouches[i]->mRefPoint += offset;
  }

  bool inputPriorityEventEnabled =
    Manager()->AsContentParent()->IsInputPriorityEventEnabled();

  if (aEvent.mMessage == eTouchMove) {
    DebugOnly<bool> ret = inputPriorityEventEnabled
      ? PBrowserParent::SendRealTouchMoveEvent(aEvent, guid, blockId,
                                               apzResponse)
      : PBrowserParent::SendNormalPriorityRealTouchMoveEvent(aEvent, guid,
                                                             blockId,
                                                             apzResponse);

    NS_WARNING_ASSERTION(ret,
                         "PBrowserParent::SendRealTouchMoveEvent() failed");
    MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
    return;
  }
  DebugOnly<bool> ret = inputPriorityEventEnabled
    ? PBrowserParent::SendRealTouchEvent(aEvent, guid, blockId, apzResponse)
    : PBrowserParent::SendNormalPriorityRealTouchEvent(aEvent, guid, blockId,
                                                       apzResponse);

  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendRealTouchEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

void
TabParent::SendPluginEvent(WidgetPluginEvent& aEvent)
{
  DebugOnly<bool> ret = PBrowserParent::SendPluginEvent(aEvent);
  NS_WARNING_ASSERTION(ret, "PBrowserParent::SendPluginEvent() failed");
  MOZ_ASSERT(!ret || aEvent.HasBeenPostedToRemoteProcess());
}

bool
TabParent::SendHandleTap(TapType aType,
                         const LayoutDevicePoint& aPoint,
                         Modifiers aModifiers,
                         const ScrollableLayerGuid& aGuid,
                         uint64_t aInputBlockId)
{
  if (mIsDestroyed || !mIsReadyToHandleInputEvents) {
    return false;
  }
  if ((aType == TapType::eSingleTap || aType == TapType::eSecondTap) &&
      GetRenderFrame()) {
    GetRenderFrame()->TakeFocusForClickFromTap();
  }
  LayoutDeviceIntPoint offset = GetChildProcessOffset();
  return Manager()->AsContentParent()->IsInputPriorityEventEnabled()
    ? PBrowserParent::SendHandleTap(aType, aPoint + offset, aModifiers, aGuid,
                                    aInputBlockId)
    : PBrowserParent::SendNormalPriorityHandleTap(aType, aPoint + offset,
                                                  aModifiers, aGuid,
                                                  aInputBlockId);
}

mozilla::ipc::IPCResult
TabParent::RecvSyncMessage(const nsString& aMessage,
                           const ClonedMessageData& aData,
                           InfallibleTArray<CpowEntry>&& aCpows,
                           const IPC::Principal& aPrincipal,
                           nsTArray<StructuredCloneData>* aRetVal)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "TabParent::RecvSyncMessage", OTHER, aMessage);

  StructuredCloneData data;
  ipc::UnpackClonedMessageDataForParent(aData, data);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  if (!ReceiveMessage(aMessage, true, &data, &cpows, aPrincipal, aRetVal)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvRpcMessage(const nsString& aMessage,
                          const ClonedMessageData& aData,
                          InfallibleTArray<CpowEntry>&& aCpows,
                          const IPC::Principal& aPrincipal,
                          nsTArray<StructuredCloneData>* aRetVal)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "TabParent::RecvRpcMessage", OTHER, aMessage);

  StructuredCloneData data;
  ipc::UnpackClonedMessageDataForParent(aData, data);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  if (!ReceiveMessage(aMessage, true, &data, &cpows, aPrincipal, aRetVal)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvAsyncMessage(const nsString& aMessage,
                            InfallibleTArray<CpowEntry>&& aCpows,
                            const IPC::Principal& aPrincipal,
                            const ClonedMessageData& aData)
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "TabParent::RecvAsyncMessage", OTHER, aMessage);

  StructuredCloneData data;
  ipc::UnpackClonedMessageDataForParent(aData, data);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  if (!ReceiveMessage(aMessage, false, &data, &cpows, aPrincipal, nullptr)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSetCursor(const nsCursor& aCursor, const bool& aForce)
{
  mCursor = aCursor;
  mCustomCursor = nullptr;

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    if (aForce) {
      widget->ClearCachedCursor();
    }
    if (mTabSetsCursor) {
      widget->SetCursor(mCursor);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSetCustomCursor(const nsCString& aCursorData,
                               const uint32_t& aWidth,
                               const uint32_t& aHeight,
                               const uint32_t& aStride,
                               const gfx::SurfaceFormat& aFormat,
                               const uint32_t& aHotspotX,
                               const uint32_t& aHotspotY,
                               const bool& aForce)
{
  mCursor = eCursorInvalid;

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    if (aForce) {
      widget->ClearCachedCursor();
    }

    if (mTabSetsCursor) {
      const gfx::IntSize size(aWidth, aHeight);

      RefPtr<gfx::DataSourceSurface> customCursor =
          gfx::CreateDataSourceSurfaceFromData(size,
                                               aFormat,
                                               reinterpret_cast<const uint8_t*>(aCursorData.BeginReading()),
                                               aStride);

      RefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(customCursor, size);
      nsCOMPtr<imgIContainer> cursorImage(image::ImageOps::CreateFromDrawable(drawable));
      widget->SetCursor(cursorImage, aHotspotX, aHotspotY);
      mCustomCursor = cursorImage;
      mCustomCursorHotspotX = aHotspotX;
      mCustomCursorHotspotY = aHotspotY;
    }
  }

  return IPC_OK();
}

nsIXULBrowserWindow*
TabParent::GetXULBrowserWindow()
{
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

  nsCOMPtr<nsIXULWindow> window = do_GetInterface(treeOwner);
  if (!window) {
    return nullptr;
  }

  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow;
  window->GetXULBrowserWindow(getter_AddRefs(xulBrowserWindow));
  return xulBrowserWindow;
}

mozilla::ipc::IPCResult
TabParent::RecvSetStatus(const uint32_t& aType, const nsString& aStatus)
{
  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  if (!xulBrowserWindow) {
    return IPC_OK();
  }

  switch (aType) {
   case nsIWebBrowserChrome::STATUS_LINK:
    xulBrowserWindow->SetOverLink(aStatus, nullptr);
    break;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvShowTooltip(const uint32_t& aX, const uint32_t& aY, const nsString& aTooltip,
                           const nsString& aDirection)
{
  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  if (!xulBrowserWindow) {
    return IPC_OK();
  }

  nsCOMPtr<nsIFrameLoaderOwner> frame = do_QueryInterface(mFrameElement);
  if (!frame)
    return IPC_OK();

  xulBrowserWindow->ShowTooltip(aX, aY, aTooltip, aDirection, frame);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvHideTooltip()
{
  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  if (!xulBrowserWindow) {
    return IPC_OK();
  }

  xulBrowserWindow->HideTooltip();
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvNotifyIMEFocus(const ContentCache& aContentCache,
                              const IMENotification& aIMENotification,
                              NotifyIMEFocusResolver&& aResolve)
{
  if (mIsDestroyed) {
    return IPC_OK();
  }

  nsCOMPtr<nsIWidget> widget = GetDocWidget();
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

mozilla::ipc::IPCResult
TabParent::RecvNotifyIMETextChange(const ContentCache& aContentCache,
                                   const IMENotification& aIMENotification)
{
  nsCOMPtr<nsIWidget> widget = GetDocWidget();
  if (!widget || !IMEStateManager::DoesTabParentHaveIMEFocus(this)) {
    return IPC_OK();
  }
  mContentCache.AssignContent(aContentCache, widget, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvNotifyIMECompositionUpdate(
             const ContentCache& aContentCache,
             const IMENotification& aIMENotification)
{
  nsCOMPtr<nsIWidget> widget = GetDocWidget();
  if (!widget || !IMEStateManager::DoesTabParentHaveIMEFocus(this)) {
    return IPC_OK();
  }
  mContentCache.AssignContent(aContentCache, widget, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvNotifyIMESelection(const ContentCache& aContentCache,
                                  const IMENotification& aIMENotification)
{
  nsCOMPtr<nsIWidget> widget = GetDocWidget();
  if (!widget || !IMEStateManager::DoesTabParentHaveIMEFocus(this)) {
    return IPC_OK();
  }
  mContentCache.AssignContent(aContentCache, widget, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvUpdateContentCache(const ContentCache& aContentCache)
{
  nsCOMPtr<nsIWidget> widget = GetDocWidget();
  if (!widget || !IMEStateManager::DoesTabParentHaveIMEFocus(this)) {
    return IPC_OK();
  }

  mContentCache.AssignContent(aContentCache, widget);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvNotifyIMEMouseButtonEvent(
             const IMENotification& aIMENotification,
             bool* aConsumedByIME)
{

  nsCOMPtr<nsIWidget> widget = GetDocWidget();
  if (!widget || !IMEStateManager::DoesTabParentHaveIMEFocus(this)) {
    *aConsumedByIME = false;
    return IPC_OK();
  }
  nsresult rv = IMEStateManager::NotifyIME(aIMENotification, widget, this);
  *aConsumedByIME = rv == NS_SUCCESS_EVENT_CONSUMED;
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvNotifyIMEPositionChange(const ContentCache& aContentCache,
                                       const IMENotification& aIMENotification)
{
  nsCOMPtr<nsIWidget> widget = GetDocWidget();
  if (!widget || !IMEStateManager::DoesTabParentHaveIMEFocus(this)) {
    return IPC_OK();
  }
  mContentCache.AssignContent(aContentCache, widget, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvOnEventNeedingAckHandled(const EventMessage& aMessage)
{
  // This is called when the child process receives WidgetCompositionEvent or
  // WidgetSelectionEvent.
  // FYI: Don't check if widget is nullptr here because it's more important to
  //      notify mContentCahce of this than handling something in it.
  nsCOMPtr<nsIWidget> widget = GetDocWidget();

  // While calling OnEventNeedingAckHandled(), TabParent *might* be destroyed
  // since it may send notifications to IME.
  RefPtr<TabParent> kungFuDeathGrip(this);
  mContentCache.OnEventNeedingAckHandled(widget, aMessage);
  return IPC_OK();
}

void
TabParent::HandledWindowedPluginKeyEvent(const NativeEventData& aKeyEventData,
                                         bool aIsConsumed)
{
  DebugOnly<bool> ok =
    SendHandledWindowedPluginKeyEvent(aKeyEventData, aIsConsumed);
  NS_WARNING_ASSERTION(ok, "SendHandledWindowedPluginKeyEvent failed");
}

mozilla::ipc::IPCResult
TabParent::RecvOnWindowedPluginKeyEvent(const NativeEventData& aKeyEventData)
{
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

mozilla::ipc::IPCResult
TabParent::RecvRequestFocus(const bool& aCanRaise)
{
  nsCOMPtr<nsIFocusManager> fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return IPC_OK();
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(mFrameElement);
  if (!content || !content->OwnerDoc()) {
    return IPC_OK();
  }

  uint32_t flags = nsIFocusManager::FLAG_NOSCROLL;
  if (aCanRaise)
    flags |= nsIFocusManager::FLAG_RAISE;

  RefPtr<Element> element = mFrameElement;
  fm->SetFocus(element, flags);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvEnableDisableCommands(const nsString& aAction,
                                     nsTArray<nsCString>&& aEnabledCommands,
                                     nsTArray<nsCString>&& aDisabledCommands)
{
  nsCOMPtr<nsIRemoteBrowser> remoteBrowser = do_QueryInterface(mFrameElement);
  if (remoteBrowser) {
    UniquePtr<const char*[]> enabledCommands, disabledCommands;

    if (aEnabledCommands.Length()) {
      enabledCommands = MakeUnique<const char*[]>(aEnabledCommands.Length());
      for (uint32_t c = 0; c < aEnabledCommands.Length(); c++) {
        enabledCommands[c] = aEnabledCommands[c].get();
      }
    }

    if (aDisabledCommands.Length()) {
      disabledCommands = MakeUnique<const char*[]>(aDisabledCommands.Length());
      for (uint32_t c = 0; c < aDisabledCommands.Length(); c++) {
        disabledCommands[c] = aDisabledCommands[c].get();
      }
    }

    remoteBrowser->EnableDisableCommands(aAction,
                                         aEnabledCommands.Length(), enabledCommands.get(),
                                         aDisabledCommands.Length(), disabledCommands.get());
  }

  return IPC_OK();
}

NS_IMETHODIMP
TabParent::GetChildProcessOffset(int32_t* aOutCssX, int32_t* aOutCssY)
{
  NS_ENSURE_ARG(aOutCssX);
  NS_ENSURE_ARG(aOutCssY);
  CSSPoint offset = LayoutDevicePoint(GetChildProcessOffset())
      * GetLayoutDeviceToCSSScale();
  *aOutCssX = offset.x;
  *aOutCssY = offset.y;
  return NS_OK;
}

LayoutDeviceIntPoint
TabParent::GetChildProcessOffset()
{
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
  return -nsLayoutUtils::TranslateViewToWidget(presContext, rootView, pt, widget);
}

LayoutDeviceIntPoint
TabParent::GetClientOffset()
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  nsCOMPtr<nsIWidget> docWidget = GetDocWidget();

  if (widget == docWidget) {
    return widget->GetClientOffset();
  }

  return (docWidget->GetClientOffset() +
          nsLayoutUtils::WidgetToWidgetOffset(widget, docWidget));
}

mozilla::ipc::IPCResult
TabParent::RecvReplyKeyEvent(const WidgetKeyboardEvent& aEvent)
{
  NS_ENSURE_TRUE(mFrameElement, IPC_OK());

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.MarkAsHandledInRemoteProcess();

  // Here we convert the WidgetEvent that we received to an Event
  // to be able to dispatch it to the <browser> element as the target element.
  nsIDocument* doc = mFrameElement->OwnerDoc();
  nsPresContext* presContext = doc->GetPresContext();
  NS_ENSURE_TRUE(presContext, IPC_OK());

  AutoHandlingUserInputStatePusher userInpStatePusher(localEvent.IsTrusted(),
                                                      &localEvent, doc);

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

mozilla::ipc::IPCResult
TabParent::RecvAccessKeyNotHandled(const WidgetKeyboardEvent& aEvent)
{
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
  nsIDocument* doc = mFrameElement->OwnerDoc();
  nsIPresShell* presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, IPC_OK());

  if (presShell->CanDispatchEvent()) {
    nsPresContext* presContext = presShell->GetPresContext();
    NS_ENSURE_TRUE(presContext, IPC_OK());

    EventDispatcher::Dispatch(mFrameElement, presContext, &localEvent);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSetHasBeforeUnload(const bool& aHasBeforeUnload)
{
  mHasBeforeUnload = aHasBeforeUnload;
  return IPC_OK();
}

bool
TabParent::HandleQueryContentEvent(WidgetQueryContentEvent& aEvent)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }
  if (NS_WARN_IF(!mContentCache.HandleQueryContentEvent(aEvent, widget)) ||
      NS_WARN_IF(!aEvent.mSucceeded)) {
    return true;
  }
  switch (aEvent.mMessage) {
    case eQueryTextRect:
    case eQueryCaretRect:
    case eQueryEditorRect:
    {
      nsCOMPtr<nsIWidget> widget = GetWidget();
      nsCOMPtr<nsIWidget> docWidget = GetDocWidget();
      if (widget != docWidget) {
        aEvent.mReply.mRect +=
          nsLayoutUtils::WidgetToWidgetOffset(widget, docWidget);
      }
      aEvent.mReply.mRect -= GetChildProcessOffset();
      break;
    }
    default:
      break;
  }
  return true;
}

bool
TabParent::SendCompositionEvent(WidgetCompositionEvent& aEvent)
{
  if (mIsDestroyed) {
    return false;
  }

  if (!mContentCache.OnCompositionEvent(aEvent)) {
    return true;
  }

  bool ret =
    Manager()->AsContentParent()->IsInputPriorityEventEnabled()
      ? PBrowserParent::SendCompositionEvent(aEvent)
      : PBrowserParent::SendNormalPriorityCompositionEvent(aEvent);
  if (NS_WARN_IF(!ret)) {
    return false;
  }
  MOZ_ASSERT(aEvent.HasBeenPostedToRemoteProcess());
  return true;
}

bool
TabParent::SendSelectionEvent(WidgetSelectionEvent& aEvent)
{
  if (mIsDestroyed) {
    return false;
  }
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }
  mContentCache.OnSelectionEvent(aEvent);
  bool ret =
    Manager()->AsContentParent()->IsInputPriorityEventEnabled()
      ? PBrowserParent::SendSelectionEvent(aEvent)
      : PBrowserParent::SendNormalPrioritySelectionEvent(aEvent);
  if (NS_WARN_IF(!ret)) {
    return false;
  }
  MOZ_ASSERT(aEvent.HasBeenPostedToRemoteProcess());
  aEvent.mSucceeded = true;
  return true;
}

bool
TabParent::SendPasteTransferable(const IPCDataTransfer& aDataTransfer,
                                 const bool& aIsPrivateData,
                                 const IPC::Principal& aRequestingPrincipal,
                                 const uint32_t& aContentPolicyType)
{
  return PBrowserParent::SendPasteTransferable(aDataTransfer,
                                               aIsPrivateData,
                                               aRequestingPrincipal,
                                               aContentPolicyType);
}

/*static*/ TabParent*
TabParent::GetFrom(nsFrameLoader* aFrameLoader)
{
  if (!aFrameLoader) {
    return nullptr;
  }
  PBrowserParent* remoteBrowser = aFrameLoader->GetRemoteBrowser();
  return static_cast<TabParent*>(remoteBrowser);
}

/*static*/ TabParent*
TabParent::GetFrom(nsITabParent* aTabParent)
{
  return static_cast<TabParent*>(aTabParent);
}

/*static*/ TabParent*
TabParent::GetFrom(PBrowserParent* aTabParent)
{
  return static_cast<TabParent*>(aTabParent);
}

/*static*/ TabParent*
TabParent::GetFrom(nsIContent* aContent)
{
  nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(aContent);
  if (!loaderOwner) {
    return nullptr;
  }
  RefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
  return GetFrom(frameLoader);
}

/*static*/ TabId
TabParent::GetTabIdFrom(nsIDocShell *docShell)
{
  nsCOMPtr<nsITabChild> tabChild(TabChild::GetFrom(docShell));
  if (tabChild) {
    return static_cast<TabChild*>(tabChild.get())->GetTabId();
  }
  return TabId(0);
}

RenderFrameParent*
TabParent::GetRenderFrame()
{
  PRenderFrameParent* p = LoneManagedOrNullAsserts(ManagedPRenderFrameParent());
  RenderFrameParent* frame = static_cast<RenderFrameParent*>(p);

  return frame;
}

mozilla::ipc::IPCResult
TabParent::RecvRequestIMEToCommitComposition(const bool& aCancel,
                                             bool* aIsCommitted,
                                             nsString* aCommittedString)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    *aIsCommitted = false;
    return IPC_OK();
  }

  *aIsCommitted =
    mContentCache.RequestIMEToCommitComposition(widget, aCancel,
                                                *aCommittedString);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvStartPluginIME(const WidgetKeyboardEvent& aKeyboardEvent,
                              const int32_t& aPanelX, const int32_t& aPanelY,
                              nsString* aCommitted)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }
  Unused << widget->StartPluginIME(aKeyboardEvent,
                                   (int32_t&)aPanelX,
                                   (int32_t&)aPanelY,
                                   *aCommitted);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSetPluginFocused(const bool& aFocused)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }
  widget->SetPluginFocused((bool&)aFocused);
  return IPC_OK();
}

 mozilla::ipc::IPCResult
TabParent::RecvSetCandidateWindowForPlugin(
             const CandidateWindowPosition& aPosition)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  widget->SetCandidateWindowForPlugin(aPosition);
  return IPC_OK();
}

 mozilla::ipc::IPCResult
TabParent::RecvEnableIMEForPlugin(const bool& aEnable)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }
  widget->EnableIMEForPlugin(aEnable);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvDefaultProcOfPluginEvent(const WidgetPluginEvent& aEvent)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  widget->DefaultProcOfPluginEvent(aEvent);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvGetInputContext(widget::IMEState* aState)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    *aState = widget::IMEState(IMEState::DISABLED,
                               IMEState::OPEN_STATE_NOT_SUPPORTED);
    return IPC_OK();
  }

  *aState = widget->GetInputContext().mIMEState;
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvSetInputContext(
  const InputContext& aContext,
  const InputContextAction& aAction)
{
  IMEStateManager::SetInputContextForChildProcess(this, aContext, aAction);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvIsParentWindowMainWidgetVisible(bool* aIsVisible)
{
  nsCOMPtr<nsIContent> frame = do_QueryInterface(mFrameElement);
  if (!frame)
    return IPC_OK();
  nsCOMPtr<nsIDOMWindowUtils> windowUtils =
    do_QueryInterface(frame->OwnerDoc()->GetWindow());
  nsresult rv = windowUtils->GetIsParentWindowMainWidgetVisible(aIsVisible);
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

already_AddRefed<nsIWidget>
TabParent::GetTopLevelWidget()
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mFrameElement);
  if (content) {
    nsIPresShell* shell = content->OwnerDoc()->GetShell();
    if (shell) {
      nsViewManager* vm = shell->GetViewManager();
      nsCOMPtr<nsIWidget> widget;
      vm->GetRootWidget(getter_AddRefs(widget));
      return widget.forget();
    }
  }
  return nullptr;
}

mozilla::ipc::IPCResult
TabParent::RecvSetNativeChildOfShareableWindow(const uintptr_t& aChildWindow)
{
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
    "TabParent::RecvSetNativeChildOfShareableWindow not implemented!");
  return IPC_FAIL_NO_REASON(this);
#endif
}

mozilla::ipc::IPCResult
TabParent::RecvDispatchFocusToTopLevelWindow()
{
  nsCOMPtr<nsIWidget> widget = GetTopLevelWidget();
  if (widget) {
    widget->SetFocus(false);
  }
  return IPC_OK();
}

bool
TabParent::ReceiveMessage(const nsString& aMessage,
                          bool aSync,
                          StructuredCloneData* aData,
                          CpowHolder* aCpows,
                          nsIPrincipal* aPrincipal,
                          nsTArray<StructuredCloneData>* aRetVal)
{
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader(true);
  if (frameLoader && frameLoader->GetFrameMessageManager()) {
    RefPtr<nsFrameMessageManager> manager =
      frameLoader->GetFrameMessageManager();

    manager->ReceiveMessage(mFrameElement,
                            frameLoader,
                            aMessage,
                            aSync,
                            aData,
                            aCpows,
                            aPrincipal,
                            aRetVal,
                            IgnoreErrors());
  }
  return true;
}

// nsIAuthPromptProvider

// This method is largely copied from nsDocShell::GetAuthPrompt
NS_IMETHODIMP
TabParent::GetAuthPrompt(uint32_t aPromptReason, const nsIID& iid,
                          void** aResult)
{
  // we're either allowing auth, or it's a proxy request
  nsresult rv;
  nsCOMPtr<nsIPromptFactory> wwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsPIDOMWindowOuter> window;
  nsCOMPtr<nsIContent> frame = do_QueryInterface(mFrameElement);
  if (frame)
    window = frame->OwnerDoc()->GetWindow();

  // Get an auth prompter for our window so that the parenting
  // of the dialogs works as it should when using tabs.
  nsCOMPtr<nsISupports> prompt;
  rv = wwatch->GetPrompt(window, iid, getter_AddRefs(prompt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoginManagerPrompter> prompter = do_QueryInterface(prompt);
  if (prompter) {
    prompter->SetBrowser(mFrameElement);
  }

  *aResult = prompt.forget().take();
  return NS_OK;
}

PColorPickerParent*
TabParent::AllocPColorPickerParent(const nsString& aTitle,
                                   const nsString& aInitialColor)
{
  return new ColorPickerParent(aTitle, aInitialColor);
}

bool
TabParent::DeallocPColorPickerParent(PColorPickerParent* actor)
{
  delete actor;
  return true;
}

PRenderFrameParent*
TabParent::AllocPRenderFrameParent()
{
  MOZ_ASSERT(ManagedPRenderFrameParent().IsEmpty());
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();

  RenderFrameParent* rfp = new RenderFrameParent(frameLoader);
  if (rfp->IsInitted()) {
    layers::LayersId layersId = rfp->GetLayersId();
    AddTabParentToTable(layersId, this);
  }
  return rfp;
}

bool
TabParent::DeallocPRenderFrameParent(PRenderFrameParent* aFrame)
{
  delete aFrame;
  return true;
}

bool
TabParent::SetRenderFrame(PRenderFrameParent* aRFParent)
{
  if (IsInitedByParent()) {
    return false;
  }

  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();

  if (!frameLoader) {
    return false;
  }

  RenderFrameParent* renderFrame = static_cast<RenderFrameParent*>(aRFParent);
  bool success = renderFrame->Init(frameLoader);
  if (!success) {
    return false;
  }

  frameLoader->MaybeShowFrame();

  layers::LayersId layersId = renderFrame->GetLayersId();
  AddTabParentToTable(layersId, this);

  return true;
}

bool
TabParent::GetRenderFrameInfo(TextureFactoryIdentifier* aTextureFactoryIdentifier,
                              layers::LayersId* aLayersId)
{
  RenderFrameParent* rfp = GetRenderFrame();
  if (!rfp) {
    return false;
  }

  *aLayersId = rfp->GetLayersId();
  rfp->GetTextureFactoryIdentifier(aTextureFactoryIdentifier);
  return true;
}

already_AddRefed<nsFrameLoader>
TabParent::GetFrameLoader(bool aUseCachedFrameLoaderAfterDestroy) const
{
  if (mIsDestroyed && !aUseCachedFrameLoaderAfterDestroy) {
    return nullptr;
  }

  if (mFrameLoader) {
    RefPtr<nsFrameLoader> fl = mFrameLoader;
    return fl.forget();
  }
  nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner = do_QueryInterface(mFrameElement);
  return frameLoaderOwner ? frameLoaderOwner->GetFrameLoader() : nullptr;
}

void
TabParent::TryCacheDPIAndScale()
{
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

already_AddRefed<nsIWidget>
TabParent::GetWidget() const
{
  if (!mFrameElement) {
    return nullptr;
  }
  nsCOMPtr<nsIWidget> widget = nsContentUtils::WidgetForContent(mFrameElement);
  if (!widget) {
    widget = nsContentUtils::WidgetForDocument(mFrameElement->OwnerDoc());
  }
  return widget.forget();
}

already_AddRefed<nsIWidget>
TabParent::GetDocWidget() const
{
  if (!mFrameElement) {
    return nullptr;
  }
  return do_AddRef(nsContentUtils::WidgetForDocument(mFrameElement->OwnerDoc()));
}

void
TabParent::ApzAwareEventRoutingToChild(ScrollableLayerGuid* aOutTargetGuid,
                                       uint64_t* aOutInputBlockId,
                                       nsEventStatus* aOutApzResponse)
{
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
      // the layersId of this renderframe. In such cases the main-thread hit-
      // testing code "wins" so we need to update the guid to reflect this.
      if (RenderFrameParent* rfp = GetRenderFrame()) {
        if (aOutTargetGuid->mLayersId != rfp->GetLayersId()) {
          *aOutTargetGuid = ScrollableLayerGuid(rfp->GetLayersId(), 0, FrameMetrics::NULL_SCROLL_ID);
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

mozilla::ipc::IPCResult
TabParent::RecvBrowserFrameOpenWindow(PBrowserParent* aOpener,
                                      PRenderFrameParent* aRenderFrame,
                                      const nsString& aURL,
                                      const nsString& aName,
                                      const nsString& aFeatures,
                                      BrowserFrameOpenWindowResolver&& aResolve)
{
  CreatedWindowInfo cwi;
  cwi.rv() = NS_OK;
  cwi.layersId() = LayersId{0};
  cwi.maxTouchPoints() = 0;

  BrowserElementParent::OpenWindowResult opened =
    BrowserElementParent::OpenWindowOOP(TabParent::GetFrom(aOpener),
                                        this, aRenderFrame, aURL, aName, aFeatures,
                                        &cwi.textureFactoryIdentifier(),
                                        &cwi.layersId());
  cwi.compositorOptions() =
    static_cast<RenderFrameParent*>(aRenderFrame)->GetCompositorOptions();
  cwi.windowOpened() = (opened == BrowserElementParent::OPEN_WINDOW_ADDED);
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    cwi.maxTouchPoints() = widget->GetMaxTouchPoints();
    cwi.dimensions() = GetDimensionInfo();
  }

  // Resolve the request with the information we collected.
  aResolve(cwi);

  if (!cwi.windowOpened()) {
    Destroy();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvRespondStartSwipeEvent(const uint64_t& aInputBlockId,
                                      const bool& aStartSwipe)
{
  if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
    widget->ReportSwipeStarted(aInputBlockId, aStartSwipe);
  }
  return IPC_OK();
}

already_AddRefed<nsILoadContext>
TabParent::GetLoadContext()
{
  nsCOMPtr<nsILoadContext> loadContext;
  if (mLoadContext) {
    loadContext = mLoadContext;
  } else {
    bool isPrivate = mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
    SetPrivateBrowsingAttributes(isPrivate);
    bool useTrackingProtection = false;
    nsCOMPtr<nsIDocShell> docShell = mFrameElement->OwnerDoc()->GetDocShell();
    if (docShell) {
      docShell->GetUseTrackingProtection(&useTrackingProtection);
    }
    loadContext = new LoadContext(GetOwnerElement(),
                                  true /* aIsContent */,
                                  isPrivate,
                                  mChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW,
                                  useTrackingProtection,
                                  OriginAttributesRef());
    mLoadContext = loadContext;
  }
  return loadContext.forget();
}

NS_IMETHODIMP
TabParent::GetUseAsyncPanZoom(bool* useAsyncPanZoom)
{
  *useAsyncPanZoom = AsyncPanZoomEnabled();
  return NS_OK;
}

// defined in nsITabParent
NS_IMETHODIMP
TabParent::SetDocShellIsActive(bool isActive)
{
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

  // Let's inform the priority manager. This operation can end up with the
  // changing of the process priority.
  ProcessPriorityManager::TabActivityChanged(this, isActive);

  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetDocShellIsActive(bool* aIsActive)
{
  *aIsActive = mDocShellIsActive;
  return NS_OK;
}

NS_IMETHODIMP
TabParent::SetRenderLayers(bool aEnabled)
{
  if (aEnabled == mRenderLayers) {
    if (aEnabled && mHasLayers && mPreserveLayers) {
      // RenderLayers might be called when we've been preserving layers,
      // and already had layers uploaded. In that case, the MozLayerTreeReady
      // event will not naturally arrive, which can confuse the front-end
      // layer. So we fire the event here.
      RefPtr<TabParent> self = this;
      uint64_t epoch = mLayerTreeEpoch;
      NS_DispatchToMainThread(NS_NewRunnableFunction(
        "dom::TabParent::RenderLayers",
        [self, epoch] () {
          MOZ_ASSERT(NS_IsMainThread());
          self->LayerTreeUpdate(epoch, true);
        }));
    }

    return NS_OK;
  }

  // Preserve layers means that attempts to stop rendering layers
  // will be ignored.
  if (!aEnabled && mPreserveLayers) {
    return NS_OK;
  }

  mRenderLayers = aEnabled;

  SetRenderLayersInternal(aEnabled, false /* aForceRepaint */);
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetRenderLayers(bool* aResult)
{
  *aResult = mRenderLayers;
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetHasLayers(bool* aResult)
{
  *aResult = mHasLayers;
  return NS_OK;
}

NS_IMETHODIMP
TabParent::ForceRepaint()
{
  SetRenderLayersInternal(true /* aEnabled */,
                          true /* aForceRepaint */);
  return NS_OK;
}

void
TabParent::SetRenderLayersInternal(bool aEnabled, bool aForceRepaint)
{
  // Increment the epoch so that layer tree updates from previous
  // RenderLayers requests are ignored.
  mLayerTreeEpoch++;

  Unused << SendRenderLayers(aEnabled, aForceRepaint, mLayerTreeEpoch);

  // Ask the child to repaint using the PHangMonitor channel/thread (which may
  // be less congested).
  if (aEnabled) {
    ContentParent* cp = Manager()->AsContentParent();
    cp->PaintTabWhileInterruptingJS(this, aForceRepaint, mLayerTreeEpoch);
  }
}

NS_IMETHODIMP
TabParent::PreserveLayers(bool aPreserveLayers)
{
  mPreserveLayers = aPreserveLayers;
  return NS_OK;
}

NS_IMETHODIMP
TabParent::SuppressDisplayport(bool aEnabled)
{
  if (IsDestroyed()) {
    return NS_OK;
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
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetTabId(uint64_t* aId)
{
  *aId = GetTabId();
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetOsPid(int32_t* aId)
{
  *aId = Manager()->Pid();
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetHasContentOpener(bool* aResult)
{
  *aResult = mHasContentOpener;
  return NS_OK;
}

void
TabParent::SetHasContentOpener(bool aHasContentOpener)
{
  mHasContentOpener = aHasContentOpener;
}

NS_IMETHODIMP
TabParent::GetHasPresented(bool* aResult)
{
  *aResult = mHasPresented;
  return NS_OK;
}

NS_IMETHODIMP
TabParent::NavigateByKey(bool aForward, bool aForDocumentNavigation)
{
  Unused << SendNavigateByKey(aForward, aForDocumentNavigation);
  return NS_OK;
}

NS_IMETHODIMP
TabParent::TransmitPermissionsForPrincipal(nsIPrincipal* aPrincipal)
{
  nsCOMPtr<nsIContentParent> manager = Manager();
  if (!manager->IsContentParent()) {
    return NS_ERROR_UNEXPECTED;
  }

  return manager->AsContentParent()
    ->TransmitPermissionsForPrincipal(aPrincipal);
}

NS_IMETHODIMP
TabParent::GetHasBeforeUnload(bool* aResult)
{
  *aResult = mHasBeforeUnload;
  return NS_OK;
}

void
TabParent::LayerTreeUpdate(uint64_t aEpoch, bool aActive)
{
  // Ignore updates from old epochs. They might tell us that layers are
  // available when we've already sent a message to clear them. We can't trust
  // the update in that case since layers could disappear anytime after that.
  if (aEpoch != mLayerTreeEpoch || mIsDestroyed) {
    return;
  }

  nsCOMPtr<mozilla::dom::EventTarget> target = do_QueryInterface(mFrameElement);
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

mozilla::ipc::IPCResult
TabParent::RecvPaintWhileInterruptingJSNoOp(const uint64_t& aLayerObserverEpoch)
{
  // We sent a PaintWhileInterruptingJS message when layers were already visible. In this
  // case, we should act as if an update occurred even though we already have
  // the layers.
  LayerTreeUpdate(aLayerObserverEpoch, true);
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvRemotePaintIsReady()
{
  nsCOMPtr<mozilla::dom::EventTarget> target = do_QueryInterface(mFrameElement);
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

mozilla::ipc::IPCResult
TabParent::RecvRemoteIsReadyToHandleInputEvents()
{
  // When enabling input event prioritization, input events may preempt other
  // normal priority IPC messages. To prevent the input events preempt
  // PBrowserConstructor, we use an IPC 'RemoteIsReadyToHandleInputEvents' to
  // notify the parent that TabChild is created and ready to handle input
  // events.
  SetReadyToHandleInputEvents();
  return IPC_OK();
}

mozilla::plugins::PPluginWidgetParent*
TabParent::AllocPPluginWidgetParent()
{
#ifdef XP_WIN
  return new mozilla::plugins::PluginWidgetParent();
#else
  MOZ_ASSERT_UNREACHABLE("AllocPPluginWidgetParent only supports Windows");
  return nullptr;
#endif
}

bool
TabParent::DeallocPPluginWidgetParent(mozilla::plugins::PPluginWidgetParent* aActor)
{
  delete aActor;
  return true;
}

PPaymentRequestParent*
TabParent::AllocPPaymentRequestParent()
{
  RefPtr<PaymentRequestParent> actor = new PaymentRequestParent(GetTabId());
  return actor.forget().take();
}

bool
TabParent::DeallocPPaymentRequestParent(PPaymentRequestParent* aActor)
{
  RefPtr<PaymentRequestParent> actor =
    dont_AddRef(static_cast<PaymentRequestParent*>(aActor));
  return true;
}

nsresult
TabParent::HandleEvent(Event* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (eventType.EqualsLiteral("MozUpdateWindowPos") && !mIsDestroyed) {
    // This event is sent when the widget moved.  Therefore we only update
    // the position.
    return UpdatePosition();
  }
  return NS_OK;
}

class FakeChannel final : public nsIChannel,
                          public nsIAuthPromptCallback,
                          public nsIInterfaceRequestor,
                          public nsILoadContext
{
public:
  FakeChannel(const nsCString& aUri, uint64_t aCallbackId, Element* aElement)
    : mCallbackId(aCallbackId)
    , mElement(aElement)
  {
    NS_NewURI(getter_AddRefs(mUri), aUri);
  }

  NS_DECL_ISUPPORTS
#define NO_IMPL override { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetName(nsACString&) NO_IMPL
  NS_IMETHOD IsPending(bool*) NO_IMPL
  NS_IMETHOD GetStatus(nsresult*) NO_IMPL
  NS_IMETHOD Cancel(nsresult) NO_IMPL
  NS_IMETHOD Suspend() NO_IMPL
  NS_IMETHOD Resume() NO_IMPL
  NS_IMETHOD GetLoadGroup(nsILoadGroup**) NO_IMPL
  NS_IMETHOD SetLoadGroup(nsILoadGroup*) NO_IMPL
  NS_IMETHOD SetLoadFlags(nsLoadFlags) NO_IMPL
  NS_IMETHOD GetLoadFlags(nsLoadFlags*) NO_IMPL
  NS_IMETHOD GetIsDocument(bool *) NO_IMPL
  NS_IMETHOD GetOriginalURI(nsIURI**) NO_IMPL
  NS_IMETHOD SetOriginalURI(nsIURI*) NO_IMPL
  NS_IMETHOD GetURI(nsIURI** aUri) override
  {
    nsCOMPtr<nsIURI> copy = mUri;
    copy.forget(aUri);
    return NS_OK;
  }
  NS_IMETHOD GetOwner(nsISupports**) NO_IMPL
  NS_IMETHOD SetOwner(nsISupports*) NO_IMPL
  NS_IMETHOD GetLoadInfo(nsILoadInfo** aLoadInfo) override
  {
    nsCOMPtr<nsILoadInfo> copy = mLoadInfo;
    copy.forget(aLoadInfo);
    return NS_OK;
  }
  NS_IMETHOD SetLoadInfo(nsILoadInfo* aLoadInfo) override
  {
    mLoadInfo = aLoadInfo;
    return NS_OK;
  }
  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor** aRequestor) override
  {
    NS_ADDREF(*aRequestor = this);
    return NS_OK;
  }
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor*) NO_IMPL
  NS_IMETHOD GetSecurityInfo(nsISupports**) NO_IMPL
  NS_IMETHOD GetContentType(nsACString&) NO_IMPL
  NS_IMETHOD SetContentType(const nsACString&) NO_IMPL
  NS_IMETHOD GetContentCharset(nsACString&) NO_IMPL
  NS_IMETHOD SetContentCharset(const nsACString&) NO_IMPL
  NS_IMETHOD GetContentLength(int64_t*) NO_IMPL
  NS_IMETHOD SetContentLength(int64_t) NO_IMPL
  NS_IMETHOD Open(nsIInputStream**) NO_IMPL
  NS_IMETHOD Open2(nsIInputStream**) NO_IMPL
  NS_IMETHOD AsyncOpen(nsIStreamListener*, nsISupports*) NO_IMPL
  NS_IMETHOD AsyncOpen2(nsIStreamListener*) NO_IMPL
  NS_IMETHOD GetContentDisposition(uint32_t*) NO_IMPL
  NS_IMETHOD SetContentDisposition(uint32_t) NO_IMPL
  NS_IMETHOD GetContentDispositionFilename(nsAString&) NO_IMPL
  NS_IMETHOD SetContentDispositionFilename(const nsAString&) NO_IMPL
  NS_IMETHOD GetContentDispositionHeader(nsACString&) NO_IMPL
  NS_IMETHOD OnAuthAvailable(nsISupports *aContext, nsIAuthInformation *aAuthInfo) override;
  NS_IMETHOD OnAuthCancelled(nsISupports *aContext, bool userCancel) override;
  NS_IMETHOD GetInterface(const nsIID & uuid, void **result) override
  {
    return QueryInterface(uuid, result);
  }
  NS_IMETHOD GetAssociatedWindow(mozIDOMWindowProxy**) NO_IMPL
  NS_IMETHOD GetTopWindow(mozIDOMWindowProxy**) NO_IMPL
  NS_IMETHOD GetTopFrameElement(Element** aElement) override
  {
    nsCOMPtr<Element> elem = mElement;
    elem.forget(aElement);
    return NS_OK;
  }
  NS_IMETHOD GetNestedFrameId(uint64_t*) NO_IMPL
  NS_IMETHOD GetIsContent(bool*) NO_IMPL
  NS_IMETHOD GetUsePrivateBrowsing(bool*) NO_IMPL
  NS_IMETHOD SetUsePrivateBrowsing(bool) NO_IMPL
  NS_IMETHOD SetPrivateBrowsing(bool) NO_IMPL
  NS_IMETHOD GetIsInIsolatedMozBrowserElement(bool*) NO_IMPL
  NS_IMETHOD GetScriptableOriginAttributes(JS::MutableHandleValue) NO_IMPL
  NS_IMETHOD_(void) GetOriginAttributes(mozilla::OriginAttributes& aAttrs) override {}
  NS_IMETHOD GetUseRemoteTabs(bool*) NO_IMPL
  NS_IMETHOD SetRemoteTabs(bool) NO_IMPL
  NS_IMETHOD GetUseTrackingProtection(bool*) NO_IMPL
  NS_IMETHOD SetUseTrackingProtection(bool) NO_IMPL
#undef NO_IMPL

protected:
  ~FakeChannel() {}

  nsCOMPtr<nsIURI> mUri;
  uint64_t mCallbackId;
  nsCOMPtr<Element> mElement;
  nsCOMPtr<nsILoadInfo> mLoadInfo;
};

NS_IMPL_ISUPPORTS(FakeChannel, nsIChannel, nsIAuthPromptCallback,
                  nsIRequest, nsIInterfaceRequestor, nsILoadContext);

mozilla::ipc::IPCResult
TabParent::RecvAsyncAuthPrompt(const nsCString& aUri,
                               const nsString& aRealm,
                               const uint64_t& aCallbackId)
{
  nsCOMPtr<nsIAuthPrompt2> authPrompt;
  GetAuthPrompt(nsIAuthPromptProvider::PROMPT_NORMAL,
                NS_GET_IID(nsIAuthPrompt2),
                getter_AddRefs(authPrompt));
  RefPtr<FakeChannel> channel = new FakeChannel(aUri, aCallbackId, mFrameElement);
  uint32_t promptFlags = nsIAuthInformation::AUTH_HOST;

  RefPtr<nsAuthInformationHolder> holder =
    new nsAuthInformationHolder(promptFlags, aRealm,
                                EmptyCString());

  uint32_t level = nsIAuthPrompt2::LEVEL_NONE;
  nsCOMPtr<nsICancelable> dummy;
  nsresult rv =
    authPrompt->AsyncPromptAuth(channel, channel, nullptr,
                                level, holder, getter_AddRefs(dummy));

  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvInvokeDragSession(nsTArray<IPCDataTransfer>&& aTransfers,
                                 const uint32_t& aAction,
                                 const OptionalShmem& aVisualDnDData,
                                 const uint32_t& aStride, const gfx::SurfaceFormat& aFormat,
                                 const LayoutDeviceIntRect& aDragRect,
                                 const nsCString& aPrincipalURISpec)
{
  mInitialDataTransferItems.Clear();
  nsIPresShell* shell = mFrameElement->OwnerDoc()->GetShell();
  if (!shell) {
    if (Manager()->IsContentParent()) {
      Unused << Manager()->AsContentParent()->SendEndDragSession(true, true,
                                                                 LayoutDeviceIntPoint(),
                                                                 0);
      // Continue sending input events with input priority when stopping the dnd
      // session.
      Manager()->AsContentParent()->SetInputPriorityEventEnabled(true);
    }
    return IPC_OK();
  }

  EventStateManager* esm = shell->GetPresContext()->EventStateManager();
  for (uint32_t i = 0; i < aTransfers.Length(); ++i) {
    mInitialDataTransferItems.AppendElement(std::move(aTransfers[i].items()));
  }
  if (Manager()->IsContentParent()) {
    nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
    if (dragService) {
      dragService->MaybeAddChildProcess(Manager()->AsContentParent());
    }
  }

  if (aVisualDnDData.type() == OptionalShmem::Tvoid_t ||
      !aVisualDnDData.get_Shmem().IsReadable() ||
      aVisualDnDData.get_Shmem().Size<char>() < aDragRect.height * aStride) {
    mDnDVisualization = nullptr;
  } else {
    mDnDVisualization =
        gfx::CreateDataSourceSurfaceFromData(gfx::IntSize(aDragRect.width, aDragRect.height),
                                             aFormat,
                                             aVisualDnDData.get_Shmem().get<uint8_t>(),
                                             aStride);
  }

  mDragValid = true;
  mDragRect = aDragRect;
  mDragPrincipalURISpec = aPrincipalURISpec;

  esm->BeginTrackingRemoteDragGesture(mFrameElement);

  if (aVisualDnDData.type() == OptionalShmem::TShmem) {
    Unused << DeallocShmem(aVisualDnDData);
  }

  return IPC_OK();
}

void
TabParent::AddInitialDnDDataTo(DataTransfer* aDataTransfer,
                               nsACString& aPrincipalURISpec)
{
  aPrincipalURISpec.Assign(mDragPrincipalURISpec);

  nsCOMPtr<nsIPrincipal> principal;
  if (!mDragPrincipalURISpec.IsEmpty()) {
    // If principal is given, try using it first.
    principal = BasePrincipal::CreateCodebasePrincipal(mDragPrincipalURISpec);
  }
  if (!principal) {
    // Fallback to system principal, to handle like the data is from browser
    // chrome or OS.
    principal = nsContentUtils::GetSystemPrincipal();
  }

  for (uint32_t i = 0; i < mInitialDataTransferItems.Length(); ++i) {
    nsTArray<IPCDataTransferItem>& itemArray = mInitialDataTransferItems[i];
    for (auto& item : itemArray) {
      RefPtr<nsVariantCC> variant = new nsVariantCC();
      // Special case kFilePromiseMime so that we get the right
      // nsIFlavorDataProvider for it.
      if (item.flavor().EqualsLiteral(kFilePromiseMime)) {
        RefPtr<nsISupports> flavorDataProvider =
          new nsContentAreaDragDropDataProvider();
        variant->SetAsISupports(flavorDataProvider);
      } else if (item.data().type() == IPCDataTransferData::TnsString) {
        variant->SetAsAString(item.data().get_nsString());
      } else if (item.data().type() == IPCDataTransferData::TIPCBlob) {
        RefPtr<BlobImpl> impl =
          IPCBlobUtils::Deserialize(item.data().get_IPCBlob());
        variant->SetAsISupports(impl);
      } else if (item.data().type() == IPCDataTransferData::TShmem) {
        if (nsContentUtils::IsFlavorImage(item.flavor())) {
          // An image! Get the imgIContainer for it and set it in the variant.
          nsCOMPtr<imgIContainer> imageContainer;
          nsresult rv =
            nsContentUtils::DataTransferItemToImage(item,
                                                    getter_AddRefs(imageContainer));
          if (NS_FAILED(rv)) {
            continue;
          }
          variant->SetAsISupports(imageContainer);
        } else {
          Shmem data = item.data().get_Shmem();
          variant->SetAsACString(nsDependentCString(data.get<char>(), data.Size<char>()));
        }

        mozilla::Unused << DeallocShmem(item.data().get_Shmem());
      }

      // We set aHidden to false, as we don't need to worry about hiding data
      // from content in the parent process where there is no content.
      // XXX: Nested Content Processes may change this
      aDataTransfer->SetDataWithPrincipalFromOtherProcess(NS_ConvertUTF8toUTF16(item.flavor()),
                                                          variant, i,
                                                          principal,
                                                          /* aHidden = */ false);
    }
  }
  mInitialDataTransferItems.Clear();
  mDragPrincipalURISpec.Truncate(0);
}

bool
TabParent::TakeDragVisualization(RefPtr<mozilla::gfx::SourceSurface>& aSurface,
                                 LayoutDeviceIntRect* aDragRect)
{
  if (!mDragValid)
    return false;

  aSurface = mDnDVisualization.forget();
  *aDragRect = mDragRect;
  mDragValid = false;
  return true;
}

bool
TabParent::AsyncPanZoomEnabled() const
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  return widget && widget->AsyncPanZoomEnabled();
}

void
TabParent::StartPersistence(uint64_t aOuterWindowID,
                            nsIWebBrowserPersistDocumentReceiver* aRecv,
                            ErrorResult& aRv)
{
  nsCOMPtr<nsIContentParent> manager = Manager();
  if (!manager->IsContentParent()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  auto* actor = new WebBrowserPersistDocumentParent();
  actor->SetOnReady(aRecv);
  bool ok = manager->AsContentParent()
    ->SendPWebBrowserPersistDocumentConstructor(actor, this, aOuterWindowID);
  if (!ok) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
  // (The actor will be destroyed on constructor failure.)
}

NS_IMETHODIMP
TabParent::StartApzAutoscroll(float aAnchorX, float aAnchorY,
                              nsViewID aScrollId, uint32_t aPresShellId,
                              bool* aOutRetval)
{
  if (!AsyncPanZoomEnabled()) {
    *aOutRetval = false;
    return NS_OK;
  }

  bool success = false;
  if (RenderFrameParent* renderFrame = GetRenderFrame()) {
    layers::LayersId layersId = renderFrame->GetLayersId();
    if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
      ScrollableLayerGuid guid{layersId, aPresShellId, aScrollId};

      // The anchor coordinates that are passed in are relative to the origin
      // of the screen, but we are sending them to APZ which only knows about
      // coordinates relative to the widget, so convert them accordingly.
      CSSPoint anchorCss{aAnchorX, aAnchorY};
      LayoutDeviceIntPoint anchor = RoundedToInt(anchorCss * widget->GetDefaultScale());
      anchor -= widget->WidgetToScreenOffset();

      success = widget->StartAsyncAutoscroll(
          ViewAs<ScreenPixel>(anchor, PixelCastJustification::LayoutDeviceIsScreenForBounds),
          guid);
    }
  }
  *aOutRetval = success;
  return NS_OK;
}

NS_IMETHODIMP
TabParent::StopApzAutoscroll(nsViewID aScrollId, uint32_t aPresShellId)
{
  if (!AsyncPanZoomEnabled()) {
    return NS_OK;
  }

  if (RenderFrameParent* renderFrame = GetRenderFrame()) {
    layers::LayersId layersId = renderFrame->GetLayersId();
    if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
      ScrollableLayerGuid guid{layersId, aPresShellId, aScrollId};
      widget->StopAsyncAutoscroll(guid);
    }
  }
  return NS_OK;
}

ShowInfo
TabParent::GetShowInfo()
{
  TryCacheDPIAndScale();
  if (mFrameElement) {
    nsAutoString name;
    mFrameElement->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
    bool allowFullscreen =
      mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::allowfullscreen) ||
      mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::mozallowfullscreen);
    bool isPrivate = mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::mozprivatebrowsing);
    bool isTransparent =
      nsContentUtils::IsChromeDoc(mFrameElement->OwnerDoc()) &&
      mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::transparent);
    return ShowInfo(name, allowFullscreen, isPrivate, false,
                    isTransparent, mDPI, mRounding, mDefaultScale.scale);
  }

  return ShowInfo(EmptyString(), false, false, false,
                  false, mDPI, mRounding, mDefaultScale.scale);
}

mozilla::ipc::IPCResult
TabParent::RecvGetTabCount(uint32_t* aValue)
{
  *aValue = 0;

  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  NS_ENSURE_TRUE(xulBrowserWindow, IPC_OK());

  uint32_t tabCount;
  nsresult rv = xulBrowserWindow->GetTabCount(&tabCount);
  NS_ENSURE_SUCCESS(rv, IPC_OK());

  *aValue = tabCount;
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvLookUpDictionary(const nsString& aText,
                                nsTArray<FontRange>&& aFontRangeArray,
                                const bool& aIsVertical,
                                const LayoutDeviceIntPoint& aPoint)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return IPC_OK();
  }

  widget->LookUpDictionary(aText, aFontRangeArray, aIsVertical,
                           aPoint - GetChildProcessOffset());
  return IPC_OK();
}

mozilla::ipc::IPCResult
TabParent::RecvShowCanvasPermissionPrompt(const nsCString& aFirstPartyURI)
{
  nsCOMPtr<nsIBrowser> browser = do_QueryInterface(mFrameElement);
  if (!browser) {
    // If the tab is being closed, the browser may not be available.
    // In this case we can ignore the request.
    return IPC_OK();
  }
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (!os) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsresult rv = os->NotifyObservers(browser, "canvas-permissions-prompt",
                                    NS_ConvertUTF8toUTF16(aFirstPartyURI).get());
  if (NS_FAILED(rv)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

void
TabParent::LiveResizeStarted()
{
  SuppressDisplayport(true);
}

void
TabParent::LiveResizeStopped()
{
  SuppressDisplayport(false);
}

NS_IMETHODIMP
FakeChannel::OnAuthAvailable(nsISupports *aContext, nsIAuthInformation *aAuthInfo)
{
  nsAuthInformationHolder* holder =
    static_cast<nsAuthInformationHolder*>(aAuthInfo);

  if (!net::gNeckoChild->SendOnAuthAvailable(mCallbackId,
                                             holder->User(),
                                             holder->Password(),
                                             holder->Domain())) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
FakeChannel::OnAuthCancelled(nsISupports *aContext, bool userCancel)
{
  if (!net::gNeckoChild->SendOnAuthCancelled(mCallbackId, userCancel)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}


} // namespace dom
} // namespace mozilla
