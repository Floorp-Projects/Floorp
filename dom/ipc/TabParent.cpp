/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "TabParent.h"

#include "AudioChannelService.h"
#include "AppProcessChecker.h"
#include "mozIApplication.h"
#ifdef ACCESSIBILITY
#include "mozilla/a11y/DocAccessibleParent.h"
#include "nsAccessibilityService.h"
#endif
#include "mozilla/BrowserElementParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/plugins/PluginWidgetParent.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Hal.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/ipc/DocumentRendererParent.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/unused.h"
#include "BlobParent.h"
#include "nsCOMPtr.h"
#include "nsContentAreaDragDrop.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsIBaseWindow.h"
#include "nsIContent.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadInfo.h"
#include "nsPrincipal.h"
#include "nsIPromptFactory.h"
#include "nsIURI.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowCreator2.h"
#include "nsIXULBrowserWindow.h"
#include "nsIXULWindow.h"
#include "nsIRemoteBrowser.h"
#include "nsViewManager.h"
#include "nsVariant.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"
#ifndef XP_WIN
#include "nsJARProtocolHandler.h"
#endif
#include "nsOpenURIInFrameParams.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowWatcher.h"
#include "nsPresShell.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsWindowWatcher.h"
#include "private/pprio.h"
#include "PermissionMessageUtils.h"
#include "StructuredCloneData.h"
#include "ColorPickerParent.h"
#include "FilePickerParent.h"
#include "TabChild.h"
#include "LoadContext.h"
#include "nsNetCID.h"
#include "nsIAuthInformation.h"
#include "nsIAuthPromptCallback.h"
#include "SourceSurfaceRawData.h"
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

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::services;
using namespace mozilla::widget;
using namespace mozilla::jsipc;

#if DEUBG
  #define LOG(args...) printf_stderr(args)
#else
  #define LOG(...)
#endif

// The flags passed by the webProgress notifications are 16 bits shifted
// from the ones registered by webProgressListeners.
#define NOTIFY_FLAG_SHIFT 16

class OpenFileAndSendFDRunnable : public nsRunnable
{
    const nsString mPath;
    nsRefPtr<TabParent> mTabParent;
    nsCOMPtr<nsIEventTarget> mEventTarget;
    PRFileDesc* mFD;

public:
    OpenFileAndSendFDRunnable(const nsAString& aPath, TabParent* aTabParent)
      : mPath(aPath), mTabParent(aTabParent), mFD(nullptr)
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(!aPath.IsEmpty());
        MOZ_ASSERT(aTabParent);
    }

    void Dispatch()
    {
        MOZ_ASSERT(NS_IsMainThread());

        mEventTarget = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
        NS_ENSURE_TRUE_VOID(mEventTarget);

        nsresult rv = mEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
        NS_ENSURE_SUCCESS_VOID(rv);
    }

private:
    ~OpenFileAndSendFDRunnable()
    {
        MOZ_ASSERT(!mFD);
    }

    // This shouldn't be called directly except by the event loop. Use Dispatch
    // to start the sequence.
    NS_IMETHOD Run()
    {
        if (NS_IsMainThread()) {
            SendResponse();
        } else if (mFD) {
            CloseFile();
        } else {
            OpenFile();
        }

        return NS_OK;
    }

    void SendResponse()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mTabParent);
        MOZ_ASSERT(mEventTarget);

        nsRefPtr<TabParent> tabParent;
        mTabParent.swap(tabParent);

        using mozilla::ipc::FileDescriptor;

        FileDescriptor fd;
        if (mFD) {
            FileDescriptor::PlatformHandleType handle =
                FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(mFD));
            fd = FileDescriptor(handle);
        }

        // Our TabParent may have been destroyed already.  If so, don't send any
        // fds over, just go back to the IO thread and close them.
        if (!tabParent->IsDestroyed()) {
            mozilla::unused << tabParent->SendCacheFileDescriptor(mPath, fd);
        }

        if (!mFD) {
            return;
        }

        nsCOMPtr<nsIEventTarget> eventTarget;
        mEventTarget.swap(eventTarget);

        if (NS_FAILED(eventTarget->Dispatch(this, NS_DISPATCH_NORMAL))) {
            NS_WARNING("Failed to dispatch to stream transport service!");

            // It's probably safer to take the main thread IO hit here rather
            // than leak a file descriptor.
            CloseFile();
        }
    }

    // Helper method to avoid gnarly control flow for failures.
    void OpenBlobImpl()
    {
        MOZ_ASSERT(!NS_IsMainThread());
        MOZ_ASSERT(!mFD);

        nsCOMPtr<nsIFile> file;
        nsresult rv = NS_NewLocalFile(mPath, false, getter_AddRefs(file));
        NS_ENSURE_SUCCESS_VOID(rv);

        PRFileDesc* fd;
        rv = file->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);
        NS_ENSURE_SUCCESS_VOID(rv);

        mFD = fd;
    }

    void OpenFile()
    {
        MOZ_ASSERT(!NS_IsMainThread());

        OpenBlobImpl();

        if (NS_FAILED(NS_DispatchToMainThread(this))) {
            NS_WARNING("Failed to dispatch to main thread!");

            // Intentionally leak the runnable (but not the fd) rather
            // than crash when trying to release a main thread object
            // off the main thread.
            mozilla::unused << mTabParent.forget();
            CloseFile();
        }
    }

    void CloseFile()
    {
        // It's possible for this to happen on the main thread if the dispatch
        // to the stream service fails after we've already opened the file so
        // we can't assert the thread we're running on.

        MOZ_ASSERT(mFD);

        PRStatus prrc;
        prrc = PR_Close(mFD);
        if (prrc != PR_SUCCESS) {
          NS_ERROR("PR_Close() failed.");
        }
        mFD = nullptr;
    }
};

namespace mozilla {
namespace dom {

TabParent::LayerToTabParentTable* TabParent::sLayerToTabParentTable = nullptr;

NS_IMPL_ISUPPORTS(TabParent,
                  nsITabParent,
                  nsIAuthPromptProvider,
                  nsISecureBrowserUI,
                  nsISupportsWeakReference,
                  nsIWebBrowserPersistable)

TabParent::TabParent(nsIContentParent* aManager,
                     const TabId& aTabId,
                     const TabContext& aContext,
                     uint32_t aChromeFlags)
  : TabContext(aContext)
  , mFrameElement(nullptr)
  , mRect(0, 0, 0, 0)
  , mDimensions(0, 0)
  , mOrientation(0)
  , mDPI(0)
  , mDefaultScale(0)
  , mUpdatedDimensions(false)
  , mManager(aManager)
  , mDocShellIsActive(false)
  , mMarkedDestroying(false)
  , mIsDestroyed(false)
  , mIsDetached(true)
  , mAppPackageFileDescriptorSent(false)
  , mSendOfflineStatus(true)
  , mChromeFlags(aChromeFlags)
  , mDragAreaX(0)
  , mDragAreaY(0)
  , mInitedByParent(false)
  , mTabId(aTabId)
  , mCreatingWindow(false)
  , mNeedLayerTreeReadyNotification(false)
  , mCursor(nsCursor(-1))
  , mTabSetsCursor(false)
  , mHasContentOpener(false)
  , mActiveSupressDisplayportCount(0)
{
  MOZ_ASSERT(aManager);
}

TabParent::~TabParent()
{
}

TabParent*
TabParent::GetTabParentFromLayersId(uint64_t aLayersId)
{
  if (!sLayerToTabParentTable) {
    return nullptr;
  }
  return sLayerToTabParentTable->Get(aLayersId);
}

void
TabParent::AddTabParentToTable(uint64_t aLayersId, TabParent* aTabParent)
{
  if (!sLayerToTabParentTable) {
    sLayerToTabParentTable = new LayerToTabParentTable();
  }
  sLayerToTabParentTable->Put(aLayersId, aTabParent);
}

void
TabParent::RemoveTabParentFromTable(uint64_t aLayersId)
{
  if (!sLayerToTabParentTable) {
    return;
  }
  sLayerToTabParentTable->Remove(aLayersId);
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

void
TabParent::SetOwnerElement(Element* aElement)
{
  // If we held previous content then unregister for its events.
  RemoveWindowListeners();

  // If we change top-level documents then we need to change our
  // registration with them.
  nsRefPtr<nsPIWindowRoot> curTopLevelWin, newTopLevelWin;
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

  AddWindowListeners();
  TryCacheDPIAndScale();
}

void
TabParent::AddWindowListeners()
{
  if (mFrameElement && mFrameElement->OwnerDoc()) {
    if (nsCOMPtr<nsPIDOMWindow> window = mFrameElement->OwnerDoc()->GetWindow()) {
      nsCOMPtr<EventTarget> eventTarget = window->GetTopWindowRoot();
      if (eventTarget) {
        eventTarget->AddEventListener(NS_LITERAL_STRING("MozUpdateWindowPos"),
                                      this, false, false);
      }
    }
    if (nsIPresShell* shell = mFrameElement->OwnerDoc()->GetShell()) {
      mPresShellWithRefreshListener = shell;
      shell->AddPostRefreshObserver(this);
    }
  }
}

void
TabParent::RemoveWindowListeners()
{
  if (mFrameElement && mFrameElement->OwnerDoc()->GetWindow()) {
    nsCOMPtr<nsPIDOMWindow> window = mFrameElement->OwnerDoc()->GetWindow();
    nsCOMPtr<EventTarget> eventTarget = window->GetTopWindowRoot();
    if (eventTarget) {
      eventTarget->RemoveEventListener(NS_LITERAL_STRING("MozUpdateWindowPos"),
                                       this, false);
    }
  }
  if (mPresShellWithRefreshListener) {
    mPresShellWithRefreshListener->RemovePostRefreshObserver(this);
    mPresShellWithRefreshListener = nullptr;
  }
}

void
TabParent::DidRefresh()
{
  if (mChromeOffset != -GetChildProcessOffset()) {
    UpdatePosition();
  }
}

void
TabParent::GetAppType(nsAString& aOut)
{
  aOut.Truncate();
  nsCOMPtr<Element> elem = do_QueryInterface(mFrameElement);
  if (!elem) {
    return;
  }

  elem->GetAttr(kNameSpaceID_None, nsGkAtoms::mozapptype, aOut);
}

bool
TabParent::IsVisible()
{
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    return false;
  }

  bool visible = false;
  frameLoader->GetVisible(&visible);
  return visible;
}

static void LogChannelRelevantInfo(nsIURI* aURI,
                                   nsIPrincipal* aLoadingPrincipal,
                                   nsIPrincipal* aChannelResultPrincipal,
                                   nsContentPolicyType aContentPolicyType) {
  nsCString loadingOrigin;
  aLoadingPrincipal->GetOrigin(loadingOrigin);

  nsCString uriString;
  aURI->GetAsciiSpec(uriString);
  LOG("Loading %s from origin %s (type: %d)\n", uriString.get(),
                                                loadingOrigin.get(),
                                                aContentPolicyType);

  nsCString resultPrincipalOrigin;
  aChannelResultPrincipal->GetOrigin(resultPrincipalOrigin);
  LOG("Result principal origin: %s\n", resultPrincipalOrigin.get());
}

bool
TabParent::ShouldSwitchProcess(nsIChannel* aChannel)
{
  // If we lack of any information which is required to decide the need of
  // process switch, consider that we should switch process.

  // Prepare the channel loading principal.
  nsCOMPtr<nsILoadInfo> loadInfo;
  aChannel->GetLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, true);
  nsCOMPtr<nsIPrincipal> loadingPrincipal;
  loadInfo->GetLoadingPrincipal(getter_AddRefs(loadingPrincipal));
  NS_ENSURE_TRUE(loadingPrincipal, true);

