/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCOMPtr.h"
#include "nsGlobalWindow.h"
#include "nscore.h"
#include "nsRect.h"
#include "nslayout.h"
#include "prmem.h"
#include "prtime.h"
#include "plstr.h"
#include "prinrval.h"
#include "nsIFactory.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"
#include "nsIServiceManager.h"
#include "nsITimer.h"
#include "nsEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsIDOMBarProp.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMPaintListener.h"
#include "nsJSUtils.h"
#include "nsIScriptEventListener.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "nsIScriptContextOwner.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsIPref.h"
#include "nsCRT.h"
#include "nsRect.h"
#ifdef NECKO
#include "nsIPrompt.h"
#else
#include "nsINetSupport.h"
#endif
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIPresShell.h"
#include "nsScreen.h"
#include "nsHistory.h"
#include "nsBarProps.h"
#include "nsIScriptSecurityManager.h"
#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsNeckoUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO

#if defined(OJI)
#include "nsIJVMManager.h"
#endif

#include "nsMimeTypeArray.h"
#include "nsPluginArray.h"

#include "jsapi.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectDataIID, NS_ISCRIPTGLOBALOBJECTDATA_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIScriptEventListenerIID, NS_ISCRIPTEVENTLISTENER_IID);
static NS_DEFINE_IID(kIDOMWindowIID, NS_IDOMWINDOW_IID);
static NS_DEFINE_IID(kIDOMNavigatorIID, NS_IDOMNAVIGATOR_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMMouseMotionListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);
static NS_DEFINE_IID(kIDOMFocusListenerIID, NS_IDOMFOCUSLISTENER_IID);
static NS_DEFINE_IID(kIDOMFormListenerIID, NS_IDOMFORMLISTENER_IID);
static NS_DEFINE_IID(kIDOMLoadListenerIID, NS_IDOMLOADLISTENER_IID);
static NS_DEFINE_IID(kIDOMDragListenerIID, NS_IDOMDRAGLISTENER_IID);
static NS_DEFINE_IID(kIDOMPaintListenerIID, NS_IDOMPAINTLISTENER_IID);
static NS_DEFINE_IID(kIEventListenerManagerIID, NS_IEVENTLISTENERMANAGER_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID, NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIDOMEventCapturerIID, NS_IDOMEVENTCAPTURER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMEventTargetIID, NS_IDOMEVENTTARGET_IID);
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
#ifndef NECKO
static NS_DEFINE_IID(kINetSupportIID, NS_INETSUPPORT_IID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
#endif // NECKO

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

GlobalWindowImpl::GlobalWindowImpl()
{
  NS_INIT_REFCNT();
  mContext = nsnull;
  mScriptObject = nsnull;
  mDocument = nsnull;
  mNavigator = nsnull;
  mScreen = nsnull;
  mHistory = nsnull;
  mMenubar = nsnull;
  mToolbar = nsnull;
  mLocationbar = nsnull;
  mPersonalbar = nsnull;
  mStatusbar = nsnull;
  mScrollbars = nsnull;
  mLocation = nsnull;
  mFrames = nsnull;
  mOpener = nsnull;
  mPrincipals = nsnull;

  mTimeouts = nsnull;
  mTimeoutInsertionPoint = nsnull;
  mRunningTimeout = nsnull;
  mTimeoutPublicIdCounter = 1;
  mListenerManager = nsnull;

  mFirstDocumentLoad = PR_TRUE;

  mChromeDocument = nsnull;
}

GlobalWindowImpl::~GlobalWindowImpl() 
{  
  if (mPrincipals && mContext) {
    JSPRINCIPALS_DROP((JSContext*)mContext->GetNativeContext(), mPrincipals);
  }

  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mNavigator);
  NS_IF_RELEASE(mScreen);
  NS_IF_RELEASE(mHistory);
  NS_IF_RELEASE(mMenubar);
  NS_IF_RELEASE(mToolbar);
  NS_IF_RELEASE(mLocationbar);
  NS_IF_RELEASE(mPersonalbar);
  NS_IF_RELEASE(mStatusbar);
  NS_IF_RELEASE(mScrollbars);
  NS_IF_RELEASE(mLocation);
  NS_IF_RELEASE(mFrames);
  NS_IF_RELEASE(mOpener);
  NS_IF_RELEASE(mListenerManager);
}

NS_IMPL_ADDREF(GlobalWindowImpl)
NS_IMPL_RELEASE(GlobalWindowImpl)

