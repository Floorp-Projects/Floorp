/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Travis Bogard <travis@netscape.com> 
 */
#ifndef nsGlobalWindow_h___
#define nsGlobalWindow_h___

// Local Includes
// Helper Classes
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsHashtable.h"

// Interfaces Needed
#include "nsDOMWindowList.h"
#include "nsIBaseWindow.h"
#include "nsIChromeEventHandler.h"
#include "nsIControllers.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMNSLocation.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMWindowEventOwner.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptGlobalObject.h"
#include "nsITimer.h"
#include "nsIWebBrowserChrome.h"
#include "nsPIDOMWindow.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMPkcs11.h"
#include "nsISidebar.h"
#include "nsIPrincipal.h"

#define DEFAULT_HOME_PAGE "www.mozilla.org"
#define PREF_BROWSER_STARTUP_HOMEPAGE "browser.startup.homepage"

class nsIDOMBarProp;
class nsIDocument;
class nsIContent;
class nsIPresContext;
class nsIDOMEvent;
class nsIScrollableView;

typedef struct nsTimeoutImpl nsTimeoutImpl;

class BarPropImpl;
class LocationImpl;
class NavigatorImpl;
class ScreenImpl;
class HistoryImpl;
class nsIDocShellLoadInfo;

//*****************************************************************************
// GlobalWindowImpl: Global Object for Scripting
//*****************************************************************************   
class GlobalWindowImpl :   public nsIScriptGlobalObject,
                           public nsIDOMWindowInternal,
                           public nsIJSScriptObject,
                           public nsIScriptObjectPrincipal,
                           public nsIDOMEventReceiver,
                           public nsPIDOMWindow, 
                           public nsIDOMViewCSS,
                           public nsSupportsWeakReference,
                           public nsIDOMWindowEventOwner
{
public:
   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIScriptObjectOwner
   NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
   NS_IMETHOD SetScriptObject(void *aScriptObject);

   // nsIScriptGlobalObject
   NS_IMETHOD SetContext(nsIScriptContext *aContext);
   NS_IMETHOD GetContext(nsIScriptContext **aContext);
   NS_IMETHOD SetNewDocument(nsIDOMDocument *aDocument);
   NS_IMETHOD SetDocShell(nsIDocShell* aDocShell);
   NS_IMETHOD GetDocShell(nsIDocShell** aDocShell);
   NS_IMETHOD SetOpenerWindow(nsIDOMWindowInternal *aOpener);
   NS_IMETHOD SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner);
   NS_IMETHOD GetGlobalObjectOwner(nsIScriptGlobalObjectOwner** aOwner);
   NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent, 
                              nsIDOMEvent** aDOMEvent, PRUint32 aFlags, 
                              nsEventStatus* aEventStatus);

   // nsIScriptObjectPrincipal
   NS_IMETHOD GetPrincipal(nsIPrincipal **prin);

   // nsIDOMWindow
   NS_DECL_IDOMWINDOW

   // nsIDOMWindowInternal
   NS_DECL_IDOMWINDOWINTERNAL

   // nsIJSScriptObject
   virtual PRBool AddProperty(JSContext *aContext, JSObject *aObj, 
                             jsval aID, jsval *aVp);
   virtual PRBool DeleteProperty(JSContext *aContext, JSObject *aObj, 
                             jsval aID, jsval *aVp);
   virtual PRBool GetProperty(JSContext *aContext, JSObject *aObj, 
                             jsval aID, jsval *aVp);
   virtual PRBool SetProperty(JSContext *aContext, JSObject *aObj, 
                             jsval aID, jsval *aVp);
   virtual PRBool EnumerateProperty(JSContext *aContext, JSObject *aObj);
   virtual PRBool Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                          PRBool *aDidDefineProperty);
   virtual PRBool Convert(JSContext *aContext, JSObject *aObj, jsval aID);
   virtual void   Finalize(JSContext *aContext, JSObject *aObj);

   // nsIDOMEventTarget
   NS_IMETHOD AddEventListener(const nsAReadableString& aType, 
                           nsIDOMEventListener* aListener, PRBool aUseCapture);
   NS_IMETHOD RemoveEventListener(const nsAReadableString& aType, 
                           nsIDOMEventListener* aListener, PRBool aUseCapture);
   NS_IMETHOD DispatchEvent(nsIDOMEvent* aEvent);

   // nsIDOMEventReceiver
   NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener, 
                                    const nsIID& aIID);
   NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener, 
                                    const nsIID& aIID);
   NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
   NS_IMETHOD GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult);
   NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

   // nsPIDOMWindow
   NS_IMETHOD GetPrivateParent(nsPIDOMWindow** aResult);
   NS_IMETHOD GetPrivateRoot(nsIDOMWindowInternal** aResult);

   NS_IMETHOD GetLocation(nsIDOMLocation** aLocation);
   
   NS_IMETHOD SetObjectProperty(const PRUnichar* aProperty, nsISupports* aObject);
   NS_IMETHOD GetObjectProperty(const PRUnichar* aProperty, nsISupports** aObject);

   NS_IMETHOD Activate();
   NS_IMETHOD Deactivate();

   NS_IMETHOD GetRootCommandDispatcher(nsIDOMXULCommandDispatcher ** aDispatcher);

	
  NS_IMETHOD SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy, PRBool fRepaint);
  NS_IMETHOD GetPositionAndSize(PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy);


   // nsIDOMViewCSS
   NS_DECL_IDOMVIEWCSS

   // nsIDOMAbstractView
   NS_DECL_IDOMABSTRACTVIEW

   // nsIDOMWindowEventOwner
   NS_DECL_IDOMWINDOWEVENTOWNER