  // Prepare the channel result principal.
  nsCOMPtr<nsIPrincipal> resultPrincipal;
  nsContentUtils::GetSecurityManager()->
    GetChannelResultPrincipal(aChannel, getter_AddRefs(resultPrincipal));

  // Log the debug info which is used to decide the need of proces switch.
  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  LogChannelRelevantInfo(uri, loadingPrincipal, resultPrincipal,
                         loadInfo->GetContentPolicyType());

  // Check if the signed package is loaded from the same origin.
  bool sameOrigin = false;
  loadingPrincipal->Equals(resultPrincipal, &sameOrigin);
  if (sameOrigin) {
    LOG("Loading singed package from the same origin. Don't switch process.\n");
    return false;
  }

  // If this is not a top level document, there's no need to switch process.
  if (nsIContentPolicy::TYPE_DOCUMENT != loadInfo->GetContentPolicyType()) {
    LOG("Subresource of a document. No need to switch process.\n");
    return false;
  }

  // If this is a brand new process created to load the signed package
  // (triggered by previous OnStartSignedPackageRequest), the loading origin
  // will be "moz-safe-about:blank". In that case, we don't need to switch process
  // again. We compare with "moz-safe-about:blank" without appId/isBrowserElement/etc
  // taken into account. That's why we use originNoSuffix.
  nsCString loadingOriginNoSuffix;
  loadingPrincipal->GetOriginNoSuffix(loadingOriginNoSuffix);
  if (loadingOriginNoSuffix.EqualsLiteral("moz-safe-about:blank")) {
    LOG("The content is already loaded by a brand new process.\n");
    return false;
  }

  return true;
}

void
TabParent::OnStartSignedPackageRequest(nsIChannel* aChannel)
{
  if (!ShouldSwitchProcess(aChannel)) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));

  aChannel->Cancel(NS_BINDING_FAILED);

  nsCString uriString;
  uri->GetAsciiSpec(uriString);
  LOG("We decide to switch process. Call nsFrameLoader::SwitchProcessAndLoadURIs: %s\n",
       uriString.get());

  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  NS_ENSURE_TRUE_VOID(frameLoader);

  nsresult rv = frameLoader->SwitchProcessAndLoadURI(uri);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to switch process.");
  }
}

void
TabParent::DestroyInternal()
{
  IMEStateManager::OnTabParentDestroying(this);

  RemoveWindowListeners();

  // If this fails, it's most likely due to a content-process crash,
  // and auto-cleanup will kick in.  Otherwise, the child side will
  // destroy itself and send back __delete__().
  unused << SendDestroy();

  if (RenderFrameParent* frame = GetRenderFrame()) {
    RemoveTabParentFromTable(frame->GetLayersId());
    frame->Destroy();
  }

  // Let all PluginWidgets know we are tearing down. Prevents
  // these objects from sending async events after the child side
  // is shut down.
  const nsTArray<PPluginWidgetParent*>& kids = ManagedPPluginWidgetParent();
  for (uint32_t idx = 0; idx < kids.Length(); ++idx) {
      static_cast<mozilla::plugins::PluginWidgetParent*>(kids[idx])->ParentDestroy();
  }
}

void
TabParent::Destroy()
{
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

void
TabParent::Detach()
{
  if (mIsDetached) {
    return;
  }
  RemoveWindowListeners();
  if (RenderFrameParent* frame = GetRenderFrame()) {
    RemoveTabParentFromTable(frame->GetLayersId());
  }
  mIsDetached = true;
}

void
TabParent::Attach(nsFrameLoader* aFrameLoader)
{
  MOZ_ASSERT(mIsDetached);
  if (!mIsDetached) {
    return;
  }
  Element* ownerElement = aFrameLoader->GetOwnerContent();
  SetOwnerElement(ownerElement);
  if (RenderFrameParent* frame = GetRenderFrame()) {
    AddTabParentToTable(frame->GetLayersId(), this);
    frame->OwnerContentChanged(ownerElement);
  }
  mIsDetached = false;
}

bool
TabParent::Recv__delete__()
{
  if (XRE_IsParentProcess()) {
    ContentParent::DeallocateTabId(mTabId,
                                   Manager()->AsContentParent()->ChildID(),
                                   mMarkedDestroying);
  }
  else {
    Manager()->AsContentBridgeParent()->NotifyTabDestroyed();
    ContentParent::DeallocateTabId(mTabId,
                                   Manager()->ChildID(),
                                   mMarkedDestroying);
  }

  return true;
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

  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader(true);
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (frameLoader) {
    nsCOMPtr<Element> frameElement(mFrameElement);
    ReceiveMessage(CHILD_PROCESS_SHUTDOWN_MESSAGE, false, nullptr, nullptr,
                   nullptr);
    frameLoader->DestroyComplete();

    if (why == AbnormalShutdown && os) {
      os->NotifyObservers(NS_ISUPPORTS_CAST(nsIFrameLoader*, frameLoader),
                          "oop-frameloader-crashed", nullptr);
      nsContentUtils::DispatchTrustedEvent(frameElement->OwnerDoc(), frameElement,
                                           NS_LITERAL_STRING("oop-browser-crashed"),
                                           true, true);
    }

    mFrameLoader = nullptr;
  }

  if (os) {
    os->NotifyObservers(NS_ISUPPORTS_CAST(nsITabParent*, this), "ipc:browser-destroyed", nullptr);
  }
}

bool
TabParent::RecvMoveFocus(const bool& aForward, const bool& aForDocumentNavigation)
{
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm) {
    nsCOMPtr<nsIDOMElement> dummy;

    uint32_t type = aForward ?
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARDDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARD)) :
      (aForDocumentNavigation ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARDDOC) :
                                static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARD));
    nsCOMPtr<nsIDOMElement> frame = do_QueryInterface(mFrameElement);
    fm->MoveFocus(nullptr, frame, type, nsIFocusManager::FLAG_BYKEY,
                  getter_AddRefs(dummy));
  }
  return true;
}

bool
TabParent::RecvEvent(const RemoteDOMEvent& aEvent)
{
  nsCOMPtr<nsIDOMEvent> event = do_QueryInterface(aEvent.mEvent);
  NS_ENSURE_TRUE(event, true);

  nsCOMPtr<mozilla::dom::EventTarget> target = do_QueryInterface(mFrameElement);
  NS_ENSURE_TRUE(target, true);

  event->SetOwner(target);

  bool dummy;
  target->DispatchEvent(event, &dummy);
  return true;
}

struct MOZ_STACK_CLASS TabParent::AutoUseNewTab final
{
public:
  AutoUseNewTab(TabParent* aNewTab, bool* aWindowIsNew, nsCString* aURLToLoad)
   : mNewTab(aNewTab), mWindowIsNew(aWindowIsNew), mURLToLoad(aURLToLoad)
  {
    MOZ_ASSERT(!TabParent::sNextTabParent);
    MOZ_ASSERT(!aNewTab->mCreatingWindow);

    TabParent::sNextTabParent = aNewTab;
    aNewTab->mCreatingWindow = true;
    aNewTab->mDelayedURL.Truncate();
  }

  ~AutoUseNewTab()
  {
    mNewTab->mCreatingWindow = false;
    *mURLToLoad = mNewTab->mDelayedURL;

    if (TabParent::sNextTabParent) {
      MOZ_ASSERT(TabParent::sNextTabParent == mNewTab);
      TabParent::sNextTabParent = nullptr;
      *mWindowIsNew = false;
    }
  }

private:
  TabParent* mNewTab;
  bool* mWindowIsNew;
  nsCString* mURLToLoad;
};

static already_AddRefed<nsIDOMWindow>
FindMostRecentOpenWindow()
{
  nsCOMPtr<nsIWindowMediator> windowMediator =
    do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  windowMediator->GetEnumerator(MOZ_UTF16("navigator:browser"),
                                getter_AddRefs(windowEnumerator));

  nsCOMPtr<nsIDOMWindow> latest;

  bool hasMore = false;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(windowEnumerator->HasMoreElements(&hasMore)));
  while (hasMore) {
    nsCOMPtr<nsISupports> item;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(windowEnumerator->GetNext(getter_AddRefs(item))));
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(item);

    bool isClosed;
    if (window && NS_SUCCEEDED(window->GetClosed(&isClosed)) && !isClosed) {
      latest = window;
    }

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(windowEnumerator->HasMoreElements(&hasMore)));
  }

  return latest.forget();
}

