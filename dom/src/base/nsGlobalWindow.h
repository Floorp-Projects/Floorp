/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Dan Rosen <dr@netscape.com>
 *   Vidur Apparao <vidur@netscape.com>
 *   Johnny Stenback <jst@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIObserver.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMNSLocation.h"
#include "nsIDOMWindowInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMJSWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsITimer.h"
#include "nsIWebBrowserChrome.h"
#include "nsPIDOMWindow.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMPkcs11.h"
#include "nsISidebar.h"
#include "nsIPrincipal.h"
#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"
#include "nsIXPCScriptable.h"
#include "nsPoint.h"
#include "nsSize.h"

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
// Beware that all scriptable interfaces implemented by
// GlobalWindowImpl will be reachable from JS, if you make this class
// implement new interfaces you better know what you're
// doing. Security wise this is very sensitive code. --
// jst@netscape.com


class GlobalWindowImpl : public nsIScriptGlobalObject,
                         public nsIDOMWindowInternal,
                         public nsIDOMJSWindow,
                         public nsIScriptObjectPrincipal,
                         public nsIDOMEventReceiver,
                         public nsIDOM3EventTarget,
                         public nsPIDOMWindow,
                         public nsIDOMViewCSS,
                         public nsSupportsWeakReference,
                         public nsIInterfaceRequestor
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIScriptGlobalObject
  NS_IMETHOD SetContext(nsIScriptContext *aContext);
  NS_IMETHOD GetContext(nsIScriptContext **aContext);
  NS_IMETHOD SetNewDocument(nsIDOMDocument *aDocument,
                            PRBool aRemoveEventListeners,
                            PRBool aClearScopeHint);
  NS_IMETHOD SetDocShell(nsIDocShell* aDocShell);
  NS_IMETHOD GetDocShell(nsIDocShell** aDocShell);
  NS_IMETHOD SetOpenerWindow(nsIDOMWindowInternal *aOpener);
  NS_IMETHOD SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner);
  NS_IMETHOD GetGlobalObjectOwner(nsIScriptGlobalObjectOwner** aOwner);
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD_(JSObject *) GetGlobalJSObject();
  NS_IMETHOD OnFinalize(JSObject *aJSObject);
  NS_IMETHOD SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts);

  // nsIScriptObjectPrincipal
  NS_IMETHOD GetPrincipal(nsIPrincipal **prin);

  // nsIDOMWindow
  NS_DECL_NSIDOMWINDOW

  // nsIDOMWindowInternal
  NS_DECL_NSIDOMWINDOWINTERNAL

  // nsIDOMJSWindow
  NS_DECL_NSIDOMJSWINDOW

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  // nsIDOMEventReceiver
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

  // nsPIDOMWindow
  NS_IMETHOD GetPrivateParent(nsPIDOMWindow** aResult);
  NS_IMETHOD GetPrivateRoot(nsIDOMWindowInternal** aResult);
  NS_IMETHOD GetObjectProperty(const PRUnichar* aProperty,
                               nsISupports** aObject);
  NS_IMETHOD Activate();
  NS_IMETHOD Deactivate();
  NS_IMETHOD GetChromeEventHandler(nsIChromeEventHandler** aHandler);
  NS_IMETHOD HasMutationListeners(PRUint32 aMutationEventType,
                                  PRBool* aResult);
  NS_IMETHOD SetMutationListeners(PRUint32 aEventType);
  NS_IMETHOD GetRootFocusController(nsIFocusController** aResult);
  NS_IMETHOD GetExtantDocument(nsIDOMDocument** aDocument);

  NS_IMETHOD ReallyCloseWindow();
  NS_IMETHOD IsLoadingOrRunningTimeout(PRBool* aResult);

  NS_IMETHOD GetFrameElementInternal(nsIDOMElement** aFrameElement);
  NS_IMETHOD SetFrameElementInternal(nsIDOMElement* aFrameElement);

  // nsIDOMViewCSS
  NS_DECL_NSIDOMVIEWCSS

  // nsIDOMAbstractView
  NS_DECL_NSIDOMABSTRACTVIEW

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR

  // Object Management
  GlobalWindowImpl();

  static void ShutDown();
  static PRBool IsCallerChrome();