public:
   // Object Management
   GlobalWindowImpl();

protected:
   // Object Management
   virtual ~GlobalWindowImpl();
   void CleanUp();
   
   // Window Control Functions
   NS_IMETHOD OpenInternal(JSContext* cx, jsval* argv, PRUint32 argc, 
      PRBool aDialog, nsIDOMWindowInternal** aReturn);
   NS_IMETHOD AttachArguments(nsIDOMWindowInternal* aWindow, jsval* argv, PRUint32 argc);
   PRUint32 CalculateChromeFlags(char* aFeatures, PRBool aDialog);
   NS_IMETHOD SizeOpenedDocShellItem(nsIDocShellTreeItem* aDocShellItem,
      char* aFeatures, PRUint32 aChromeFlags);
   NS_IMETHOD ReadyOpenedDocShellItem(nsIDocShellTreeItem* aDocShellItem,
      nsIDOMWindowInternal** aDOMWindow);
   NS_IMETHOD CheckWindowName(JSContext* cx, nsString& aName);
   PRInt32 WinHasOption(char* aOptions, const char* aName, PRInt32 aDefault, PRBool* aPresenceFlag);
   static void CloseWindow(nsISupports* aWindow);

   // Timeout Functions
   NS_IMETHOD SetTimeoutOrInterval(JSContext *cx, jsval *argv, PRUint32 argc,
      PRInt32* aReturn, PRBool aIsInterval);
   PRBool RunTimeout(nsTimeoutImpl *aTimeout);
   void DropTimeout(nsTimeoutImpl *aTimeout, nsIScriptContext* aContext=nsnull);
   void HoldTimeout(nsTimeoutImpl *aTimeout);
   NS_IMETHOD ClearTimeoutOrInterval(PRInt32 aTimerID);
   void ClearAllTimeouts();
   void InsertTimeoutIntoList(nsTimeoutImpl **aInsertionPoint, 
      nsTimeoutImpl *aTimeout);
   friend void nsGlobalWindow_RunTimeout(nsITimer *aTimer, void *aClosure);

   // Helper Functions
   NS_IMETHOD GetTreeOwner(nsIDocShellTreeOwner** aTreeOwner);
   NS_IMETHOD GetTreeOwner(nsIBaseWindow** aTreeOwner);
   NS_IMETHOD GetWebBrowserChrome(nsIWebBrowserChrome** aBrowserChrome);
   NS_IMETHOD GetScrollInfo(nsIScrollableView** aScrollableView, float* aP2T,
      float* aT2P);
   nsresult RegisterEventListener(const char* aEventName,
                                  REFNSIID aIID);
   void FlushPendingNotifications();
   nsresult CheckSecurityWidthAndHeight(PRInt32* width, PRInt32* height);
   nsresult CheckSecurityLeftAndTop(PRInt32* left, PRInt32* top);

