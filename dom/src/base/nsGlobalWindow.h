/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsGlobalWindow_h___
#define nsGlobalWindow_h___

// Local Includes
// Helper Classes
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
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
#include "nsIScriptSecurityManager.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMDocument.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMPkcs11.h"
#include "nsIPrincipal.h"
#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"
#include "nsIXPCScriptable.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "mozFlushType.h"

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
                         public nsPIDOMWindow,
                         public nsIDOMJSWindow,
                         public nsIScriptObjectPrincipal,
                         public nsIDOMEventReceiver,
                         public nsIDOM3EventTarget,
                         public nsIDOMViewCSS,
                         public nsSupportsWeakReference,
                         public nsIInterfaceRequestor
{
public:
  // public methods
  nsPIDOMWindow* GetPrivateParent();
  // callback for close event
  void ReallyCloseWindow();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIScriptGlobalObject
  virtual void SetContext(nsIScriptContext *aContext);
  virtual nsIScriptContext *GetContext();
  virtual nsresult SetNewDocument(nsIDOMDocument *aDocument,
                                  PRBool aRemoveEventListeners,
                                  PRBool aClearScopeHint);
  virtual void SetDocShell(nsIDocShell* aDocShell);
  virtual nsIDocShell *GetDocShell();
  virtual void SetOpenerWindow(nsIDOMWindowInternal *aOpener);
  virtual void SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner);
  virtual nsIScriptGlobalObjectOwner *GetGlobalObjectOwner();
  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);
  virtual JSObject *GetGlobalJSObject();
  virtual void OnFinalize(JSObject *aJSObject);
  virtual void SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts);

  // nsIScriptObjectPrincipal
  virtual nsIPrincipal* GetPrincipal();

  // nsIDOMWindow
  NS_DECL_NSIDOMWINDOW

  // nsIDOMWindow2
  NS_DECL_NSIDOMWINDOW2

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
  virtual NS_HIDDEN_(nsPIDOMWindow*) GetPrivateRoot();
  virtual NS_HIDDEN_(nsresult) GetObjectProperty(const PRUnichar* aProperty,
                                                 nsISupports** aObject);
  virtual NS_HIDDEN_(nsresult) Activate();
  virtual NS_HIDDEN_(nsresult) Deactivate();
  virtual NS_HIDDEN_(nsIFocusController*) GetRootFocusController();

  virtual NS_HIDDEN_(void) SetOpenerScriptURL(nsIURI* aURI);

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
  void ClearControllers();

  // Get the parent, returns null if this is a toplevel window
  nsIDOMWindowInternal *GetParentInternal();

  // popup tracking
  PRBool         IsPopupSpamWindow() const { return mIsPopupSpam; }
  void           SetPopupSpamWindow(PRBool aPopup) { mIsPopupSpam = aPopup; }

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
  PRUint32 CheckForAbusePoint();
  PRUint32 CheckOpenAllow(PRUint32 aAbuseLevel, const nsAString &aName);
  void     FireAbuseEvents(PRBool aBlocked, PRBool aWindow,
                           const nsAString &aPopupURL,
                           const nsAString &aPopupWindowFeatures);

  void FlushPendingNotifications(mozFlushType aType);
  void EnsureReflowFlushAndPaint();
  nsresult CheckSecurityWidthAndHeight(PRInt32* width, PRInt32* height);
  nsresult CheckSecurityLeftAndTop(PRInt32* left, PRInt32* top);
  static PRBool CanSetProperty(const char *aPrefName);

  static void MakeScriptDialogTitle(const nsAString &aInTitle,
                                    nsAString &aOutTitle);

  // Helper for window.find()
  nsresult FindInternal(const nsAString& aStr, PRBool caseSensitive,
                       PRBool backwards, PRBool wrapAround, PRBool wholeWord, 
                       PRBool searchInFrames, PRBool showDialog, 
                       PRBool *aReturn);

  nsresult ConvertCharset(const nsAString& aStr, char** aDest);

  PRBool   GetBlurSuppression();

  nsresult GetScrollXY(PRInt32* aScrollX, PRInt32* aScrollY);
  nsresult GetScrollMaxXY(PRInt32* aScrollMaxX, PRInt32* aScrollMaxY);

  PRBool IsFrame()
  {
    return GetParentInternal() != nsnull;
  }