protected:
  // Object Management
  virtual ~GlobalWindowImpl();
  void CleanUp();

  // Get the parent, returns null if this is a toplevel window
  void GetParentInternal(nsIDOMWindowInternal **parent);

  // Window Control Functions
  NS_IMETHOD OpenInternal(const nsAString& aUrl,
                          const nsAString& aName,
                          const nsAString& aOptions,
                          PRBool aDialog, jsval *argv, PRUint32 argc,
                          nsISupports *aExtraArgument, nsIDOMWindow **aReturn);
  static void CloseWindow(nsISupports* aWindow);

  // Timeout Functions
  nsresult SetTimeoutOrInterval(PRBool aIsInterval, PRInt32* aReturn);
  void RunTimeout(nsTimeoutImpl *aTimeout);
  nsresult ClearTimeoutOrInterval();
  void ClearAllTimeouts();
  void InsertTimeoutIntoList(nsTimeoutImpl **aInsertionPoint,
                             nsTimeoutImpl *aTimeout);
  static void TimerCallback(nsITimer *aTimer, void *aClosure);

  // Helper Functions
  nsresult GetTreeOwner(nsIDocShellTreeOwner** aTreeOwner);
  nsresult GetTreeOwner(nsIBaseWindow** aTreeOwner);
  nsresult GetWebBrowserChrome(nsIWebBrowserChrome** aBrowserChrome);
  nsresult GetScrollInfo(nsIScrollableView** aScrollableView, float* aP2T,
                         float* aT2P);
  nsresult SecurityCheckURL(const char *aURL);
  PRBool   CheckForAbusePoint();

  void FlushPendingNotifications(PRBool aFlushReflows);
  void EnsureReflowFlushAndPaint();
  nsresult CheckSecurityWidthAndHeight(PRInt32* width, PRInt32* height);
  nsresult CheckSecurityLeftAndTop(PRInt32* left, PRInt32* top);
  static nsresult CheckSecurityIsChromeCaller(PRBool *isChrome);
  static PRBool CanSetProperty(const char *aPrefName);

  void MakeScriptDialogTitle(const nsAString &aInTitle, nsAString &aOutTitle);

  // Helper for window.find()
  nsresult FindInternal(const nsAString& aStr, PRBool caseSensitive,
                       PRBool backwards, PRBool wrapAround, PRBool wholeWord, 
                       PRBool searchInFrames, PRBool showDialog, 
                       PRBool *aReturn);

protected:
  // When adding new member variables, be careful not to create cycles
  // through JavaScript.  If there is any chance that a member variable
  // could own objects that are implemented in JavaScript, then those
  // objects will keep the global object (this object) alive.  To prevent
  // these cycles, ownership of such members must be released in
  // |CleanUp| and |SetDocShell|.
  nsCOMPtr<nsIScriptContext>    mContext;
  nsCOMPtr<nsIDOMDocument>      mDocument;
  nsCOMPtr<nsIDOMWindowInternal> mOpener;
  nsCOMPtr<nsIControllers>      mControllers;
  nsCOMPtr<nsIEventListenerManager> mListenerManager;
  nsCOMPtr<nsISidebar>          mSidebar;
  JSObject*                     mJSObject;
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
  PRPackedBool                  mFirstDocumentLoad;
  PRPackedBool                  mIsScopeClear;
  PRPackedBool                  mIsDocumentLoaded; // true between onload and onunload events
  PRPackedBool                  mFullScreen;
  PRTime                        mLastMouseButtonAction;
  nsString                      mStatus;
  nsString                      mDefaultStatus;

  nsIScriptGlobalObjectOwner*   mGlobalObjectOwner; // Weak Reference
  nsIDocShell*                  mDocShell;  // Weak Reference
  PRUint32                      mMutationBits;
  nsCOMPtr<nsIChromeEventHandler> mChromeEventHandler; // [Strong] We break it when we get torn down.
  nsCOMPtr<nsIDOMCrypto>        mCrypto;
  nsCOMPtr<nsIDOMPkcs11>        mPkcs11;
  nsCOMPtr<nsIPrincipal>        mDocumentPrincipal;

  nsIDOMElement*                mFrameElement; // WEAK

  friend class nsDOMScriptableHelper;
  static nsIXPConnect *sXPConnect;
  static nsIScriptSecurityManager *sSecMan;
  static nsIFactory *sComputedDOMStyleFactory;
};

/*
 * nsGlobalChromeWindow inherits from GlobalWindowImpl. It is the global
 * object created for a Chrome Window only.
 */
class nsGlobalChromeWindow : public GlobalWindowImpl,
                             public nsIDOMChromeWindow
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMChromeWindow interface
  NS_DECL_NSIDOMCHROMEWINDOW

protected:
  nsresult GetMainWidget(nsIWidget** aMainWidget);

  nsString mTitle;
};

/*
 * Timeout struct that holds information about each JavaScript
 * timeout.
 */
struct nsTimeoutImpl
{
  nsTimeoutImpl()
  {
    memset(this, 0, sizeof(*this));

    MOZ_COUNT_CTOR(nsTimeoutImpl);
  }