bool
TabParent::RecvCreateWindow(PBrowserParent* aNewTab,
                            const uint32_t& aChromeFlags,
                            const bool& aCalledFromJS,
                            const bool& aPositionSpecified,
                            const bool& aSizeSpecified,
                            const nsString& aURI,
                            const nsString& aName,
                            const nsCString& aFeatures,
                            const nsString& aBaseURI,
                            nsresult* aResult,
                            bool* aWindowIsNew,
                            InfallibleTArray<FrameScriptInfo>* aFrameScripts,
                            nsCString* aURLToLoad)
{
  // We always expect to open a new window here. If we don't, it's an error.
  *aWindowIsNew = true;

  // The content process should never be in charge of computing whether or
  // not a window should be private or remote - the parent will do that.
  MOZ_ASSERT(!(aChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW));
  MOZ_ASSERT(!(aChromeFlags & nsIWebBrowserChrome::CHROME_NON_PRIVATE_WINDOW));
  MOZ_ASSERT(!(aChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME));
  MOZ_ASSERT(!(aChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW));

  if (NS_WARN_IF(IsBrowserOrApp()))
    return false;

  nsCOMPtr<nsPIWindowWatcher> pwwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, aResult);

  if (NS_WARN_IF(NS_FAILED(*aResult)))
    return true;

  TabParent* newTab = TabParent::GetFrom(aNewTab);
  MOZ_ASSERT(newTab);

  // Content has requested that we open this new content window, so
  // we must have an opener.
  newTab->SetHasContentOpener(true);

  nsCOMPtr<nsIContent> frame(do_QueryInterface(mFrameElement));

  nsCOMPtr<nsIDOMWindow> parent;
  if (frame) {
    parent = do_QueryInterface(frame->OwnerDoc()->GetWindow());

    // If our chrome window is in the process of closing, don't try to open a
    // new tab in it.
    if (parent) {
      bool isClosed;
      if (NS_SUCCEEDED(parent->GetClosed(&isClosed)) && isClosed) {
        parent = nullptr;
      }
    }
  }

  nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin = mBrowserDOMWindow;

  // If we haven't found a chrome window to open in, just use the most recently
  // opened one.
  if (!parent) {
    parent = FindMostRecentOpenWindow();
    if (NS_WARN_IF(!parent)) {
      *aResult = NS_ERROR_FAILURE;
      return true;
    }

    nsCOMPtr<nsIDOMChromeWindow> rootChromeWin = do_QueryInterface(parent);
    if (rootChromeWin) {
      rootChromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
    }
  }

  int32_t openLocation =
    nsWindowWatcher::GetWindowOpenLocation(parent, aChromeFlags, aCalledFromJS,
                                           aPositionSpecified, aSizeSpecified);

  MOZ_ASSERT(openLocation == nsIBrowserDOMWindow::OPEN_NEWTAB ||
             openLocation == nsIBrowserDOMWindow::OPEN_NEWWINDOW);

  // Opening new tabs is the easy case...
  if (openLocation == nsIBrowserDOMWindow::OPEN_NEWTAB) {
    if (NS_WARN_IF(!browserDOMWin)) {
      *aResult = NS_ERROR_FAILURE;
      return true;
    }

    bool isPrivate;
    nsCOMPtr<nsILoadContext> loadContext = GetLoadContext();
    loadContext->GetUsePrivateBrowsing(&isPrivate);

    nsCOMPtr<nsIOpenURIInFrameParams> params = new nsOpenURIInFrameParams();
    params->SetReferrer(aBaseURI);
    params->SetIsPrivate(isPrivate);

    AutoUseNewTab aunt(newTab, aWindowIsNew, aURLToLoad);

    nsCOMPtr<nsIFrameLoaderOwner> frameLoaderOwner;
    browserDOMWin->OpenURIInFrame(nullptr, params,
                                  openLocation,
                                  nsIBrowserDOMWindow::OPEN_NEW,
                                  getter_AddRefs(frameLoaderOwner));
    if (!frameLoaderOwner) {
      *aWindowIsNew = false;
    }

    aFrameScripts->SwapElements(newTab->mDelayedFrameScripts);
    return true;
  }

  // WindowWatcher is going to expect a valid URI to open a window
  // to. If it can't find one, it's going to attempt to figure one
  // out on its own, which is problematic because it can't access
  // the document for the remote browser we're opening. Luckily,
  // TabChild has sent us a baseURI with which we can ensure that
  // the URI we pass to WindowWatcher is valid.
  nsCOMPtr<nsIURI> baseURI;
  *aResult = NS_NewURI(getter_AddRefs(baseURI), aBaseURI);

  if (NS_WARN_IF(NS_FAILED(*aResult)))
    return true;

  nsAutoCString finalURIString;
  if (!aURI.IsEmpty()) {
    nsCOMPtr<nsIURI> finalURI;
    *aResult = NS_NewURI(getter_AddRefs(finalURI), NS_ConvertUTF16toUTF8(aURI).get(), baseURI);

    if (NS_WARN_IF(NS_FAILED(*aResult)))
      return true;

    finalURI->GetSpec(finalURIString);
  }

  nsCOMPtr<nsIDOMWindow> window;

  AutoUseNewTab aunt(newTab, aWindowIsNew, aURLToLoad);

  const char* features = aFeatures.Length() ? aFeatures.get() : nullptr;

  *aResult = pwwatch->OpenWindow2(parent, finalURIString.get(),
                                  NS_ConvertUTF16toUTF8(aName).get(),
                                  features, aCalledFromJS,
                                  false, false, this, nullptr, getter_AddRefs(window));

  if (NS_WARN_IF(NS_FAILED(*aResult)))
    return true;

  *aResult = NS_ERROR_FAILURE;

  nsCOMPtr<nsPIDOMWindow> pwindow = do_QueryInterface(window);
  if (NS_WARN_IF(!pwindow)) {
    return true;
  }

  nsCOMPtr<nsIDocShell> windowDocShell = pwindow->GetDocShell();
  if (NS_WARN_IF(!windowDocShell)) {
    return true;
  }

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  windowDocShell->GetTreeOwner(getter_AddRefs(treeOwner));

  nsCOMPtr<nsIXULWindow> xulWin = do_GetInterface(treeOwner);
  if (NS_WARN_IF(!xulWin)) {
    return true;
  }

  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWin;
  xulWin->GetXULBrowserWindow(getter_AddRefs(xulBrowserWin));
  if (NS_WARN_IF(!xulBrowserWin)) {
    return true;
  }

  nsCOMPtr<nsITabParent> newRemoteTab;
  *aResult = xulBrowserWin->ForceInitialBrowserRemote(getter_AddRefs(newRemoteTab));

  if (NS_WARN_IF(NS_FAILED(*aResult)))
    return true;

  MOZ_ASSERT(TabParent::GetFrom(newRemoteTab) == newTab);

  aFrameScripts->SwapElements(newTab->mDelayedFrameScripts);
  return true;
}

TabParent* TabParent::sNextTabParent;

/* static */ TabParent*
TabParent::GetNextTabParent()
{
  TabParent* result = sNextTabParent;
  sNextTabParent = nullptr;
  return result;
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

bool
TabParent::InitBrowserConfiguration(const nsCString& aURI,
                                    BrowserConfiguration& aConfiguration)
{
  return ContentParent::GetBrowserConfiguration(aURI, aConfiguration);
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

    uint32_t appId = OwnOrContainingAppId();
    if (mSendOfflineStatus && NS_IsAppOffline(appId)) {
      // If the app is offline in the parent process
      // pass that state to the child process as well
      unused << SendAppOfflineStatus(appId, true);
    }
    mSendOfflineStatus = false;

    // This object contains the configuration for this new app.
    BrowserConfiguration configuration;
    if (NS_WARN_IF(!InitBrowserConfiguration(spec, configuration))) {
      return;
    }

    unused << SendLoadURL(spec, configuration);

    // If this app is a packaged app then we can speed startup by sending over
    // the file descriptor for the "application.zip" file that it will
    // invariably request. Only do this once.
    if (!mAppPackageFileDescriptorSent) {
        mAppPackageFileDescriptorSent = true;

        nsCOMPtr<mozIApplication> app = GetOwnOrContainingApp();
        if (app) {
            nsString manifestURL;
            nsresult rv = app->GetManifestURL(manifestURL);
            NS_ENSURE_SUCCESS_VOID(rv);

            if (StringBeginsWith(manifestURL, NS_LITERAL_STRING("app:"))) {
                nsString basePath;
                rv = app->GetBasePath(basePath);
                NS_ENSURE_SUCCESS_VOID(rv);

                nsString appId;
                rv = app->GetId(appId);
                NS_ENSURE_SUCCESS_VOID(rv);

                nsCOMPtr<nsIFile> packageFile;
                rv = NS_NewLocalFile(basePath, false,
                                     getter_AddRefs(packageFile));
                NS_ENSURE_SUCCESS_VOID(rv);

                rv = packageFile->Append(appId);
                NS_ENSURE_SUCCESS_VOID(rv);

                rv = packageFile->Append(NS_LITERAL_STRING("application.zip"));
                NS_ENSURE_SUCCESS_VOID(rv);

                nsString path;
                rv = packageFile->GetPath(path);
                NS_ENSURE_SUCCESS_VOID(rv);

#ifndef XP_WIN
                PRFileDesc* cachedFd = nullptr;
                gJarHandler->JarCache()->GetFd(packageFile, &cachedFd);

                if (cachedFd) {
                    FileDescriptor::PlatformHandleType handle =
                        FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(cachedFd));
                    unused << SendCacheFileDescriptor(path, FileDescriptor(handle));
                } else
#endif
                {
                    nsRefPtr<OpenFileAndSendFDRunnable> openFileRunnable =
                        new OpenFileAndSendFDRunnable(path, this);
                    openFileRunnable->Dispatch();
                }
            }
        }
    }
}

void
TabParent::Show(const ScreenIntSize& size, bool aParentIsActive)
{
    mDimensions = size;
    if (mIsDestroyed) {
        return;
    }

    TextureFactoryIdentifier textureFactoryIdentifier;
    uint64_t layersId = 0;
    bool success = false;
    RenderFrameParent* renderFrame = nullptr;
    // If TabParent is initialized by parent side then the RenderFrame must also
    // be created here. If TabParent is initialized by child side,
    // child side will create RenderFrame.
    MOZ_ASSERT(!GetRenderFrame());
    if (IsInitedByParent()) {
        nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
        if (frameLoader) {
          renderFrame =
              new RenderFrameParent(frameLoader,
                                    &textureFactoryIdentifier,
                                    &layersId,
                                    &success);
          MOZ_ASSERT(success);
          AddTabParentToTable(layersId, this);
          unused << SendPRenderFrameConstructor(renderFrame);
        }
    }

    TryCacheDPIAndScale();
    ShowInfo info(EmptyString(), false, false, mDPI, mDefaultScale.scale);

    if (mFrameElement) {
      nsAutoString name;
      mFrameElement->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
      bool allowFullscreen =
        mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::allowfullscreen) ||
        mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::mozallowfullscreen);
      bool isPrivate = mFrameElement->HasAttr(kNameSpaceID_None, nsGkAtoms::mozprivatebrowsing);
      info = ShowInfo(name, allowFullscreen, isPrivate, mDPI, mDefaultScale.scale);
    }

    unused << SendShow(size, info, textureFactoryIdentifier,
                       layersId, renderFrame, aParentIsActive);
}

bool
TabParent::RecvSetDimensions(const uint32_t& aFlags,
                             const int32_t& aX, const int32_t& aY,
                             const int32_t& aCx, const int32_t& aCy)
{
  MOZ_ASSERT(!(aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER),
             "We should never see DIM_FLAGS_SIZE_INNER here!");

  nsCOMPtr<nsIWidget> widget = GetWidget();
  NS_ENSURE_TRUE(mFrameElement, true);
  nsCOMPtr<nsIDocShell> docShell = mFrameElement->OwnerDoc()->GetDocShell();
  NS_ENSURE_TRUE(docShell, true);
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShell->GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = do_QueryInterface(treeOwner);
  NS_ENSURE_TRUE(treeOwnerAsWin, true);

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION &&
      aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER) {
    treeOwnerAsWin->SetPositionAndSize(aX, aY, aCx, aCy, true);
    return true;
  }

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION) {
    treeOwnerAsWin->SetPosition(aX, aY);
    return true;
  }

  if (aFlags & nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_OUTER) {
    treeOwnerAsWin->SetSize(aCx, aCy, true);
    return true;
  }

  MOZ_ASSERT(false, "Unknown flags!");
  return false;
}

nsresult
TabParent::UpdatePosition()
{
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
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
  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  ScreenOrientationInternal orientation = config.orientation();
  LayoutDeviceIntPoint chromeOffset = -GetChildProcessOffset();

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    NS_WARNING("No widget found in TabParent::UpdateDimensions");
    return;
  }
  nsIntRect contentRect = rect;
  contentRect.x += widget->GetClientOffset().x;
  contentRect.y += widget->GetClientOffset().y;

  if (!mUpdatedDimensions || mOrientation != orientation ||
      mDimensions != size || !mRect.IsEqualEdges(contentRect) ||
      chromeOffset != mChromeOffset) {

    mUpdatedDimensions = true;
    mRect = contentRect;
    mDimensions = size;
    mOrientation = orientation;
    mChromeOffset = chromeOffset;

    CSSToLayoutDeviceScale widgetScale = widget->GetDefaultScale();

    LayoutDeviceIntRect devicePixelRect =
      ViewAs<LayoutDevicePixel>(mRect,
                                PixelCastJustification::LayoutDeviceIsScreenForTabDims);
    LayoutDeviceIntSize devicePixelSize =
      ViewAs<LayoutDevicePixel>(mDimensions.ToUnknownSize(),
                                PixelCastJustification::LayoutDeviceIsScreenForTabDims);

    CSSRect unscaledRect = devicePixelRect / widgetScale;
    CSSSize unscaledSize = devicePixelSize / widgetScale;
    unused << SendUpdateDimensions(unscaledRect, unscaledSize,
                                   widget->SizeMode(),
                                   orientation, chromeOffset);
  }
}

void
TabParent::UpdateFrame(const FrameMetrics& aFrameMetrics)
{
  if (!mIsDestroyed) {
    unused << SendUpdateFrame(aFrameMetrics);
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
    unused << SendUIResolutionChanged(mDPI, mDPI < 0 ? -1.0 : mDefaultScale.scale);
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
    unused << SendThemeChanged(LookAndFeel::GetIntCache());
  }
}

