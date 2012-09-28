/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "TabChild.h"

#include "BasicLayers.h"
#include "Blob.h"
#include "ContentChild.h"
#include "IndexedDBChild.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/IntentionalCrash.h"
#include "mozilla/docshell/OfflineCacheUpdateChild.h"
#include "mozilla/dom/PContentChild.h"
#include "mozilla/dom/PContentDialogChild.h"
#include "mozilla/ipc/DocumentRendererChild.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/PLayersChild.h"
#include "mozilla/layout/RenderFrameChild.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozIApplication.h"
#include "nsComponentManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsEmbedCID.h"
#include "nsEventListenerManager.h"
#include "nsIAppsService.h"
#include "nsIBaseWindow.h"
#include "nsIComponentManager.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsISSLStatusProvider.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsISecureBrowserUI.h"
#include "nsIServiceManager.h"
#include "nsISupportsImpl.h"
#include "nsIURI.h"
#include "nsIView.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWebBrowserSetup.h"
#include "nsIWebProgress.h"
#include "nsIXPCSecurityManager.h"
#include "nsInterfaceHashtable.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"
#include "nsScriptLoader.h"
#include "nsSerializationHelper.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"
#include "PCOMContentPermissionRequestChild.h"
#include "PuppetWidget.h"
#include "StructuredCloneUtils.h"
#include "xpcpublic.h"

#define BROWSER_ELEMENT_CHILD_SCRIPT \
    NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js")

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::docshell;
using namespace mozilla::dom::indexedDB;
using namespace mozilla::widget;

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

StaticRefPtr<TabChild> sPreallocatedTab;

/*static*/ void
TabChild::PreloadSlowThings()
{
    MOZ_ASSERT(!sPreallocatedTab);

    nsRefPtr<TabChild> tab(new TabChild(0, false,
                                        nsIScriptSecurityManager::NO_APP_ID));
    if (!NS_SUCCEEDED(tab->Init()) ||
        !tab->InitTabChildGlobal(DONT_LOAD_SCRIPTS)) {
        return;
    }
    tab->TryCacheLoadAndCompileScript(BROWSER_ELEMENT_CHILD_SCRIPT);

    sPreallocatedTab = tab;
    ClearOnShutdown(&sPreallocatedTab);
}

/*static*/ already_AddRefed<TabChild>
TabChild::Create(uint32_t aChromeFlags,
                 bool aIsBrowserElement, uint32_t aAppId)
{
    if (sPreallocatedTab &&
        sPreallocatedTab->mChromeFlags == aChromeFlags &&
        (aIsBrowserElement || 
         aAppId != nsIScriptSecurityManager::NO_APP_ID)) {
        nsRefPtr<TabChild> child = sPreallocatedTab.get();
        sPreallocatedTab = nullptr;

        MOZ_ASSERT(!child->mTriedBrowserInit);

        child->SetAppBrowserConfig(aIsBrowserElement, aAppId);

        return child.forget();
    }

    nsRefPtr<TabChild> iframe = new TabChild(aChromeFlags, aIsBrowserElement,
                                             aAppId);
    return NS_SUCCEEDED(iframe->Init()) ? iframe.forget() : nullptr;
}


TabChild::TabChild(uint32_t aChromeFlags, bool aIsBrowserElement,
                   uint32_t aAppId)
  : mRemoteFrame(nullptr)
  , mTabChildGlobal(nullptr)
  , mChromeFlags(aChromeFlags)
  , mOuterRect(0, 0, 0, 0)
  , mLastBackgroundColor(NS_RGB(255, 255, 255))
  , mAppId(aAppId)
  , mDidFakeShow(false)
  , mIsBrowserElement(aIsBrowserElement)
  , mNotified(false)
  , mTriedBrowserInit(false)
{
    printf("creating %d!\n", NS_IsMainThread());
}

