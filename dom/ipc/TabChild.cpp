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

#ifdef MOZ_WIDGET_QT
#include <QtGui/QX11EmbedWidget>
#include <QApplication> 
#endif
#include "TabChild.h"

#include "nsIWebBrowser.h"
#include "nsEmbedCID.h"
#include "nsComponentManagerUtils.h"
#include "nsIBaseWindow.h"
#include "nsIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsThreadUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "mozilla/ipc/DocumentRendererChild.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsISupportsImpl.h"
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

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#endif

using namespace mozilla::dom;

#ifdef MOZ_WIDGET_QT
static QApplication *gQApp = nsnull;
extern int    gArgc;
extern char **gArgv;
#endif

NS_IMPL_ISUPPORTS1(ContentListener, nsIDOMEventListener)

NS_IMETHODIMP
ContentListener::HandleEvent(nsIDOMEvent* aEvent)
{
  RemoteDOMEvent remoteEvent;
  remoteEvent.mEvent = do_QueryInterface(aEvent);
  NS_ENSURE_STATE(remoteEvent.mEvent);
  mTabChild->SendsendEvent(remoteEvent);
  return NS_OK;
}

TabChild::TabChild()
{
    printf("creating %d!\n", NS_IsMainThread());
}

nsresult
TabChild::Init()
{
#ifdef MOZ_WIDGET_GTK2
  gtk_init(NULL, NULL);
#elif defined(MOZ_WIDGET_QT)
  if (!qApp)
    gQApp = new QApplication(gArgc, (char**)gArgv);
#endif

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

NS_IMPL_ISUPPORTS7(TabChild, nsIWebBrowserChrome, nsIWebBrowserChrome2,
                   nsIEmbeddingSiteWindow, nsIEmbeddingSiteWindow2,
                   nsIWebBrowserChromeFocus, nsIInterfaceRequestor,
                   nsIWindowProvider)

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
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::SetChromeFlags(PRUint32 aChromeFlags)
{
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
  SendmoveFocus(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::FocusPrevElement()
{
  SendmoveFocus(PR_FALSE);
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
                        PRBool aPositionSpecified, PRBool aSizeSpecified,
                        nsIURI* aURI, const nsAString& aName,
                        const nsACString& aFeatures, PRBool* aWindowIsNew,
                        nsIDOMWindow** aReturn)
{
    *aReturn = nsnull;

    PIFrameEmbeddingChild* newChild;
    if (!CallcreateWindow(&newChild)) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIDOMWindow> win =
        do_GetInterface(static_cast<TabChild*>(newChild)->mWebNav);
    win.forget(aReturn);
    return NS_OK;
}

bool
TabChild::RecvcreateWidget(const MagicWindowHandle& parentWidget)
{
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebNav);
    if (!baseWindow) {
        NS_ERROR("mWebNav doesn't QI to nsIBaseWindow");
        return true;
    }

#ifdef MOZ_WIDGET_GTK2
    GtkWidget* win = gtk_plug_new((GdkNativeWindow)parentWidget);
    gtk_widget_show(win);
#elif defined(MOZ_WIDGET_QT)
    QX11EmbedWidget *win = new QX11EmbedWidget();
    NS_ENSURE_TRUE(win, true);
    win->embedInto(parentWidget);
    win->show();
#elif defined(XP_WIN)
    HWND win = parentWidget;
#elif defined(XP_MACOSX)
#  warning IMPLEMENT ME
#else
#error You lose!
#endif

#if !defined(XP_MACOSX)
    baseWindow->InitWindow(win, 0, 0, 0, 0, 0);
    baseWindow->Create();
    baseWindow->SetVisibility(PR_TRUE);
#endif

    return InitTabChildGlobal();
}

bool
TabChild::destroyWidget()
{
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebNav);
    if (baseWindow)
        baseWindow->Destroy();

    return true;
}

TabChild::~TabChild()
{
#ifdef MOZ_WIDGET_QT 
    if (gQApp) 
      delete gQApp; 
    gQApp = nsnull; 
#endif
    destroyWidget();
    nsCOMPtr<nsIWebBrowser> webBrowser = do_QueryInterface(mWebNav);
    if (webBrowser) {
      webBrowser->SetContainerWindow(nsnull);
    }
    if (mCx) {
      nsIXPConnect* xpc = nsContentUtils::XPConnect();
      if (xpc) {
         xpc->ReleaseJSContext(mCx, PR_FALSE);
      } else {
        JS_DestroyContext(mCx);
      }
    }
}

bool
TabChild::RecvloadURL(const nsCString& uri)
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

bool
TabChild::Recvmove(const PRUint32& x,
                     const PRUint32& y,
                     const PRUint32& width,
                     const PRUint32& height)
{
    printf("[TabChild] MOVE to (x,y)=(%ud, %ud), (w,h)= (%ud, %ud)\n",
           x, y, width, height);

    nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mWebNav);
    baseWin->SetPositionAndSize(x, y, width, height, PR_TRUE);
    return true;
}

bool
TabChild::Recvactivate()
{
  nsCOMPtr<nsIWebBrowserFocus> browser = do_QueryInterface(mWebNav);
  browser->Activate();
  return true;
}