  ~nsTimeoutImpl()
  {
    MOZ_COUNT_DTOR(nsTimeoutImpl);
  }

  void Release(nsIScriptContext* aContext);
  void AddRef();

  GlobalWindowImpl    *window;        /* window for which this timeout fires */
  JSString            *expr;          /* the JS expression to evaluate */
  JSObject            *funobj;        /* or function to call, if !expr */
  nsCOMPtr<nsITimer>  timer;          /* The actual timer object */
  jsval               *argv;          /* function actual arguments */
  PRUint16            argc;           /* and argument count */
  PRPackedBool        cleared;        /* True if the timeout was cleared */
  PRPackedBool        spare;          /* alignment padding */
  PRUint32            public_id;      /* Returned as value of setTimeout() */
  PRInt32             interval;       /* Non-zero if repetitive timeout */
  PRInt64             when;           /* nominal time to run this timeout */
  nsCOMPtr<nsIPrincipal> principal;   /* principals with which to execute */
  char                *filename;      /* filename of setTimeout call */
  PRUint32            lineno;         /* line number of setTimeout call */
  const char          *version;       /* JS language version string constant */
  PRUint32            firingDepth;    /* stack depth at which timeout is
                                         firing */
  nsTimeoutImpl       *next;

private:
  PRInt32             mRefCnt;        /* reference count for shared usage */
};

//*****************************************************************************
// NavigatorImpl: Script "navigator" object
//*****************************************************************************

class NavigatorImpl : public nsIDOMNavigator,
                      public nsIDOMJSNavigator
{
public:
  NavigatorImpl(nsIDocShell *aDocShell);
  virtual ~NavigatorImpl();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMNAVIGATOR
  NS_DECL_NSIDOMJSNAVIGATOR
  
  void SetDocShell(nsIDocShell *aDocShell);
  nsresult RefreshMIMEArray();

protected:
  MimeTypeArrayImpl* mMimeTypes;
  PluginArrayImpl* mPlugins;
  nsIDocShell* mDocShell; // weak reference

  static jsval       sPrefInternal_id;
};

class nsIURI;

//*****************************************************************************
// LocationImpl: Script "location" object
//*****************************************************************************

class LocationImpl : public nsIDOMLocation,
                     public nsIDOMNSLocation
{
public:
  LocationImpl(nsIDocShell *aDocShell);
  virtual ~LocationImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD_(void)       SetDocShell(nsIDocShell *aDocShell);

  // nsIDOMLocation
  NS_DECL_NSIDOMLOCATION

  // nsIDOMNSLocation
  NS_DECL_NSIDOMNSLOCATION

protected:
  nsresult GetURI(nsIURI** aURL);
  nsresult GetWritableURI(nsIURI** aURL);
  nsresult SetURI(nsIURI* aURL);
  nsresult SetHrefWithBase(const nsAString& aHref, nsIURI* aBase,
                           PRBool aReplace);
  nsresult SetHrefWithContext(JSContext* cx, const nsAString& aHref,
                              PRBool aReplace);

  nsresult GetSourceURL(JSContext* cx, nsIURI** sourceURL);
  nsresult GetSourceBaseURL(JSContext* cx, nsIURI** sourceURL);
  nsresult GetSourceDocument(JSContext* cx, nsIDocument** aDocument);

  nsresult CheckURL(nsIURI *url, nsIDocShellLoadInfo** aLoadInfo);

  nsIDocShell *mDocShell; // Weak Reference
};

#define DOM_CONTROLLER
#ifdef DOM_CONTROLLER
class nsIContentViewerEdit;

class nsISelectionController;

class nsDOMWindowController : public nsIController,
                              public nsIObserver
{
public:
  nsDOMWindowController(nsIDOMWindowInternal* aWindow);
  virtual ~nsDOMWindowController();
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTROLLER
  NS_DECL_NSIOBSERVER

private:
  nsresult GetEventStateManager(nsIEventStateManager **esm);
  static int PR_CALLBACK BrowseWithCaretPrefCallback(const char* aPrefName, void* instance_data);
  nsresult GetPresShell(nsIPresShell **aPresShell);
  nsresult GetEditInterface(nsIContentViewerEdit** aEditInterface);
  nsresult GetSelectionController(nsISelectionController ** aSelCon);

  nsresult DoCommandWithWebNavigationInterface(const char * aCommandName);
  nsresult DoCommandWithEditInterface(const char * aCommandName);
  nsresult DoCommandWithSelectionController(const char * aCommandName);

  nsIDOMWindowInternal *mWindow;
  PRBool mBrowseWithCaret;
};
#endif // DOM_CONTROLLER

#endif /* nsGlobalWindow_h___ */
