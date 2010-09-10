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
#include "mozilla/ipc/DocumentRendererShmemChild.h"
#include "mozilla/ipc/DocumentRendererNativeIDChild.h"
#include "mozilla/dom/ExternalHelperAppChild.h"
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

#ifdef MOZ_WIDGET_QT
#include <QGraphicsView>
#include <QGraphicsWidget>
#endif

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#endif

using namespace mozilla::dom;

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
  : mTabChildGlobal(nsnull)
  , mChromeFlags(aChromeFlags)
{
    printf("creating %d!\n", NS_IsMainThread());
}

nsresult
TabChild::Init()
{
#ifdef MOZ_WIDGET_GTK2
  gtk_init(NULL, NULL);
#endif

  nsCOMPtr<nsIWebBrowser> webBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  if (!webBrowser) {
    NS_ERROR("Couldn't create a nsWebBrowser?");
    return NS_ERROR_FAILURE;
  }

  webBrowser->SetContainerWindow(this);
  nsCOMPtr<nsIWeakReference> weak =
    do_GetWeakReference(static_cast<nsSupportsWeakReference*>(this));
  webBrowser->AddWebBrowserListener(weak, NS_GET_IID(nsIWebProgressListener));

  mWebNav = do_QueryInterface(webBrowser);
  NS_ASSERTION(mWebNav, "nsWebBrowser doesn't implement nsIWebNavigation?");

  nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(mWebNav));
  docShellItem->SetItemType(nsIDocShellTreeItem::typeContentWrapper);
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(TabChild)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebProgressListener2)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow2)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener2)
  NS_INTERFACE_MAP_ENTRY(nsSupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITabChild)
  NS_INTERFACE_MAP_ENTRY(nsIDialogCreator)
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

bool
TabChild::RecvCreateWidget(const MagicWindowHandle& parentWidget)
{
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebNav);
    if (!baseWindow) {
        NS_ERROR("mWebNav doesn't QI to nsIBaseWindow");
        return true;
    }

#ifdef MOZ_WIDGET_GTK2
    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#elif defined(MOZ_WIDGET_QT)
    QGraphicsView *view = new QGraphicsView(new QGraphicsScene());
    NS_ENSURE_TRUE(view, false);
    QGraphicsWidget *win = new QGraphicsWidget();
    NS_ENSURE_TRUE(win, false);
    view->scene()->addItem(win);
#elif defined(XP_WIN)
    HWND win = parentWidget;
#elif defined(ANDROID)
    // Fake pointer to make baseWindow->InitWindow work
    // The android widget code is mostly disabled in the child process
    // so it won't choke on this
    void *win = (void *)0x1234;
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
TabChild::DestroyWidget()
{
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebNav);
    if (baseWindow)
        baseWindow->Destroy();

    return true;
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
    nsCOMPtr<nsIWeakReference> weak =
      do_GetWeakReference(static_cast<nsSupportsWeakReference*>(this));
    webBrowser->RemoveWebBrowserListener(weak, NS_GET_IID(nsIWebProgressListener));

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

NS_IMETHODIMP
TabChild::OnStateChange(nsIWebProgress *aWebProgress,
                        nsIRequest *aRequest,
                        PRUint32 aStateFlags,
                        nsresult aStatus)
{
  SendNotifyStateChange(aStateFlags, aStatus);
  return NS_OK;
}

