/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TabChild.h"
#include "mozilla/IntentionalCrash.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/PContentDialogChild.h"
#include "mozilla/layers/PLayersChild.h"
#include "mozilla/layout/RenderFrameChild.h"
#include "mozilla/docshell/OfflineCacheUpdateChild.h"

#include "BasicLayers.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserSetup.h"
#include "nsEmbedCID.h"
#include "nsComponentManagerUtils.h"
#include "nsIBaseWindow.h"
#include "nsIDOMWindow.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsThreadUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "mozilla/ipc/DocumentRendererChild.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsISupportsImpl.h"
#include "nsIURI.h"
#include "nsIWebBrowserFocus.h"
#include "nsIDOMEvent.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIJSRuntimeService.h"
#include "nsContentUtils.h"
#include "nsIDOMClassInfo.h"
#include "nsIXPCSecurityManager.h"
#include "nsIJSContextStack.h"
#include "nsComponentManagerUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsScriptLoader.h"
#include "nsPIWindowRoot.h"
#include "nsIScriptContext.h"
#include "nsInterfaceHashtable.h"
#include "nsPresContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsWeakReference.h"
#include "nsISecureBrowserUI.h"
#include "nsISSLStatusProvider.h"
#include "nsSerializationHelper.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsEventListenerManager.h"
#include "PCOMContentPermissionRequestChild.h"
#include "xpcpublic.h"
#include "IndexedDBChild.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::docshell;
using namespace mozilla::dom::indexedDB;

NS_IMPL_ISUPPORTS1(ContentListener, nsIDOMEventListener)

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


