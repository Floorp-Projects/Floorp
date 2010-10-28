/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TabChild.h"
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
#include "nsIPrivateDOMEvent.h"
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
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsWeakReference.h"
#include "nsISecureBrowserUI.h"
#include "nsISSLStatusProvider.h"
#include "nsSerializationHelper.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIEventListenerManager.h"
#include "PCOMContentPermissionRequestChild.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::docshell;

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
  virtual bool Recv__delete__(const nsTArray<int>& aIntParams,
                              const nsTArray<nsString>& aStringParams);
};


TabChild::TabChild(PRUint32 aChromeFlags)
  : mRemoteFrame(nsnull)
  , mTabChildGlobal(nsnull)
  , mChromeFlags(aChromeFlags)
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
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow2)
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
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
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
  NS_ERROR("trying to SetChromeFlags from content process?");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::DestroyBrowserWindow()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::ShowAsModal()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::IsWindowModal(PRBool* aRetVal)
{
  *aRetVal = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::ExitModalEventLoop(nsresult aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetStatusWithContext(PRUint32 aStatusType,
                                    const nsAString& aStatusText,
                                    nsISupports* aStatusContext)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetDimensions(PRUint32 aFlags, PRInt32 aX, PRInt32 aY,
                             PRInt32 aCx, PRInt32 aCy)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetDimensions(PRUint32 aFlags, PRInt32* aX,
                             PRInt32* aY, PRInt32* aCx, PRInt32* aCy)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetFocus()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetVisibility(PRBool* aVisibility)
{
  *aVisibility = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetVisibility(PRBool aVisibility)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetTitle(PRUnichar** aTitle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetTitle(const PRUnichar* aTitle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetSiteWindow(void** aSiteWindow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::Blur()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::FocusNextElement()
{
  SendMoveFocus(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::FocusPrevElement()
{
  SendMoveFocus(PR_FALSE);
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
                        PRBool aCalledFromJS,
                        PRBool aPositionSpecified, PRBool aSizeSpecified,
                        nsIURI* aURI, const nsAString& aName,
                        const nsACString& aFeatures, PRBool* aWindowIsNew,
                        nsIDOMWindow** aReturn)
{
    *aReturn = nsnull;

    PBrowserChild* newChild;
    if (!CallCreateWindow(&newChild)) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIDOMWindow> win =
        do_GetInterface(static_cast<TabChild*>(newChild)->mWebNav);
    win.forget(aReturn);
    return NS_OK;
}

static nsInterfaceHashtable<nsVoidPtrHashKey, nsIDialogParamBlock> gActiveDialogs;

NS_IMETHODIMP
TabChild::OpenDialog(PRUint32 aType, const nsACString& aName,
                     const nsACString& aFeatures,
                     nsIDialogParamBlock* aArguments,
                     nsIDOMElement* aFrameElement)
{
  if (!gActiveDialogs.IsInitialized()) {
    NS_ENSURE_STATE(gActiveDialogs.Init());
  }
  nsTArray<PRInt32> intParams;
  nsTArray<nsString> stringParams;
  ParamsToArrays(aArguments, intParams, stringParams);
  PContentDialogChild* dialog =
    SendPContentDialogConstructor(aType, nsCString(aName),
                                  nsCString(aFeatures), intParams, stringParams);
  NS_ENSURE_STATE(gActiveDialogs.Put(dialog, aArguments));
  nsIThread *thread = NS_GetCurrentThread();
  while (gActiveDialogs.GetWeak(dialog)) {
    if (!NS_ProcessNextEvent(thread)) {
      break;
    }
  }
  return NS_OK;
}

bool
ContentDialogChild::Recv__delete__(const nsTArray<int>& aIntParams,
                                   const nsTArray<nsString>& aStringParams)
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
                         nsTArray<int>& aIntParams,
                         nsTArray<nsString>& aStringParams)
{
  if (aParams) {
    for (PRInt32 i = 0; i < 8; ++i) {
      PRInt32 val = 0;
      aParams->GetInt(i, &val);
      aIntParams.AppendElement(val);
    }
    PRInt32 j = 0;
    PRUnichar* str = nsnull;
    while (NS_SUCCEEDED(aParams->GetString(j, &str))) {
      nsAdoptingString strVal(str);
      aStringParams.AppendElement(strVal);
      ++j;
    }
  }
}

void
TabChild::ArraysToParams(const nsTArray<int>& aIntParams,
                         const nsTArray<nsString>& aStringParams,
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
  // The messageManager relays messages via the TabChild which
  // no longer exists.
  static_cast<nsFrameMessageManager*>
    (mTabChildGlobal->mMessageManager.get())->Disconnect();
  mTabChildGlobal->mMessageManager = nsnull;
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
    
    nsIEventListenerManager* elm = mTabChildGlobal->GetListenerManager(PR_FALSE);
    if (elm) {
      elm->Disconnect();
    }
    mTabChildGlobal->mTabChild = nsnull;
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

    return NS_SUCCEEDED(rv);
}

bool
TabChild::RecvShow(const nsIntSize& size)
{
    printf("[TabChild] SHOW (w,h)= (%d, %d)\n", size.width, size.height);

    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebNav);
    if (!baseWindow) {
        NS_ERROR("mWebNav doesn't QI to nsIBaseWindow");
        return false;
    }

    if (!InitWidget(size)) {
        return false;
    }

    baseWindow->InitWindow(0, mWidget,
                           0, 0, size.width, size.height);
    baseWindow->Create();
    baseWindow->SetVisibility(PR_TRUE);

    // IPC uses a WebBrowser object for which DNS prefetching is turned off
    // by default. But here we really want it, so enable it explicitly
    nsCOMPtr<nsIWebBrowserSetup> webBrowserSetup = do_QueryInterface(baseWindow);
    if (webBrowserSetup) {
      webBrowserSetup->SetProperty(nsIWebBrowserSetup::SETUP_ALLOW_DNS_PREFETCH,
                                   PR_TRUE);
    } else {
        NS_WARNING("baseWindow doesn't QI to nsIWebBrowserSetup, skipping "
                   "DNS prefetching enable step.");
    }

    return InitTabChildGlobal();
}

bool
TabChild::RecvMove(const nsIntSize& size)
{
    printf("[TabChild] RESIZE to (w,h)= (%ud, %ud)\n", size.width, size.height);

    mWidget->Resize(0, 0, size.width, size.height,
                    PR_TRUE);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mWebNav);
    baseWin->SetPositionAndSize(0, 0, size.width, size.height,
                                PR_TRUE);
    return true;
}

bool
TabChild::RecvActivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(mWebNav);
  browser->Activate();
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
TabChild::RecvKeyEvent(const nsString& aType,
                       const PRInt32& aKeyCode,
                       const PRInt32& aCharCode,
                       const PRInt32& aModifiers,
                       const bool& aPreventDefault)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  PRBool ignored = PR_FALSE;
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
                              const nsTArray<int>&,
                              const nsTArray<nsString>&)
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
    return false;

  LoadFrameScriptInternal(aURL);
  return true;
}

bool
TabChild::RecvAsyncMessage(const nsString& aMessage,
                           const nsString& aJSON)
{
  if (mTabChildGlobal) {
    static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get())->
      ReceiveMessage(static_cast<nsPIDOMEventTarget*>(mTabChildGlobal),
                     aMessage, PR_FALSE, aJSON, nsnull, nsnull);
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
      event->InitEvent(NS_LITERAL_STRING("unload"), PR_FALSE, PR_FALSE);
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
      privateEvent->SetTrusted(PR_TRUE);

      PRBool dummy;
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
  // Let the frame scripts know the child is being closed
  nsContentUtils::AddScriptRunner(
    new UnloadScriptEvent(this, mTabChildGlobal)
  );

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

  nsCOMPtr<nsIJSRuntimeService> runtimeSvc = 
    do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
  NS_ENSURE_TRUE(runtimeSvc, false);

  JSRuntime* rt = nsnull;
  runtimeSvc->GetRuntime(&rt);
  NS_ENSURE_TRUE(rt, false);

  JSContext* cx = JS_NewContext(rt, 8192);
  NS_ENSURE_TRUE(cx, false);

  mCx = cx;

  nsContentUtils::XPConnect()->SetSecurityManagerForJSContext(cx, nsContentUtils::GetSecurityManager(), 0);
  nsContentUtils::GetSecurityManager()->GetSystemPrincipal(getter_AddRefs(mPrincipal));

  PRUint32 stackDummy;
  jsuword stackLimit, currentStackAddr = (jsuword)&stackDummy;

  // 256k stack space.
  const jsuword kStackSize = 0x40000;

#if JS_STACK_GROWTH_DIRECTION < 0
  stackLimit = (currentStackAddr > kStackSize) ?
               currentStackAddr - kStackSize :
               0;
#else
  stackLimit = (currentStackAddr + kStackSize > currentStackAddr) ?
               currentStackAddr + kStackSize :
               (jsuword) -1;
#endif

  JS_SetThreadStackLimit(cx, stackLimit);
  JS_SetScriptStackQuota(cx, 100*1024*1024);

  JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_JIT | JSOPTION_ANONFUNFIX | JSOPTION_PRIVATE_IS_NSISUPPORTS);
  JS_SetVersion(cx, JSVERSION_LATEST);
  
  JSAutoRequest ar(cx);
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  const PRUint32 flags = nsIXPConnect::INIT_JS_STANDARD_CLASSES |
                         /*nsIXPConnect::OMIT_COMPONENTS_OBJECT ?  |*/
                         nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT;

  nsRefPtr<TabChildGlobal> scope = new TabChildGlobal(this);
  NS_ENSURE_TRUE(scope, false);

  mTabChildGlobal = scope;

  nsISupports* scopeSupports =
    NS_ISUPPORTS_CAST(nsPIDOMEventTarget*, scope);
  JS_SetContextPrivate(cx, scopeSupports);

  nsresult rv =
    xpc->InitClassesWithNewWrappedGlobal(cx, scopeSupports,
                                         NS_GET_IID(nsISupports),
                                         scope->GetPrincipal(), EmptyCString(),
                                         flags, getter_AddRefs(mGlobal));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
  NS_ENSURE_TRUE(root, false);
  root->SetParentTarget(scope);
  
  JSObject* global = nsnull;
  rv = mGlobal->GetJSObject(&global);
  NS_ENSURE_SUCCESS(rv, false);

  JS_SetGlobalObject(cx, global);
  DidCreateCx();
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
        nsnull                  // nsIDeviceContext
        );

    RenderFrameChild* remoteFrame =
        static_cast<RenderFrameChild*>(SendPRenderFrameConstructor());
    if (!remoteFrame) {
      NS_WARNING("failed to construct RenderFrame");
      return false;
    }

    NS_ABORT_IF_FALSE(0 == remoteFrame->ManagedPLayersChild().Length(),
                      "shouldn't have a shadow manager yet");
    PLayersChild* shadowManager = remoteFrame->SendPLayersConstructor();
    if (!shadowManager) {
      NS_WARNING("failed to construct LayersChild");
      // This results in |remoteFrame| being deleted.
      PRenderFrameChild::Send__delete__(remoteFrame);
      return false;
    }

    LayerManager* lm = mWidget->GetLayerManager();
    NS_ABORT_IF_FALSE(LayerManager::LAYERS_BASIC == lm->GetBackendType(),
                      "content processes should only be using BasicLayers");

    BasicShadowLayerManager* bslm = static_cast<BasicShadowLayerManager*>(lm);
    NS_ABORT_IF_FALSE(!bslm->HasShadowManager(),
                      "PuppetWidget shouldn't have shadow manager yet");
    bslm->SetShadowManager(shadowManager);

    mRemoteFrame = remoteFrame;
    return true;
}

static bool
SendSyncMessageToParent(void* aCallbackData,
                        const nsAString& aMessage,
                        const nsAString& aJSON,
                        nsTArray<nsString>* aJSONRetVal)
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
  mMessageManager = new nsFrameMessageManager(PR_FALSE,
                                              SendSyncMessageToParent,
                                              SendAsyncMessageToParent,
                                              nsnull,
                                              mTabChild,
                                              nsnull,
                                              aTabChild->GetJSContext());
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
TabChildGlobal::GetDocShell(nsIDocShell** aDocShell)
{
  *aDocShell = nsnull;
  if (!mTabChild)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mTabChild->WebNavigation());
  docShell.swap(*aDocShell);
  return NS_OK;
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