// Only one of OnProgressChange / OnProgressChange64 will be called.
// According to interface, it should be OnProgressChange64, but looks
// like docLoader only sends the former.
NS_IMETHODIMP
TabChild::OnProgressChange(nsIWebProgress *aWebProgress,
                           nsIRequest *aRequest,
                           PRInt32 aCurSelfProgress,
                           PRInt32 aMaxSelfProgress,
                           PRInt32 aCurTotalProgress,
                           PRInt32 aMaxTotalProgress)
{
  SendNotifyProgressChange(aCurSelfProgress, aMaxSelfProgress,
                           aCurTotalProgress, aMaxTotalProgress);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnStatusChange(nsIWebProgress *aWebProgress,
                         nsIRequest *aRequest,
                         nsresult aStatus,
                         const PRUnichar* aMessage)
{
  nsDependentString message(aMessage);
  SendNotifyStatusChange(aStatus, message);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnSecurityChange(nsIWebProgress *aWebProgress,
                           nsIRequest *aRequest,
                           PRUint32 aState)
{
  nsCString secInfoAsString;
  if (aState & nsIWebProgressListener::STATE_IS_SECURE) {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    if (channel) {
      nsCOMPtr<nsISupports> secInfoSupports;
      channel->GetSecurityInfo(getter_AddRefs(secInfoSupports));

      nsCOMPtr<nsISerializable> secInfoSerializable =
          do_QueryInterface(secInfoSupports);
      NS_SerializeToString(secInfoSerializable, secInfoAsString);
    }
  }

  PRBool useSSLStatusObject = PR_FALSE;
  nsAutoString securityTooltip;
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aWebProgress);
  if (docShell) {
    nsCOMPtr<nsISecureBrowserUI> secureUI;
    docShell->GetSecurityUI(getter_AddRefs(secureUI));
    if (secureUI) {
      secureUI->GetTooltipText(securityTooltip);
      nsCOMPtr<nsISupports> supports;
      nsCOMPtr<nsISSLStatusProvider> provider = do_QueryInterface(secureUI);
      nsresult rv = provider->GetSSLStatus(getter_AddRefs(supports));
      if (NS_SUCCEEDED(rv) && supports) {
        /*
         * useSSLStatusObject: Security UI internally holds 4 states: secure, mixed,
         * broken, no security.  In cases of secure, mixed and broken it holds reference
         * to a valid SSL status object.  But, in case of the 'broken' state it doesn't
         * return the SSL status object (returns null), in contrary to the 'mixed' state
         * for which it returns.
         * 
         * However, mixed and broken states are both reported to the upper level
         * as nsIWebProgressListener::STATE_IS_BROKEN, i.e. states are merged,
         * so we cannot determine, if to return the status object or not.
         *
         * TabParent is extracting the SSL status object from the security info
         * serialization (string). SSL status object is always present there
         * even security UI implementation doesn't present it.  This argument 
         * tells the parent if the SSL status object is being presented by 
         * the security UI here, on the child process, and so if it has to be
         * presented also on the parent process.
         */
        useSSLStatusObject = PR_TRUE;
      }
    }
  }

  SendNotifySecurityChange(aState, useSSLStatusObject, securityTooltip,
                           secInfoAsString);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnLocationChange(nsIWebProgress *aWebProgress,
                           nsIRequest *aRequest,
                           nsIURI *aLocation)
{
  NS_ENSURE_ARG_POINTER(aLocation);
  nsCString uri;
  aLocation->GetSpec(uri);
  SendNotifyLocationChange(uri);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnProgressChange64(nsIWebProgress *aWebProgress,
                             nsIRequest *aRequest,
                             PRInt64 aCurSelfProgress,
                             PRInt64 aMaxSelfProgress,
                             PRInt64 aCurTotalProgress,
                             PRInt64 aMaxTotalProgress)
{
  SendNotifyProgressChange(aCurSelfProgress, aMaxSelfProgress,
                           aCurTotalProgress, aMaxTotalProgress);
  return NS_OK;
}

NS_IMETHODIMP
TabChild::OnRefreshAttempted(nsIWebProgress *aWebProgress,
                             nsIURI *aURI, PRInt32 aMillis,
                             PRBool aSameURL, PRBool *aRefreshAllowed)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsCString uri;
  aURI->GetSpec(uri);
  bool sameURL = aSameURL;
  bool refreshAllowed;
  SendRefreshAttempted(uri, aMillis, sameURL, &refreshAllowed);
  *aRefreshAllowed = refreshAllowed;
  return NS_OK;
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

bool
TabChild::RecvMove(const PRUint32& x,
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
TabChild::RecvQueryContentEvent(const nsQueryContentEvent& event)
{
  nsQueryContentEvent localEvent(event);
  DispatchWidgetEvent(localEvent);
  // Send result back even if query failed
  SendQueryContentResult(localEvent);
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
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
  NS_ENSURE_TRUE(window, false);

  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, false);

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, false);

  nsIFrame *frame = presShell->GetRootFrame();
  NS_ENSURE_TRUE(frame, false);

  nsIView *view = frame->GetView();
  NS_ENSURE_TRUE(view, false);

  nsCOMPtr<nsIWidget> widget = view->GetNearestWidget(nsnull);
  NS_ENSURE_TRUE(widget, false);

  nsEventStatus status;
  event.widget = widget;
  NS_ENSURE_SUCCESS(widget->DispatchEvent(&event, status), false);
  return true;
}

mozilla::ipc::PDocumentRendererChild*
TabChild::AllocPDocumentRenderer(const PRInt32& x,
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

mozilla::ipc::PDocumentRendererShmemChild*
TabChild::AllocPDocumentRendererShmem(
        const PRInt32& x,
        const PRInt32& y,
        const PRInt32& w,
        const PRInt32& h,
        const nsString& bgcolor,
        const PRUint32& flags,
        const bool& flush,
        const gfxMatrix& aMatrix,
        Shmem& buf)
{
    return new mozilla::ipc::DocumentRendererShmemChild();
}

bool
TabChild::DeallocPDocumentRendererShmem(PDocumentRendererShmemChild* actor)
{
    delete actor;
    return true;
}

bool
TabChild::RecvPDocumentRendererShmemConstructor(
        PDocumentRendererShmemChild *__a,
        const PRInt32& aX,
        const PRInt32& aY,
        const PRInt32& aW,
        const PRInt32& aH,
        const nsString& bgcolor,
        const PRUint32& flags,
        const bool& flush,
        const gfxMatrix& aMatrix,
        Shmem& aBuf)
{
    mozilla::ipc::DocumentRendererShmemChild *render = 
        static_cast<mozilla::ipc::DocumentRendererShmemChild *>(__a);

    nsCOMPtr<nsIWebBrowser> browser = do_QueryInterface(mWebNav);
    if (!browser)
        return true; // silently ignore
 
   nsCOMPtr<nsIDOMWindow> window;
    if (NS_FAILED(browser->GetContentDOMWindow(getter_AddRefs(window))) ||
        !window)
         return true; // silently ignore
 
    render->RenderDocument(window, aX, aY, aW, aH, bgcolor, flags, flush,
                           aMatrix, aBuf);

    gfxRect dirtyArea(0, 0, nsPresContext::AppUnitsToIntCSSPixels(aW), 
                      nsPresContext::AppUnitsToIntCSSPixels(aH));

    dirtyArea = aMatrix.Transform(dirtyArea);

    return PDocumentRendererShmemChild::Send__delete__(__a, dirtyArea.X(), dirtyArea.Y(), 
                                                       dirtyArea.Width(), dirtyArea.Height(),
                                                       aBuf);
}

mozilla::ipc::PDocumentRendererNativeIDChild*
TabChild::AllocPDocumentRendererNativeID(
        const PRInt32& x,
        const PRInt32& y,
        const PRInt32& w,
        const PRInt32& h,
        const nsString& bgcolor,
        const PRUint32& flags,
        const bool& flush,
        const gfxMatrix& aMatrix,
        const PRUint32& nativeID)
{
    return new mozilla::ipc::DocumentRendererNativeIDChild();
}

bool
TabChild::DeallocPDocumentRendererNativeID(PDocumentRendererNativeIDChild* actor)
{
    delete actor;
    return true;
}

bool
TabChild::RecvPDocumentRendererNativeIDConstructor(
        PDocumentRendererNativeIDChild *__a,
        const PRInt32& aX,
        const PRInt32& aY,
        const PRInt32& aW,
        const PRInt32& aH,
        const nsString& bgcolor,
        const PRUint32& flags,
        const bool& flush,
        const gfxMatrix& aMatrix,
        const PRUint32& aNativeID)
{
    mozilla::ipc::DocumentRendererNativeIDChild* render =
        static_cast<mozilla::ipc::DocumentRendererNativeIDChild*>(__a);

    nsCOMPtr<nsIWebBrowser> browser = do_QueryInterface(mWebNav);
    if (!browser)
        return true; // silently ignore

    nsCOMPtr<nsIDOMWindow> window;
    if (NS_FAILED(browser->GetContentDOMWindow(getter_AddRefs(window))) ||
        !window)
        return true; // silently ignore

    render->RenderDocument(window, aX, aY, aW, aH, bgcolor, flags, flush,
                           aMatrix, aNativeID);

    gfxRect dirtyArea(0, 0, nsPresContext::AppUnitsToIntCSSPixels(aW),
                      nsPresContext::AppUnitsToIntCSSPixels(aH));

    dirtyArea = aMatrix.Transform(dirtyArea);

    return PDocumentRendererNativeIDChild::Send__delete__(__a, dirtyArea.X(), dirtyArea.Y(),
                                                          dirtyArea.Width(), dirtyArea.Height(),
                                                          aNativeID);
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
  DestroyWidget();

  return Send__delete__(this);
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

PExternalHelperAppChild*
TabChild::AllocPExternalHelperApp(const IPC::URI& uri,
                                  const nsCString& aMimeContentType,
                                  const bool& aForceSave,
                                  const PRInt64& aContentLength)
{
  ExternalHelperAppChild *child = new ExternalHelperAppChild();
  child->AddRef();
  return child;
}

bool
TabChild::DeallocPExternalHelperApp(PExternalHelperAppChild* aService)
{
  ExternalHelperAppChild *child = static_cast<ExternalHelperAppChild*>(aService);
  child->Release();
  return true;
}