TabChild::TabChild(PRUint32 aChromeFlags, bool aIsBrowserFrame)
  : mRemoteFrame(nsnull)
  , mTabChildGlobal(nsnull)
  , mChromeFlags(aChromeFlags)
  , mOuterRect(0, 0, 0, 0)
  , mLastBackgroundColor(NS_RGB(255, 255, 255))
  , mDidFakeShow(false)
  , mIsBrowserFrame(aIsBrowserFrame)
{
    printf("creating %d!\n", NS_IsMainThread());
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
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(TabChild)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
  NS_INTERFACE_MAP_ENTRY(nsITabChild)
  NS_INTERFACE_MAP_ENTRY(nsIDialogCreator)
  NS_INTERFACE_MAP_ENTRY(nsSupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(TabChild)
NS_IMPL_RELEASE(TabChild)

NS_IMETHODIMP
TabChild::SetStatus(PRUint32 aStatusType, const PRUnichar* aStatus)
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
TabChild::GetChromeFlags(PRUint32* aChromeFlags)
{
  *aChromeFlags = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetChromeFlags(PRUint32 aChromeFlags)
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
TabChild::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
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
TabChild::SetStatusWithContext(PRUint32 aStatusType,
                                    const nsAString& aStatusText,
                                    nsISupports* aStatusContext)
{
  // FIXME/bug 617804: should the platform support this?
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetDimensions(PRUint32 aFlags, PRInt32 aX, PRInt32 aY,
                             PRInt32 aCx, PRInt32 aCy)
{
  NS_NOTREACHED("TabChild::SetDimensions not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetDimensions(PRUint32 aFlags, PRInt32* aX,
                             PRInt32* aY, PRInt32* aCx, PRInt32* aCy)
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
TabChild::ProvideWindow(nsIDOMWindow* aParent, PRUint32 aChromeFlags,
                        bool aCalledFromJS,
                        bool aPositionSpecified, bool aSizeSpecified,
                        nsIURI* aURI, const nsAString& aName,
                        const nsACString& aFeatures, bool* aWindowIsNew,
                        nsIDOMWindow** aReturn)
{
    *aReturn = nsnull;

    // If aParent is inside an <iframe mozbrowser> and this isn't a request to
    // open a modal-type window, we're going to create a new <iframe mozbrowser>
    // and return its window here.
    nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
    bool inBrowserFrame = false;
    if (docshell) {
      docshell->GetContainedInBrowserFrame(&inBrowserFrame);
    }

    if (inBrowserFrame &&
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
  *aReturn = nsnull;

  nsRefPtr<TabChild> newChild =
    static_cast<TabChild*>(Manager()->SendPBrowserConstructor(
      /* aChromeFlags = */ 0,
      /* aIsBrowserFrame = */ true));

  nsCAutoString spec;
  aURI->GetSpec(spec);

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

static nsInterfaceHashtable<nsPtrHashKey<PContentDialogChild>, nsIDialogParamBlock> gActiveDialogs;

NS_IMETHODIMP
TabChild::OpenDialog(PRUint32 aType, const nsACString& aName,
                     const nsACString& aFeatures,
                     nsIDialogParamBlock* aArguments,
                     nsIDOMElement* aFrameElement)
{
  if (!gActiveDialogs.IsInitialized()) {
    gActiveDialogs.Init();
  }
  InfallibleTArray<PRInt32> intParams;
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
    for (PRInt32 i = 0; i < 8; ++i) {
      PRInt32 val = 0;
      aParams->GetInt(i, &val);
      aIntParams.AppendElement(val);
    }
    PRInt32 j = 0;
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
    for (PRInt32 i = 0; PRUint32(i) < aIntParams.Length(); ++i) {
      aParams->SetInt(i, aIntParams[i]);
    }
    for (PRInt32 j = 0; PRUint32(j) < aStringParams.Length(); ++j) {
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
        mRemoteFrame = nsnull;
    }
}

void
TabChild::ActorDestroy(ActorDestroyReason why)
{
  if (mTabChildGlobal) {
    // The messageManager relays messages via the TabChild which
    // no longer exists.
    static_cast<nsFrameMessageManager*>
      (mTabChildGlobal->mMessageManager.get())->Disconnect();
    mTabChildGlobal->mMessageManager = nsnull;
  }
}

TabChild::~TabChild()
{
    nsCOMPtr<nsIWebBrowser> webBrowser = do_QueryInterface(mWebNav);
    if (webBrowser) {
      webBrowser->SetContainerWindow(nsnull);
    }
    if (mCx) {
      DestroyCx();
    }
    
    if (mTabChildGlobal) {
      nsEventListenerManager* elm = mTabChildGlobal->GetListenerManager(false);
      if (elm) {
        elm->Disconnect();
      }
      mTabChildGlobal->mTabChild = nsnull;
    }
}

bool
TabChild::RecvLoadURL(const nsCString& uri)
{
    printf("loading %s, %d\n", uri.get(), NS_IsMainThread());

    nsresult rv = mWebNav->LoadURI(NS_ConvertUTF8toUTF16(uri).get(),
                                   nsIWebNavigation::LOAD_FLAGS_NONE,
                                   NULL, NULL, NULL);
    if (NS_FAILED(rv)) {
        NS_WARNING("mWebNav->LoadURI failed. Eating exception, what else can I do?");
    }

    return true;
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

    if (!InitWidget(size)) {
        // We can fail to initialize our widget if the <browser
        // remote> has already been destroyed, and we couldn't hook
        // into the parent-process's layer system.  That's not a fatal
        // error.
        return true;
    }

    baseWindow->InitWindow(0, mWidget,
                           0, 0, size.width, size.height);
    baseWindow->Create();
    baseWindow->SetVisibility(true);

    // IPC uses a WebBrowser object for which DNS prefetching is turned off
    // by default. But here we really want it, so enable it explicitly
    nsCOMPtr<nsIWebBrowserSetup> webBrowserSetup = do_QueryInterface(baseWindow);
    if (webBrowserSetup) {
      webBrowserSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_DNS_PREFETCH,
                                   true);
    } else {
        NS_WARNING("baseWindow doesn't QI to nsIWebBrowserSetup, skipping "
                   "DNS prefetching enable step.");
    }

    return InitTabChildGlobal();
}

bool
TabChild::RecvUpdateDimensions(const nsRect& rect, const nsIntSize& size)
{
#ifdef DEBUG
    printf("[TabChild] Update Dimensions to (x,y,w,h)= (%ud, %ud, %ud, %ud) and move to (w,h)= (%ud, %ud)\n", rect.x, rect.y, rect.width, rect.height, size.width, size.height);
#endif

    if (!mRemoteFrame) {
        return true;
    }

    mOuterRect.x = rect.x;
    mOuterRect.y = rect.y;
    mOuterRect.width = rect.width;
    mOuterRect.height = rect.height;

    mWidget->Resize(0, 0, size.width, size.height,
                    true);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mWebNav);
    baseWin->SetPositionAndSize(0, 0, size.width, size.height,
                                true);

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
                         const PRInt32&  aButton,
                         const PRInt32&  aClickCount,
                         const PRInt32&  aModifiers,
                         const bool&     aIgnoreRootScrollFrame)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  utils->SendMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
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
TabChild::RecvMouseScrollEvent(const nsMouseScrollEvent& event)
{
  nsMouseScrollEvent localEvent(event);
  DispatchWidgetEvent(localEvent);
  return true;
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
                       const PRInt32& aKeyCode,
                       const PRInt32& aCharCode,
                       const PRInt32& aModifiers,
                       const bool& aPreventDefault)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
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

bool
TabChild::DispatchWidgetEvent(nsGUIEvent& event)
{
  if (!mWidget)
    return false;

  nsEventStatus status;
  event.widget = mWidget;
  NS_ENSURE_SUCCESS(mWidget->DispatchEvent(&event, status), false);
  return true;
}

PDocumentRendererChild*
TabChild::AllocPDocumentRenderer(const nsRect& documentRect,
                                 const gfxMatrix& transform,
                                 const nsString& bgcolor,
                                 const PRUint32& renderFlags,
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
                                           const PRUint32& renderFlags,
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
TabChild::AllocPContentDialog(const PRUint32&,
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
TabChild::AllocPContentPermissionRequest(const nsCString& aType, const IPC::URI&)
{
  NS_RUNTIMEABORT("unused");
  return nsnull;
}

bool
TabChild::DeallocPContentPermissionRequest(PContentPermissionRequestChild* actor)
{
    static_cast<PCOMContentPermissionRequestChild*>(actor)->IPDLRelease();
    return true;
}

bool
TabChild::RecvActivateFrameEvent(const nsString& aType, const bool& capture)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
  NS_ENSURE_TRUE(window, true);
  nsCOMPtr<nsIDOMEventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, true);
  nsRefPtr<ContentListener> listener = new ContentListener(this);
  NS_ENSURE_TRUE(listener, true);
  chromeHandler->AddEventListener(aType, listener, capture);
  return true;
}

POfflineCacheUpdateChild*
TabChild::AllocPOfflineCacheUpdate(const URI& manifestURI,
            const URI& documentURI,
            const nsCString& clientID,
            const bool& stickDocument)
{
  NS_RUNTIMEABORT("unused");
  return nsnull;
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
                           const nsString& aJSON)
{
  if (mTabChildGlobal) {
    nsFrameScriptCx cx(static_cast<nsIWebBrowserChrome*>(this), this);
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    mm->ReceiveMessage(static_cast<nsIDOMEventTarget*>(mTabChildGlobal),
                       aMessage, false, aJSON, nsnull, nsnull);
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
    NS_NewDOMEvent(getter_AddRefs(event), nsnull, nsnull);
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

  // XXX what other code in ~TabChild() should we be running here?
  DestroyWindow();

  return Send__delete__(this);
}

PRenderFrameChild*
TabChild::AllocPRenderFrame()
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
TabChild::InitTabChildGlobal()
{
  if (mCx && mTabChildGlobal)
    return true;

  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
  NS_ENSURE_TRUE(window, false);
  nsCOMPtr<nsIDOMEventTarget> chromeHandler =
    do_QueryInterface(window->GetChromeEventHandler());
  NS_ENSURE_TRUE(chromeHandler, false);

  nsRefPtr<TabChildGlobal> scope = new TabChildGlobal(this);
  NS_ENSURE_TRUE(scope, false);

  mTabChildGlobal = scope;

  nsISupports* scopeSupports =
    NS_ISUPPORTS_CAST(nsIDOMEventTarget*, scope);
  
  NS_ENSURE_TRUE(InitTabChildGlobalInternal(scopeSupports), false); 

  scope->Init();

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
  NS_ENSURE_TRUE(root, false);
  root->SetParentTarget(scope);

  // Initialize the child side of the browser element machinery, if appropriate.
  if (mIsBrowserFrame) {
    RecvLoadRemoteScript(
      NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js"));
  }

  return true;
}

bool
TabChild::InitWidget(const nsIntSize& size)
{
    NS_ABORT_IF_FALSE(!mWidget && !mRemoteFrame, "CreateWidget twice?");

    mWidget = nsIWidget::CreatePuppetWidget(this);
    if (!mWidget) {
        NS_ERROR("couldn't create fake widget");
        return false;
    }
    mWidget->Create(
        nsnull, 0,              // no parents
        nsIntRect(nsIntPoint(0, 0), size),
        nsnull,                 // HandleWidgetEvent
        nsnull                  // nsDeviceContext
        );

    RenderFrameChild* remoteFrame =
        static_cast<RenderFrameChild*>(SendPRenderFrameConstructor());
    if (!remoteFrame) {
      NS_WARNING("failed to construct RenderFrame");
      return false;
    }

    NS_ABORT_IF_FALSE(0 == remoteFrame->ManagedPLayersChild().Length(),
                      "shouldn't have a shadow manager yet");
    LayerManager::LayersBackend be;
    PRInt32 maxTextureSize;
    PLayersChild* shadowManager = remoteFrame->SendPLayersConstructor(&be, &maxTextureSize);
    if (!shadowManager) {
      NS_WARNING("failed to construct LayersChild");
      // This results in |remoteFrame| being deleted.
      PRenderFrameChild::Send__delete__(remoteFrame);
      return false;
    }

    ShadowLayerForwarder* lf =
        mWidget->GetLayerManager(shadowManager, be)->AsShadowForwarder();
    NS_ABORT_IF_FALSE(lf && lf->HasShadowManager(),
                      "PuppetWidget should have shadow manager");
    lf->SetParentBackendType(be);
    lf->SetMaxTextureSize(maxTextureSize);

    mRemoteFrame = remoteFrame;
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

NS_IMETHODIMP
TabChild::GetMessageManager(nsIContentFrameMessageManager** aResult)
{
  if (mTabChildGlobal) {
    NS_ADDREF(*aResult = mTabChildGlobal);
    return NS_OK;
  }
  *aResult = nsnull;
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

static bool
SendSyncMessageToParent(void* aCallbackData,
                        const nsAString& aMessage,
                        const nsAString& aJSON,
                        InfallibleTArray<nsString>* aJSONRetVal)
{
  return static_cast<TabChild*>(aCallbackData)->
    SendSyncMessage(nsString(aMessage), nsString(aJSON),
                    aJSONRetVal);
}

static bool
SendAsyncMessageToParent(void* aCallbackData,
                         const nsAString& aMessage,
                         const nsAString& aJSON)
{
  return static_cast<TabChild*>(aCallbackData)->
    SendAsyncMessage(nsString(aMessage), nsString(aJSON));
}

TabChildGlobal::TabChildGlobal(TabChild* aTabChild)
: mTabChild(aTabChild)
{
}

void
TabChildGlobal::Init()
{
  NS_ASSERTION(!mMessageManager, "Re-initializing?!?");
  mMessageManager = new nsFrameMessageManager(false,
                                              SendSyncMessageToParent,
                                              SendAsyncMessageToParent,
                                              nsnull,
                                              mTabChild,
                                              nsnull,
                                              mTabChild->GetJSContext());
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TabChildGlobal)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TabChildGlobal,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mMessageManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TabChildGlobal,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mMessageManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TabChildGlobal)
  NS_INTERFACE_MAP_ENTRY(nsIFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsISyncMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptContextPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentFrameMessageManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TabChildGlobal, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TabChildGlobal, nsDOMEventTargetHelper)

NS_IMETHODIMP
TabChildGlobal::GetContent(nsIDOMWindow** aContent)
{
  *aContent = nsnull;
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
  *aDocShell = nsnull;
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
    return nsnull;
  return mTabChild->GetJSContext();
}

nsIPrincipal* 
TabChildGlobal::GetPrincipal()
{
  if (!mTabChild)
    return nsnull;
  return mTabChild->GetPrincipal();
}