void
TabParent::HandleAccessKey(nsTArray<uint32_t>& aCharCodes,
                           const bool& aIsTrusted,
                           const int32_t& aModifierMask)
{
  if (!mIsDestroyed) {
    unused << SendHandleAccessKey(aCharCodes, aIsTrusted, aModifierMask);
  }
}

void
TabParent::RequestFlingSnap(const FrameMetrics::ViewID& aScrollId,
                            const mozilla::CSSPoint& aDestination)
{
  if (!mIsDestroyed) {
    unused << SendRequestFlingSnap(aScrollId, aDestination);
  }
}

void
TabParent::AcknowledgeScrollUpdate(const ViewID& aScrollId, const uint32_t& aScrollGeneration)
{
  if (!mIsDestroyed) {
    unused << SendAcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
  }
}

void TabParent::HandleDoubleTap(const CSSPoint& aPoint,
                                Modifiers aModifiers,
                                const ScrollableLayerGuid &aGuid)
{
  if (!mIsDestroyed) {
    unused << SendHandleDoubleTap(aPoint, aModifiers, aGuid);
  }
}

void TabParent::HandleSingleTap(const CSSPoint& aPoint,
                                Modifiers aModifiers,
                                const ScrollableLayerGuid &aGuid)
{
  if (!mIsDestroyed) {
    unused << SendHandleSingleTap(aPoint, aModifiers, aGuid);
  }
}

void TabParent::HandleLongTap(const CSSPoint& aPoint,
                              Modifiers aModifiers,
                              const ScrollableLayerGuid &aGuid,
                              uint64_t aInputBlockId)
{
  if (!mIsDestroyed) {
    unused << SendHandleLongTap(aPoint, aModifiers, aGuid, aInputBlockId);
  }
}

void TabParent::NotifyAPZStateChange(ViewID aViewId,
                                     APZStateChange aChange,
                                     int aArg)
{
  if (!mIsDestroyed) {
    unused << SendNotifyAPZStateChange(aViewId, aChange, aArg);
  }
}

void
TabParent::NotifyMouseScrollTestEvent(const ViewID& aScrollId, const nsString& aEvent)
{
  if (!mIsDestroyed) {
    unused << SendMouseScrollTestEvent(aScrollId, aEvent);
  }
}

void
TabParent::NotifyFlushComplete()
{
  if (!mIsDestroyed) {
    unused << SendNotifyFlushComplete();
  }
}

void
TabParent::Activate()
{
  if (!mIsDestroyed) {
    unused << SendActivate();
  }
}

void
TabParent::Deactivate()
{
  if (!mIsDestroyed) {
    unused << SendDeactivate();
  }
}

NS_IMETHODIMP
TabParent::Init(nsIDOMWindow *window)
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
                                     const uint64_t&)
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

bool
TabParent::RecvPDocAccessibleConstructor(PDocAccessibleParent* aDoc,
                                         PDocAccessibleParent* aParentDoc,
                                         const uint64_t& aParentID)
{
#ifdef ACCESSIBILITY
  auto doc = static_cast<a11y::DocAccessibleParent*>(aDoc);
  if (aParentDoc) {
    // A document should never directly be the parent of another document.
    // There should always be an outer doc accessible child of the outer
    // document containing the child.
    MOZ_ASSERT(aParentID);
    if (!aParentID) {
      return false;
    }

    auto parentDoc = static_cast<a11y::DocAccessibleParent*>(aParentDoc);
    return parentDoc->AddChildDoc(doc, aParentID);
  } else {
    // null aParentDoc means this document is at the top level in the child
    // process.  That means it makes no sense to get an id for an accessible
    // that is its parent.
    MOZ_ASSERT(!aParentID);
    if (aParentID) {
      return false;
    }

    doc->SetTopLevel();
    a11y::DocManager::RemoteDocAdded(doc);
  }
#endif
  return true;
}

PDocumentRendererParent*
TabParent::AllocPDocumentRendererParent(const nsRect& documentRect,
                                        const gfx::Matrix& transform,
                                        const nsString& bgcolor,
                                        const uint32_t& renderFlags,
                                        const bool& flushLayout,
                                        const nsIntSize& renderSize)
{
    return new DocumentRendererParent();
}

bool
TabParent::DeallocPDocumentRendererParent(PDocumentRendererParent* actor)
{
    delete actor;
    return true;
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
  if (manager->IsContentParent()) {
    if (NS_WARN_IF(!AssertAppPrincipal(manager->AsContentParent(),
                                       principal))) {
      return nullptr;
    }
  } else {
    MOZ_CRASH("Figure out security checks for bridged content!");
  }

  if (NS_WARN_IF(!mFrameElement)) {
    return nullptr;
  }

  return
    mozilla::dom::indexedDB::AllocPIndexedDBPermissionRequestParent(mFrameElement,
                                                                    principal);
}

bool
TabParent::RecvPIndexedDBPermissionRequestConstructor(
                                      PIndexedDBPermissionRequestParent* aActor,
                                      const Principal& aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);

  return
    mozilla::dom::indexedDB::RecvPIndexedDBPermissionRequestConstructor(aActor);
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
    unused << PBrowserParent::SendMouseEvent(nsString(aType), aX, aY,
                                             aButton, aClickCount,
                                             aModifiers, aIgnoreRootScrollFrame);
  }
}

void
TabParent::SendKeyEvent(const nsAString& aType,
                        int32_t aKeyCode,
                        int32_t aCharCode,
                        int32_t aModifiers,
                        bool aPreventDefault)
{
  if (!mIsDestroyed) {
    unused << PBrowserParent::SendKeyEvent(nsString(aType), aKeyCode, aCharCode,
                                           aModifiers, aPreventDefault);
  }
}

bool TabParent::SendRealMouseEvent(WidgetMouseEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  event.refPoint += GetChildProcessOffset();

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    // When we mouseenter the tab, the tab's cursor should
    // become the current cursor.  When we mouseexit, we stop.
    if (eMouseEnterIntoWidget == event.mMessage) {
      mTabSetsCursor = true;
      if (mCustomCursor) {
        widget->SetCursor(mCustomCursor, mCustomCursorHotspotX, mCustomCursorHotspotY);
      } else if (mCursor != nsCursor(-1)) {
        widget->SetCursor(mCursor);
      }
    } else if (eMouseExitFromWidget == event.mMessage) {
      mTabSetsCursor = false;
    }
  }

  ScrollableLayerGuid guid;
  uint64_t blockId;
  ApzAwareEventRoutingToChild(&guid, &blockId, nullptr);

  if (eMouseMove == event.mMessage) {
    if (event.reason == WidgetMouseEvent::eSynthesized) {
      return SendSynthMouseMoveEvent(event, guid, blockId);
    } else {
      return SendRealMouseMoveEvent(event, guid, blockId);
    }
  }

  return SendRealMouseButtonEvent(event, guid, blockId);
}

LayoutDeviceToCSSScale
TabParent::GetLayoutDeviceToCSSScale()
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mFrameElement);
  nsIDocument* doc = (content ? content->OwnerDoc() : nullptr);
  nsIPresShell* shell = (doc ? doc->GetShell() : nullptr);
  nsPresContext* ctx = (shell ? shell->GetPresContext() : nullptr);
  return LayoutDeviceToCSSScale(ctx
    ? (float)ctx->AppUnitsPerDevPixel() / nsPresContext::AppUnitsPerCSSPixel()
    : 0.0f);
}

bool
TabParent::SendRealDragEvent(WidgetDragEvent& event, uint32_t aDragAction,
                             uint32_t aDropEffect)
{
  if (mIsDestroyed) {
    return false;
  }
  event.refPoint += GetChildProcessOffset();
  return PBrowserParent::SendRealDragEvent(event, aDragAction, aDropEffect);
}

CSSPoint TabParent::AdjustTapToChildWidget(const CSSPoint& aPoint)
{
  return aPoint + (LayoutDevicePoint(GetChildProcessOffset()) * GetLayoutDeviceToCSSScale());
}

bool TabParent::SendHandleSingleTap(const CSSPoint& aPoint, const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid)
{
  if (mIsDestroyed) {
    return false;
  }

  return PBrowserParent::SendHandleSingleTap(AdjustTapToChildWidget(aPoint), aModifiers, aGuid);
}

bool TabParent::SendHandleLongTap(const CSSPoint& aPoint, const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId)
{
  if (mIsDestroyed) {
    return false;
  }

  return PBrowserParent::SendHandleLongTap(AdjustTapToChildWidget(aPoint), aModifiers, aGuid, aInputBlockId);
}

bool TabParent::SendHandleDoubleTap(const CSSPoint& aPoint, const Modifiers& aModifiers, const ScrollableLayerGuid& aGuid)
{
  if (mIsDestroyed) {
    return false;
  }

  return PBrowserParent::SendHandleDoubleTap(AdjustTapToChildWidget(aPoint), aModifiers, aGuid);
}

bool TabParent::SendMouseWheelEvent(WidgetWheelEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }

  ScrollableLayerGuid guid;
  uint64_t blockId;
  ApzAwareEventRoutingToChild(&guid, &blockId, nullptr);
  event.refPoint += GetChildProcessOffset();
  return PBrowserParent::SendMouseWheelEvent(event, guid, blockId);
}

bool TabParent::RecvDispatchWheelEvent(const mozilla::WidgetWheelEvent& aEvent)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }

  WidgetWheelEvent localEvent(aEvent);
  localEvent.widget = widget;
  localEvent.refPoint -= GetChildProcessOffset();

  widget->DispatchAPZAwareEvent(&localEvent);
  return true;
}

bool
TabParent::RecvDispatchMouseEvent(const mozilla::WidgetMouseEvent& aEvent)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }

  WidgetMouseEvent localEvent(aEvent);
  localEvent.widget = widget;
  localEvent.refPoint -= GetChildProcessOffset();

  widget->DispatchInputEvent(&localEvent);
  return true;
}

bool
TabParent::RecvDispatchKeyboardEvent(const mozilla::WidgetKeyboardEvent& aEvent)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.widget = widget;
  localEvent.refPoint -= GetChildProcessOffset();

  widget->DispatchInputEvent(&localEvent);
  return true;
}

static void
DoCommandCallback(mozilla::Command aCommand, void* aData)
{
  static_cast<InfallibleTArray<mozilla::CommandInt>*>(aData)->AppendElement(aCommand);
}

bool
TabParent::RecvRequestNativeKeyBindings(const WidgetKeyboardEvent& aEvent,
                                        MaybeNativeKeyBinding* aBindings)
{
  AutoInfallibleTArray<mozilla::CommandInt, 4> singleLine;
  AutoInfallibleTArray<mozilla::CommandInt, 4> multiLine;
  AutoInfallibleTArray<mozilla::CommandInt, 4> richText;

  *aBindings = mozilla::void_t();

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }

  WidgetKeyboardEvent localEvent(aEvent);

  if (NS_FAILED(widget->AttachNativeKeyEvent(localEvent))) {
    return true;
  }

  widget->ExecuteNativeKeyBinding(nsIWidget::NativeKeyBindingsForSingleLineEditor,
                                  localEvent, DoCommandCallback, &singleLine);
  widget->ExecuteNativeKeyBinding(nsIWidget::NativeKeyBindingsForMultiLineEditor,
                                  localEvent, DoCommandCallback, &multiLine);
  widget->ExecuteNativeKeyBinding(nsIWidget::NativeKeyBindingsForRichTextEditor,
                                  localEvent, DoCommandCallback, &richText);

  if (!singleLine.IsEmpty() || !multiLine.IsEmpty() || !richText.IsEmpty()) {
    *aBindings = NativeKeyBinding(singleLine, multiLine, richText);
  }

  return true;
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

  NS_IMETHODIMP Observe(nsISupports* aSubject,
                        const char* aTopic,
                        const char16_t* aData) override
  {
    if (!mTabParent) {
      // We already sent the notification
      return NS_OK;
    }

    if (!mTabParent->SendNativeSynthesisResponse(mObserverId, nsCString(aTopic))) {
      NS_WARNING("Unable to send native event synthesization response!");
    }
    // Null out tabparent to indicate we already sent the response
    mTabParent = nullptr;
    return NS_OK;
  }

