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
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsITimer.h"
#include "nsEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIScriptEventListener.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "nsIScriptContextOwner.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsRect.h"
#include "nsINetSupport.h"
#include "nsIContentViewer.h"
#include "nsScreen.h"
#include "nsHistory.h"

#include "jsapi.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
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
static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kINetSupportIID, NS_INETSUPPORT_IID);

GlobalWindowImpl::GlobalWindowImpl()
{
  NS_INIT_REFCNT();
  mContext = nsnull;
  mScriptObject = nsnull;
  mDocument = nsnull;
  mNavigator = nsnull;
  mScreen = nsnull;
  mHistory = nsnull;
  mLocation = nsnull;
  mFrames = nsnull;
  mOpener = nsnull;

  mTimeouts = nsnull;
  mTimeoutInsertionPoint = nsnull;
  mRunningTimeout = nsnull;
  mTimeoutPublicIdCounter = 1;
  mListenerManager = nsnull;
}

GlobalWindowImpl::~GlobalWindowImpl() 
{
  if (nsnull != mScriptObject) {
    mContext->RemoveReference(&mScriptObject, mScriptObject);
    mScriptObject = nsnull;
  }
  
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mNavigator);
  NS_IF_RELEASE(mScreen);
  NS_IF_RELEASE(mHistory);
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
GlobalWindowImpl::SetNewDocument(nsIDOMDocument *aDocument)
{
  if (nsnull != mDocument) {
    ClearAllTimeouts();

    if (nsnull != mScriptObject && nsnull != mContext) {
      JS_ClearScope((JSContext *)mContext->GetNativeContext(),
                    (JSObject *)mScriptObject);
    }
    
    NS_RELEASE(mDocument);
    if (nsnull != mContext) {
      mContext->GC();
    }
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
  if (nsnull == mScreen) {
    mScreen = new ScreenImpl();
    NS_IF_ADDREF(mScreen);
  }

  *aScreen = mScreen;
  NS_IF_ADDREF(mScreen);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetHistory(nsIDOMHistory** aHistory)
{
  if (nsnull == mHistory) {
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
GlobalWindowImpl::GetOpener(nsIDOMWindow** aOpener)
{
  *aOpener = mOpener;
  NS_IF_ADDREF(*aOpener);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetOpener(nsIDOMWindow* aOpener)
{
  if (nsnull == aOpener) {
    NS_IF_RELEASE(mOpener);
    mOpener = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetParent(nsIDOMWindow** aParent)
{
  nsresult ret = NS_OK;
  nsIWebShell *mParentWebShell;
  mWebShell->GetParent(mParentWebShell);

  *aParent = nsnull;
  
  if (nsnull != mParentWebShell) {
    nsIScriptContextOwner *mParentContextOwner;
    if (NS_OK == mParentWebShell->QueryInterface(kIScriptContextOwnerIID, (void**)&mParentContextOwner)) {
      nsIScriptGlobalObject *mParentGlobalObject;
      if (NS_OK == mParentContextOwner->GetScriptGlobalObject(&mParentGlobalObject)) {
        ret = mParentGlobalObject->QueryInterface(kIDOMWindowIID, (void**)aParent);
        NS_RELEASE(mParentGlobalObject);
      }
      NS_RELEASE(mParentContextOwner);
    }
    NS_RELEASE(mParentWebShell);
  } 
  else {
    *aParent = this;
    NS_ADDREF(this);
  }

  return ret;
}

NS_IMETHODIMP    
GlobalWindowImpl::GetLocation(nsIDOMLocation** aLocation)
{
  if (nsnull == mLocation) {
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
  nsIWebShell *mRootWebShell;
  mWebShell->GetRootWebShell(mRootWebShell);

  *aTop = nsnull;

  if (nsnull != mRootWebShell) {
    nsIScriptContextOwner *mRootContextOwner;
    if (NS_OK == mRootWebShell->QueryInterface(kIScriptContextOwnerIID, (void**)&mRootContextOwner)) {
      nsIScriptGlobalObject *mRootGlobalObject;
      if (NS_OK == mRootContextOwner->GetScriptGlobalObject(&mRootGlobalObject)) {
        ret = mRootGlobalObject->QueryInterface(kIDOMWindowIID, (void**)aTop);
        NS_RELEASE(mRootGlobalObject);
      }
      NS_RELEASE(mRootContextOwner);
    }
    NS_RELEASE(mRootWebShell);
  }
  return ret;
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
  if (nsnull == mFrames) {
    mFrames = new nsDOMWindowList(mWebShell);
    if (nsnull == mFrames) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mFrames);
  }

  *aFrames = (nsIDOMWindowCollection *)mFrames;
  NS_ADDREF(mFrames);

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
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetStatus(const nsString& aStatus)
{
  nsIBrowserWindow *mBrowser;
  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    mBrowser->SetStatus(aStatus);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetDefaultStatus(nsString& aDefaultStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetDefaultStatus(const nsString& aDefaultStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetName(nsString& aName)
{
  const PRUnichar *name;
  mWebShell->GetName(&name);
  aName = name;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetName(const nsString& aName)
{
  mWebShell->SetName(aName);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetInnerWidth(PRInt32* aInnerWidth)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetBounds(r);
    *aInnerWidth = r.width;
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetInnerWidth(PRInt32 aInnerWidth)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetBounds(r);

    mBrowser->SizeTo(aInnerWidth, r.height);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetInnerHeight(PRInt32* aInnerHeight)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetBounds(r);
    *aInnerHeight = r.height;
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetInnerHeight(PRInt32 aInnerHeight)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetBounds(r);

    mBrowser->SizeTo(r.width, aInnerHeight);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetOuterWidth(PRInt32* aOuterWidth)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);
    *aOuterWidth = r.width;
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetOuterWidth(PRInt32 aOuterWidth)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);

    mBrowser->SizeTo(aOuterWidth, r.height);
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetOuterHeight(PRInt32* aOuterHeight)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);
    *aOuterHeight = r.height;
    NS_RELEASE(mBrowser);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetOuterHeight(PRInt32 aOuterHeight)
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    nsRect r;
    mBrowser->GetWindowBounds(r);

    mBrowser->SizeTo(r.width, aOuterHeight);
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
GlobalWindowImpl::Alert(const nsString& aStr)
{
  nsresult ret;
  
  nsIWebShell *rootWebShell;
  ret = mWebShell->GetRootWebShell(rootWebShell);
  if (nsnull != rootWebShell) {
    nsIWebShellContainer *rootContainer;
    ret = rootWebShell->GetContainer(rootContainer);
    if (nsnull != rootContainer) {
      nsINetSupport *support;
        if (NS_OK == (ret = rootContainer->QueryInterface(kINetSupportIID, (void**)&support))) {
          support->Alert(aStr);
          NS_RELEASE(support);
        }
      NS_RELEASE(rootContainer);
    }
    NS_RELEASE(rootWebShell);
  }
  return ret;
  // XXX Temporary
  //return Dump(aStr);
}

NS_IMETHODIMP
GlobalWindowImpl::Focus()
{
  mWebShell->SetFocus();
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Blur()
{
  mWebShell->RemoveFocus();
  return NS_OK;
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
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    //XXX tbi
    //mBrowser->Forward();
    NS_RELEASE(mBrowser);
  } else {
    mWebShell->Forward(); // I added this - rods
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Back()
{
  nsIBrowserWindow *mBrowser;

  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    //XXX tbi
    //mBrowser->Back();
    NS_RELEASE(mBrowser);
  }  else {
    mWebShell->Back();// I added this - rods
  }
  return NS_OK;  
}

NS_IMETHODIMP
GlobalWindowImpl::Home()
{
  // XXX: This should look in the preferences to find the home URL. 
  // Temporary hardcoded to www.netscape.com
  nsString homeURL("http://www.netscape.com");
  PRUnichar* urlToLoad = homeURL.ToNewUnicode();
  mWebShell->LoadURL(urlToLoad);
  delete[] urlToLoad;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Stop()
{
  mWebShell->Stop();
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Print()
{
  nsIContentViewer *viewer = nsnull;

  mWebShell->GetContentViewer(&viewer);

  if (nsnull != viewer)
  {
    viewer->Print();
    NS_RELEASE(viewer);
  }
  return NS_OK;
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
    mBrowser->SizeTo(aWidth, aHeight);
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

    mBrowser->SizeTo(r.width + aWidthDif, r.height + aHeightDif);
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
  for (top = &mTimeouts; ((timeout = *top) != NULL); top = &timeout->next) {
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
    if (mRunningTimeout == timeout)
      mTimeoutInsertionPoint = nsnull;
        
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
GlobalWindowImpl::DropTimeout(nsTimeoutImpl *aTimeout)
{
  JSContext *cx;
  
  if (--aTimeout->ref_count > 0) {
    return;
  }
  
  cx = (JSContext *)mContext->GetNativeContext();
  
  if (aTimeout->expr) {
    PR_FREEIF(aTimeout->expr);
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

  timeout->window->RunTimeout(timeout);
  // Drop the ref_count since the timer won't be holding on to the
  // timeout struct anymore
  timeout->window->DropTimeout(timeout);
}

void
GlobalWindowImpl::RunTimeout(nsTimeoutImpl *aTimeout)
{
    nsTimeoutImpl *next, *timeout;
    nsTimeoutImpl *last_expired_timeout;
    nsTimeoutImpl dummy_timeout;
    JSContext *cx;
    PRInt64 now;
    jsval result;
    nsITimer *timer;

    timer = aTimeout->timer;
    cx = (JSContext *)mContext->GetNativeContext();

    /*
     *   A native timer has gone off.  See which of our timeouts need
     *   servicing
     */
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
    if (!last_expired_timeout)
        return;

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
        JS_EvaluateScript(cx, (JSObject *)mScriptObject,
                          timeout->expr, 
                          PL_strlen(timeout->expr),
                          timeout->filename, timeout->lineno, 
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

      mContext->ScriptEvaluated();

      mRunningTimeout = nsnull;

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
          return;
        } 
        
        err = timeout->timer->Init(nsGlobalWindow_RunTimeout, timeout, 
                                   delay32);
        if (NS_OK != err) {
          return;
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
      DropTimeout(timeout);

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
  char *expr = nsnull;
  JSObject *funobj = nsnull;
  JSString *str;
  nsTimeoutImpl *timeout, **insertion_point;
  jsdouble interval;
  PRInt64 now, delta;

  if (argc >= 2) {
    if (!JS_ValueToNumber(cx, argv[1], &interval)) {
      JS_ReportError(cx, "Second argument to %s must be a millisecond interval",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    switch (JS_TypeOfValue(cx, argv[0])) {
      case JSTYPE_FUNCTION:
        funobj = JSVAL_TO_OBJECT(argv[0]);
        break;
      case JSTYPE_STRING:
      case JSTYPE_OBJECT:
        if (!(str = JS_ValueToString(cx, argv[0])))
            return NS_ERROR_FAILURE;
        expr = PL_strdup(JS_GetStringBytes(str));
        if (nsnull == expr)
            return NS_ERROR_OUT_OF_MEMORY;
        break;
      default:
        JS_ReportError(cx, "useless %s call (missing quotes around argument?)", aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
        return NS_ERROR_FAILURE;
    }

    timeout = PR_NEWZAP(nsTimeoutImpl);
    if (nsnull == timeout) {
      PR_FREEIF(expr);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Initial ref_count to indicate that this timeout struct will
    // be held as the closure of a timer.
    timeout->ref_count = 1;
    if (aIsInterval)
        timeout->interval = (PRInt32)interval;
    timeout->expr = expr;
    timeout->funobj = funobj;
    timeout->principals = nsnull;
    if (expr) {
      timeout->argv = 0;
      timeout->argc = 0;
    } 
    else {
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

    if (mTimeoutInsertionPoint == NULL)
        insertion_point = &mTimeouts;
    else
        insertion_point = mTimeoutInsertionPoint;

    InsertTimeoutIntoList(insertion_point, timeout);
    timeout->public_id = ++mTimeoutPublicIdCounter;
    *aReturn = timeout->public_id;
  }
  else {
    JS_ReportError(cx, "Function %s requires at least 2 parameters", aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_FAILURE;
  }
  
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
  PRUint32 mChrome = 0;
  PRInt32 mWidth = 0, mHeight = 0;
  PRInt32 mLeft = 0, mTop = 0;
  nsRect mDefaultBounds;
  nsAutoString mAbsURL, name;
  JSString* str;
  *aReturn = nsnull;

  if (argc > 0) {
    JSString *mJSStrURL = JS_ValueToString(cx, argv[0]);
    if (nsnull == mJSStrURL || nsnull == mDocument) {
      return NS_ERROR_FAILURE;
    }

    nsAutoString mURL, mEmpty;
    nsIURL* mDocURL = 0;
    nsIDocument* mDoc;

    mURL.SetString(JS_GetStringChars(mJSStrURL));

    if (NS_OK == mDocument->QueryInterface(kIDocumentIID, (void**)&mDoc)) {
      mDocURL = mDoc->GetDocumentURL();
      NS_RELEASE(mDoc);
    }
     
    if (NS_OK != NS_MakeAbsoluteURL(mDocURL, mEmpty, mURL, mAbsURL)) {
      return NS_ERROR_FAILURE;
    }
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

  /* set default location/size of new window. */
  nsIBrowserWindow *mBrowser;
  if (NS_OK == GetBrowserWindowInterface(mBrowser)) {
    mBrowser->GetWindowBounds(mDefaultBounds);
    NS_RELEASE(mBrowser);
  }
  
  if (argc > 2) {
    if (!(str = JS_ValueToString(cx, argv[2]))) {
      return NS_ERROR_FAILURE;
    }
    char *options = JS_GetStringBytes(str);

    mChrome |= WinHasOption(options, "toolbar") ? NS_CHROME_TOOL_BAR_ON : 0;
    mChrome |= WinHasOption(options, "location") ? NS_CHROME_LOCATION_BAR_ON : 0;
    mChrome |= (WinHasOption(options, "directories") | WinHasOption(options, "personalbar"))
      ? NS_CHROME_PERSONAL_TOOLBAR_ON : 0;
    mChrome |= WinHasOption(options, "status") ? NS_CHROME_STATUS_BAR_ON : 0;
    mChrome |= WinHasOption(options, "menubar") ? NS_CHROME_MENU_BAR_ON : 0;
    mChrome |= WinHasOption(options, "scrollbars") ? NS_CHROME_SCROLLBARS_ON : 0;
    mChrome |= WinHasOption(options, "resizable") ? NS_CHROME_WINDOW_RESIZE_ON : 0;
    mChrome |= NS_CHROME_WINDOW_CLOSE_ON;

    mWidth = WinHasOption(options, "innerWidth") | WinHasOption(options, "width");
    mHeight = WinHasOption(options, "innerHeight") | WinHasOption(options, "height");

    /*mWidth = WinHasOption(options, "outerWidth");
    mHeight = WinHasOption(options, "outerHeight");*/

    mLeft = WinHasOption(options, "left") | WinHasOption(options, "screenX");
    mTop = WinHasOption(options, "top") | WinHasOption(options, "screenY");
    /*z-ordering, history, dependent
    mChrome->topmost         = WinHasOption(options, "alwaysRaised");
    mChrome->bottommost              = WinHasOption(options, "alwaysLowered");
    mChrome->z_lock          = WinHasOption(options, "z-lock");
    mChrome->is_modal            = WinHasOption(options, "modal");
    mChrome->hide_title_bar  = !(WinHasOption(options, "titlebar"));
    mChrome->dependent              = WinHasOption(options, "dependent");
    mChrome->copy_history           = FALSE;
    */

    /* Allow disabling of commands only if there is no menubar */
    /*if (!mChrome & NS_CHROME_MENU_BAR_ON) {
        mChrome->disable_commands = !WinHasOption(options, "hotkeys");
        if (XP_STRCASESTR(options,"hotkeys")==NULL)
            mChrome->disable_commands = FALSE;
    }*/
    /* If titlebar condition not specified, default to shown */
    /*if (XP_STRCASESTR(options,"titlebar")==0)*/
    mChrome |= NS_CHROME_TITLEBAR_ON;

    /* XXX Add security check on z-ordering, modal, hide title, disable hotkeys */

    /* XXX Add security check for sizing and positioning.  
     * See mozilla/lib/libmocha/lm_win.c for current constraints */

  } 
  else {
      /* Make a fully mChromed window, but don't copy history. */
    mChrome = (PRUint32)~0;
  }

  nsIBrowserWindow *newWindow = nsnull;
  nsIScriptGlobalObject *newGlobalObject = nsnull;
  nsIWebShell *newWebShell = nsnull;
  nsIWebShellContainer *webShellContainer, *newContainer;
  
  /* XXX check for existing window of same name.  If exists, set url and 
   * update chrome */
  if (NS_OK == mWebShell->GetContainer(webShellContainer) && nsnull != webShellContainer) {
    // Check for existing window of same name.
    webShellContainer->FindWebShellWithName(name.GetUnicode(), newWebShell);
    if (nsnull == newWebShell) {
      // No window of that name so create a new one.
      webShellContainer->NewWebShell(mChrome, PR_FALSE, newWebShell);
    }
    if (nsnull != newWebShell) {
      newWebShell->SetName(name);
      newWebShell->LoadURL(mAbsURL);

      if (NS_OK == newWebShell->GetContainer(newContainer) && nsnull != newContainer) {
        newContainer->QueryInterface(kIBrowserWindowIID, (void**)&newWindow);
        NS_RELEASE(newContainer);
      }
    }
    NS_RELEASE(webShellContainer);
  }

  if (nsnull != newWindow && nsnull != newWebShell) {
    // beard: don't resize/reposition the window if it is the same web shell.
    if (newWebShell != mWebShell) {
      // How should we do default size/pos?
      // How about inheriting from the current window?
      newWindow->SizeTo(mWidth ? mWidth : mDefaultBounds.width, mHeight ? mHeight : mDefaultBounds.height);
      newWindow->MoveTo(mLeft ? mLeft : mDefaultBounds.x, mTop ? mTop : mDefaultBounds.y);
      newWindow->Show();
    }

    /* Get win obj */
    nsIScriptContextOwner *newContextOwner = nsnull;

    if (NS_OK != newWebShell->QueryInterface(kIScriptContextOwnerIID, (void**)&newContextOwner) ||
        NS_OK != newContextOwner->GetScriptGlobalObject(&newGlobalObject)) {

      NS_IF_RELEASE(newWindow);
      NS_IF_RELEASE(newWebShell);
      NS_IF_RELEASE(newContextOwner);
      return NS_ERROR_FAILURE;
    }

    NS_RELEASE(newWindow);
    NS_RELEASE(newWebShell);
    NS_RELEASE(newContextOwner);
  }

  nsIDOMWindow *newDOMWindow = nsnull;
  if (nsnull != newGlobalObject) {
    if (NS_OK == newGlobalObject->QueryInterface(kIDOMWindowIID, (void**)&newDOMWindow)) {
    *aReturn = newDOMWindow;
    }

    /* Set opener */
    newGlobalObject->SetOpenerWindow(this);
  }


  NS_IF_RELEASE(newGlobalObject);

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
      JS_ReportError(cx,
        "illegal character '%c' ('\\%o') in window name %s",
        mChar, mChar, aName);
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

PRInt32 
GlobalWindowImpl::WinHasOption(char *options, char *name)
{
  char *comma, *equal;
  PRInt32 found = 0;

  for (;;) {
    comma = strchr(options, ',');
    if (comma) *comma = '\0';
    equal = strchr(options, '=');
    if (equal) *equal = '\0';
    if (nsCRT::strcasecmp(options, name) == 0) {
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
  nsresult ret;
  
  nsIWebShell *mRootWebShell;
  mWebShell->GetRootWebShell(mRootWebShell);
  if (nsnull != mRootWebShell) {
    nsIWebShellContainer *mRootContainer;
    mRootWebShell->GetContainer(mRootContainer);
    if (nsnull != mRootContainer) {
      ret = mRootContainer->QueryInterface(kIBrowserWindowIID, (void**)&aBrowser);
      NS_RELEASE(mRootContainer);
    }
    NS_RELEASE(mRootWebShell);
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
    mPrefix.SetString(mPropName, 2);
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
  if (JSVAL_IS_STRING(aID) && 
      PL_strcmp("location", JS_GetStringBytes(JS_ValueToString(aContext, aID))) == 0) {
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

  return PR_TRUE;
}

PRBool
GlobalWindowImpl::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    nsString mPropName;
    nsAutoString mPrefix;
    mPropName.SetString(JS_GetStringChars(JS_ValueToString(aContext, aID)));
    mPrefix.SetString(mPropName, 2);
    if (mPrefix == "on") {
      return CheckForEventListener(aContext, mPropName);
    }
  }
  else if (JSVAL_IS_STRING(aID) && 
           PL_strcmp("location", JS_GetStringBytes(JS_ValueToString(aContext, aID))) == 0) {
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
  if (JSVAL_IS_STRING(aID) && 
      PL_strcmp("location", JS_GetStringBytes(JS_ValueToString(aContext, aID))) == 0) {
    ::JS_DefineProperty(aContext, (JSObject *)mScriptObject, "location",
                        JSVAL_NULL, nsnull, nsnull, 0);
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
  }
  NS_IF_RELEASE(mDoc);
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

  if (DOM_EVENT_INIT == aFlags) {
    aDOMEvent = &mDOMEvent;
  }
  
  //Capturing stage
  /*if (mEventCapturer) {
    mEventCapturer->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
  }*/

  //Local handling stage
  if (nsnull != mListenerManager) {
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aEventStatus);
  }

  //Bubbling stage
  /*Up to frames?*/

  if (DOM_EVENT_INIT == aFlags) {
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
GlobalWindowImpl::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    mManager->AddEventListener(aListener, aIID);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult 
GlobalWindowImpl::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  if (nsnull != mListenerManager) {
    mListenerManager->RemoveEventListener(aListener, aIID);
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
}

NavigatorImpl::~NavigatorImpl()
{
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
    nsINetService *service = nsnull;
    nsresult res = nsServiceManager::GetService(kNetServiceCID,
                                          kINetServiceIID,
                                          (nsISupports **)&service);

    if ((NS_OK == res) && (nsnull != service)) {
        res = service->GetUserAgent(aUserAgent);
        NS_RELEASE(service);
    }
    return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppCodeName(nsString& aAppCodeName)
{
    nsINetService *service;
    nsresult res = nsServiceManager::GetService(kNetServiceCID,
                                          kINetServiceIID,
                                          (nsISupports **)&service);

    if ((NS_OK == res) && (nsnull != service)) {
        res = service->GetAppCodeName(aAppCodeName);
        NS_RELEASE(service);
    }

    return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppVersion(nsString& aAppVersion)
{
    nsINetService *service;
    nsresult res = nsServiceManager::GetService(kNetServiceCID,
                                          kINetServiceIID,
                                          (nsISupports **)&service);

    if ((NS_OK == res) && (nsnull != service)) {
        res = service->GetAppVersion(aAppVersion);
        NS_RELEASE(service);
    }

    return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppName(nsString& aAppName)
{
    nsINetService *service;
    nsresult res = nsServiceManager::GetService(kNetServiceCID,
                                          kINetServiceIID,
                                          (nsISupports **)&service);

    if ((NS_OK == res) && (nsnull != service)) {
        res = service->GetAppName(aAppName);
        NS_RELEASE(service);
    }

    return res;
}

NS_IMETHODIMP
NavigatorImpl::GetLanguage(nsString& aLanguage)
{
    nsINetService *service;
    nsresult res = nsServiceManager::GetService(kNetServiceCID,
                                          kINetServiceIID,
                                          (nsISupports **)&service);

    if ((NS_OK == res) && (nsnull != service)) {
        res = service->GetLanguage(aLanguage);
        NS_RELEASE(service);
    }

    return res;
}

NS_IMETHODIMP
NavigatorImpl::GetPlatform(nsString& aPlatform)
{
    nsINetService *service;
    nsresult res = nsServiceManager::GetService(kNetServiceCID,
                                          kINetServiceIID,
                                          (nsISupports **)&service);

    if ((NS_OK == res) && (nsnull != service)) {
        res = service->GetPlatform(aPlatform);
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
NavigatorImpl::JavaEnabled(PRBool* aReturn)
{
  *aReturn = PR_FALSE;
  return NS_OK;
}