protected:
   nsCOMPtr<nsIScriptContext>    mContext;
   nsCOMPtr<nsIDOMDocument>      mDocument;
   nsCOMPtr<nsIDOMWindowInternal> mOpener;
   nsCOMPtr<nsIControllers>      mControllers;
   nsCOMPtr<nsIEventListenerManager> mListenerManager;
   nsCOMPtr<nsISidebar>          mSidebar;
   void*                         mScriptObject;
   NavigatorImpl*                mNavigator;
   ScreenImpl*                   mScreen;
   HistoryImpl*                  mHistory;
   nsDOMWindowList*              mFrames;
   LocationImpl*                 mLocation;
   BarPropImpl*                  mMenubar;
   BarPropImpl*                  mToolbar;
   BarPropImpl*                  mLocationbar;
   BarPropImpl*                  mPersonalbar;
   BarPropImpl*                  mStatusbar;
   BarPropImpl*                  mScrollbars;
   nsTimeoutImpl*                mTimeouts;
   nsTimeoutImpl**               mTimeoutInsertionPoint;
   nsTimeoutImpl*                mRunningTimeout;
   PRUint32                      mTimeoutPublicIdCounter;
   PRUint32                      mTimeoutFiringDepth;
   PRBool                        mFirstDocumentLoad;
   nsString                      mStatus;
   nsString                      mDefaultStatus;
   nsString                      mTitle;

   nsIScriptGlobalObjectOwner*   mGlobalObjectOwner; // Weak Reference
   nsIDocShell*                  mDocShell;  // Weak Reference
   nsIChromeEventHandler*        mChromeEventHandler; // Weak Reference
   nsCOMPtr<nsIDOMCrypto>        mCrypto;
   nsCOMPtr<nsIDOMPkcs11>        mPkcs11;
   nsCOMPtr<nsIPrincipal>        mDocumentPrincipal;
};

/* 
 * Timeout struct that holds information about each JavaScript
 * timeout.
 */
struct nsTimeoutImpl {
  PRInt32             ref_count;      /* reference count to shared usage */
  GlobalWindowImpl    *window;        /* window for which this timeout fires */
  JSString            *expr;          /* the JS expression to evaluate */
  JSObject            *funobj;        /* or function to call, if !expr */
  nsCOMPtr<nsITimer>  timer;         /* The actual timer object */
  jsval               *argv;          /* function actual arguments */
  PRUint16            argc;           /* and argument count */
  PRUint16            spare;          /* alignment padding */
  PRUint32            public_id;      /* Returned as value of setTimeout() */
  PRInt32             interval;       /* Non-zero if repetitive timeout */
  PRInt64             when;           /* nominal time to run this timeout */
  nsIPrincipal        *principal;     /* principals with which to execute */
  char                *filename;      /* filename of setTimeout call */
  PRUint32            lineno;         /* line number of setTimeout call */
  const char          *version;       /* JS language version string constant */
  PRUint32              firingDepth;    /* stack depth at which timeout isfiring */
  nsTimeoutImpl       *next;
};

//*****************************************************************************
// NavigatorImpl: Script "navigator" object
//*****************************************************************************   

class NavigatorImpl : public nsIScriptObjectOwner, public nsIDOMNavigator
{
public:
   NavigatorImpl();
   virtual ~NavigatorImpl();

   NS_DECL_ISUPPORTS
   NS_DECL_IDOMNAVIGATOR

   // nsIScriptObjectOnwer interface
   NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
   NS_IMETHOD SetScriptObject(void *aScriptObject);

protected:
   void *mScriptObject;
   nsIDOMMimeTypeArray* mMimeTypes;
   nsIDOMPluginArray* mPlugins;
};