bool
TabChild::RecvsendMouseEvent(const nsString& aType,
                             const PRInt32&  aX,
                             const PRInt32&  aY,
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

mozilla::ipc::PDocumentRendererChild*
TabChild::AllocPDocumentRenderer(
        const PRInt32& x,
        const PRInt32& y,
        const PRInt32& w,
        const PRInt32& h,
        const nsString& bgcolor,
        const PRUint32& flags,
        const bool& flush)
{
    return new mozilla::ipc::DocumentRendererChild();
}

bool
TabChild::DeallocPDocumentRenderer(PDocumentRendererChild* actor)
{
    delete actor;
    return true;
}

bool
TabChild::RecvPDocumentRendererConstructor(
        mozilla::ipc::PDocumentRendererChild *__a,
        const PRInt32& aX,
        const PRInt32& aY,
        const PRInt32& aW,
        const PRInt32& aH,
        const nsString& bgcolor,
        const PRUint32& flags,
        const bool& flush)
{
    mozilla::ipc::DocumentRendererChild *render = 
        static_cast<mozilla::ipc::DocumentRendererChild *>(__a);

    nsCOMPtr<nsIWebBrowser> browser = do_QueryInterface(mWebNav);
    if (!browser)
        return true; // silently ignore
    nsCOMPtr<nsIDOMWindow> window;
    if (NS_FAILED(browser->GetContentDOMWindow(getter_AddRefs(window))) ||
        !window)
    {
        return true; // silently ignore
    }

    PRUint32 width, height;
    nsCString data;
    bool ret = render->RenderDocument(window, aX, aY, aW, aH, bgcolor, flags, flush,
                                      width, height, data);
    if (!ret)
        return true; // silently ignore

    return PDocumentRendererChild::Send__delete__(__a, width, height, data);
}

bool
TabChild::RecvactivateFrameEvent(const nsString& aType, const bool& capture)
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

bool
TabChild::RecvloadRemoteScript(const nsString& aURL)
{
  nsCString url = NS_ConvertUTF16toUTF8(aURL);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), url);
  NS_ENSURE_SUCCESS(rv, true);
  NS_NewChannel(getter_AddRefs(mChannel), uri);
  NS_ENSURE_TRUE(mChannel, true);

  nsCOMPtr<nsIInputStream> input;
  mChannel->Open(getter_AddRefs(input));
  nsString dataString;
  if (input) {
    const PRUint32 bufferSize = 256;
    char buffer[bufferSize];
    nsCString data;
    PRUint32 avail = 0;
    input->Available(&avail);
    PRUint32 read = 0;
    if (avail) {
      while (NS_SUCCEEDED(input->Read(buffer, bufferSize, &read)) && read) {
        data.Append(buffer, read);
        read = 0;
      }
    }
    nsScriptLoader::ConvertToUTF16(mChannel, (PRUint8*)data.get(), data.Length(),
                                   EmptyString(), nsnull, dataString);
  }

  if (!dataString.IsEmpty()) {
    JSAutoRequest ar(mCx);
    nsCOMPtr<nsPIDOMWindow> w = do_GetInterface(mWebNav);
    jsval retval;
    JSObject* global = nsnull;
    rv = mRootGlobal->GetJSObject(&global);
    NS_ENSURE_SUCCESS(rv, false);
    JSPrincipals* jsprin = nsnull;
    mPrincipal->GetJSPrincipals(mCx, &jsprin);

    nsContentUtils::XPConnect()->FlagSystemFilenamePrefix(url.get(), PR_TRUE);

    nsContentUtils::ThreadJSContextStack()->Push(mCx);
    JSBool ret = JS_EvaluateUCScriptForPrincipals(mCx, global, jsprin,
                                                  (jschar*)dataString.get(),
                                                  dataString.Length(),
                                                  url.get(), 1, &retval);
    JSContext *unused;
    nsContentUtils::ThreadJSContextStack()->Pop(&unused);
    NS_ENSURE_TRUE(ret, true); // This gives us a useful warning!
  }
  return true;
}

bool
TabChild::RecvsendAsyncMessageToChild(const nsString& aMessage,
                                      const nsString& aJSON)
{
  if (mTabChildGlobal) {
    nsTArray<nsString> dummy;
    static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get())->
      ReceiveMessage(static_cast<nsPIDOMEventTarget*>(mTabChildGlobal),
                     aMessage, PR_FALSE, aJSON, nsnull);
  }
  return true;
}


bool
TabChild::InitTabChildGlobal()
{
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
  JS_SetGCParameterForThread(cx, JSGC_MAX_CODE_CACHE_BYTES, 1 * 1024 * 1024);

  
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
                                         NS_GET_IID(nsISupports), flags,
                                         getter_AddRefs(mRootGlobal));
  NS_ENSURE_SUCCESS(rv, false);

  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
  NS_ENSURE_TRUE(root, false);
  root->SetParentTarget(scope);
  
  JSObject* global = nsnull;
  rv = mRootGlobal->GetJSObject(&global);
  NS_ENSURE_SUCCESS(rv, false);

  JS_SetGlobalObject(cx, global);

  return true;
}

bool SendSyncMessageToParent(void* aCallbackData,
                             const nsAString& aMessage,
                             const nsAString& aJSON,
                             nsTArray<nsString>* aJSONRetVal)
{
  return static_cast<TabChild*>(aCallbackData)->
    SendsendSyncMessageToParent(nsString(aMessage), nsString(aJSON), aJSONRetVal);
}

bool SendAsyncMessageToParent(void* aCallbackData,
                              const nsAString& aMessage,
                              const nsAString& aJSON)
{
  return static_cast<TabChild*>(aCallbackData)->
    SendsendAsyncMessageToParent(nsString(aMessage), nsString(aJSON));
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
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mTabChild->WebNavigation());
  window.swap(*aContent);
  return NS_OK;
}

JSContext*
TabChildGlobal::GetJSContextForEventHandlers()
{
  return mTabChild->GetJSContext();
}

nsIPrincipal* 
TabChildGlobal::GetPrincipal()
{
  return mTabChild->GetPrincipal();
}