private:
  virtual ~SynthesizedEventObserver() { }

  nsRefPtr<TabParent> mTabParent;
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

bool
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
      aModifierFlags, aCharacters, aUnmodifiedCharacters,
      responder.GetObserver());
  }
  return true;
}

bool
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
  return true;
}

bool
TabParent::RecvSynthesizeNativeMouseMove(const LayoutDeviceIntPoint& aPoint,
                                         const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "mousemove");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeMouseMove(aPoint, responder.GetObserver());
  }
  return true;
}

bool
TabParent::RecvSynthesizeNativeMouseScrollEvent(const LayoutDeviceIntPoint& aPoint,
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
      aDeltaX, aDeltaY, aDeltaZ, aModifierFlags, aAdditionalFlags,
      responder.GetObserver());
  }
  return true;
}

bool
TabParent::RecvSynthesizeNativeTouchPoint(const uint32_t& aPointerId,
                                          const TouchPointerState& aPointerState,
                                          const nsIntPoint& aPointerScreenPoint,
                                          const double& aPointerPressure,
                                          const uint32_t& aPointerOrientation,
                                          const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "touchpoint");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchPoint(aPointerId, aPointerState, aPointerScreenPoint,
      aPointerPressure, aPointerOrientation, responder.GetObserver());
  }
  return true;
}

bool
TabParent::RecvSynthesizeNativeTouchTap(const nsIntPoint& aPointerScreenPoint,
                                        const bool& aLongTap,
                                        const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "touchtap");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->SynthesizeNativeTouchTap(aPointerScreenPoint, aLongTap,
      responder.GetObserver());
  }
  return true;
}

bool
TabParent::RecvClearNativeTouchSequence(const uint64_t& aObserverId)
{
  AutoSynthesizedEventResponder responder(this, aObserverId, "cleartouch");
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    widget->ClearNativeTouchSequence(responder.GetObserver());
  }
  return true;
}

bool TabParent::SendRealKeyEvent(WidgetKeyboardEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  event.refPoint += GetChildProcessOffset();

  MaybeNativeKeyBinding bindings;
  bindings = void_t();
  if (event.mMessage == eKeyPress) {
    nsCOMPtr<nsIWidget> widget = GetWidget();

    AutoInfallibleTArray<mozilla::CommandInt, 4> singleLine;
    AutoInfallibleTArray<mozilla::CommandInt, 4> multiLine;
    AutoInfallibleTArray<mozilla::CommandInt, 4> richText;

    widget->ExecuteNativeKeyBinding(nsIWidget::NativeKeyBindingsForSingleLineEditor,
                                    event, DoCommandCallback, &singleLine);
    widget->ExecuteNativeKeyBinding(nsIWidget::NativeKeyBindingsForMultiLineEditor,
                                    event, DoCommandCallback, &multiLine);
    widget->ExecuteNativeKeyBinding(nsIWidget::NativeKeyBindingsForRichTextEditor,
                                    event, DoCommandCallback, &richText);

    if (!singleLine.IsEmpty() || !multiLine.IsEmpty() || !richText.IsEmpty()) {
      bindings = NativeKeyBinding(singleLine, multiLine, richText);
    }
  }

  return PBrowserParent::SendRealKeyEvent(event, bindings);
}

bool TabParent::SendRealTouchEvent(WidgetTouchEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }

  // PresShell::HandleEventInternal adds touches on touch end/cancel.  This
  // confuses remote content and the panning and zooming logic into thinking
  // that the added touches are part of the touchend/cancel, when actually
  // they're not.
  if (event.mMessage == eTouchEnd || event.mMessage == eTouchCancel) {
    for (int i = event.touches.Length() - 1; i >= 0; i--) {
      if (!event.touches[i]->mChanged) {
        event.touches.RemoveElementAt(i);
      }
    }
  }

  ScrollableLayerGuid guid;
  uint64_t blockId;
  nsEventStatus apzResponse;
  ApzAwareEventRoutingToChild(&guid, &blockId, &apzResponse);

  if (mIsDestroyed) {
    return false;
  }

  LayoutDeviceIntPoint offset = GetChildProcessOffset();
  for (uint32_t i = 0; i < event.touches.Length(); i++) {
    event.touches[i]->mRefPoint += offset;
  }

  return (event.mMessage == eTouchMove) ?
    PBrowserParent::SendRealTouchMoveEvent(event, guid, blockId, apzResponse) :
    PBrowserParent::SendRealTouchEvent(event, guid, blockId, apzResponse);
}

bool
TabParent::RecvSyncMessage(const nsString& aMessage,
                           const ClonedMessageData& aData,
                           InfallibleTArray<CpowEntry>&& aCpows,
                           const IPC::Principal& aPrincipal,
                           nsTArray<StructuredCloneData>* aRetVal)
{
  // FIXME Permission check for TabParent in Content process
  nsIPrincipal* principal = aPrincipal;
  if (Manager()->IsContentParent()) {
    ContentParent* parent = Manager()->AsContentParent();
    if (!ContentParent::IgnoreIPCPrincipal() &&
        parent && principal && !AssertAppPrincipal(parent, principal)) {
      return false;
    }
  }

  StructuredCloneData data;
  ipc::UnpackClonedMessageDataForParent(aData, data);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  return ReceiveMessage(aMessage, true, &data, &cpows, aPrincipal, aRetVal);
}

bool
TabParent::RecvRpcMessage(const nsString& aMessage,
                          const ClonedMessageData& aData,
                          InfallibleTArray<CpowEntry>&& aCpows,
                          const IPC::Principal& aPrincipal,
                          nsTArray<StructuredCloneData>* aRetVal)
{
  // FIXME Permission check for TabParent in Content process
  nsIPrincipal* principal = aPrincipal;
  if (Manager()->IsContentParent()) {
    ContentParent* parent = Manager()->AsContentParent();
    if (!ContentParent::IgnoreIPCPrincipal() &&
        parent && principal && !AssertAppPrincipal(parent, principal)) {
      return false;
    }
  }

  StructuredCloneData data;
  ipc::UnpackClonedMessageDataForParent(aData, data);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  return ReceiveMessage(aMessage, true, &data, &cpows, aPrincipal, aRetVal);
}

bool
TabParent::RecvAsyncMessage(const nsString& aMessage,
                            const ClonedMessageData& aData,
                            InfallibleTArray<CpowEntry>&& aCpows,
                            const IPC::Principal& aPrincipal)
{
  // FIXME Permission check for TabParent in Content process
  nsIPrincipal* principal = aPrincipal;
  if (Manager()->IsContentParent()) {
    ContentParent* parent = Manager()->AsContentParent();
    if (!ContentParent::IgnoreIPCPrincipal() &&
        parent && principal && !AssertAppPrincipal(parent, principal)) {
      return false;
    }
  }

  StructuredCloneData data;
  ipc::UnpackClonedMessageDataForParent(aData, data);

  CrossProcessCpowHolder cpows(Manager(), aCpows);
  return ReceiveMessage(aMessage, false, &data, &cpows, aPrincipal, nullptr);
}

bool
TabParent::RecvSetCursor(const uint32_t& aCursor, const bool& aForce)
{
  mCursor = static_cast<nsCursor>(aCursor);
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
  return true;
}

bool
TabParent::RecvSetCustomCursor(const nsCString& aCursorData,
                               const uint32_t& aWidth,
                               const uint32_t& aHeight,
                               const uint32_t& aStride,
                               const uint8_t& aFormat,
                               const uint32_t& aHotspotX,
                               const uint32_t& aHotspotY,
                               const bool& aForce)
{
  mCursor = nsCursor(-1);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    if (aForce) {
      widget->ClearCachedCursor();
    }

    if (mTabSetsCursor) {
      const gfx::IntSize size(aWidth, aHeight);

      mozilla::RefPtr<gfx::DataSourceSurface> customCursor = new mozilla::gfx::SourceSurfaceRawData();
      mozilla::gfx::SourceSurfaceRawData* raw = static_cast<mozilla::gfx::SourceSurfaceRawData*>(customCursor.get());
      raw->InitWrappingData(
        reinterpret_cast<uint8_t*>(const_cast<nsCString&>(aCursorData).BeginWriting()),
        size, aStride, static_cast<mozilla::gfx::SurfaceFormat>(aFormat), false);
      raw->GuaranteePersistance();

      nsRefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(customCursor, size);
      nsCOMPtr<imgIContainer> cursorImage(image::ImageOps::CreateFromDrawable(drawable));
      widget->SetCursor(cursorImage, aHotspotX, aHotspotY);
      mCustomCursor = cursorImage;
      mCustomCursorHotspotX = aHotspotX;
      mCustomCursorHotspotY = aHotspotY;
    }
  }

  return true;
}

nsIXULBrowserWindow*
TabParent::GetXULBrowserWindow()
{
  nsCOMPtr<nsIContent> frame = do_QueryInterface(mFrameElement);
  if (!frame) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = frame->OwnerDoc()->GetDocShell();
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

bool
TabParent::RecvSetStatus(const uint32_t& aType, const nsString& aStatus)
{
  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  if (!xulBrowserWindow) {
    return true;
  }

  switch (aType) {
   case nsIWebBrowserChrome::STATUS_SCRIPT:
    xulBrowserWindow->SetJSStatus(aStatus);
    break;
   case nsIWebBrowserChrome::STATUS_LINK:
    xulBrowserWindow->SetOverLink(aStatus, nullptr);
    break;
  }
  return true;
}

bool
TabParent::RecvShowTooltip(const uint32_t& aX, const uint32_t& aY, const nsString& aTooltip)
{
  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  if (!xulBrowserWindow) {
    return true;
  }

  xulBrowserWindow->ShowTooltip(aX, aY, aTooltip);
  return true;
}

bool
TabParent::RecvHideTooltip()
{
  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow = GetXULBrowserWindow();
  if (!xulBrowserWindow) {
    return true;
  }

  xulBrowserWindow->HideTooltip();
  return true;
}

bool
TabParent::RecvNotifyIMEFocus(const ContentCache& aContentCache,
                              const IMENotification& aIMENotification,
                              nsIMEUpdatePreference* aPreference)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    *aPreference = nsIMEUpdatePreference();
    return true;
  }

  mContentCache.AssignContent(aContentCache, &aIMENotification);
  IMEStateManager::NotifyIME(aIMENotification, widget, true);

  if (aIMENotification.mMessage == NOTIFY_IME_OF_FOCUS) {
    *aPreference = widget->GetIMEUpdatePreference();
  }
  return true;
}

bool
TabParent::RecvNotifyIMETextChange(const ContentCache& aContentCache,
                                   const IMENotification& aIMENotification)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return true;

#ifdef DEBUG
  nsIMEUpdatePreference updatePreference = widget->GetIMEUpdatePreference();
  NS_ASSERTION(updatePreference.WantTextChange(),
               "Don't call Send/RecvNotifyIMETextChange without NOTIFY_TEXT_CHANGE");
  MOZ_ASSERT(!aIMENotification.mTextChangeData.mCausedByComposition ||
               updatePreference.WantChangesCausedByComposition(),
    "The widget doesn't want text change notification caused by composition");