class nsIURI;

//*****************************************************************************
// LocationImpl: Script "location" object
//*****************************************************************************   

class LocationImpl : public nsIDOMLocation, 
                     public nsIDOMNSLocation,
                     public nsIJSScriptObject
{

protected:
public:
   LocationImpl(nsIDocShell *aDocShell);
   virtual ~LocationImpl();

   NS_DECL_ISUPPORTS

   //nsIScriptObjectOwner
   NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
   NS_IMETHOD SetScriptObject(void *aScriptObject);

   NS_IMETHOD_(void)       SetDocShell(nsIDocShell *aDocShell);

   // nsIDOMLocation
   NS_IMETHOD    GetHash(nsAWritableString& aHash);
   NS_IMETHOD    SetHash(const nsAReadableString& aHash);
   NS_IMETHOD    GetHost(nsAWritableString& aHost);
   NS_IMETHOD    SetHost(const nsAReadableString& aHost);
   NS_IMETHOD    GetHostname(nsAWritableString& aHostname);
   NS_IMETHOD    SetHostname(const nsAReadableString& aHostname);
   NS_IMETHOD    GetHref(nsAWritableString& aHref);
   NS_IMETHOD    SetHref(const nsAReadableString& aHref);
   NS_IMETHOD    GetPathname(nsAWritableString& aPathname);
   NS_IMETHOD    SetPathname(const nsAReadableString& aPathname);
   NS_IMETHOD    GetPort(nsAWritableString& aPort);
   NS_IMETHOD    SetPort(const nsAReadableString& aPort);
   NS_IMETHOD    GetProtocol(nsAWritableString& aProtocol);
   NS_IMETHOD    SetProtocol(const nsAReadableString& aProtocol);
   NS_IMETHOD    GetSearch(nsAWritableString& aSearch);
   NS_IMETHOD    SetSearch(const nsAReadableString& aSearch);
   NS_IMETHOD    Reload(PRBool aForceget);
   NS_IMETHOD    Replace(const nsAReadableString& aUrl);
   NS_IMETHOD    Assign(const nsAReadableString& aUrl);
   NS_IMETHOD    ToString(nsAWritableString& aReturn);

   // nsIDOMNSLocation
   NS_IMETHOD    Reload(JSContext *cx, jsval *argv, PRUint32 argc);
   NS_IMETHOD    Replace(JSContext *cx, jsval *argv, PRUint32 argc);

   // nsIJSScriptObject
   virtual PRBool    AddProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp);
   virtual PRBool    DeleteProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp);
   virtual PRBool    GetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp);
   virtual PRBool    SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp);
   virtual PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj);
   virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                             PRBool* aDidDefineProperty);
   virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID);
   virtual void      Finalize(JSContext *aContext, JSObject *aObj);

   nsresult SetHrefWithContext(JSContext* cx, jsval val);

protected:
   nsresult SetURL(nsIURI* aURL);
   nsresult SetHrefWithBase(const nsAReadableString& aHref, 
                           nsIURI* aBase, 
                           PRBool aReplace);
   nsresult GetSourceURL(JSContext* cx,
                        nsIURI** sourceURL);
   nsresult CheckURL(nsIURI *url, nsIDocShellLoadInfo** aLoadInfo);

   nsIDocShell *mDocShell; // Weak Reference
   void *mScriptObject;
};

#define DOM_CONTROLLER
#ifdef DOM_CONTROLLER
class nsIContentViewerEdit;

class nsISelectionController;

class nsDOMWindowController : public nsIController
{
public:
	nsDOMWindowController( nsIDOMWindowInternal* aWindow );
    NS_DECL_ISUPPORTS
  	NS_DECL_NSICONTROLLER
  	
private:
  nsresult GetPresShell(nsIPresShell **aPresShell);
	nsresult GetEditInterface( nsIContentViewerEdit** aEditInterface);	
  nsresult GetSelectionController(nsISelectionController ** aSelCon);

	nsIDOMWindowInternal *mWindow;
};
#endif // DOM_CONTROLLER

#endif /* nsGlobalWindow_h___ */