nsresult 
GlobalWindowImpl::QueryInterface(const nsIID& aIID,
                                 void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptGlobalObjectIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptGlobalObject*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptGlobalObjectDataIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptGlobalObjectData*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMWindowIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMWindow*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIScriptGlobalObject*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIJSScriptObjectIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIJSScriptObject*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventCapturerIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIDOMEventCapturer*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventReceiverIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIDOMEventReceiver*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventTargetIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIDOMEventTarget*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult 
GlobalWindowImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult 
GlobalWindowImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptWindow(aContext, (nsIDOMWindow*)this,
                             nsnull, &mScriptObject);
    aContext->AddNamedReference(&mScriptObject, mScriptObject, 
                                "window_object");
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP_(void)       
GlobalWindowImpl::SetContext(nsIScriptContext *aContext)
{
  if (mContext) {
    NS_RELEASE(mContext);
  }

  mContext = aContext;
  NS_ADDREF(mContext);
}

NS_IMETHODIMP_(void)
GlobalWindowImpl::GetContext(nsIScriptContext **aContext)
{
  *aContext = mContext;
  NS_IF_ADDREF(*aContext);
}

NS_IMETHODIMP_(void)       
GlobalWindowImpl::SetNewDocument(nsIDOMDocument *aDocument)
{
  if (mFirstDocumentLoad) {
    mFirstDocumentLoad = PR_FALSE;
    mDocument = aDocument;
    NS_IF_ADDREF(mDocument);
    return;
  }
  
  if (mDocument) {
    nsIDocument* doc;
    nsIURI* docURL = 0;

    if (mDocument && NS_SUCCEEDED(mDocument->QueryInterface(kIDocumentIID, (void**)&doc))) {
      docURL = doc->GetDocumentURL();
      NS_RELEASE(doc);
    }

    if (docURL) {
#ifdef NECKO
      char* str;
      docURL->GetSpec(&str);
#else
      PRUnichar* str;
      docURL->ToString(&str);
#endif
      nsAutoString url = str;

      //about:blank URL's do not have ClearScope called on page change.
      if (url != "about:blank") {
        ClearAllTimeouts();
  
        if (mListenerManager) {
          mListenerManager->RemoveAllListeners();
        }

        if ((nsnull != mScriptObject) && 
            (nsnull != mContext) /* &&
            (nsnull != aDocument) XXXbe why commented out? */ ) {
          JS_ClearScope((JSContext *)mContext->GetNativeContext(),
                        (JSObject *)mScriptObject);
        }
      }
#ifdef NECKO
      nsCRT::free(str);
#else
      delete[] str;
#endif
    }
  }

  //XXX Should this be outside the about:blank clearscope exception?
  if (mPrincipals && mContext) {
    JSPRINCIPALS_DROP((JSContext *)mContext->GetNativeContext(), mPrincipals);
    mPrincipals = nsnull;
  }

  if (nsnull != mDocument) {
    NS_RELEASE(mDocument);
  }
    
  if (nsnull != mContext) {
    mContext->GC();
  }

  mDocument = aDocument;
  
  if (nsnull != mDocument) {
    NS_ADDREF(mDocument);

    if (nsnull != mContext) {
      mContext->InitContext(this);
    }
  }
}

NS_IMETHODIMP_(void)       
GlobalWindowImpl::SetWebShell(nsIWebShell *aWebShell)
{
  //mWebShell isn't refcnt'd here.  WebShell calls SetWebShell(nsnull) when deleted.

  // When SetWebShell(nsnull) is called, drop our references to the
  // script object (held via a named JS root) and the script context
  // itself.
  if ((nsnull == aWebShell) && (nsnull != mContext)) {
    if (nsnull != mScriptObject) {
      mContext->RemoveReference(&mScriptObject, mScriptObject);
      mScriptObject = nsnull;
    }
    NS_IF_RELEASE(mContext);
  }
  mWebShell = aWebShell;
  if (nsnull != mLocation) {
    mLocation->SetWebShell(aWebShell);
  }
  if (nsnull != mHistory) {
    mHistory->SetWebShell(aWebShell);
  }
  if (nsnull != mFrames) {
    mFrames->SetWebShell(aWebShell);
  }

  if (mWebShell) {
    // tell our member elements about the new browserwindow

    if (nsnull != mMenubar)  {
      nsCOMPtr<nsIBrowserWindow> browser;
      GetBrowserWindowInterface(*getter_AddRefs(browser));
      mMenubar->SetBrowserWindow(browser);
    }
    // Get our enclosing chrome shell and retrieve its global window impl, so that we can
    // do some forwarding to the chrome document.
    nsCOMPtr<nsIWebShell> chromeShell;
    mWebShell->GetContainingChromeShell(getter_AddRefs(chromeShell));
    if (chromeShell) {
      nsCOMPtr<nsIDOMWindow> chromeWindow;
      WebShellToDOMWindow(chromeShell, getter_AddRefs(chromeWindow));
      if (chromeWindow) {
        nsCOMPtr<nsIDOMDocument> chromeDoc;
        chromeWindow->GetDocument(getter_AddRefs(chromeDoc));
        nsCOMPtr<nsIDocument> realDoc = do_QueryInterface(chromeDoc);
        mChromeDocument = realDoc.get(); // Don't addref it
      }
    }
  }
}

NS_IMETHODIMP_(void)       // XXX This may be temporary - rods
GlobalWindowImpl::GetWebShell(nsIWebShell **aWebShell)
{
  if (nsnull != mWebShell)  {
    *aWebShell = mWebShell;
    NS_ADDREF(mWebShell);
  } else {
    //*mWebShell = nsnull;
  }

}

NS_IMETHODIMP_(void)       
GlobalWindowImpl::SetOpenerWindow(nsIDOMWindow *aOpener)
{
  NS_IF_RELEASE(mOpener);
  mOpener = aOpener;
  NS_IF_ADDREF(mOpener);
}

NS_IMETHODIMP    
GlobalWindowImpl::GetWindow(nsIDOMWindow** aWindow)
{
  *aWindow = this;
  NS_ADDREF(this);

  return NS_OK;
}

NS_IMETHODIMP    
GlobalWindowImpl::GetSelf(nsIDOMWindow** aWindow)
{
  *aWindow = this;
  NS_ADDREF(this);

  return NS_OK;
}

NS_IMETHODIMP    
GlobalWindowImpl::GetDocument(nsIDOMDocument** aDocument)
{
  *aDocument = mDocument;
  NS_IF_ADDREF(mDocument);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetNavigator(nsIDOMNavigator** aNavigator)
{
  if (nsnull == mNavigator) {
    mNavigator = new NavigatorImpl();
    NS_IF_ADDREF(mNavigator);
  }

  *aNavigator = mNavigator;
  NS_IF_ADDREF(mNavigator);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScreen(nsIDOMScreen** aScreen)
{
  if ((nsnull == mScreen) && (nsnull != mWebShell)) {
    mScreen = new ScreenImpl( mWebShell );
    NS_IF_ADDREF(mScreen);
  }
    
  *aScreen = mScreen;
  NS_IF_ADDREF(mScreen);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetHistory(nsIDOMHistory** aHistory)
{
  if ((nsnull == mHistory) && (nsnull != mWebShell)) {
    mHistory = new HistoryImpl();
    if (nsnull != mHistory) {
      NS_ADDREF(mHistory);
      mHistory->SetWebShell(mWebShell);
    }
  }
  
  *aHistory = mHistory;
  NS_IF_ADDREF(mHistory);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetMenubar(nsIDOMBarProp** aMenubar)
{
  if (nsnull == mMenubar) {
    mMenubar = new MenubarPropImpl();
    if (nsnull != mMenubar) {
      NS_ADDREF(mMenubar);
      nsIBrowserWindow *browser = nsnull;
      if ((nsnull != mWebShell) && NS_OK == GetBrowserWindowInterface(browser)) {
        mMenubar->SetBrowserWindow(browser);
        NS_IF_RELEASE(browser);
      }
    }
  }

  *aMenubar = mMenubar;
  NS_IF_ADDREF(mMenubar);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetToolbar(nsIDOMBarProp** aToolbar)
{
  if (nsnull == mToolbar) {
    mToolbar = new ToolbarPropImpl();
    if (nsnull != mToolbar) {
      NS_ADDREF(mToolbar);
      nsIBrowserWindow *browser = nsnull;
      if ((nsnull != mWebShell) && NS_OK == GetBrowserWindowInterface(browser)) {
        mToolbar->SetBrowserWindow(browser);
        NS_IF_RELEASE(browser);
      }
    }
  }

  *aToolbar = mToolbar;
  NS_IF_ADDREF(mToolbar);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetLocationbar(nsIDOMBarProp** aLocationbar)
{
  if (nsnull == mLocationbar) {
    mLocationbar = new LocationbarPropImpl();
    if (nsnull != mLocationbar) {
      NS_ADDREF(mLocationbar);
      nsIBrowserWindow *browser = nsnull;
      if ((nsnull != mWebShell) && NS_OK == GetBrowserWindowInterface(browser)) {
        mLocationbar->SetBrowserWindow(browser);
        NS_IF_RELEASE(browser);
      }
    }
  }

  *aLocationbar = mLocationbar;
  NS_IF_ADDREF(mLocationbar);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetPersonalbar(nsIDOMBarProp** aPersonalbar)
{
  if (nsnull == mPersonalbar) {
    mPersonalbar = new PersonalbarPropImpl();
    if (nsnull != mPersonalbar) {
      NS_ADDREF(mPersonalbar);
      nsIBrowserWindow *browser = nsnull;
      if ((nsnull != mWebShell) && NS_OK == GetBrowserWindowInterface(browser)) {
        mPersonalbar->SetBrowserWindow(browser);
        NS_IF_RELEASE(browser);
      }
    }
  }

  *aPersonalbar = mPersonalbar;
  NS_IF_ADDREF(mPersonalbar);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetStatusbar(nsIDOMBarProp** aStatusbar)
{
  if (nsnull == mStatusbar) {
    mStatusbar = new StatusbarPropImpl();
    if (nsnull != mStatusbar) {
      NS_ADDREF(mStatusbar);
      nsIBrowserWindow *browser = nsnull;
      if ((nsnull != mWebShell) && NS_OK == GetBrowserWindowInterface(browser)) {
        mStatusbar->SetBrowserWindow(browser);
        NS_IF_RELEASE(browser);
      }
    }
  }

  *aStatusbar = mStatusbar;
  NS_IF_ADDREF(mStatusbar);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScrollbars(nsIDOMBarProp** aScrollbars)
{
  if (nsnull == mScrollbars) {
    mScrollbars = new ScrollbarsPropImpl();
    if (nsnull != mScrollbars) {
      NS_ADDREF(mScrollbars);
      nsIBrowserWindow *browser = nsnull;
      if ((nsnull != mWebShell) && NS_OK == GetBrowserWindowInterface(browser)) {
        mScrollbars->SetBrowserWindow(browser);
        NS_IF_RELEASE(browser);
      }
    }
  }

  *aScrollbars = mScrollbars;
  NS_IF_ADDREF(mScrollbars);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetDirectories(nsIDOMBarProp** aDirectories)
{
  return GetPersonalbar(aDirectories);
}

NS_IMETHODIMP
GlobalWindowImpl::GetOpener(nsIDOMWindow** aOpener)
{
  *aOpener = mOpener;
  NS_IF_ADDREF(*aOpener);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetOpener(nsIDOMWindow* aOpener)
{
  NS_IF_RELEASE(mOpener);
  mOpener = aOpener;
  NS_IF_ADDREF(mOpener);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetParent(nsIDOMWindow** aParent)
{
  nsresult ret = NS_OK;
  
  *aParent = nsnull;
  if (nsnull != mWebShell) {
    nsIWebShell *parentWebShell;
    mWebShell->GetParent(parentWebShell);
    
    if (nsnull != parentWebShell) {
      ret = WebShellToDOMWindow(parentWebShell, aParent);
      NS_RELEASE(parentWebShell);
    } 
    else {
      *aParent = this;
      NS_ADDREF(this);
    }
  }

  return ret;
}

NS_IMETHODIMP    
GlobalWindowImpl::GetLocation(nsIDOMLocation** aLocation)
{
  if ((nsnull == mLocation) && (nsnull != mWebShell)) {
    mLocation = new LocationImpl(mWebShell);
    NS_IF_ADDREF(mLocation);
  }

  *aLocation = mLocation;
  NS_IF_ADDREF(mLocation);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetTop(nsIDOMWindow** aTop)
{
  nsresult ret = NS_OK;

  *aTop = nsnull;
  if (nsnull != mWebShell) {
    nsIWebShell *rootWebShell;
    mWebShell->GetRootWebShell(rootWebShell);
    if (nsnull != rootWebShell) {
      WebShellToDOMWindow(rootWebShell, aTop);
      NS_RELEASE(rootWebShell);
    }
  }

  return ret;
}

NS_IMETHODIMP
GlobalWindowImpl::GetContent(nsIDOMWindow** aContent)
{
  nsresult rv;

  *aContent = nsnull;

  nsCOMPtr<nsIBrowserWindow> browser;
  rv = GetBrowserWindowInterface(*getter_AddRefs(browser));
  if (NS_SUCCEEDED(rv) && browser) {
    nsCOMPtr<nsIWebShell> contentShell;
    rv = browser->GetContentWebShell(getter_AddRefs(contentShell));
    if (NS_SUCCEEDED(rv) && contentShell)
      rv = WebShellToDOMWindow(contentShell, aContent);
  }
  return rv;
}

NS_IMETHODIMP
GlobalWindowImpl::GetClosed(PRBool* aClosed)
{
  if (nsnull == mWebShell) {
    *aClosed = PR_TRUE;
  }
  else {
    *aClosed = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetFrames(nsIDOMWindowCollection** aFrames)
{
  if ((nsnull == mFrames) && (nsnull != mWebShell)) {
    mFrames = new nsDOMWindowList(mWebShell);
    if (nsnull == mFrames) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mFrames);
  }

  *aFrames = (nsIDOMWindowCollection *)mFrames;
  NS_IF_ADDREF(mFrames);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetStatus(nsString& aStatus)
{
  nsIBrowserWindow *mBrowser;
  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    const PRUnichar *status;
    mBrowser->GetStatus(&status);
    aStatus = status;
    NS_RELEASE(mBrowser);
  }
  else {
    aStatus.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetStatus(const nsString& aStatus)
{
  nsIBrowserWindow *mBrowser;
  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    mBrowser->SetStatus(aStatus.GetUnicode());
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetDefaultStatus(nsString& aDefaultStatus)
{
  nsIBrowserWindow *mBrowser;
  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    const PRUnichar *status;
    mBrowser->GetDefaultStatus(&status);
    aDefaultStatus = status;
    NS_RELEASE(mBrowser);
  }
  else {
    aDefaultStatus.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetDefaultStatus(const nsString& aDefaultStatus)
{
  nsIBrowserWindow *mBrowser;
  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    mBrowser->SetDefaultStatus(aDefaultStatus.GetUnicode());
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetName(nsString& aName)
{
  const PRUnichar *name = nsnull;
  if (nsnull != mWebShell) {
    mWebShell->GetName(&name);
  }
  aName = name;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetName(const nsString& aName)
{
  nsresult result = NS_OK;
  if (nsnull != mWebShell) {
    result = mWebShell->SetName(aName.GetUnicode());
  }
  return result;
}


NS_IMETHODIMP
GlobalWindowImpl::GetInnerWidth(PRInt32* aInnerWidth)
{
  nsIBrowserWindow *mBrowser;
  nsIDOMWindow* parent = nsnull;
  
  GetParent(&parent);
  if (parent == this) {
    // We are in a top level window.  Use browser window's bounds.
    if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
      nsRect r;
      mBrowser->GetContentBounds(r);
      *aInnerWidth = r.width;
      NS_RELEASE(mBrowser);
    }
    else {
      *aInnerWidth = 0;
    }
  }
  else {
    // We are in an (i)frame.  Use webshell bounds.    
    if (mWebShell) {
      PRInt32 x,y,w,h;
      mWebShell->GetBounds(x, y, w, h);
      *aInnerWidth = w;
    }
    else {
      *aInnerWidth = 0;
    }    
  }
  
  NS_RELEASE(parent);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetInnerWidth(PRInt32 aInnerWidth)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetContentBounds(r);

    mBrowser->SizeContentTo(aInnerWidth, r.height);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetInnerHeight(PRInt32* aInnerHeight)
{
  nsIBrowserWindow *mBrowser;
  nsIDOMWindow* parent = nsnull;
  
  GetParent(&parent);
  if (parent == this) {
    // We are in a top level window.  Use browser window's bounds.    
    if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
      nsRect r;
      mBrowser->GetContentBounds(r);
      *aInnerHeight = r.height;
      NS_RELEASE(mBrowser);
    }
    else {
      *aInnerHeight = 0;
    }
  }
  else {
    // We are in an (i)frame.  Use webshell bounds.    
    if (mWebShell) {
      PRInt32 x,y,w,h;
      mWebShell->GetBounds(x, y, w, h);
      *aInnerHeight = h;
    }
    else {
      *aInnerHeight = 0;
    }    
  }

  NS_RELEASE(parent);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetInnerHeight(PRInt32 aInnerHeight)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetContentBounds(r);

    mBrowser->SizeContentTo(r.width, aInnerHeight);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetOuterWidth(PRInt32* aOuterWidth)
{
  nsIBrowserWindow *mBrowser;
  nsIDOMWindow* parent = nsnull;
  
  GetParent(&parent);
  if (parent == this) {
    // We are in a top level window.  Use browser window's bounds.    
    if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
      nsRect r;
      mBrowser->GetWindowBounds(r);
      *aOuterWidth = r.width;
      NS_RELEASE(mBrowser);
    }
    else {
      *aOuterWidth = 0;
    }
  }
  else {
    // We are in an (i)frame.  Use webshell bounds.    
    if (mWebShell) {
      PRInt32 x,y,w,h;
      mWebShell->GetBounds(x, y, w, h);
      *aOuterWidth = w;
    }
    else {
      *aOuterWidth = 0;
    }    
  }
  
  NS_RELEASE(parent);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetOuterWidth(PRInt32 aOuterWidth)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);

    mBrowser->SizeWindowTo(aOuterWidth, r.height);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetOuterHeight(PRInt32* aOuterHeight)
{
  nsIBrowserWindow *mBrowser;
  nsIDOMWindow* parent = nsnull;
  
  GetParent(&parent);
  if (parent == this) {
    // We are in a top level window.  Use browser window's bounds.    
    if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
      nsRect r;
      mBrowser->GetWindowBounds(r);
      *aOuterHeight = r.height;
      NS_RELEASE(mBrowser);
    }
    else {
      *aOuterHeight = 0;
    }
  }
  else {
    // We are in an (i)frame.  Use webshell bounds.    
    if (mWebShell) {
      PRInt32 x,y,w,h;
      mWebShell->GetBounds(x, y, w, h);
      *aOuterHeight = h;
    }
    else {
      *aOuterHeight = 0;
    }    
  }

  NS_RELEASE(parent);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetOuterHeight(PRInt32 aOuterHeight)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);

    mBrowser->SizeWindowTo(r.width, aOuterHeight);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScreenX(PRInt32* aScreenX)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);
    *aScreenX = r.x;
    NS_RELEASE(mBrowser);
  }
  else {
    *aScreenX = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetScreenX(PRInt32 aScreenX)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);

    mBrowser->MoveTo(aScreenX, r.y);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScreenY(PRInt32* aScreenY)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);
    *aScreenY = r.y;
    NS_RELEASE(mBrowser);
  }
  else {
    *aScreenY = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetScreenY(PRInt32 aScreenY)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);

    mBrowser->MoveTo(r.x, aScreenY);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetPageXOffset(PRInt32* aPageXOffset)
{
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetPageXOffset(PRInt32 aPageXOffset)
{
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetPageYOffset(PRInt32* aPageYOffset)
{
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetPageYOffset(PRInt32 aPageYOffset)
{
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Dump(const nsString& aStr)
{
  char *cstr = aStr.ToNewCString();
  
  if (nsnull != cstr) {
    printf("%s", cstr);
    delete [] cstr;
  }
  
  return NS_OK;
}


NS_IMETHODIMP
GlobalWindowImpl::Alert(JSContext *cx, jsval *argv, PRUint32 argc)
{
  nsresult ret = NS_OK;
  nsAutoString str;

  if (nsnull != mWebShell) {
    if (argc > 0) {
      nsJSUtils::nsConvertJSValToString(str, cx, argv[0]);
    }
    else {
      str.SetString("undefined");
    }
    
    nsIWebShell *rootWebShell;
    ret = mWebShell->GetRootWebShellEvenIfChrome(rootWebShell);
    if (nsnull != rootWebShell) {
      nsIWebShellContainer *rootContainer;
      ret = rootWebShell->GetContainer(rootContainer);
      if (nsnull != rootContainer) {
#ifdef NECKO
        nsIPrompt *prompter;
        if (NS_OK == (ret = rootContainer->QueryInterface(nsIPrompt::GetIID(), (void**)&prompter))) {
          ret = prompter->Alert(str.GetUnicode());
          NS_RELEASE(prompter);
        }
#else
        nsINetSupport *support;
        if (NS_OK == (ret = rootContainer->QueryInterface(kINetSupportIID, (void**)&support))) {
          support->Alert(str);
          NS_RELEASE(support);
        }
#endif
        NS_RELEASE(rootContainer);
      }
      NS_RELEASE(rootWebShell);
    }
  }
  return ret;
}

NS_IMETHODIMP    
GlobalWindowImpl::Confirm(JSContext *cx, jsval *argv, PRUint32 argc, PRBool* aReturn)
{
  nsresult ret = NS_OK;
  nsAutoString str;

  *aReturn = PR_FALSE;
  if (nsnull != mWebShell) {
    if (argc > 0) {
      nsJSUtils::nsConvertJSValToString(str, cx, argv[0]);
    }
    else {
      str.SetString("undefined");
    }
    
    nsIWebShell *rootWebShell;
    ret = mWebShell->GetRootWebShellEvenIfChrome(rootWebShell);
    if (nsnull != rootWebShell) {
      nsIWebShellContainer *rootContainer;
      ret = rootWebShell->GetContainer(rootContainer);
      if (nsnull != rootContainer) {
#ifdef NECKO
        nsIPrompt *prompter;
        if (NS_OK == (ret = rootContainer->QueryInterface(nsIPrompt::GetIID(), (void**)&prompter))) {
          ret = prompter->Confirm(str.GetUnicode(), aReturn);
          NS_RELEASE(prompter);
        }
#else
        nsINetSupport *support;
        if (NS_OK == (ret = rootContainer->QueryInterface(kINetSupportIID, (void**)&support))) {
          *aReturn = support->Confirm(str);
          NS_RELEASE(support);
        }
#endif
        NS_RELEASE(rootContainer);
      }
      NS_RELEASE(rootWebShell);
    }
  }
  return ret;
}

NS_IMETHODIMP    
GlobalWindowImpl::Prompt(JSContext *cx, jsval *argv, PRUint32 argc, nsString& aReturn)
{
  nsresult ret = NS_OK;
  nsAutoString str, initial;

  aReturn.Truncate();
  if (nsnull != mWebShell) {
    if (argc > 0) {
      nsJSUtils::nsConvertJSValToString(str, cx, argv[0]);
      
      if (argc > 1) {
        nsJSUtils::nsConvertJSValToString(initial, cx, argv[1]);
      }
      else {
        initial.SetString("undefined");
      }
    }
    
    nsIWebShell *rootWebShell;
    ret = mWebShell->GetRootWebShellEvenIfChrome(rootWebShell);
    if (nsnull != rootWebShell) {
      nsIWebShellContainer *rootContainer;
      ret = rootWebShell->GetContainer(rootContainer);
      if (nsnull != rootContainer) {
#ifdef NECKO
        nsIPrompt *prompter;
        if (NS_OK == (ret = rootContainer->QueryInterface(nsIPrompt::GetIID(), (void**)&prompter))) {
          PRBool b;
          PRUnichar* uniResult = nsnull;
          ret = prompter->Prompt(str.GetUnicode(), initial.GetUnicode(), &uniResult, &b);
          aReturn = uniResult;
          if (NS_FAILED(ret) || !b) {
            // XXX Need to check return value and return null if the
            // user hits cancel. Currently, we can only return a 
            // string reference.
            aReturn.SetString("");
          }
          NS_RELEASE(prompter);
        }
#else
        nsINetSupport *support;
        if (NS_OK == (ret = rootContainer->QueryInterface(kINetSupportIID, (void**)&support))) {
          if (!support->Prompt(str, initial, aReturn)) {
            // XXX Need to check return value and return null if the
            // user hits cancel. Currently, we can only return a 
            // string reference.
            aReturn.SetString("");
          }
          NS_RELEASE(support);
        }
#endif
        NS_RELEASE(rootContainer);
      }
      NS_RELEASE(rootWebShell);
    }
  }
  return ret;
}

NS_IMETHODIMP
GlobalWindowImpl::Focus()
{
  nsIBrowserWindow *browser;
  if (NS_OK == GetBrowserWindowInterface(browser)) {
    browser->Show();
    NS_RELEASE( browser);
  }

  nsresult result = NS_OK;

  nsIContentViewer *viewer = nsnull;
  mWebShell->GetContentViewer(&viewer);
  if (viewer) {
    nsIDocumentViewer* docv = nsnull;
    viewer->QueryInterface(kIDocumentViewerIID, (void**) &docv);
    if (nsnull != docv) {
      nsIPresContext* cx = nsnull;
      docv->GetPresContext(cx);
      if (nsnull != cx) {
        nsIPresShell  *shell = nsnull;
        cx->GetShell(&shell);
        if (nsnull != shell) {
          nsIViewManager  *vm = nsnull;
          shell->GetViewManager(&vm);
          if (nsnull != vm) {
            nsIView *rootview = nsnull;
            vm->GetRootView(rootview);
            if (rootview) {
              nsIWidget* widget;
              rootview->GetWidget(widget);
              if (widget) {
                result = widget->SetFocus();
                NS_RELEASE(widget);
              }
            }
            NS_RELEASE(vm);
          }
          NS_RELEASE(shell);
        }
        NS_RELEASE(cx);
      }
      NS_RELEASE(docv);
    }
    NS_RELEASE(viewer);
  }

  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::Blur()
{
  nsresult result = NS_OK;
  if (nsnull != mWebShell) {
    result = mWebShell->RemoveFocus();
  }
  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::Close()
{
  // Basic security check.  If window has opener and therefore was opened from JS it can be
  // closed.  Need to add additional checks and privilege based closing
  if (nsnull != mOpener) {
    nsIBrowserWindow *mBrowser;
    if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
      mBrowser->Close();
      NS_RELEASE(mBrowser);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Forward()
{
  nsresult result = NS_OK;
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    //XXX tbi
    //mBrowser->Forward();
    NS_RELEASE(mBrowser);
  } else if (nsnull != mWebShell) {
    result = mWebShell->Forward(); // I added this - rods
  }
  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::Back()
{
  nsresult result = NS_OK;
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    //XXX tbi
    //mBrowser->Back();
    NS_RELEASE(mBrowser);
  }  else if (nsnull != mWebShell) {
    result = mWebShell->Back();// I added this - rods
  }
  return result;  
}

NS_IMETHODIMP
GlobalWindowImpl::Home()
{
  char *url = nsnull;
  nsresult rv = NS_OK;
  nsString homeURL;

  if (nsnull == mWebShell) {
    return rv;
  }

  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
  if (NS_FAILED(rv) || !prefs) {
    return rv;
  }

  // if we get here, we know prefs is not null
  rv = prefs->CopyCharPref(PREF_BROWSER_STARTUP_HOMEPAGE, &url);
  if (NS_FAILED(rv) || (!url)) {
    // if all else fails, use this
#ifdef DEBUG_seth
    printf("all else failed.  using %s as the home page\n",DEFAULT_HOME_PAGE);
#endif
    homeURL = DEFAULT_HOME_PAGE;
  }
  else {
    homeURL = url;
  }
  PR_FREEIF(url);
  PRUnichar* urlToLoad = homeURL.ToNewUnicode();
  rv = mWebShell->LoadURL(urlToLoad);
  delete[] urlToLoad;
  return rv;
}

NS_IMETHODIMP
GlobalWindowImpl::Stop()
{
  nsresult result = NS_OK;
  if (nsnull != mWebShell) {
    result = mWebShell->Stop();
  }
  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::Print()
{
  nsresult result = NS_OK;
  if (nsnull != mWebShell) {
    nsIContentViewer *viewer = nsnull;
    
    mWebShell->GetContentViewer(&viewer);
    
    if (nsnull != viewer) {
      result = viewer->Print();
      NS_RELEASE(viewer);
    }
  }

  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::MoveTo(PRInt32 aXPos, PRInt32 aYPos)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    mBrowser->MoveTo(aXPos, aYPos);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::MoveBy(PRInt32 aXDif, PRInt32 aYDif)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);

    mBrowser->MoveTo(r.x + aXDif, r.y + aYDif);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::ResizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    mBrowser->SizeWindowTo(aWidth, aHeight);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::ResizeBy(PRInt32 aWidthDif, PRInt32 aHeightDif)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);

    mBrowser->SizeWindowTo(r.width + aWidthDif, r.height + aHeightDif);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::ScrollTo(PRInt32 aXScroll, PRInt32 aYScroll)
{
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::ScrollBy(PRInt32 aXScrollDif, PRInt32 aYScrollDif)
{
  return NS_OK;
}

nsresult
GlobalWindowImpl::ClearTimeoutOrInterval(PRInt32 aTimerID)
{
  PRUint32 public_id;
  nsTimeoutImpl **top, *timeout;

  public_id = (PRUint32)aTimerID;
  if (!public_id)    /* id of zero is reserved for internal use */
    return NS_ERROR_FAILURE;
  for (top = &mTimeouts; (timeout = *top) != NULL; top = &timeout->next) {
    if (timeout->public_id == public_id) {
      if (mRunningTimeout == timeout) {
        /* We're running from inside the timeout.  Mark this
           timeout for deferred deletion by the code in
           win_run_timeout() */
        timeout->interval = 0;
      } 
      else {
        /* Delete the timeout from the pending timeout list */
        *top = timeout->next;
        if (timeout->timer) {
          timeout->timer->Cancel();
          NS_RELEASE(timeout->timer);
          DropTimeout(timeout);
        }
        DropTimeout(timeout);
      }
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
GlobalWindowImpl::ClearTimeout(PRInt32 aTimerID)
{
  return ClearTimeoutOrInterval(aTimerID);
}

NS_IMETHODIMP    
GlobalWindowImpl::ClearInterval(PRInt32 aTimerID)
{
  return ClearTimeoutOrInterval(aTimerID);
}

void
GlobalWindowImpl::ClearAllTimeouts()
{
  nsTimeoutImpl *timeout, *next;

  for (timeout = mTimeouts; timeout; timeout = next) {
    /* If RunTimeout() is higher up on the stack for this
       window, e.g. as a result of document.write from a timeout,
       then we need to reset the list insertion point for
       newly-created timeouts in case the user adds a timeout,
       before we pop the stack back to RunTimeout. */
    if (mRunningTimeout == timeout) {
      mTimeoutInsertionPoint = nsnull;
    }
        
    next = timeout->next;
    if (timeout->timer) {
      timeout->timer->Cancel();
      NS_RELEASE(timeout->timer);
      // Drop the count since the timer isn't going to hold on
      // anymore.
      DropTimeout(timeout);
    }
    // Drop the count since we're removing it from the list.
    DropTimeout(timeout);
  }
   
  mTimeouts = NULL;
}

void
GlobalWindowImpl::HoldTimeout(nsTimeoutImpl *aTimeout)
{
  aTimeout->ref_count++;
}

void
GlobalWindowImpl::DropTimeout(nsTimeoutImpl *aTimeout,
                              nsIScriptContext* aContext)
{
  JSContext *cx;
  
  if (--aTimeout->ref_count > 0) {
    return;
  }
  
  cx = (JSContext *)(aContext ? aContext : mContext)->GetNativeContext();
  
  if (aTimeout->expr) {
    JS_RemoveRoot(cx, &aTimeout->expr);
  }
  else if (aTimeout->funobj) {
    JS_RemoveRoot(cx, &aTimeout->funobj);
    if (aTimeout->argv) {
      int i;
      for (i = 0; i < aTimeout->argc; i++)
        JS_RemoveRoot(cx, &aTimeout->argv[i]);
      PR_FREEIF(aTimeout->argv);
    }
  }
  NS_IF_RELEASE(aTimeout->timer);
  PR_FREEIF(aTimeout->filename);
  NS_IF_RELEASE(aTimeout->window);
  PR_DELETE(aTimeout);
}

void
GlobalWindowImpl::InsertTimeoutIntoList(nsTimeoutImpl **aList, 
                                        nsTimeoutImpl *aTimeout)
{
  nsTimeoutImpl *to;
  
  while ((to = *aList) != nsnull) {
    if (LL_CMP(to->when, >, aTimeout->when))
      break;
    aList = &to->next;
  }
  aTimeout->next = to;
  *aList = aTimeout;
  // Increment the ref_count since we're in the list
  HoldTimeout(aTimeout);
}

void
nsGlobalWindow_RunTimeout(nsITimer *aTimer, void *aClosure)
{
  nsTimeoutImpl *timeout = (nsTimeoutImpl *)aClosure;

  if (timeout->window->RunTimeout(timeout)) {
    // Drop the ref_count since the timer won't be holding on to the
    // timeout struct anymore
    timeout->window->DropTimeout(timeout);
  }
}

PRBool
GlobalWindowImpl::RunTimeout(nsTimeoutImpl *aTimeout)
{
    nsTimeoutImpl *next, *timeout;
    nsTimeoutImpl *last_expired_timeout;
    nsTimeoutImpl dummy_timeout;
    JSContext *cx;
    PRInt64 now;
    jsval result;
    nsITimer *timer;

    /* Make sure that the window or the script context don't go away as 
       a result of running timeouts */
    GlobalWindowImpl* temp = this;
    NS_ADDREF(temp);
    nsIScriptContext* tempContext = mContext;
    NS_ADDREF(tempContext);

    timer = aTimeout->timer;
    cx = (JSContext *)mContext->GetNativeContext();

    /* A native timer has gone off.  See which of our timeouts need
       servicing */
    LL_I2L(now, PR_IntervalNow());

    /* The timeout list is kept in deadline order.  Discover the
       latest timeout whose deadline has expired. On some platforms,
       native timeout events fire "early", so we need to test the
       timer as well as the deadline. */
    last_expired_timeout = nsnull;
    for (timeout = mTimeouts; timeout; timeout = timeout->next) {
        if ((timeout == aTimeout) || !LL_CMP(timeout->when, >, now))
            last_expired_timeout = timeout;
    }

    /* Maybe the timeout that the event was fired for has been deleted
       and there are no others timeouts with deadlines that make them
       eligible for execution yet.  Go away. */
    if (!last_expired_timeout) {
      NS_RELEASE(temp);
      NS_RELEASE(tempContext);
      return PR_TRUE;
    }

    /* Insert a dummy timeout into the list of timeouts between the portion
       of the list that we are about to process now and those timeouts that
       will be processed in a future call to win_run_timeout().  This dummy
       timeout serves as the head of the list for any timeouts inserted as
       a result of running a timeout. */
    dummy_timeout.timer = NULL;
    dummy_timeout.public_id = 0;
    dummy_timeout.next = last_expired_timeout->next;
    last_expired_timeout->next = &dummy_timeout;

    /* Don't let ClearWindowTimeouts throw away our stack-allocated
       dummy timeout. */
    dummy_timeout.ref_count = 2;
   
    mTimeoutInsertionPoint = &dummy_timeout.next;
    
    for (timeout = mTimeouts; timeout != &dummy_timeout; timeout = next) {
      next = timeout->next;

      /* Hold the timeout in case expr or funobj releases its doc. */
      HoldTimeout(timeout);
      mRunningTimeout = timeout;

      if (timeout->expr) {
        /* Evaluate the timeout expression. */
#if 0
        // V says it would be nice if we could have a chokepoint
        // for calling scripts instead of making a bunch of
        // ScriptEvaluated() calls to clean things up. MMP
        PRBool isundefined;
        mContext->EvaluateString(nsAutoString(timeout->expr),
                                 timeout->filename,
                                 timeout->lineno, nsAutoString(""), &isundefined);
#endif
        JS_EvaluateUCScriptForPrincipals(cx,
                                         (JSObject *)mScriptObject,
                                         timeout->principals,
                                         JS_GetStringChars(timeout->expr),
                                         JS_GetStringLength(timeout->expr),
                                         timeout->filename,
                                         timeout->lineno,
                                         &result);
      } 
      else {
        PRInt64 lateness64;
        PRInt32 lateness;

        /* Add "secret" final argument that indicates timeout
           lateness in milliseconds */
        LL_SUB(lateness64, now, timeout->when);
        LL_L2I(lateness, lateness64);
        lateness = PR_IntervalToMilliseconds(lateness);
        timeout->argv[timeout->argc] = INT_TO_JSVAL((jsint)lateness);
        JS_CallFunctionValue(cx, (JSObject *)mScriptObject,
                             OBJECT_TO_JSVAL(timeout->funobj),
                             timeout->argc + 1, timeout->argv, &result);
      }

      tempContext->ScriptEvaluated();

      mRunningTimeout = nsnull;
      /* If the temporary reference is the only one that is keeping
         the timeout around, the document was released and we should
         restart this function. */
      if (timeout->ref_count == 1) {
        DropTimeout(timeout, tempContext);
        NS_RELEASE(temp);
        NS_RELEASE(tempContext);
        return PR_FALSE;
      }
      DropTimeout(timeout, tempContext);

      /* If we have a regular interval timer, we re-fire the
       *  timeout, accounting for clock drift.
       */
      if (timeout->interval) {
        /* Compute time to next timeout for interval timer. */
        PRInt32 delay32;
        PRInt64 interval, delay;
        LL_I2L(interval, PR_MillisecondsToInterval(timeout->interval));
        LL_ADD(timeout->when, timeout->when, interval);
        LL_I2L(now, PR_IntervalNow());
        LL_SUB(delay, timeout->when, now);
        LL_L2I(delay32, delay);

        /* If the next interval timeout is already supposed to
         *  have happened then run the timeout immediately.
         */
        if (delay32 < 0) {
          delay32 = 0;
        }
        delay32 = PR_IntervalToMilliseconds(delay32);

        NS_IF_RELEASE(timeout->timer);

        /* Reschedule timeout.  Account for possible error return in
           code below that checks for zero toid. */
        nsresult err = NS_NewTimer(&timeout->timer);
        if (NS_OK != err) {
          NS_RELEASE(temp);
          NS_RELEASE(tempContext);
          return PR_TRUE;
        } 
        
        err = timeout->timer->Init(nsGlobalWindow_RunTimeout, timeout, 
                                   delay32);
        if (NS_OK != err) {
          NS_RELEASE(temp);
          NS_RELEASE(tempContext);
          return PR_TRUE;
        } 
        // Increment ref_count to indicate that this timer is holding
        // on to the timeout struct.
        HoldTimeout(timeout);
      }

      /* Running a timeout can cause another timeout to be deleted,
         so we need to reset the pointer to the following timeout. */
      next = timeout->next;
      mTimeouts = next;
      // Drop timeout struct since it's out of the list
      DropTimeout(timeout, tempContext);

      /* Free the timeout if this is not a repeating interval
       *  timeout (or if it was an interval timeout, but we were
       *  unsuccessful at rescheduling it.)
       */
      if (timeout->interval && timeout->timer) {
        /* Reschedule an interval timeout */
        /* Insert interval timeout onto list sorted in deadline order. */
        InsertTimeoutIntoList(mTimeoutInsertionPoint, timeout);
      }
    }

    /* Take the dummy timeout off the head of the list */
    mTimeouts = dummy_timeout.next;
    mTimeoutInsertionPoint = nsnull;

    /* Get rid of our temporary reference to ourselves and the 
       script context */
    NS_RELEASE(temp);
    NS_RELEASE(tempContext);
    return PR_TRUE;
}

static const char *kSetIntervalStr = "setInterval";
static const char *kSetTimeoutStr = "setTimeout";

nsresult
GlobalWindowImpl::SetTimeoutOrInterval(JSContext *cx,
                                       jsval *argv, 
                                       PRUint32 argc, 
                                       PRInt32* aReturn,
                                       PRBool aIsInterval)
{
  JSString *expr = nsnull;
  JSObject *funobj = nsnull;
  nsTimeoutImpl *timeout, **insertion_point;
  jsdouble interval;
  PRInt64 now, delta;
  JSPrincipals* principals;

  if (NS_FAILED(GetPrincipals((void**)&principals))) {
    return NS_ERROR_FAILURE;
  }

  if (argc < 2) {
    JS_ReportError(cx, "Function %s requires at least 2 parameters",
                   aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_FAILURE;
  }

  if (!JS_ValueToNumber(cx, argv[1], &interval)) {
    JS_ReportError(cx,
                   "Second argument to %s must be a millisecond interval",
                   aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_ILLEGAL_VALUE;
  }

  switch (JS_TypeOfValue(cx, argv[0])) {
    case JSTYPE_FUNCTION:
      funobj = JSVAL_TO_OBJECT(argv[0]);
      break;
    case JSTYPE_STRING:
    case JSTYPE_OBJECT:
      if (!(expr = JS_ValueToString(cx, argv[0])))
        return NS_ERROR_FAILURE;
      if (nsnull == expr)
        return NS_ERROR_OUT_OF_MEMORY;
      argv[0] = STRING_TO_JSVAL(expr);
      break;
    default:
      JS_ReportError(cx, "useless %s call (missing quotes around argument?)",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
      return NS_ERROR_FAILURE;
  }

  timeout = PR_NEWZAP(nsTimeoutImpl);
  if (nsnull == timeout)
    return NS_ERROR_OUT_OF_MEMORY;

  // Initial ref_count to indicate that this timeout struct will
  // be held as the closure of a timer.
  timeout->ref_count = 1;
  if (aIsInterval)
    timeout->interval = (PRInt32)interval;
  if (expr) {
    if (!JS_AddNamedRoot(cx, &timeout->expr, "timeout.expr")) {
      PR_DELETE(timeout);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    timeout->expr = expr;
  }
  else if (funobj) {
    int i;

    /* Leave an extra slot for a secret final argument that
       indicates to the called function how "late" the timeout is. */
    timeout->argv = (jsval *)PR_MALLOC((argc - 1) * sizeof(jsval));
    if (nsnull == timeout->argv) {
      DropTimeout(timeout);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!JS_AddNamedRoot(cx, &timeout->funobj, "timeout.funobj")) {
      DropTimeout(timeout);
      return NS_ERROR_FAILURE;
    }
    timeout->funobj = funobj;
          
    timeout->argc = 0;
    for (i = 2; (PRUint32)i < argc; i++) {
      timeout->argv[i - 2] = argv[i];
      if (!JS_AddNamedRoot(cx, &timeout->argv[i - 2], "timeout.argv[i]")) {
        DropTimeout(timeout);
        return NS_ERROR_FAILURE;
      }
      timeout->argc++;
    }
  }

  timeout->principals = principals;

  LL_I2L(now, PR_IntervalNow());
  LL_D2L(delta, PR_MillisecondsToInterval((PRUint32)interval));
  LL_ADD(timeout->when, now, delta);

  nsresult err = NS_NewTimer(&timeout->timer);
  if (NS_OK != err) {
    DropTimeout(timeout);
    return err;
  } 
  
  err = timeout->timer->Init(nsGlobalWindow_RunTimeout, timeout, 
                             (PRInt32)interval);
  if (NS_OK != err) {
    DropTimeout(timeout);
    return err;
  } 

  timeout->window = this;
  NS_ADDREF(this);

  insertion_point = (mTimeoutInsertionPoint == NULL)
                    ? &mTimeouts
                    : mTimeoutInsertionPoint;

  InsertTimeoutIntoList(insertion_point, timeout);
  timeout->public_id = ++mTimeoutPublicIdCounter;
  *aReturn = timeout->public_id;
  
  return NS_OK;
}

NS_IMETHODIMP    
GlobalWindowImpl::SetTimeout(JSContext *cx,
                             jsval *argv, 
                             PRUint32 argc, 
                             PRInt32* aReturn)
{
  return SetTimeoutOrInterval(cx, argv, argc, aReturn, PR_FALSE);
}

NS_IMETHODIMP    
GlobalWindowImpl::SetInterval(JSContext *cx,
                              jsval *argv, 
                              PRUint32 argc, 
                              PRInt32* aReturn)
{
  return SetTimeoutOrInterval(cx, argv, argc, aReturn, PR_TRUE);
}

NS_IMETHODIMP    
GlobalWindowImpl::Open(JSContext *cx,
                       jsval *argv, 
                       PRUint32 argc, 
                       nsIDOMWindow** aReturn)
{
  return OpenInternal(cx, argv, argc, PR_FALSE, aReturn);
}

// like Open, but attaches to the new window any extra parameters past [features]
// as a JS property named "arguments"
NS_IMETHODIMP    
GlobalWindowImpl::OpenDialog(JSContext *cx,
                       jsval *argv, 
                       PRUint32 argc, 
                       nsIDOMWindow** aReturn)
{
  return OpenInternal(cx, argv, argc, PR_TRUE, aReturn);
}

nsresult    
GlobalWindowImpl::OpenInternal(JSContext *cx,
                       jsval *argv, 
                       PRUint32 argc, 
                       PRBool aAttachArguments,
                       nsIDOMWindow** aReturn)
{
  PRUint32 chromeFlags;
  nsAutoString mAbsURL, name;
  JSString* str;
  char* options;
  *aReturn = nsnull;

  if (argc > 0) {
    JSString *mJSStrURL = JS_ValueToString(cx, argv[0]);
    if (nsnull == mJSStrURL || nsnull == mDocument) {
      return NS_ERROR_FAILURE;
    }

    nsAutoString mURL, mEmpty;
    nsIURI* mDocURL = 0;
    nsIDocument* mDoc;

    mURL.SetString(JS_GetStringChars(mJSStrURL));

    if (NS_OK == mDocument->QueryInterface(kIDocumentIID, (void**)&mDoc)) {
      mDocURL = mDoc->GetDocumentURL();
      NS_RELEASE(mDoc);
    }

#ifndef NECKO    
    if (NS_OK != NS_MakeAbsoluteURL(mDocURL, mEmpty, mURL, mAbsURL)) {
      return NS_ERROR_FAILURE;
    }
#else
    nsresult rv;
    nsIURI *baseUri = nsnull;
    rv = mDocURL->QueryInterface(nsIURI::GetIID(), (void**)&baseUri);
    if (NS_FAILED(rv)) return rv;

    rv = NS_MakeAbsoluteURI(mURL, baseUri, mAbsURL);
    NS_RELEASE(baseUri);
    if (NS_FAILED(rv)) return rv;
#endif // NECKO

  }
  
  /* Sanity-check the optional window_name argument. */
  if (argc > 1) {
    JSString *mJSStrName = JS_ValueToString(cx, argv[1]);
    if (nsnull == mJSStrName) {
      return NS_ERROR_FAILURE;
    }
    name.SetString(JS_GetStringChars(mJSStrName));

    if (NS_OK != CheckWindowName(cx, name)) {
      return NS_ERROR_FAILURE;
    }
  } 
  else {
    name.SetString("");
  }

  options = nsnull;
  if (argc > 2) {
    if (!(str = JS_ValueToString(cx, argv[2]))) {
      return NS_ERROR_FAILURE;
    }
    options = JS_GetStringBytes(str);
  }
  chromeFlags = CalculateChromeFlags(options);

  nsIWebShell *newOuterShell = nsnull;
  nsIWebShellContainer *webShellContainer;

  /* XXX check for existing window of same name.  If exists, set url and 
   * update chrome */
  if ((nsnull != mWebShell) && 
      (NS_OK == mWebShell->GetContainer(webShellContainer)) && 
      (nsnull != webShellContainer)) {

    PRBool windowIsNew;

    // Check for existing window of same name.
    windowIsNew = PR_FALSE;
    webShellContainer->FindWebShellWithName(name.GetUnicode(), newOuterShell);
    if (nsnull == newOuterShell) {
                        // No window of that name, and we are allowed to create a new one now.
      webShellContainer->NewWebShell(chromeFlags, PR_FALSE, newOuterShell);
      windowIsNew = PR_TRUE;
    }

    if (nsnull != newOuterShell) {
      if (NS_SUCCEEDED(ReadyOpenedWebShell(newOuterShell, aReturn))) {
        if (aAttachArguments && argc > 3)
          AttachArguments(*aReturn, argv+3, argc-3);
        newOuterShell->SetName(name.GetUnicode());
        newOuterShell->LoadURL(mAbsURL.GetUnicode());
        SizeAndShowOpenedWebShell(newOuterShell, options, windowIsNew);
      }
      NS_RELEASE(newOuterShell);
    }
    NS_RELEASE(webShellContainer);
  }

  return NS_OK;
}

// attach the given array of JS values to the given window, as a property array
// named "arguments"
nsresult
GlobalWindowImpl::AttachArguments(nsIDOMWindow *aWindow, jsval *argv, PRUint32 argc)
{
  if (argc == 0)
    return NS_OK;

  // copy the extra parameters into a JS Array and attach it
  JSObject *args;
  JSObject *scriptObject;
  nsIScriptGlobalObject *scriptGlobal;
  nsIScriptObjectOwner *owner;
  JSContext *jsContext;
  nsIScriptContext *scriptContext;

  if (NS_SUCCEEDED(aWindow->QueryInterface(kIScriptGlobalObjectIID, (void **)&scriptGlobal))) {
    scriptGlobal->GetContext(&scriptContext);
    if (scriptContext) {
      jsContext = (JSContext *) scriptContext->GetNativeContext();
      if (NS_SUCCEEDED(aWindow->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner))) {
        owner->GetScriptObject(scriptContext, (void **) &scriptObject);
        args = JS_NewArrayObject(jsContext, argc, argv);
        if (args) {
          jsval argsVal = OBJECT_TO_JSVAL(args);
//        JS_DefineProperty(jsContext, scriptObject, "arguments",
//          argsVal, NULL, NULL, JSPROP_PERMANENT);
          JS_SetProperty(jsContext, scriptObject, "arguments", &argsVal);
        }
        NS_RELEASE(owner);
      }
      NS_RELEASE(scriptContext);
    }
    NS_RELEASE(scriptGlobal);
  }
  return NS_OK;
}


PRUint32
GlobalWindowImpl::CalculateChromeFlags(char *aFeatures) {

  PRUint32  chromeFlags;

  if (nsnull == aFeatures)
    return NS_CHROME_ALL_CHROME;

  PRBool presenceFlag = PR_FALSE;

  chromeFlags = NS_CHROME_WINDOW_BORDERS_ON;

  // ((only) titlebars default to "on" if not mentioned)
  chromeFlags |= WinHasOption(aFeatures, "titlebar", presenceFlag) ? NS_CHROME_TITLEBAR_ON : 0;
  chromeFlags |= WinHasOption(aFeatures, "close", presenceFlag) ? NS_CHROME_WINDOW_CLOSE_ON : 0;
  chromeFlags |= WinHasOption(aFeatures, "toolbar", presenceFlag) ? NS_CHROME_TOOL_BAR_ON : 0;
  chromeFlags |= WinHasOption(aFeatures, "location", presenceFlag) ? NS_CHROME_LOCATION_BAR_ON : 0;
  chromeFlags |= (WinHasOption(aFeatures, "directories", presenceFlag) | WinHasOption(aFeatures, "personalbar", presenceFlag))
    ? NS_CHROME_PERSONAL_TOOLBAR_ON : 0;
  chromeFlags |= WinHasOption(aFeatures, "status", presenceFlag) ? NS_CHROME_STATUS_BAR_ON : 0;
  chromeFlags |= WinHasOption(aFeatures, "menubar", presenceFlag) ? NS_CHROME_MENU_BAR_ON : 0;
  chromeFlags |= WinHasOption(aFeatures, "scrollbars", presenceFlag) ? NS_CHROME_SCROLLBARS_ON : 0;
  chromeFlags |= WinHasOption(aFeatures, "resizable", presenceFlag) ? NS_CHROME_WINDOW_RESIZE_ON : 0;
  // default to "on" if titlebar, closebox, or resizable aren't mentioned
  if (!PL_strcasestr(aFeatures, "titlebar"))
    chromeFlags |= NS_CHROME_TITLEBAR_ON;
  if (!PL_strcasestr(aFeatures, "close"))
    chromeFlags |= NS_CHROME_WINDOW_CLOSE_ON;
  if (!PL_strcasestr(aFeatures, "resizable"))
    chromeFlags |= NS_CHROME_WINDOW_RESIZE_ON;

  // From this point onward, if the above features weren't specified at all,
  // we will assume that all chrome is present.

  //XXX This is incorrect.  Except for the last three, if the
  //    feature wasn't mentioned its not there -joki
  //if (!presenceFlag) 
  //  chromeFlags |= NS_CHROME_ALL_CHROME;

  chromeFlags |= WinHasOption(aFeatures, "chrome", presenceFlag) ? NS_CHROME_OPEN_AS_CHROME : 0;
  chromeFlags |= WinHasOption(aFeatures, "modal", presenceFlag) ? NS_CHROME_MODAL : 0;
  chromeFlags |= WinHasOption(aFeatures, "dialog", presenceFlag) ? NS_CHROME_OPEN_AS_DIALOG : 0;

  /*z-ordering, history, dependent
  chromeFlags->topmost         = WinHasOption(aFeatures, "alwaysRaised");
  chromeFlags->bottommost              = WinHasOption(aFeatures, "alwaysLowered");
  chromeFlags->z_lock          = WinHasOption(aFeatures, "z-lock");
  chromeFlags->is_modal            = WinHasOption(aFeatures, "modal");
  chromeFlags->hide_title_bar  = !(WinHasOption(aFeatures, "titlebar"));
  chromeFlags->dependent              = WinHasOption(aFeatures, "dependent");
  chromeFlags->copy_history           = FALSE;
  */

  /* Allow disabling of commands only if there is no menubar */
  /*if (!chromeFlags & NS_CHROME_MENU_BAR_ON) {
      chromeFlags->disable_commands = !WinHasOption(aFeatures, "hotkeys");
      if (XP_STRCASESTR(aFeatures,"hotkeys")==NULL)
          chromeFlags->disable_commands = FALSE;
  }*/

  /* XXX Add security check on z-ordering, modal, hide title, disable hotkeys */

  /* XXX Add security check for sizing and positioning.  
   * See mozilla/lib/libmocha/lm_win.c for current constraints */

  return chromeFlags;
}

// set the newly opened webshell's (window) size, and show it
nsresult
GlobalWindowImpl::SizeAndShowOpenedWebShell(nsIWebShell *aOuterShell, char *aFeatures,
                                            PRBool aNewWindow)
{
  if (nsnull == aOuterShell)
    return NS_ERROR_NULL_POINTER;

  nsRect           defaultBounds;
  PRInt32          top = 0, left = 0, height = 0, width = 0;
  nsIBrowserWindow *thisWindow,
                   *openedWindow = nsnull;

  // use this window's size as the default
  if (aNewWindow && NS_SUCCEEDED(GetBrowserWindowInterface(thisWindow))) {
    thisWindow->GetWindowBounds(defaultBounds);
    NS_RELEASE(thisWindow);
  }

  // get the nsIBrowserWindow corresponding to the given aOuterShell
  nsIWebShell *rootShell;
  aOuterShell->GetRootWebShellEvenIfChrome(rootShell);
  if (nsnull != rootShell) {
    nsIWebShellContainer *newContainer;
    rootShell->GetContainer(newContainer);
    if (nsnull != newContainer) {
      if (NS_FAILED(newContainer->QueryInterface(kIBrowserWindowIID, (void**)&openedWindow)))
        openedWindow = nsnull;
        NS_RELEASE(newContainer);
      }
      NS_RELEASE(rootShell);
    }

  // set size
  if (nsnull != openedWindow) {

    nsRect   contentOffsets; // constructor sets all values to 0
    PRBool   sizeSpecified = PR_FALSE;
    PRUint32 chromeFlags = CalculateChromeFlags(aFeatures);
    PRBool   openAsContent = ((chromeFlags & NS_CHROME_OPEN_AS_CHROME) == 0);

    // if it's an extant window, we are already our default size
    if (!aNewWindow)
      if (openAsContent) {
        // defaultBounds are the content rect. also, save window size diffs
        openedWindow->GetWindowBounds(contentOffsets);
        openedWindow->GetContentBounds(defaultBounds);
        contentOffsets.x -= defaultBounds.x;
        contentOffsets.y -= defaultBounds.y;
        contentOffsets.width -= defaultBounds.width;
        contentOffsets.height -= defaultBounds.height;
      } else
        openedWindow->GetWindowBounds(defaultBounds);

    if (nsnull != aFeatures) {
      PRBool presenceFlag = PR_FALSE; // Unused. Yuck.
      if (openAsContent) {
        width = WinHasOption(aFeatures, "innerWidth", presenceFlag) | WinHasOption(aFeatures, "width", presenceFlag);
        height = WinHasOption(aFeatures, "innerHeight", presenceFlag) | WinHasOption(aFeatures, "height", presenceFlag);
      }
      else {
        // Chrome. Look for outerWidth, outerHeight, or width/height
        width = WinHasOption(aFeatures, "outerWidth", presenceFlag) | WinHasOption(aFeatures, "width", presenceFlag);
        height = WinHasOption(aFeatures, "outerHeight", presenceFlag) | WinHasOption(aFeatures, "height", presenceFlag);
      }

      left = WinHasOption(aFeatures, "left", presenceFlag) | WinHasOption(aFeatures, "screenX", presenceFlag);
      top = WinHasOption(aFeatures, "top", presenceFlag) | WinHasOption(aFeatures, "screenY", presenceFlag);

      if (left)
        defaultBounds.x = left;
      if (top)
        defaultBounds.y = top;
      if (width) {
        sizeSpecified = PR_TRUE;
        defaultBounds.width = width;
      }
      if (height) {
        sizeSpecified = PR_TRUE;
        defaultBounds.height = height;
      }
    }

    // beard: don't resize/reposition the window if it is the same web shell.
    if (aOuterShell != mWebShell) {

      // whimper. special treatment for windows which will be intrinsically sized.
      // we can count on a Show() coming through at EndDocumentLoad time, and we
      // can count on their size being wrong at this point, and flashing.  so
      // delay some things, if this is true.
      PRBool sizeLater = PR_FALSE;
      openedWindow->IsIntrinsicallySized(sizeLater);

      if (openAsContent) {
        openedWindow->SizeWindowTo(defaultBounds.width + contentOffsets.width,
                                   defaultBounds.height + contentOffsets.height);
        // oy. sizing the content makes sense: that's what the user asked for,
        // however, it doesn't have the desired effect because all sizing
        // functions eventually end up sizing the window, and the subwindows
        // used to calculate the appropriate deltas are different. here we use
        // the HTML content area; the window uses the main webshell.  maybe
        // i could calculate a triple offset, but commenting it out is easier,
        // and probably effectively the same thing.
//      openedWindow->SizeContentTo(defaultBounds.width, defaultBounds.height);
      } else if (sizeSpecified)
        openedWindow->SizeWindowTo(defaultBounds.width, defaultBounds.height);

      openedWindow->MoveTo(defaultBounds.x + contentOffsets.x,
                      defaultBounds.y + contentOffsets.y);

      if (sizeLater)
        openedWindow->ShowAfterCreation();
      else
        openedWindow->Show();
    }

    NS_RELEASE(openedWindow);
  }
  return NS_OK;
}

// return the nsIDOMWindow corresponding to the given nsIWebShell
// note this forces the creation of a script context, if one has not already been created
// note it also sets the window's opener to this -- because it's just convenient, that's all
nsresult
GlobalWindowImpl::ReadyOpenedWebShell(nsIWebShell *aWebShell, nsIDOMWindow **aDOMWindow)
{
  nsIScriptContextOwner *newContextOwner = nsnull;
  nsIScriptGlobalObject *newGlobalObject = nsnull;
  nsresult              res;

  *aDOMWindow = nsnull;
  res = aWebShell->QueryInterface(kIScriptContextOwnerIID, (void**)&newContextOwner);
  if (NS_SUCCEEDED(res)) {
    res = newContextOwner->GetScriptGlobalObject(&newGlobalObject);
    if (NS_SUCCEEDED(res)) {
      res = newGlobalObject->QueryInterface(kIDOMWindowIID, (void**)aDOMWindow);
      newGlobalObject->SetOpenerWindow(this); // damnit
      NS_RELEASE(newGlobalObject);
    }
    NS_RELEASE(newContextOwner);
  }
  return res;
}

// simple utility conversion routine
nsresult
GlobalWindowImpl::WebShellToDOMWindow(nsIWebShell *aWebShell, nsIDOMWindow **aDOMWindow)
{
  nsresult rv;

  NS_ASSERTION(aWebShell, "null in param to WebShellToDOMWindow");
  NS_ASSERTION(aDOMWindow, "null out param to WebShellToDOMWindow");

  *aDOMWindow = nsnull;

  nsCOMPtr<nsIScriptContextOwner> owner = do_QueryInterface(aWebShell, &rv);
  if (owner) {
    nsCOMPtr<nsIScriptGlobalObject> scriptobj;
    rv = owner->GetScriptGlobalObject(getter_AddRefs(scriptobj));
    if (NS_SUCCEEDED(rv) && scriptobj)
      rv = scriptobj->QueryInterface(nsIDOMWindow::GetIID(), (void **) aDOMWindow);
  }
  return rv;
}

NS_IMETHODIMP
GlobalWindowImpl::CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent, 
                              PRInt32 aXPos, PRInt32 aYPos, 
                              const nsString& aPopupType, 
                              const nsString& anAnchorAlignment, const nsString& aPopupAlignment,
                              nsIDOMWindow** outPopup)
{
  if (nsnull != mWebShell) {
    // Pass this off to the parent.
    nsCOMPtr<nsIWebShellContainer> webShellContainer = do_QueryInterface(mWebShell);
    if (webShellContainer) {
      webShellContainer->CreatePopup(aElement, aPopupContent, aXPos, aYPos, aPopupType,
                                     anAnchorAlignment, aPopupAlignment, this, outPopup);
    }
  }
  return NS_OK;
}

nsresult 
GlobalWindowImpl::CheckWindowName(JSContext *cx, nsString& aName)
{
  PRInt32 index;
  PRUnichar mChar;

  for (index = 0; index < aName.Length(); index++) {
    mChar = aName.CharAt(index);
    if (!nsString::IsAlpha(mChar) && !nsString::IsDigit(mChar) && mChar != '_') {
      char* cp = aName.ToNewCString();
      JS_ReportError(cx,
        "illegal character '%c' ('\\%o') in window name %s",
        mChar, mChar, cp);
      delete [] cp;
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

PRInt32 
GlobalWindowImpl::WinHasOption(char *options, char *name, PRBool& aPresenceFlag)
{
  char *comma, *equal;
  PRInt32 found = 0;

  for (;;) {
    comma = strchr(options, ',');
    if (comma) *comma = '\0';
    equal = strchr(options, '=');
    if (equal) *equal = '\0';
    if (nsCRT::strcasecmp(options, name) == 0) {
      aPresenceFlag = PR_TRUE;
      if (!equal || nsCRT::strcasecmp(equal + 1, "yes") == 0)
        found = 1;
      else
        found = atoi(equal + 1);
    }
    if (equal) *equal = '=';
    if (comma) *comma = ',';
    if (found || !comma)
      break;
    options = comma + 1;
  }
  return found;
}

nsresult 
GlobalWindowImpl::GetBrowserWindowInterface(nsIBrowserWindow*& aBrowser)
{
  nsresult ret = NS_ERROR_FAILURE;
  
  if (nsnull != mWebShell) {
    nsIWebShell *mRootWebShell;
    mWebShell->GetRootWebShellEvenIfChrome(mRootWebShell);
    if (nsnull != mRootWebShell) {
      nsIWebShellContainer *mRootContainer;
      mRootWebShell->GetContainer(mRootContainer);
      if (nsnull != mRootContainer) {
        ret = mRootContainer->QueryInterface(kIBrowserWindowIID, (void**)&aBrowser);
        NS_RELEASE(mRootContainer);
      }
      NS_RELEASE(mRootWebShell);
    }
  }
  return ret;
}

PRBool
GlobalWindowImpl::CheckForEventListener(JSContext *aContext, nsString& aPropName)
{
  nsIEventListenerManager *mManager = nsnull;

  if (aPropName == "onmousedown" || aPropName == "onmouseup" || aPropName ==  "onclick" ||
     aPropName == "onmouseover" || aPropName == "onmouseout") {
    if (NS_OK == GetListenerManager(&mManager)) {
      nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
      if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMMouseListenerIID)) {
        NS_RELEASE(mManager);
        return PR_FALSE;
      }
    }
  }
  else if (aPropName == "onkeydown" || aPropName == "onkeyup" || aPropName == "onkeypress") {
    if (NS_OK == GetListenerManager(&mManager)) {
      nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
      if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMKeyListenerIID)) {
        NS_RELEASE(mManager);
        return PR_FALSE;
      }
    }
  }
  else if (aPropName == "onmousemove") {
    if (NS_OK == GetListenerManager(&mManager)) {
      nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
      if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMMouseMotionListenerIID)) {
        NS_RELEASE(mManager);
        return PR_FALSE;
      }
    }
  }
  else if (aPropName == "onfocus" || aPropName == "onblur") {
    if (NS_OK == GetListenerManager(&mManager)) {
      nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
      if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMFocusListenerIID)) {
        NS_RELEASE(mManager);
        return PR_FALSE;
      }
    }
  }
  else if (aPropName == "onsubmit" || aPropName == "onreset" || aPropName == "onchange") {
    if (NS_OK == GetListenerManager(&mManager)) {
      nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
      if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMFormListenerIID)) {
        NS_RELEASE(mManager);
        return PR_FALSE;
      }
    }
  }
  else if (aPropName == "onload" || aPropName == "onunload" || aPropName == "onabort" ||
           aPropName == "onerror") {
    if (NS_OK == GetListenerManager(&mManager)) {
      nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
      if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMLoadListenerIID)) {
        NS_RELEASE(mManager);
        return PR_FALSE;
      }
    }
  }
  else if (aPropName == "onpaint") {
    if (NS_OK == GetListenerManager(&mManager)) {
      nsIScriptContext *mScriptCX = (nsIScriptContext *)
        JS_GetContextPrivate(aContext);
      if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this,
                                                     kIDOMPaintListenerIID)) {
        NS_RELEASE(mManager);
        return PR_FALSE;
      }
    }
  }
  NS_IF_RELEASE(mManager);

  return PR_TRUE;
}

PRBool
GlobalWindowImpl::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    nsString mPropName;
    nsAutoString mPrefix;
    mPropName.SetString(JS_GetStringChars(JS_ValueToString(aContext, aID)));
    mPrefix.SetString(mPropName.GetUnicode(), 2);
    if (mPrefix == "on") {
      return CheckForEventListener(aContext, mPropName);
    }
  }
  return PR_TRUE;
}

PRBool    
GlobalWindowImpl::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    
GlobalWindowImpl::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  if (JSVAL_IS_STRING(aID)) {
    char* cString = JS_GetStringBytes(JS_ValueToString(aContext, aID));
    if (PL_strcmp("location", cString) == 0) {
      nsIDOMLocation *location;
    
      if (NS_OK == GetLocation(&location)) {
        if (location != nsnull) {
          nsIScriptObjectOwner *owner = nsnull;
          if (NS_OK == location->QueryInterface(kIScriptObjectOwnerIID, 
                                                (void**)&owner)) {
            JSObject *object = nsnull;
            nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(aContext);
            if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {
              // set the return value
              *aVp = OBJECT_TO_JSVAL(object);
            }
            NS_RELEASE(owner);
          }
          NS_RELEASE(location);
        }
        else {
          *aVp = JSVAL_NULL;
        }
      }
      else {
        return PR_FALSE;
      }
    }
    else if (PL_strcmp("title", cString) == 0) {
      if (mWebShell) {
        // See if we're a chrome shell.
        nsWebShellType type;
        mWebShell->GetWebShellType(type);
        if (type == nsWebShellChrome) {
          nsCOMPtr<nsIBrowserWindow> browser;
          if (NS_OK == GetBrowserWindowInterface(*getter_AddRefs(browser)) && browser) {
            // We got a browser window interface
            const PRUnichar* title;
            browser->GetTitle(&title);

            JSString* jsString = JS_NewUCStringCopyZ(aContext, (const jschar*)title);
            if (!jsString)
              return PR_FALSE;
              
            *aVp = STRING_TO_JSVAL(jsString);
          }
        }
      }
    }
  }
  return PR_TRUE;
}

PRBool
GlobalWindowImpl::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    nsString mPropName;
    nsAutoString mPrefix;
    mPropName.SetString(JS_GetStringChars(JS_ValueToString(aContext, aID)));
    mPrefix.SetString(mPropName.GetUnicode(), 2);
    if (mPrefix == "on") {
      return CheckForEventListener(aContext, mPropName);
    }
  }
  else if (JSVAL_IS_STRING(aID)) {
    char* cString = JS_GetStringBytes(JS_ValueToString(aContext, aID));
    
    if (PL_strcmp("location", cString) == 0) {
      JSString *jsstring = JS_ValueToString(aContext, *aVp);

      if (nsnull != jsstring) {
        nsIDOMLocation *location;
        nsAutoString locationStr;
      
        locationStr.SetString(JS_GetStringChars(jsstring));
        if (NS_OK == GetLocation(&location)) {
          if (NS_OK != location->SetHref(locationStr)) {
            NS_RELEASE(location);
            return PR_FALSE;
          }
          NS_RELEASE(location);
        }
        else {
          return PR_FALSE;
        }
      }
    }
    else if (PL_strcmp("title", cString) == 0) {
      if (mWebShell) {
        // See if we're a chrome shell.
        nsWebShellType type;
        mWebShell->GetWebShellType(type);
        if (type == nsWebShellChrome) {
          nsCOMPtr<nsIBrowserWindow> browser;
          if (NS_OK == GetBrowserWindowInterface(*getter_AddRefs(browser)) && browser) {
            // We got a browser window interface
            JSString *jsString = JS_ValueToString(aContext, *aVp);
            if (!jsString)
              return PR_FALSE;
            const PRUnichar* uniTitle = JS_GetStringChars(jsString);
            browser->SetTitle(uniTitle);
          }
        }
      }
    }
  }

  return PR_TRUE;
}

PRBool    
GlobalWindowImpl::EnumerateProperty(JSContext *aContext)
{
  return PR_TRUE;
}

PRBool    
GlobalWindowImpl::Resolve(JSContext *aContext, jsval aID)
{
  if (JSVAL_IS_STRING(aID)) {
    if (PL_strcmp("location", JS_GetStringBytes(JS_ValueToString(aContext, aID))) == 0) {
      ::JS_DefineProperty(aContext, (JSObject *)mScriptObject, "location",
                          JSVAL_NULL, nsnull, nsnull, 0);
    }
    else if (nsnull != mWebShell) {
      PRInt32 count;
      if (NS_SUCCEEDED(mWebShell->GetChildCount(count)) && count) {
        nsIWebShell *child = nsnull;
        nsAutoString name(JS_GetStringBytes(JS_ValueToString(aContext, aID))); 
        if (NS_SUCCEEDED(mWebShell->FindChildWithName(name.GetUnicode(), child))) {
          if (child) {
            JSObject *childObj;
            //We found a subframe of the right name.  The rest of this is to get its script object.
            nsIScriptContextOwner *contextOwner;
            if (NS_SUCCEEDED(child->QueryInterface(kIScriptContextOwnerIID, (void**)&contextOwner))) {
              nsIScriptGlobalObject *childGlobalObj;
              if (NS_SUCCEEDED(contextOwner->GetScriptGlobalObject(&childGlobalObj))) {
                nsIScriptObjectOwner *objOwner;
                  if (NS_SUCCEEDED(childGlobalObj->QueryInterface(kIScriptObjectOwnerIID, (void**)&objOwner))) {
                    nsIScriptContext *scriptContext;
                    childGlobalObj->GetContext(&scriptContext);
                    if (scriptContext) {
                      objOwner->GetScriptObject(scriptContext, (void**)&childObj);
                      NS_RELEASE(scriptContext);
                    }
                    NS_RELEASE(objOwner);
                  }
                NS_RELEASE(childGlobalObj);
              }
              NS_RELEASE(contextOwner);
            }
            //Okay, if we now have a childObj, we can define it and proceed.
            if (childObj) {
              ::JS_DefineProperty(aContext, (JSObject *)mScriptObject, 
                                  JS_GetStringBytes(JS_ValueToString(aContext, aID)),
                                  OBJECT_TO_JSVAL(childObj), nsnull, nsnull, 0);
            }
            NS_RELEASE(child);
          }
        }
      }
    }
  }

  return PR_TRUE;
}

PRBool    
GlobalWindowImpl::Convert(JSContext *aContext, jsval aID)
{
  return PR_TRUE;
}

void      
GlobalWindowImpl::Finalize(JSContext *aContext)
{
}

nsresult 
GlobalWindowImpl::GetListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  if (nsnull != mListenerManager) {
    return mListenerManager->QueryInterface(kIEventListenerManagerIID, (void**) aInstancePtrResult);;
  }
  //This is gonna get ugly.  Can't use NS_NewEventListenerManager because of a circular link problem.
  nsIDOMEventCapturer *mDoc;
  if (nsnull != mDocument && NS_OK == mDocument->QueryInterface(kIDOMEventCapturerIID, (void**)&mDoc)) {
    if (NS_OK == mDoc->GetNewListenerManager(aInstancePtrResult)) {
      mListenerManager = *aInstancePtrResult;
      NS_ADDREF(mListenerManager);
      NS_RELEASE(mDoc);
      return NS_OK;
    }
    NS_IF_RELEASE(mDoc);
  }
  return NS_ERROR_FAILURE;
}

//XXX I need another way around the circular link problem.
nsresult 
GlobalWindowImpl::GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  return NS_ERROR_FAILURE;
}

nsresult 
GlobalWindowImpl::HandleDOMEvent(nsIPresContext& aPresContext, 
                                 nsEvent* aEvent, 
                                 nsIDOMEvent** aDOMEvent,
                                 PRUint32 aFlags,
                                 nsEventStatus& aEventStatus)
{
  nsresult mRet = NS_OK;
  nsIDOMEvent* mDOMEvent = nsnull;

  if (NS_EVENT_FLAG_INIT == aFlags) {
    aDOMEvent = &mDOMEvent;
    aEvent->flags = NS_EVENT_FLAG_NONE;
  }
  
  //Capturing stage
  if (NS_EVENT_FLAG_BUBBLE != aFlags && mChromeDocument) {
    // Check chrome document capture here
    mChromeDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
  }

  //Local handling stage
  if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
    aEvent->flags = aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_CAPTURE != aFlags && mChromeDocument) {
    // Bubble to a chrome document if it exists
    // XXX Need a way to know if an event should really bubble or not.
    // For now filter out load and unload, since they cause problems.
    if (aEvent->message != NS_PAGE_LOAD && aEvent->message != NS_PAGE_UNLOAD)
      mChromeDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_BUBBLE, aEventStatus);
  }

  if (NS_EVENT_FLAG_INIT == aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event, release here.
    if (nsnull != *aDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
      //Okay, so someone in the DOM loop (a listener, JS object) still has a ref to the DOM Event but
      //the internal data hasn't been malloc'd.  Force a copy of the data here so the DOM Event is still valid.
        nsIPrivateDOMEvent *mPrivateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(kIPrivateDOMEventIID, (void**)&mPrivateEvent)) {
          mPrivateEvent->DuplicatePrivateData();
          NS_RELEASE(mPrivateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return mRet;
}

nsresult 
GlobalWindowImpl::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    mManager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult 
GlobalWindowImpl::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  if (nsnull != mListenerManager) {
    mListenerManager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
GlobalWindowImpl::AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                   PRBool aUseCapture)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
GlobalWindowImpl::RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                      PRBool aUseCapture)
{
  if (nsnull != mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult 
GlobalWindowImpl::CaptureEvent(const nsString& aType)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    //mManager->CaptureEvent(aListener);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult 
GlobalWindowImpl::ReleaseEvent(const nsString& aType)
{
  if (nsnull != mListenerManager) {
    //mListenerManager->ReleaseEvent(aListener);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::GetPrincipals(void** aPrincipals) 
{
  if (!mPrincipals) {
    if (mContext) {
      nsIScriptSecurityManager* secMan = nsnull;
      mContext->GetSecurityManager(&secMan);
      if (secMan) {
        nsAutoString codebase;
        if (NS_SUCCEEDED(GetOrigin(&codebase))) {
          secMan->NewJSPrincipals(nsnull, nsnull, &codebase, &mPrincipals);
        }
        NS_RELEASE(secMan);
      }
    }

    if (!mPrincipals) {
      return NS_ERROR_FAILURE;
    }
    if (mContext) {
      JSPRINCIPALS_HOLD((JSContext *)mContext->GetNativeContext(), mPrincipals);
    }
  }

  *aPrincipals = (void*)mPrincipals;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetPrincipals(void* aPrincipals)
{
  if (mPrincipals && mContext) {
    JSPRINCIPALS_DROP((JSContext *)mContext->GetNativeContext(), mPrincipals);
  }

  mPrincipals = (JSPrincipals*)aPrincipals;

  if (mPrincipals && mContext) {
    JSPRINCIPALS_HOLD((JSContext *)mContext->GetNativeContext(), mPrincipals);
  }

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetOrigin(nsString* aOrigin)
{
  nsIDocument* doc;
  if (mDocument && NS_OK == mDocument->QueryInterface(kIDocumentIID, (void**)&doc)) {
    nsIURI* docURL = doc->GetDocumentURL();
    if (docURL) {
#ifdef NECKO
      char* str;
      docURL->GetSpec(&str);
#else
      PRUnichar* str;
      docURL->ToString(&str);
#endif
      *aOrigin = str;
#ifdef NECKO
      nsCRT::free(str);
#else
      delete [] str;
#endif
      NS_RELEASE(docURL);
    }
    NS_RELEASE(doc);
  }


#if 0
  //Old code from 4.0 to show what funcitonality needs replicating
  History_entry *he;
  const char *address;
  JSContext *aCx;
  MochaDecoder *decoder;

  he = SHIST_GetCurrent(&context->hist);
  if (he) {
    address = he->wysiwyg_url;
    if (!address)
      address = he->address;
    switch (NET_URL_Type(address)) {
      case MOCHA_TYPE_URL:
        /* This type cannot name the true origin (server) of JS code. */
        break;
      case VIEW_SOURCE_TYPE_URL:
        NS_ASSERTION(0, "Invalid url type");
      default:
        return address;
    }
  }

  if (context->grid_parent) {
    address = FindCreatorURL(context->grid_parent);
    if (address)
      return address;
  }

  aCx = context->mocha_context;
  if (aCx) {
    decoder = JS_GetPrivate(aCx, JS_GetGlobalObject(aCx));
    if (decoder && decoder->opener) {
      /* self.opener property is valid, check its MWContext. */
      MochaDecoder *opener = JS_GetPrivate(aCx, decoder->opener);
      if (!opener->visited) {
        opener->visited = PR_TRUE;
        address = opener->window_context
                ? FindCreatorURL(opener->window_context)
                : nsnull;
        opener->visited = PR_FALSE;
        if (address)
          return address;
      }
    }
  }
#endif
  return NS_OK;
}

extern "C" NS_DOM nsresult
NS_NewScriptGlobalObject(nsIScriptGlobalObject **aResult)
{
  if (nsnull == aResult) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  GlobalWindowImpl *global = new GlobalWindowImpl();
  if (nsnull == global) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return global->QueryInterface(kIScriptGlobalObjectIID, (void **)aResult);
}



//
//  Navigator class implementation 
//
NavigatorImpl::NavigatorImpl()
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  mMimeTypes = nsnull;
  mPlugins = nsnull;
}

NavigatorImpl::~NavigatorImpl()
{
  NS_IF_RELEASE(mMimeTypes);
  NS_IF_RELEASE(mPlugins);
}

NS_IMPL_ADDREF(NavigatorImpl)
NS_IMPL_RELEASE(NavigatorImpl)

nsresult 
NavigatorImpl::QueryInterface(const nsIID& aIID,
                              void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNavigatorIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMNavigator*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult 
NavigatorImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult 
NavigatorImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptNavigator(aContext, (nsISupports *)(nsIDOMNavigator *)this, global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetUserAgent(nsString& aUserAgent)
{
  nsresult res;
#ifndef NECKO
  nsINetService *service = nsnull;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&service);
#else
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &res);
#endif // NECKO

  if ((NS_OK == res) && (nsnull != service)) {
#ifndef NECKO
    res = service->GetUserAgent(aUserAgent);
#else
    PRUnichar *ua = nsnull;
    res = service->GetUserAgent(&ua);
    aUserAgent = ua;
    delete [] ua;
#endif // NECKO
    NS_RELEASE(service);
  }
  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppCodeName(nsString& aAppCodeName)
{
  nsresult res;
#ifndef NECKO
  nsINetService *service = nsnull;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&service);
#else
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &res);
#endif // NECKO

  if ((NS_OK == res) && (nsnull != service)) {
#ifndef NECKO
    res = service->GetAppCodeName(aAppCodeName);
#else
    PRUnichar *appName = nsnull;
    res = service->GetAppCodeName(&appName);
    aAppCodeName = appName;
    delete [] appName;
#endif // NECKO
    NS_RELEASE(service);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppVersion(nsString& aAppVersion)
{
  nsresult res;
#ifndef NECKO
  nsINetService *service = nsnull;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&service);
#else
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &res);
#endif // NECKO

  if ((NS_OK == res) && (nsnull != service)) {
#ifndef NECKO
    res = service->GetAppVersion(aAppVersion);
#else
    PRUnichar *appVer = nsnull;
    res = service->GetAppVersion(&appVer);
    aAppVersion = appVer;
    delete [] appVer;
#endif // NECKO
    NS_RELEASE(service);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppName(nsString& aAppName)
{
  nsresult res;
#ifndef NECKO
  nsINetService *service = nsnull;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&service);
#else
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &res);
#endif // NECKO

  if ((NS_OK == res) && (nsnull != service)) {
#ifndef NECKO
    res = service->GetAppName(aAppName);
#else
    PRUnichar *appName = nsnull;
    res = service->GetAppName(&appName);
    aAppName = appName;
    delete [] appName;
#endif // NECKO
    NS_RELEASE(service);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetLanguage(nsString& aLanguage)
{
  nsresult res;
#ifndef NECKO
  nsINetService *service = nsnull;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&service);
#else
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &res);
#endif // NECKO

  if ((NS_OK == res) && (nsnull != service)) {
#ifndef NECKO
    res = service->GetLanguage(aLanguage);
#else
    PRUnichar *lang = nsnull;
    res = service->GetLanguage(&lang);
    aLanguage = lang;
    delete [] lang;
#endif // NECKO
    NS_RELEASE(service);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetPlatform(nsString& aPlatform)
{
  nsresult res;
#ifndef NECKO
  nsINetService *service = nsnull;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&service);
#else
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &res);
#endif // NECKO

  if ((NS_OK == res) && (nsnull != service)) {
#ifndef NECKO
    res = service->GetPlatform(aPlatform);
#else
    PRUnichar *plat = nsnull;
    res = service->GetPlatform(&plat);
    aPlatform = plat;
    delete [] plat;
#endif // NECKO
    NS_RELEASE(service);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetSecurityPolicy(nsString& aSecurityPolicy)
{
  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::GetMimeTypes(nsIDOMMimeTypeArray** aMimeTypes)
{
  if (nsnull == mMimeTypes) {
    mMimeTypes = new MimeTypeArrayImpl(this);
    NS_IF_ADDREF(mMimeTypes);
  }

  *aMimeTypes = mMimeTypes;
  NS_IF_ADDREF(mMimeTypes);

  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::GetPlugins(nsIDOMPluginArray** aPlugins)
{
  if (nsnull == mPlugins) {
    mPlugins = new PluginArrayImpl(this);
    NS_IF_ADDREF(mPlugins);
  }

  *aPlugins = mPlugins;
  NS_IF_ADDREF(mPlugins);

  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::JavaEnabled(PRBool* aReturn)
{
  nsresult rv = NS_OK;
  *aReturn = PR_FALSE;

  // determine whether user has enabled java.
  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
  if (NS_FAILED(rv) || prefs == nsnull) {
    return rv;
  }
  
  // if pref doesn't exist, map result to false.
  if (prefs->GetBoolPref("security.enable_java", aReturn) != NS_OK)
    *aReturn = PR_FALSE;

  return rv;
}