nsresult
TabChild::Observe(nsISupports *aSubject,
                  const char *aTopic,
                  const PRUnichar *aData)
{
  if (!strcmp(aTopic, "cancel-default-pan-zoom")) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aSubject));
    nsCOMPtr<nsITabChild> tabChild(GetTabChildFrom(docShell));
    if (tabChild == this) {
      mRemoteFrame->CancelDefaultPanZoom();
    }
  } else if (!strcmp(aTopic, "browser-zoom-to-rect")) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aSubject));
    nsCOMPtr<nsITabChild> tabChild(GetTabChildFrom(docShell));
    if (tabChild == this) {
      gfxRect rect;
      sscanf(NS_ConvertUTF16toUTF8(aData).get(),
             "{\"x\":%lf,\"y\":%lf,\"w\":%lf,\"h\":%lf}",
             &rect.x, &rect.y, &rect.width, &rect.height);
      SendZoomToRect(rect);
    }
  }

  return NS_OK;
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
  
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mWebNav);
  if (!baseWindow) {
    NS_ERROR("mWebNav doesn't QI to nsIBaseWindow");
    return NS_ERROR_FAILURE;
  }

  mWidget = nsIWidget::CreatePuppetWidget(this);
  if (!mWidget) {
    NS_ERROR("couldn't create fake widget");
    return NS_ERROR_FAILURE;
  }
  mWidget->Create(
    nullptr, 0,              // no parents
    nsIntRect(nsIntPoint(0, 0), nsIntSize(0, 0)),
    nullptr,                 // HandleWidgetEvent
    nullptr                  // nsDeviceContext
  );

  baseWindow->InitWindow(0, mWidget, 0, 0, 0, 0);
  baseWindow->Create();

  SetAppBrowserConfig(mIsBrowserElement, mAppId);

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

  return NS_OK;
}