#endif

  mContentCache.AssignContent(aContentCache, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return true;
}

bool
TabParent::RecvNotifyIMECompositionUpdate(
             const ContentCache& aContentCache,
             const IMENotification& aIMENotification)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }

  mContentCache.AssignContent(aContentCache, &aIMENotification);
  mContentCache.MaybeNotifyIME(widget, aIMENotification);
  return true;
}

bool
TabParent::RecvNotifyIMESelection(const ContentCache& aContentCache,
                                  const IMENotification& aIMENotification)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return true;

  mContentCache.AssignContent(aContentCache, &aIMENotification);

  const nsIMEUpdatePreference updatePreference =
    widget->GetIMEUpdatePreference();
  if (updatePreference.WantSelectionChange() &&
      (updatePreference.WantChangesCausedByComposition() ||
       !aIMENotification.mSelectionChangeData.mCausedByComposition)) {
    mContentCache.MaybeNotifyIME(widget, aIMENotification);
  }
  return true;
}

bool
TabParent::RecvUpdateContentCache(const ContentCache& aContentCache)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }

  mContentCache.AssignContent(aContentCache);
  return true;
}

bool
TabParent::RecvNotifyIMEMouseButtonEvent(
             const IMENotification& aIMENotification,
             bool* aConsumedByIME)
{

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    *aConsumedByIME = false;
    return true;
  }
  nsresult rv = IMEStateManager::NotifyIME(aIMENotification, widget, true);
  *aConsumedByIME = rv == NS_SUCCESS_EVENT_CONSUMED;
  return true;
}

bool
TabParent::RecvNotifyIMEPositionChange(const ContentCache& aContentCache,
                                       const IMENotification& aIMENotification)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }

  mContentCache.AssignContent(aContentCache, &aIMENotification);

  const nsIMEUpdatePreference updatePreference =
    widget->GetIMEUpdatePreference();
  if (updatePreference.WantPositionChanged()) {
    mContentCache.MaybeNotifyIME(widget, aIMENotification);
  }
  return true;
}

bool
TabParent::RecvOnEventNeedingAckHandled(const EventMessage& aMessage)
{
  // This is called when the child process receives WidgetCompositionEvent or
  // WidgetSelectionEvent.
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }

  // While calling OnEventNeedingAckHandled(), TabParent *might* be destroyed
  // since it may send notifications to IME.
  nsRefPtr<TabParent> kungFuDeathGrip(this);
  mContentCache.OnEventNeedingAckHandled(widget, aMessage);
  return true;
}

bool
TabParent::RecvRequestFocus(const bool& aCanRaise)
{
  nsCOMPtr<nsIFocusManager> fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return true;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(mFrameElement);
  if (!content || !content->OwnerDoc()) {
    return true;
  }

  uint32_t flags = nsIFocusManager::FLAG_NOSCROLL;
  if (aCanRaise)
    flags |= nsIFocusManager::FLAG_RAISE;

  nsCOMPtr<nsIDOMElement> node = do_QueryInterface(mFrameElement);
  fm->SetFocus(node, flags);
  return true;
}

bool
TabParent::RecvEnableDisableCommands(const nsString& aAction,
                                     nsTArray<nsCString>&& aEnabledCommands,
                                     nsTArray<nsCString>&& aDisabledCommands)
{
  nsCOMPtr<nsIRemoteBrowser> remoteBrowser = do_QueryInterface(mFrameElement);
  if (remoteBrowser) {
    nsAutoArrayPtr<const char*> enabledCommands, disabledCommands;

    if (aEnabledCommands.Length()) {
      enabledCommands = new const char* [aEnabledCommands.Length()];
      for (uint32_t c = 0; c < aEnabledCommands.Length(); c++) {
        enabledCommands[c] = aEnabledCommands[c].get();
      }
    }

    if (aDisabledCommands.Length()) {
      disabledCommands = new const char* [aDisabledCommands.Length()];
      for (uint32_t c = 0; c < aDisabledCommands.Length(); c++) {
        disabledCommands[c] = aDisabledCommands[c].get();
      }
    }

    remoteBrowser->EnableDisableCommands(aAction,
                                         aEnabledCommands.Length(), enabledCommands,
                                         aDisabledCommands.Length(), disabledCommands);
  }

  return true;
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
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    return offset;
  }
  nsIFrame* targetFrame = frameLoader->GetPrimaryFrameOfOwningContent();
  if (!targetFrame) {
    return offset;
  }

  // Find out how far we're offset from the nearest widget.
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return offset;
  }
  nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(widget,
                                                            LayoutDeviceIntPoint(0, 0),
                                                            targetFrame);

  return LayoutDeviceIntPoint::FromAppUnitsToNearest(
           pt, targetFrame->PresContext()->AppUnitsPerDevPixel());
}

bool
TabParent::RecvReplyKeyEvent(const WidgetKeyboardEvent& event)
{
  NS_ENSURE_TRUE(mFrameElement, true);

  WidgetKeyboardEvent localEvent(event);
  // Set mNoCrossProcessBoundaryForwarding to avoid this event from
  // being infinitely redispatched and forwarded to the child again.
  localEvent.mFlags.mNoCrossProcessBoundaryForwarding = true;

  // Here we convert the WidgetEvent that we received to an nsIDOMEvent
  // to be able to dispatch it to the <browser> element as the target element.
  nsIDocument* doc = mFrameElement->OwnerDoc();
  nsIPresShell* presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, true);
  nsPresContext* presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, true);

  EventDispatcher::Dispatch(mFrameElement, presContext, &localEvent);
  return true;
}

bool
TabParent::RecvDispatchAfterKeyboardEvent(const WidgetKeyboardEvent& aEvent)
{
  NS_ENSURE_TRUE(mFrameElement, true);

  WidgetKeyboardEvent localEvent(aEvent);
  localEvent.widget = GetWidget();

  nsIDocument* doc = mFrameElement->OwnerDoc();
  nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, true);

  if (mFrameElement &&
      PresShell::BeforeAfterKeyboardEventEnabled() &&
      localEvent.mMessage != eKeyPress) {
    presShell->DispatchAfterKeyboardEvent(mFrameElement, localEvent,
                                          aEvent.mFlags.mDefaultPrevented);
  }

  return true;
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
      aEvent.mReply.mRect -= GetChildProcessOffset();
      break;
    default:
      break;
  }
  return true;
}

bool
TabParent::SendCompositionEvent(WidgetCompositionEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }

  if (!mContentCache.OnCompositionEvent(event)) {
    return true;
  }
  return PBrowserParent::SendCompositionEvent(event);
}

bool
TabParent::SendSelectionEvent(WidgetSelectionEvent& event)
{
  if (mIsDestroyed) {
    return false;
  }
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }
  mContentCache.OnSelectionEvent(event);
  if (NS_WARN_IF(!PBrowserParent::SendSelectionEvent(event))) {
    return false;
  }
  event.mSucceeded = true;
  return true;
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
TabParent::GetFrom(nsIFrameLoader* aFrameLoader)
{
  if (!aFrameLoader)
    return nullptr;
  return GetFrom(static_cast<nsFrameLoader*>(aFrameLoader));
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
  nsRefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
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
  if (ManagedPRenderFrameParent().IsEmpty()) {
    return nullptr;
  }
  return static_cast<RenderFrameParent*>(ManagedPRenderFrameParent()[0]);
}

bool
TabParent::RecvEndIMEComposition(const bool& aCancel,
                                 bool* aNoCompositionEvent,
                                 nsString* aComposition)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }
  *aNoCompositionEvent =
    !mContentCache.RequestToCommitComposition(widget, aCancel, *aComposition);
  return true;
}

bool
TabParent::RecvStartPluginIME(const WidgetKeyboardEvent& aKeyboardEvent,
                              const int32_t& aPanelX, const int32_t& aPanelY,
                              nsString* aCommitted)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }
  widget->StartPluginIME(aKeyboardEvent,
                         (int32_t&)aPanelX, 
                         (int32_t&)aPanelY,
                         *aCommitted);
  return true;
}

bool
TabParent::RecvSetPluginFocused(const bool& aFocused)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return true;
  }
  widget->SetPluginFocused((bool&)aFocused);
  return true;
}

bool
TabParent::RecvGetInputContext(int32_t* aIMEEnabled,
                               int32_t* aIMEOpen,
                               intptr_t* aNativeIMEContext)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    *aIMEEnabled = IMEState::DISABLED;
    *aIMEOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
    *aNativeIMEContext = 0;
    return true;
  }

  InputContext context = widget->GetInputContext();
  *aIMEEnabled = static_cast<int32_t>(context.mIMEState.mEnabled);
  *aIMEOpen = static_cast<int32_t>(context.mIMEState.mOpen);
  *aNativeIMEContext = reinterpret_cast<intptr_t>(context.mNativeIMEContext);
  return true;
}

bool
TabParent::RecvSetInputContext(const int32_t& aIMEEnabled,
                               const int32_t& aIMEOpen,
                               const nsString& aType,
                               const nsString& aInputmode,
                               const nsString& aActionHint,
                               const int32_t& aCause,
                               const int32_t& aFocusChange)
{
  InputContext context;
  context.mIMEState.mEnabled = static_cast<IMEState::Enabled>(aIMEEnabled);
  context.mIMEState.mOpen = static_cast<IMEState::Open>(aIMEOpen);
  context.mHTMLInputType.Assign(aType);
  context.mHTMLInputInputmode.Assign(aInputmode);
  context.mActionHint.Assign(aActionHint);
  context.mOrigin = InputContext::ORIGIN_CONTENT;

  InputContextAction action(
    static_cast<InputContextAction::Cause>(aCause),
    static_cast<InputContextAction::FocusChange>(aFocusChange));

  IMEStateManager::SetInputContextForChildProcess(this, context, action);

  return true;
}

bool
TabParent::RecvIsParentWindowMainWidgetVisible(bool* aIsVisible)
{
  nsCOMPtr<nsIContent> frame = do_QueryInterface(mFrameElement);
  if (!frame)
    return true;
  nsCOMPtr<nsIDOMWindowUtils> windowUtils =
    do_QueryInterface(frame->OwnerDoc()->GetWindow());
  nsresult rv = windowUtils->GetIsParentWindowMainWidgetVisible(aIsVisible);
  return NS_SUCCEEDED(rv);
}

bool
TabParent::RecvGetDPI(float* aValue)
{
  TryCacheDPIAndScale();

  MOZ_ASSERT(mDPI > 0,
             "Must not ask for DPI before OwnerElement is received!");
  *aValue = mDPI;
  return true;
}

bool
TabParent::RecvGetDefaultScale(double* aValue)
{
  TryCacheDPIAndScale();

  MOZ_ASSERT(mDefaultScale.scale > 0,
             "Must not ask for scale before OwnerElement is received!");
  *aValue = mDefaultScale.scale;
  return true;
}

bool
TabParent::RecvGetMaxTouchPoints(uint32_t* aTouchPoints)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (widget) {
    *aTouchPoints = widget->GetMaxTouchPoints();
  } else {
    *aTouchPoints = 0;
  }
  return true;
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

bool
TabParent::RecvGetWidgetNativeData(WindowsHandle* aValue)
{
  *aValue = 0;
  nsCOMPtr<nsIWidget> widget = GetTopLevelWidget();
  if (widget) {
    *aValue = reinterpret_cast<WindowsHandle>(
      widget->GetNativeData(NS_NATIVE_SHAREABLE_WINDOW));
  }
  return true;
}