protected:
  // When adding new member variables, be careful not to create cycles
  // through JavaScript.  If there is any chance that a member variable
  // could own objects that are implemented in JavaScript, then those
  // objects will keep the global object (this object) alive.  To prevent
  // these cycles, ownership of such members must be released in
  // |CleanUp| and |SetDocShell|.
  PRPackedBool                  mFirstDocumentLoad;
  PRPackedBool                  mIsScopeClear;
  PRPackedBool                  mFullScreen;
  PRPackedBool                  mIsClosed;
  PRPackedBool                  mOpenerWasCleared;
  PRPackedBool                  mIsPopupSpam;
  nsCOMPtr<nsIScriptContext>    mContext;
  nsCOMPtr<nsIDOMWindowInternal> mOpener;
  nsCOMPtr<nsIControllers>      mControllers;
  nsCOMPtr<nsIEventListenerManager> mListenerManager;
  JSObject*                     mJSObject;
  nsRefPtr<NavigatorImpl>       mNavigator;
  nsRefPtr<ScreenImpl>          mScreen;
  nsRefPtr<HistoryImpl>         mHistory;
  nsRefPtr<nsDOMWindowList>     mFrames;
  nsRefPtr<LocationImpl>        mLocation;
  nsRefPtr<BarPropImpl>         mMenubar;
  nsRefPtr<BarPropImpl>         mToolbar;
  nsRefPtr<BarPropImpl>         mLocationbar;
  nsRefPtr<BarPropImpl>         mPersonalbar;
  nsRefPtr<BarPropImpl>         mStatusbar;
  nsRefPtr<BarPropImpl>         mScrollbars;
  nsCOMPtr<nsIWeakReference>    mWindowUtils;
  nsTimeoutImpl*                mTimeouts;
  nsTimeoutImpl**               mTimeoutInsertionPoint;
  PRUint32                      mTimeoutPublicIdCounter;
  PRUint32                      mTimeoutFiringDepth;
  PRTime                        mLastMouseButtonAction;
  nsString                      mStatus;
  nsString                      mDefaultStatus;

  nsIScriptGlobalObjectOwner*   mGlobalObjectOwner; // Weak Reference
  nsIDocShell*                  mDocShell;  // Weak Reference
  nsEvent*                      mCurrentEvent;
  nsCOMPtr<nsIDOMCrypto>        mCrypto;
  nsCOMPtr<nsIDOMPkcs11>        mPkcs11;
  nsCOMPtr<nsIPrincipal>        mDocumentPrincipal;

  // XXX We need mNavigatorHolder because we make two SetNewDocument()
  // calls when transitioning from page to page. This keeps a reference
  // to the JSObject holder for the navigator object in between
  // SetNewDocument() calls so that the JSObject doesn't get garbage
  // collected in between these calls.
  // See bug 163645 for more on why we need this and bug 209607 for info
  // on how we can remove the need for this.
  nsCOMPtr<nsIXPConnectJSObjectHolder> mNavigatorHolder;

  friend class nsDOMScriptableHelper;
  friend class nsDOMWindowUtils;
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
#ifdef DEBUG_jst
    {
      extern int gTimeoutCnt;

      ++gTimeoutCnt;
    }
#endif

    memset(this, 0, sizeof(*this));

    MOZ_COUNT_CTOR(nsTimeoutImpl);
  }

  ~nsTimeoutImpl()
  {
#ifdef DEBUG_jst
    {
      extern int gTimeoutCnt;

      --gTimeoutCnt;
    }
#endif

    MOZ_COUNT_DTOR(nsTimeoutImpl);
  }

  void Release(nsIScriptContext* aContext);
  void AddRef();

  // Window for which this timeout fires
  GlobalWindowImpl *mWindow;

  // The JS expression to evaluate or function to call, if !mExpr
  JSString *mExpr;
  JSObject *mFunObj;

  // The actual timer object
  nsCOMPtr<nsITimer> mTimer;

  // Function actual arguments and argument count
  jsval *mArgv;
  PRUint16 mArgc;

  // True if the timeout was cleared
  PRPackedBool mCleared;

  // True if this is one of the timeouts that are currently running
  PRPackedBool mRunning;

  // Returned as value of setTimeout()
  PRUint32 mPublicId;

  // Non-zero if repetitive timeout
  PRInt32 mInterval;

  // Nominal time to run this timeout
  PRInt64 mWhen;

  // Principal with which to execute
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // filename, line number and JS language version string of the
  // caller of setTimeout()
  char *mFileName;
  PRUint32 mLineNo;
  const char *mVersion;

  // stack depth at which timeout is firing
  PRUint32 mFiringDepth;

  // Pointer to the next timeout in the linked list of scheduled
  // timeouts
  nsTimeoutImpl *mNext;

private:
  // reference count for shared usage
  PRInt32 mRefCnt;
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
  void LoadingNewDocument();
  nsresult RefreshMIMEArray();

protected:
  nsRefPtr<MimeTypeArrayImpl> mMimeTypes;
  nsRefPtr<PluginArrayImpl> mPlugins;
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

  void SetDocShell(nsIDocShell *aDocShell);

  // nsIDOMLocation
  NS_DECL_NSIDOMLOCATION

  // nsIDOMNSLocation
  NS_DECL_NSIDOMNSLOCATION

protected:
  // In the case of jar: uris, we sometimes want the place the jar was
  // fetched from as the URI instead of the jar: uri itself.  Pass in
  // PR_TRUE for aGetInnermostURI when that's the case.
  nsresult GetURI(nsIURI** aURL, PRBool aGetInnermostURI = PR_FALSE);
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
  nsresult FindUsableBaseURI(nsIURI * aBaseURI, nsIDocShell * aParent, nsIURI ** aUsableURI);

  nsIDocShell *mDocShell; // Weak Reference
};

/* factory function */
nsresult NS_NewScriptGlobalObject(PRBool aIsChrome,
                                  nsIScriptGlobalObject **aResult);

#endif /* nsGlobalWindow_h___ */