void
TabChild::SetAppBrowserConfig(bool aIsBrowserElement, uint32_t aAppId)
{
    mIsBrowserElement = aIsBrowserElement;
    mAppId = aAppId;

    nsCOMPtr<nsIDocShell> docShell = do_GetInterface(mWebNav);
    MOZ_ASSERT(docShell);

    if (docShell) {
        docShell->SetAppId(mAppId);
        if (mIsBrowserElement) {
            docShell->SetIsBrowserElement();
        }
    }
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
TabChild::SetStatus(uint32_t aStatusType, const PRUnichar* aStatus)
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
TabChild::GetChromeFlags(uint32_t* aChromeFlags)
{
  *aChromeFlags = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetChromeFlags(uint32_t aChromeFlags)
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
TabChild::SizeBrowserTo(int32_t aCX, int32_t aCY)
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
TabChild::SetStatusWithContext(uint32_t aStatusType,
                                    const nsAString& aStatusText,
                                    nsISupports* aStatusContext)
{
  // FIXME/bug 617804: should the platform support this?
  return NS_OK;
}

NS_IMETHODIMP
TabChild::SetDimensions(uint32_t aFlags, int32_t aX, int32_t aY,
                             int32_t aCx, int32_t aCy)
{
  NS_NOTREACHED("TabChild::SetDimensions not supported in TabChild");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
TabChild::GetDimensions(uint32_t aFlags, int32_t* aX,
                             int32_t* aY, int32_t* aCx, int32_t* aCy)
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
TabChild::ProvideWindow(nsIDOMWindow* aParent, uint32_t aChromeFlags,
                        bool aCalledFromJS,
                        bool aPositionSpecified, bool aSizeSpecified,
                        nsIURI* aURI, const nsAString& aName,
                        const nsACString& aFeatures, bool* aWindowIsNew,
                        nsIDOMWindow** aReturn)
{
    *aReturn = nullptr;

    // If aParent is inside an <iframe mozbrowser> and this isn't a request to
    // open a modal-type window, we're going to create a new <iframe mozbrowser>
    // and return its window here.
    nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
    if (docshell && docshell->GetIsBelowContentBoundary() &&
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
  *aReturn = nullptr;

  uint32_t chromeFlags = 0;
  nsRefPtr<TabChild> newChild = new TabChild(chromeFlags,
                                             mIsBrowserElement, mAppId);
  if (!NS_SUCCEEDED(newChild->Init())) {
      return NS_ERROR_ABORT;
  }
  unused << Manager()->SendPBrowserConstructor(
      // We release this ref in DeallocPBrowserChild
      nsRefPtr<TabChild>(newChild).forget().get(),
      chromeFlags, mIsBrowserElement, this);
  nsAutoCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }

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
TabChild::OpenDialog(uint32_t aType, const nsACString& aName,
                     const nsACString& aFeatures,
                     nsIDialogParamBlock* aArguments,
                     nsIDOMElement* aFrameElement)
{
  if (!gActiveDialogs.IsInitialized()) {
    gActiveDialogs.Init();
  }
  InfallibleTArray<int32_t> intParams;
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
    for (int32_t i = 0; i < 8; ++i) {
      int32_t val = 0;
      aParams->GetInt(i, &val);
      aIntParams.AppendElement(val);
    }
    int32_t j = 0;
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
    for (int32_t i = 0; uint32_t(i) < aIntParams.Length(); ++i) {
      aParams->SetInt(i, aIntParams[i]);
    }
    for (int32_t j = 0; uint32_t(j) < aStringParams.Length(); ++j) {
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
        mRemoteFrame = nullptr;
    }
}

bool
TabChild::UseDirectCompositor()
{
    return !!CompositorChild::Get();
}

void
TabChild::ActorDestroy(ActorDestroyReason why)
{
  if (mTabChildGlobal) {
    // The messageManager relays messages via the TabChild which
    // no longer exists.
    static_cast<nsFrameMessageManager*>
      (mTabChildGlobal->mMessageManager.get())->Disconnect();
    mTabChildGlobal->mMessageManager = nullptr;
  }
}

TabChild::~TabChild()
{
    DestroyWindow();

    nsCOMPtr<nsIWebBrowser> webBrowser = do_QueryInterface(mWebNav);
    if (webBrowser) {
      webBrowser->SetContainerWindow(nullptr);
    }
    if (mCx) {
      DestroyCx();
    }
    
    if (mTabChildGlobal) {
      nsEventListenerManager* elm = mTabChildGlobal->GetListenerManager(false);
      if (elm) {
        elm->Disconnect();
      }
      mTabChildGlobal->mTabChild = nullptr;
    }
}

void
TabChild::SetProcessNameToAppName()
{
  if (mIsBrowserElement || (mAppId == nsIScriptSecurityManager::NO_APP_ID)) {
    return;
  }
  nsCOMPtr<nsIAppsService> appsService =
    do_GetService(APPS_SERVICE_CONTRACTID);
  if (!appsService) {
    NS_WARNING("No AppsService");
    return;
  }
  nsresult rv;
  nsCOMPtr<mozIDOMApplication> domApp;
  rv = appsService->GetAppByLocalId(mAppId, getter_AddRefs(domApp));
  if (NS_FAILED(rv) || !domApp) {
    NS_WARNING("GetAppByLocalId failed");
    return;
  }
  nsCOMPtr<mozIApplication> app = do_QueryInterface(domApp);
  if (!app) {
    NS_WARNING("app isn't a mozIApplication");
    return;
  }
  nsAutoString appName;
  rv = app->GetName(appName);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to retrieve app name");
    return;
  }
  SetThisProcessName(NS_LossyConvertUTF16toASCII(appName).get());
}

bool
TabChild::IsRootContentDocument()
{
    if (mIsBrowserElement || mAppId == nsIScriptSecurityManager::NO_APP_ID) {
        // We're the child side of a browser element.  This always
        // behaves like a root content document.
        return true;
    }

    // Otherwise, we're the child side of an <html:app remote=true>
    // embedded in an outer <html:app>.  These don't behave like root
    // content documents in nested contexts.  Because of bug 761935,
    // <html:browser remote> and <html:app remote> can't nest, so we
    // assume this isn't the root.  When that bug is fixed, we need to
    // revisit that assumption.
    return false;
}

bool
TabChild::RecvLoadURL(const nsCString& uri)
{
    printf("loading %s, %d\n", uri.get(), NS_IsMainThread());
    SetProcessNameToAppName();

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

    if (!InitRenderingState()) {
        // We can fail to initialize our widget if the <browser
        // remote> has already been destroyed, and we couldn't hook
        // into the parent-process's layer system.  That's not a fatal
        // error.
        return true;
    }

    baseWindow->SetVisibility(true);

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

void
TabChild::DispatchMessageManagerMessage(const nsAString& aMessageName,
                                        const nsACString& aJSONData)
{
    JSAutoRequest ar(mCx);
    jsval json = JSVAL_NULL;
    StructuredCloneData cloneData;
    JSAutoStructuredCloneBuffer buffer;
    if (JS_ParseJSON(mCx,
                      static_cast<const jschar*>(NS_ConvertUTF8toUTF16(aJSONData).get()),
                      aJSONData.Length(),
                      &json)) {
        WriteStructuredClone(mCx, json, buffer, cloneData.mClosure);
        cloneData.mData = buffer.data();
        cloneData.mDataLength = buffer.nbytes();
    }

    nsFrameScriptCx cx(static_cast<nsIWebBrowserChrome*>(this), this);
    // Let the BrowserElementScrolling helper (if it exists) for this
    // content manipulate the frame state.
    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    mm->ReceiveMessage(static_cast<nsIDOMEventTarget*>(mTabChildGlobal),
                       aMessageName, false, &cloneData, nullptr, nullptr);
}

bool
TabChild::RecvUpdateFrame(const FrameMetrics& aFrameMetrics)
{
    if (!mCx || !mTabChildGlobal) {
        return true;
    }

    nsCString data;
    data += nsPrintfCString("{ \"x\" : %d", NS_lround(aFrameMetrics.mViewportScrollOffset.x));
    data += nsPrintfCString(", \"y\" : %d", NS_lround(aFrameMetrics.mViewportScrollOffset.y));
    // We don't treat the x and y scales any differently for this
    // semi-platform-specific code.
    data += nsPrintfCString(", \"zoom\" : %f", aFrameMetrics.mResolution.width);
    data += nsPrintfCString(", \"displayPort\" : ");
        data += nsPrintfCString("{ \"left\" : %d", aFrameMetrics.mDisplayPort.X());
        data += nsPrintfCString(", \"top\" : %d", aFrameMetrics.mDisplayPort.Y());
        data += nsPrintfCString(", \"width\" : %d", aFrameMetrics.mDisplayPort.Width());
        data += nsPrintfCString(", \"height\" : %d", aFrameMetrics.mDisplayPort.Height());
        data += nsPrintfCString(", \"resolution\" : %f", aFrameMetrics.mResolution.width);
        data += nsPrintfCString(" }");
    data += nsPrintfCString(", \"screenSize\" : ");
        data += nsPrintfCString("{ \"width\" : %d", aFrameMetrics.mViewport.width);
        data += nsPrintfCString(", \"height\" : %d", aFrameMetrics.mViewport.height);
        data += nsPrintfCString(" }");
    data += nsPrintfCString(", \"cssPageRect\" : ");
        data += nsPrintfCString("{ \"x\" : %f", aFrameMetrics.mCSSContentRect.x);
        data += nsPrintfCString(", \"y\" : %f", aFrameMetrics.mCSSContentRect.y);
        data += nsPrintfCString(", \"width\" : %f", aFrameMetrics.mCSSContentRect.width);
        data += nsPrintfCString(", \"height\" : %f", aFrameMetrics.mCSSContentRect.height);
        data += nsPrintfCString(" }");
    data += nsPrintfCString(" }");

    DispatchMessageManagerMessage(NS_LITERAL_STRING("Viewport:Change"), data);

    return true;
}

bool
TabChild::RecvHandleDoubleTap(const nsIntPoint& aPoint)
{
    if (!mCx || !mTabChildGlobal) {
        return true;
    }

    nsCString data;
    data += nsPrintfCString("{ \"x\" : %d", aPoint.x);
    data += nsPrintfCString(", \"y\" : %d", aPoint.y);
    data += nsPrintfCString(" }");

    DispatchMessageManagerMessage(NS_LITERAL_STRING("Gesture:DoubleTap"), data);

    return true;
}

bool
TabChild::RecvHandleSingleTap(const nsIntPoint& aPoint)
{
  if (!mCx || !mTabChildGlobal) {
    return true;
  }

  RecvMouseEvent(NS_LITERAL_STRING("mousedown"), aPoint.x, aPoint.y, 0, 1, 0, false);
  RecvMouseEvent(NS_LITERAL_STRING("mouseup"), aPoint.x, aPoint.y, 0, 1, 0, false);

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
                         const int32_t&  aButton,
                         const int32_t&  aClickCount,
                         const int32_t&  aModifiers,
                         const bool&     aIgnoreRootScrollFrame)
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
  nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
  NS_ENSURE_TRUE(utils, true);
  utils->SendMouseEvent(aType, aX, aY, aButton, aClickCount, aModifiers,
                        aIgnoreRootScrollFrame, 0, 0);
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
TabChild::RecvMouseWheelEvent(const WheelEvent& event)
{
  WheelEvent localEvent(event);
  DispatchWidgetEvent(localEvent);
  return true;
}

void
TabChild::DispatchSynthesizedMouseEvent(const nsTouchEvent& aEvent)
{
  // Synthesize a phony mouse event.
  uint32_t msg;
  switch (aEvent.message) {
    case NS_TOUCH_START:
      msg = NS_MOUSE_BUTTON_DOWN;
      break;
    case NS_TOUCH_MOVE:
      msg = NS_MOUSE_MOVE;
      break;
    case NS_TOUCH_END:
    case NS_TOUCH_CANCEL:
      msg = NS_MOUSE_BUTTON_UP;
      break;
    default:
      MOZ_NOT_REACHED("Unknown touch event message");
  }

  nsIntPoint refPoint(0, 0);
  if (aEvent.touches.Length()) {
    refPoint = aEvent.touches[0]->mRefPoint;
  }

  nsMouseEvent event(true, msg, NULL,
      nsMouseEvent::eReal, nsMouseEvent::eNormal);
  event.refPoint = refPoint;
  event.time = aEvent.time;
  event.button = nsMouseEvent::eLeftButton;
  if (msg != NS_MOUSE_MOVE) {
    event.clickCount = 1;
  }

  DispatchWidgetEvent(event);
}

bool
TabChild::RecvRealTouchEvent(const nsTouchEvent& aEvent)
{
  nsTouchEvent localEvent(aEvent);
  nsEventStatus status = DispatchWidgetEvent(localEvent);

  if (IsAsyncPanZoomEnabled()) {
    nsCOMPtr<nsPIDOMWindow> outerWindow = do_GetInterface(mWebNav);
    nsCOMPtr<nsPIDOMWindow> innerWindow = outerWindow->GetCurrentInnerWindow();

    if (innerWindow && innerWindow->HasTouchEventListeners()) {
      SendContentReceivedTouch(nsIPresShell::gPreventMouseEvents);
    }
  } else if (status != nsEventStatus_eConsumeNoDefault) {
    DispatchSynthesizedMouseEvent(aEvent);
  }

  return true;
}

bool
TabChild::RecvRealTouchMoveEvent(const nsTouchEvent& aEvent)
{
  return RecvRealTouchEvent(aEvent);
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
                       const int32_t& aKeyCode,
                       const int32_t& aCharCode,
                       const int32_t& aModifiers,
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

nsEventStatus
TabChild::DispatchWidgetEvent(nsGUIEvent& event)
{
  if (!mWidget)
    return nsEventStatus_eConsumeNoDefault;

  nsEventStatus status;
  event.widget = mWidget;
  NS_ENSURE_SUCCESS(mWidget->DispatchEvent(&event, status),
                    nsEventStatus_eConsumeNoDefault);
  return status;
}

PDocumentRendererChild*
TabChild::AllocPDocumentRenderer(const nsRect& documentRect,
                                 const gfxMatrix& transform,
                                 const nsString& bgcolor,
                                 const uint32_t& renderFlags,
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
                                           const uint32_t& renderFlags,
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
TabChild::AllocPContentDialog(const uint32_t&,
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
TabChild::AllocPContentPermissionRequest(const nsCString& aType, const IPC::Principal&)
{
  NS_RUNTIMEABORT("unused");
  return nullptr;
}

bool
TabChild::DeallocPContentPermissionRequest(PContentPermissionRequestChild* actor)
{
    PCOMContentPermissionRequestChild* child =
        static_cast<PCOMContentPermissionRequestChild*>(actor);
#ifdef DEBUG
    child->mIPCOpen = false;
#endif /* DEBUG */
    child->IPDLRelease();
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
TabChild::AllocPOfflineCacheUpdate(const URIParams& manifestURI,
                                   const URIParams& documentURI,
                                   const bool& isInBrowserElement,
                                   const uint32_t& appId,
                                   const bool& stickDocument)
{
  NS_RUNTIMEABORT("unused");
  return nullptr;
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
                           const ClonedMessageData& aData)
{
  if (mTabChildGlobal) {
    nsFrameScriptCx cx(static_cast<nsIWebBrowserChrome*>(this), this);

    const SerializedStructuredCloneBuffer& buffer = aData.data();
    const InfallibleTArray<PBlobChild*>& blobChildList = aData.blobsChild();

    StructuredCloneData cloneData;
    cloneData.mData = buffer.data;
    cloneData.mDataLength = buffer.dataLength;

    if (!blobChildList.IsEmpty()) {
      uint32_t length = blobChildList.Length();
      cloneData.mClosure.mBlobs.SetCapacity(length);
      for (uint32_t i = 0; i < length; ++i) {
        BlobChild* blobChild = static_cast<BlobChild*>(blobChildList[i]);
        MOZ_ASSERT(blobChild);

        nsCOMPtr<nsIDOMBlob> blob = blobChild->GetBlob();
        MOZ_ASSERT(blob);

        cloneData.mClosure.mBlobs.AppendElement(blob);
      }
    }

    nsRefPtr<nsFrameMessageManager> mm =
      static_cast<nsFrameMessageManager*>(mTabChildGlobal->mMessageManager.get());
    mm->ReceiveMessage(static_cast<nsIDOMEventTarget*>(mTabChildGlobal),
                       aMessage, false, &cloneData, nullptr, nullptr);
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
    NS_NewDOMEvent(getter_AddRefs(event), nullptr, nullptr);
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
TabChild::AllocPRenderFrame(ScrollingBehavior* aScrolling,
                            LayersBackend* aBackend,
                            int32_t* aMaxTextureSize,
                            uint64_t* aLayersId)
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
TabChild::InitTabChildGlobal(FrameScriptLoading aScriptLoading)
{
  if (!mCx && !mTabChildGlobal) {
    nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mWebNav);
    NS_ENSURE_TRUE(window, false);
    nsCOMPtr<nsIDOMEventTarget> chromeHandler =
      do_QueryInterface(window->GetChromeEventHandler());
    NS_ENSURE_TRUE(chromeHandler, false);

    nsRefPtr<TabChildGlobal> scope = new TabChildGlobal(this);
    NS_ENSURE_TRUE(scope, false);

    mTabChildGlobal = scope;

    nsISupports* scopeSupports = NS_ISUPPORTS_CAST(nsIDOMEventTarget*, scope);
  
    NS_ENSURE_TRUE(InitTabChildGlobalInternal(scopeSupports), false); 

    scope->Init();

    nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(chromeHandler);
    NS_ENSURE_TRUE(root, false);
    root->SetParentTarget(scope);
  }

  if (aScriptLoading != DONT_LOAD_SCRIPTS && !mTriedBrowserInit) {
    mTriedBrowserInit = true;
    // Initialize the child side of the browser element machinery,
    // if appropriate.
    if (mIsBrowserElement || mAppId != nsIScriptSecurityManager::NO_APP_ID) {
      RecvLoadRemoteScript(BROWSER_ELEMENT_CHILD_SCRIPT);
    }
  }

  return true;
}

bool
TabChild::InitRenderingState()
{
    static_cast<PuppetWidget*>(mWidget.get())->InitIMEState();

    LayersBackend be;
    uint64_t id;
    int32_t maxTextureSize;
    RenderFrameChild* remoteFrame =
        static_cast<RenderFrameChild*>(SendPRenderFrameConstructor(
                                           &mScrolling, &be, &maxTextureSize, &id));
    if (!remoteFrame) {
      NS_WARNING("failed to construct RenderFrame");
      return false;
    }

    PLayersChild* shadowManager = nullptr;
    if (id != 0) {
        // Pushing layers transactions directly to a separate
        // compositor context.
        shadowManager =
            CompositorChild::Get()->SendPLayersConstructor(be, id,
                                                           &be,
                                                           &maxTextureSize);
    } else {
        // Pushing transactions to the parent content.
        shadowManager = remoteFrame->SendPLayersConstructor();
    }

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

    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

    if (observerService) {
        observerService->AddObserver(this,
                                     "cancel-default-pan-zoom",
                                     false);
        observerService->AddObserver(this,
                                     "browser-zoom-to-rect",
                                     false);
    }

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

void
TabChild::GetDPI(float* aDPI)
{
    *aDPI = -1.0;
    if (!mRemoteFrame) {
        return;
    }

    SendGetDPI(aDPI);
}

void
TabChild::NotifyPainted()
{
    if (UseDirectCompositor() && !mNotified) {
        mRemoteFrame->SendNotifyCompositorTransaction();
        mNotified = true;
    }
}

bool
TabChild::IsAsyncPanZoomEnabled()
{
    return mScrolling == ASYNC_PAN_ZOOM;
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
                        const StructuredCloneData& aData,
                        InfallibleTArray<nsString>* aJSONRetVal)
{
  TabChild* tabChild = static_cast<TabChild*>(aCallbackData);
  ContentChild* cc = static_cast<ContentChild*>(tabChild->Manager());
  ClonedMessageData data;
  SerializedStructuredCloneBuffer& buffer = data.data();
  buffer.data = aData.mData;
  buffer.dataLength = aData.mDataLength;

  const nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = aData.mClosure.mBlobs;
  if (!blobs.IsEmpty()) {
    InfallibleTArray<PBlobChild*>& blobChildList = data.blobsChild();
    uint32_t length = blobs.Length();
    blobChildList.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      BlobChild* blobChild = cc->GetOrCreateActorForBlob(blobs[i]);
      if (!blobChild) {
        return false;
      }
      blobChildList.AppendElement(blobChild);
    }
  }
  return tabChild->SendSyncMessage(nsString(aMessage), data, aJSONRetVal);
}

static bool
SendAsyncMessageToParent(void* aCallbackData,
                         const nsAString& aMessage,
                         const StructuredCloneData& aData)
{
  TabChild* tabChild = static_cast<TabChild*>(aCallbackData);
  ContentChild* cc = static_cast<ContentChild*>(tabChild->Manager());
  ClonedMessageData data;
  SerializedStructuredCloneBuffer& buffer = data.data();
  buffer.data = aData.mData;
  buffer.dataLength = aData.mDataLength;

  const nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = aData.mClosure.mBlobs;
  if (!blobs.IsEmpty()) {
    InfallibleTArray<PBlobChild*>& blobChildList = data.blobsChild();
    uint32_t length = blobs.Length();
    blobChildList.SetCapacity(length);
    for (uint32_t i = 0; i < length; ++i) {
      BlobChild* blobChild = cc->GetOrCreateActorForBlob(blobs[i]);
      if (!blobChild) {
        return false;
      }
      blobChildList.AppendElement(blobChild);
    }
  }

  return tabChild->SendAsyncMessage(nsString(aMessage), data);
}


TabChildGlobal::TabChildGlobal(TabChild* aTabChild)
: mTabChild(aTabChild)
{
}

void
TabChildGlobal::Init()
{
  NS_ASSERTION(!mMessageManager, "Re-initializing?!?");
  mMessageManager = new nsFrameMessageManager(false, /* aChrome */
                                              SendSyncMessageToParent,
                                              SendAsyncMessageToParent,
                                              nullptr,
                                              mTabChild,
                                              nullptr,
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
  NS_INTERFACE_MAP_ENTRY(nsIMessageListenerManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsISyncMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptContextPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentFrameMessageManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(TabChildGlobal, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TabChildGlobal, nsDOMEventTargetHelper)

/* [notxpcom] boolean markForCC (); */
// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
TabChildGlobal::MarkForCC()
{
  return mMessageManager ? mMessageManager->MarkForCC() : false;
}

NS_IMETHODIMP
TabChildGlobal::GetContent(nsIDOMWindow** aContent)
{
  *aContent = nullptr;
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
  *aDocShell = nullptr;
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
    return nullptr;
  return mTabChild->GetJSContext();
}

nsIPrincipal* 
TabChildGlobal::GetPrincipal()
{
  if (!mTabChild)
    return nullptr;
  return mTabChild->GetPrincipal();
}