bool
TabParent::RecvSetNativeChildOfShareableWindow(const uintptr_t& aChildWindow)
{
#if defined(XP_WIN)
  nsCOMPtr<nsIWidget> widget = GetTopLevelWidget();
  if (widget) {
    // Note that this call will probably cause a sync native message to the
    // process that owns the child window.
    widget->SetNativeData(NS_NATIVE_CHILD_OF_SHAREABLE_WINDOW, aChildWindow);
  }
  return true;
#else
  NS_NOTREACHED(
    "TabParent::RecvSetNativeChildOfShareableWindow not implemented!");
  return false;
#endif
}

bool
TabParent::RecvDispatchFocusToTopLevelWindow()
{
  nsCOMPtr<nsIWidget> widget = GetTopLevelWidget();
  if (widget) {
    widget->SetFocus(false);
  }
  return true;
}

bool
TabParent::ReceiveMessage(const nsString& aMessage,
                          bool aSync,
                          StructuredCloneData* aData,
                          CpowHolder* aCpows,
                          nsIPrincipal* aPrincipal,
                          nsTArray<StructuredCloneData>* aRetVal)
{
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader(true);
  if (frameLoader && frameLoader->GetFrameMessageManager()) {
    nsRefPtr<nsFrameMessageManager> manager =
      frameLoader->GetFrameMessageManager();

    manager->ReceiveMessage(mFrameElement,
                            frameLoader,
                            aMessage,
                            aSync,
                            aData,
                            aCpows,
                            aPrincipal,
                            aRetVal);
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

  nsCOMPtr<nsIDOMWindow> window;
  nsCOMPtr<nsIContent> frame = do_QueryInterface(mFrameElement);
  if (frame)
    window = do_QueryInterface(frame->OwnerDoc()->GetWindow());

  // Get an auth prompter for our window so that the parenting
  // of the dialogs works as it should when using tabs.
  nsCOMPtr<nsISupports> prompt;
  rv = wwatch->GetPrompt(window, iid, getter_AddRefs(prompt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoginManagerPrompter> prompter = do_QueryInterface(prompt);
  if (prompter) {
    nsCOMPtr<nsIDOMElement> browser = do_QueryInterface(mFrameElement);
    prompter->SetE10sData(browser, nullptr);
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
  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  TextureFactoryIdentifier textureFactoryIdentifier;
  uint64_t layersId = 0;
  bool success = false;

  PRenderFrameParent* renderFrame =
    new RenderFrameParent(frameLoader,
                          &textureFactoryIdentifier,
                          &layersId,
                          &success);
  if (success) {
    AddTabParentToTable(layersId, this);
  }
  return renderFrame;
}

bool
TabParent::DeallocPRenderFrameParent(PRenderFrameParent* aFrame)
{
  delete aFrame;
  return true;
}

bool
TabParent::RecvGetRenderFrameInfo(PRenderFrameParent* aRenderFrame,
                                  TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                  uint64_t* aLayersId)
{
  RenderFrameParent* renderFrame = static_cast<RenderFrameParent*>(aRenderFrame);
  renderFrame->GetTextureFactoryIdentifier(aTextureFactoryIdentifier);
  *aLayersId = renderFrame->GetLayersId();

  if (mNeedLayerTreeReadyNotification) {
    RequestNotifyLayerTreeReady();
    mNeedLayerTreeReadyNotification = false;
  }

  return true;
}

bool
TabParent::RecvAudioChannelActivityNotification(const uint32_t& aAudioChannel,
                                                const bool& aActive)
{
  if (aAudioChannel >= NUMBER_OF_AUDIO_CHANNELS) {
    return false;
  }

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    nsRefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    nsAutoCString topic;
    topic.Assign("audiochannel-activity-");
    topic.Append(AudioChannelService::GetAudioChannelTable()[aAudioChannel].tag);

    os->NotifyObservers(NS_ISUPPORTS_CAST(nsITabParent*, this),
                        topic.get(),
                        aActive ? MOZ_UTF16("active") : MOZ_UTF16("inactive"));
  }

  return true;
}

already_AddRefed<nsFrameLoader>
TabParent::GetFrameLoader(bool aUseCachedFrameLoaderAfterDestroy) const
{
  if (mIsDestroyed && !aUseCachedFrameLoaderAfterDestroy) {
    return nullptr;
  }

  if (mFrameLoader) {
    nsRefPtr<nsFrameLoader> fl = mFrameLoader;
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
    mDefaultScale = widget->GetDefaultScale();
  }
}

already_AddRefed<nsIWidget>
TabParent::GetWidget() const
{
  if (!mFrameElement) {
    return nullptr;
  }
  nsCOMPtr<nsIWidget> widget = nsContentUtils::WidgetForDocument(mFrameElement->OwnerDoc());
  return widget.forget();
}

void
TabParent::ApzAwareEventRoutingToChild(ScrollableLayerGuid* aOutTargetGuid,
                                       uint64_t* aOutInputBlockId,
                                       nsEventStatus* aOutApzResponse)
{
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

    // Let the widget know that the event will be sent to the child process,
    // which will (hopefully) send a confirmation notice back to APZ.
    InputAPZContext::SetRoutedToChildProcess();
  } else {
    if (aOutInputBlockId) {
      *aOutInputBlockId = 0;
    }
    if (aOutApzResponse) {
      *aOutApzResponse = nsEventStatus_eIgnore;
    }
  }
}

bool
TabParent::RecvBrowserFrameOpenWindow(PBrowserParent* aOpener,
                                      const nsString& aURL,
                                      const nsString& aName,
                                      const nsString& aFeatures,
                                      bool* aOutWindowOpened)
{
  BrowserElementParent::OpenWindowResult opened =
    BrowserElementParent::OpenWindowOOP(TabParent::GetFrom(aOpener),
                                        this, aURL, aName, aFeatures);
  *aOutWindowOpened = (opened == BrowserElementParent::OPEN_WINDOW_ADDED);
  return true;
}

bool
TabParent::RecvZoomToRect(const uint32_t& aPresShellId,
                          const ViewID& aViewId,
                          const CSSRect& aRect)
{
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->ZoomToRect(aPresShellId, aViewId, aRect);
  }
  return true;
}

bool
TabParent::RecvUpdateZoomConstraints(const uint32_t& aPresShellId,
                                     const ViewID& aViewId,
                                     const MaybeZoomConstraints& aConstraints)
{
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->UpdateZoomConstraints(aPresShellId, aViewId, aConstraints);
  }
  return true;
}

bool
TabParent::RecvRespondStartSwipeEvent(const uint64_t& aInputBlockId,
                                      const bool& aStartSwipe)
{
  if (nsCOMPtr<nsIWidget> widget = GetWidget()) {
    widget->ReportSwipeStarted(aInputBlockId, aStartSwipe);
  }
  return true;
}

bool
TabParent::RecvContentReceivedInputBlock(const ScrollableLayerGuid& aGuid,
                                         const uint64_t& aInputBlockId,
                                         const bool& aPreventDefault)
{
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->ContentReceivedInputBlock(aGuid, aInputBlockId, aPreventDefault);
  }
  return true;
}

bool
TabParent::RecvSetTargetAPZC(const uint64_t& aInputBlockId,
                             nsTArray<ScrollableLayerGuid>&& aTargets)
{
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->SetTargetAPZC(aInputBlockId, aTargets);
  }
  return true;
}

bool
TabParent::RecvSetAllowedTouchBehavior(const uint64_t& aInputBlockId,
                                       nsTArray<TouchBehaviorFlags>&& aFlags)
{
  if (RenderFrameParent* rfp = GetRenderFrame()) {
    rfp->SetAllowedTouchBehavior(aInputBlockId, aFlags);
  }
  return true;
}

already_AddRefed<nsILoadContext>
TabParent::GetLoadContext()
{
  nsCOMPtr<nsILoadContext> loadContext;
  if (mLoadContext) {
    loadContext = mLoadContext;
  } else {
    // TODO Bug 1191740 - Add OriginAttributes in TabContext
    OriginAttributes attrs = OriginAttributes(OwnOrContainingAppId(), IsBrowserElement());
    loadContext = new LoadContext(GetOwnerElement(),
                                  true /* aIsContent */,
                                  mChromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW,
                                  mChromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW,
                                  attrs);
    mLoadContext = loadContext;
  }
  return loadContext.forget();
}

NS_IMETHODIMP
TabParent::InjectTouchEvent(const nsAString& aType,
                            uint32_t* aIdentifiers,
                            int32_t* aXs,
                            int32_t* aYs,
                            uint32_t* aRxs,
                            uint32_t* aRys,
                            float* aRotationAngles,
                            float* aForces,
                            uint32_t aCount,
                            int32_t aModifiers)
{
  EventMessage msg;
  nsContentUtils::GetEventMessageAndAtom(aType, eTouchEventClass, &msg);
  if (msg != eTouchStart && msg != eTouchMove &&
      msg != eTouchEnd && msg != eTouchCancel) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  WidgetTouchEvent event(true, msg, widget);
  event.modifiers = aModifiers;
  event.time = PR_IntervalNow();

  nsCOMPtr<nsIContent> content = do_QueryInterface(mFrameElement);
  if (!content || !content->OwnerDoc()) {
    return NS_ERROR_FAILURE;
  }

  nsIDocument* doc = content->OwnerDoc();
  if (!doc || !doc->GetShell()) {
    return NS_ERROR_FAILURE;
  }
  nsPresContext* presContext = doc->GetShell()->GetPresContext();

  event.touches.SetCapacity(aCount);
  for (uint32_t i = 0; i < aCount; ++i) {
    LayoutDeviceIntPoint pt =
      LayoutDeviceIntPoint::FromAppUnitsRounded(
        CSSPoint::ToAppUnits(CSSPoint(aXs[i], aYs[i])),
        presContext->AppUnitsPerDevPixel());

    nsRefPtr<Touch> t = new Touch(aIdentifiers[i],
                                  pt,
                                  nsIntPoint(aRxs[i], aRys[i]),
                                  aRotationAngles[i],
                                  aForces[i]);

    // Consider all injected touch events as changedTouches. For more details
    // about the meaning of changedTouches for each event, see
    // https://developer.mozilla.org/docs/Web/API/TouchEvent.changedTouches
    t->mChanged = true;
    event.touches.AppendElement(t);
  }

  SendRealTouchEvent(event);
  return NS_OK;
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
  unused << SendSetDocShellIsActive(isActive);
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetDocShellIsActive(bool* aIsActive)
{
  *aIsActive = mDocShellIsActive;
  return NS_OK;
}

NS_IMETHODIMP
TabParent::SuppressDisplayport(bool aEnabled)
{
  if (IsDestroyed()) {
    return NS_OK;
  }

  if (aEnabled) {
    mActiveSupressDisplayportCount++;
  } else {
    mActiveSupressDisplayportCount--;
  }

  MOZ_ASSERT(mActiveSupressDisplayportCount >= 0);

  unused << SendSuppressDisplayport(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
TabParent::GetTabId(uint64_t* aId)
{
  *aId = GetTabId();
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
TabParent::NavigateByKey(bool aForward, bool aForDocumentNavigation)
{
  unused << SendNavigateByKey(aForward, aForDocumentNavigation);
  return NS_OK;
}

class LayerTreeUpdateRunnable final
  : public nsRunnable
{
  uint64_t mLayersId;
  bool mActive;

public:
  explicit LayerTreeUpdateRunnable(uint64_t aLayersId, bool aActive)
    : mLayersId(aLayersId), mActive(aActive) {}

private:
  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());
    TabParent* tabParent = TabParent::GetTabParentFromLayersId(mLayersId);
    if (tabParent) {
      tabParent->LayerTreeUpdate(mActive);
    }
    return NS_OK;
  }
};

// This observer runs on the compositor thread, so we dispatch a runnable to the
// main thread to actually dispatch the event.
class LayerTreeUpdateObserver : public CompositorUpdateObserver
{
  virtual void ObserveUpdate(uint64_t aLayersId, bool aActive) {
    nsRefPtr<LayerTreeUpdateRunnable> runnable = new LayerTreeUpdateRunnable(aLayersId, aActive);
    NS_DispatchToMainThread(runnable);
  }
};

bool
TabParent::RequestNotifyLayerTreeReady()
{
  RenderFrameParent* frame = GetRenderFrame();
  if (!frame) {
    mNeedLayerTreeReadyNotification = true;
  } else {
    CompositorParent::RequestNotifyLayerTreeReady(frame->GetLayersId(),
                                                  new LayerTreeUpdateObserver());
  }
  return true;
}

bool
TabParent::RequestNotifyLayerTreeCleared()
{
  RenderFrameParent* frame = GetRenderFrame();
  if (!frame) {
    return false;
  }

  CompositorParent::RequestNotifyLayerTreeCleared(frame->GetLayersId(),
                                                  new LayerTreeUpdateObserver());
  return true;
}

bool
TabParent::LayerTreeUpdate(bool aActive)
{
  nsCOMPtr<mozilla::dom::EventTarget> target = do_QueryInterface(mFrameElement);
  if (!target) {
    NS_WARNING("Could not locate target for layer tree message.");
    return true;
  }

  nsRefPtr<Event> event = NS_NewDOMEvent(mFrameElement, nullptr, nullptr);
  if (aActive) {
    event->InitEvent(NS_LITERAL_STRING("MozLayerTreeReady"), true, false);
  } else {
    event->InitEvent(NS_LITERAL_STRING("MozLayerTreeCleared"), true, false);
  }
  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;
  bool dummy;
  mFrameElement->DispatchEvent(event, &dummy);
  return true;
}

void
TabParent::SwapLayerTreeObservers(TabParent* aOther)
{
  if (IsDestroyed() || aOther->IsDestroyed()) {
    return;
  }

  RenderFrameParent* rfp = GetRenderFrame();
  RenderFrameParent* otherRfp = aOther->GetRenderFrame();
  if(!rfp || !otherRfp) {
    return;
  }

  CompositorParent::SwapLayerTreeObservers(rfp->GetLayersId(),
                                           otherRfp->GetLayersId());
}

bool
TabParent::RecvRemotePaintIsReady()
{
  nsCOMPtr<mozilla::dom::EventTarget> target = do_QueryInterface(mFrameElement);
  if (!target) {
    NS_WARNING("Could not locate target for MozAfterRemotePaint message.");
    return true;
  }

  nsRefPtr<Event> event = NS_NewDOMEvent(mFrameElement, nullptr, nullptr);
  event->InitEvent(NS_LITERAL_STRING("MozAfterRemotePaint"), false, false);
  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;
  bool dummy;
  mFrameElement->DispatchEvent(event, &dummy);
  return true;
}

mozilla::plugins::PPluginWidgetParent*
TabParent::AllocPPluginWidgetParent()
{
  return new mozilla::plugins::PluginWidgetParent();
}

bool
TabParent::DeallocPPluginWidgetParent(mozilla::plugins::PPluginWidgetParent* aActor)
{
  delete aActor;
  return true;
}

nsresult
TabParent::HandleEvent(nsIDOMEvent* aEvent)
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
  NS_IMETHOD GetAssociatedWindow(nsIDOMWindow**) NO_IMPL
  NS_IMETHOD GetTopWindow(nsIDOMWindow**) NO_IMPL
  NS_IMETHOD GetTopFrameElement(nsIDOMElement** aElement) override
  {
    nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(mElement);
    elem.forget(aElement);
    return NS_OK;
  }
  NS_IMETHOD GetNestedFrameId(uint64_t*) NO_IMPL
  NS_IMETHOD IsAppOfType(uint32_t, bool*) NO_IMPL
  NS_IMETHOD GetIsContent(bool*) NO_IMPL
  NS_IMETHOD GetUsePrivateBrowsing(bool*) NO_IMPL
  NS_IMETHOD SetUsePrivateBrowsing(bool) NO_IMPL
  NS_IMETHOD SetPrivateBrowsing(bool) NO_IMPL
  NS_IMETHOD GetIsInBrowserElement(bool*) NO_IMPL
  NS_IMETHOD GetAppId(uint32_t*) NO_IMPL
  NS_IMETHOD GetOriginAttributes(JS::MutableHandleValue) NO_IMPL
  NS_IMETHOD GetUseRemoteTabs(bool*) NO_IMPL
  NS_IMETHOD SetRemoteTabs(bool) NO_IMPL
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

bool
TabParent::RecvAsyncAuthPrompt(const nsCString& aUri,
                               const nsString& aRealm,
                               const uint64_t& aCallbackId)
{
  nsCOMPtr<nsIAuthPrompt2> authPrompt;
  GetAuthPrompt(nsIAuthPromptProvider::PROMPT_NORMAL,
                NS_GET_IID(nsIAuthPrompt2),
                getter_AddRefs(authPrompt));
  nsRefPtr<FakeChannel> channel = new FakeChannel(aUri, aCallbackId, mFrameElement);
  uint32_t promptFlags = nsIAuthInformation::AUTH_HOST;

  nsRefPtr<nsAuthInformationHolder> holder =
    new nsAuthInformationHolder(promptFlags, aRealm,
                                EmptyCString());

  uint32_t level = nsIAuthPrompt2::LEVEL_NONE;
  nsCOMPtr<nsICancelable> dummy;
  nsresult rv =
    authPrompt->AsyncPromptAuth(channel, channel, nullptr,
                                level, holder, getter_AddRefs(dummy));

  return rv == NS_OK;
}

bool
TabParent::RecvInvokeDragSession(nsTArray<IPCDataTransfer>&& aTransfers,
                                 const uint32_t& aAction,
                                 const nsCString& aVisualDnDData,
                                 const uint32_t& aWidth, const uint32_t& aHeight,
                                 const uint32_t& aStride, const uint8_t& aFormat,
                                 const int32_t& aDragAreaX, const int32_t& aDragAreaY)
{
  mInitialDataTransferItems.Clear();
  nsIPresShell* shell = mFrameElement->OwnerDoc()->GetShell();
  if (!shell) {
    if (Manager()->IsContentParent()) {
      unused << Manager()->AsContentParent()->SendEndDragSession(true, true);
    }
    return true;
  }

  EventStateManager* esm = shell->GetPresContext()->EventStateManager();
  for (uint32_t i = 0; i < aTransfers.Length(); ++i) {
    auto& items = aTransfers[i].items();
    nsTArray<DataTransferItem>* itemArray = mInitialDataTransferItems.AppendElement();
    for (uint32_t j = 0; j < items.Length(); ++j) {
      const IPCDataTransferItem& item = items[j];
      DataTransferItem* localItem = itemArray->AppendElement();
      localItem->mFlavor = item.flavor();
      if (item.data().type() == IPCDataTransferData::TnsString) {
        localItem->mType = DataTransferItem::DataType::eString;
        localItem->mStringData = item.data().get_nsString();
      } else if (item.data().type() == IPCDataTransferData::TPBlobChild) {
        localItem->mType = DataTransferItem::DataType::eBlob;
        BlobParent* blobParent =
          static_cast<BlobParent*>(item.data().get_PBlobParent());
        if (blobParent) {
          localItem->mBlobData = blobParent->GetBlobImpl();
        }
      }
    }
  }
  if (Manager()->IsContentParent()) {
    nsCOMPtr<nsIDragService> dragService =
      do_GetService("@mozilla.org/widget/dragservice;1");
    if (dragService) {
      dragService->MaybeAddChildProcess(Manager()->AsContentParent());
    }
  }

  if (aVisualDnDData.IsEmpty() ||
      (aVisualDnDData.Length() < aHeight * aStride)) {
    mDnDVisualization = nullptr;
  } else {
    mDnDVisualization =
      new mozilla::gfx::SourceSurfaceRawData();
    mozilla::gfx::SourceSurfaceRawData* raw =
      static_cast<mozilla::gfx::SourceSurfaceRawData*>(mDnDVisualization.get());
    raw->InitWrappingData(
      reinterpret_cast<uint8_t*>(const_cast<nsCString&>(aVisualDnDData).BeginWriting()),
      mozilla::gfx::IntSize(aWidth, aHeight), aStride,
      static_cast<mozilla::gfx::SurfaceFormat>(aFormat), false);
    raw->GuaranteePersistance();
  }
  mDragAreaX = aDragAreaX;
  mDragAreaY = aDragAreaY;

  esm->BeginTrackingRemoteDragGesture(mFrameElement);

  return true;
}

void
TabParent::AddInitialDnDDataTo(DataTransfer* aDataTransfer)
{
  for (uint32_t i = 0; i < mInitialDataTransferItems.Length(); ++i) {
    nsTArray<DataTransferItem>& itemArray = mInitialDataTransferItems[i];
    for (uint32_t j = 0; j < itemArray.Length(); ++j) {
      DataTransferItem& item = itemArray[j];
      nsRefPtr<nsVariant> variant = new nsVariant();
      // Special case kFilePromiseMime so that we get the right
      // nsIFlavorDataProvider for it.
      if (item.mFlavor.EqualsLiteral(kFilePromiseMime)) {
        nsRefPtr<nsISupports> flavorDataProvider =
          new nsContentAreaDragDropDataProvider();
        variant->SetAsISupports(flavorDataProvider);
      } else if (item.mType == DataTransferItem::DataType::eString) {
        variant->SetAsAString(item.mStringData);
      } else if (item.mType == DataTransferItem::DataType::eBlob) {
        variant->SetAsISupports(item.mBlobData);
      }
      // Using system principal here, since once the data is on parent process
      // side, it can be handled as being from browser chrome or OS.
      aDataTransfer->SetDataWithPrincipal(NS_ConvertUTF8toUTF16(item.mFlavor),
                                          variant, i,
                                          nsContentUtils::GetSystemPrincipal());
    }
  }
  mInitialDataTransferItems.Clear();
}

void
TabParent::TakeDragVisualization(RefPtr<mozilla::gfx::SourceSurface>& aSurface,
                                 int32_t& aDragAreaX, int32_t& aDragAreaY)
{
  aSurface = mDnDVisualization.forget();
  aDragAreaX = mDragAreaX;
  aDragAreaY = mDragAreaY;
}

bool
TabParent::AsyncPanZoomEnabled() const
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  return widget && widget->AsyncPanZoomEnabled();
}

NS_IMETHODIMP
TabParent::StartPersistence(uint64_t aOuterWindowID,
                            nsIWebBrowserPersistDocumentReceiver* aRecv)
{
  nsCOMPtr<nsIContentParent> manager = Manager();
  if (!manager->IsContentParent()) {
    return NS_ERROR_UNEXPECTED;
  }
  auto* actor = new WebBrowserPersistDocumentParent();
  actor->SetOnReady(aRecv);
  return manager->AsContentParent()
    ->SendPWebBrowserPersistDocumentConstructor(actor, this, aOuterWindowID)
    ? NS_OK : NS_ERROR_FAILURE;
  // (The actor will be destroyed on constructor failure.)
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
