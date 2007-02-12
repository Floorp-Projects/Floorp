/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
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
 *   Brendan Eich <brendan@mozilla.org>
 *   David Hyatt (hyatt@netscape.com)
 *   Dan Rosen <dr@netscape.com>
 *   Vidur Apparao <vidur@netscape.com>
 *   Johnny Stenback <jst@netscape.com>
 *   Mark Hammond <mhammond@skippinet.com.au>
 *   Ryan Jones <sciguyryan@gmail.com>
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

// Local Includes
#include "nsGlobalWindow.h"
#include "nsScreen.h"
#include "nsHistory.h"
#include "nsBarProps.h"
#include "nsDOMStorage.h"
#include "nsDOMError.h"

// Helper Classes
#include "nsXPIDLString.h"
#include "nsJSUtils.h"
#include "prmem.h"
#include "jsapi.h"              // for JSAutoRequest
#include "jsdbgapi.h"           // for JS_ClearWatchPointsForObject
#include "nsReadableUtils.h"
#include "nsDOMClassInfo.h"

// Other Classes
#include "nsIEventListenerManager.h"
#include "nsEscape.h"
#include "nsStyleCoord.h"
#include "nsMimeTypeArray.h"
#include "nsNetUtil.h"
#include "nsPluginArray.h"
#include "nsIPluginHost.h"
#ifdef OJI
#include "nsIJVMManager.h"
#endif
#include "nsContentCID.h"
#include "nsLayoutStatics.h"

// Interfaces Needed
#include "nsIWidget.h"
#include "nsIBaseWindow.h"
#include "nsICharsetConverterManager.h"
#include "nsIContent.h"
#include "nsIContentViewerEdit.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocCharset.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMPkcs11.h"
#include "nsDOMString.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsThreadUtils.h"
#include "nsIEventStateManager.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIPrefBranch.h"
#include "nsIPresShell.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIProgrammingLanguage.h"
#include "nsIAuthPrompt.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollableView.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsISelectionController.h"
#include "nsISelection.h"
#include "nsIPrompt.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserFind.h"  // For window.find()
#include "nsIWebContentHandlerRegistrar.h"
#include "nsIWindowMediator.h"  // For window.find()
#include "nsIComputedDOMStyle.h"
#include "nsIEntropyCollector.h"
#include "nsDOMCID.h"
#include "nsDOMError.h"
#include "nsDOMWindowUtils.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIContentViewer.h"
#include "nsDOMClassInfo.h"
#include "nsIJSNativeInitializer.h"
#include "nsIFullScreen.h"
#include "nsIScriptError.h"
#include "nsIScriptEventManager.h" // For GetInterface()
#include "nsIConsoleService.h"
#include "nsIControllerContext.h"
#include "nsGlobalWindowCommands.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsCSSProps.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsEventDispatcher.h"
#include "nsIObserverService.h"
#include "nsNetUtil.h"

#include "plbase64.h"

#ifdef NS_PRINTING
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"
#include "nsIWebBrowserPrint.h"
#endif

#include "nsWindowRoot.h"
#include "nsNetCID.h"
#include "nsIArray.h"
#include "nsIScriptRuntime.h"

// XXX An unfortunate dependency exists here (two XUL files).
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"

#include "nsIBindingManager.h"
#include "nsIXBLService.h"

// used for popup blocking, needs to be converted to something
// belonging to the back-end like nsIContentPolicy
#include "nsIPopupWindowManager.h"

#ifdef MOZ_LOGGING
// so we can get logging even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gDOMLeakPRLog;
#endif

#include "nsBuildID.h"

nsIFactory *nsGlobalWindow::sComputedDOMStyleFactory   = nsnull;

static nsIEntropyCollector *gEntropyCollector          = nsnull;
static PRInt32              gRefCnt                    = 0;
static PRInt32              gOpenPopupSpamCount        = 0;
static PopupControlState    gPopupControlState         = openAbused;
static PRInt32              gRunningTimeoutDepth       = 0;

#ifdef DEBUG_jst
PRInt32 gTimeoutCnt                                    = 0;
#endif

#if defined(DEBUG_bryner) || defined(DEBUG_chb)
#define DEBUG_PAGE_CACHE
#endif

// The shortest interval/timeout we permit
#define DOM_MIN_TIMEOUT_VALUE 10 // 10ms

// The longest interval (as PRIntervalTime) we permit, or that our
// timer code can handle, really. See DELAY_INTERVAL_LIMIT in
// nsTimerImpl.h for details.
#define DOM_MAX_TIMEOUT_VALUE    PR_BIT(8 * sizeof(PRIntervalTime) - 1)

#define FORWARD_TO_OUTER(method, args, err_rval)                              \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return outer->method args;                                                \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_VOID(method, args)                                   \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return;                                                                 \
    }                                                                         \
    outer->method args;                                                       \
    return;                                                                   \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_OUTER_CHROME(method, args, err_rval)                       \
  PR_BEGIN_MACRO                                                              \
  if (IsInnerWindow()) {                                                      \
    nsGlobalWindow *outer = GetOuterWindowInternal();                         \
    if (!outer) {                                                             \
      NS_WARNING("No outer window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return ((nsGlobalChromeWindow *)outer)->method args;                      \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER(method, args, err_rval)                              \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return err_rval;                                                        \
    }                                                                         \
    return GetCurrentInnerWindowInternal()->method args;                      \
  }                                                                           \
  PR_END_MACRO

#define FORWARD_TO_INNER_VOID(method, args)                                   \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      NS_WARNING("No inner window available!");                               \
      return;                                                                 \
    }                                                                         \
    GetCurrentInnerWindowInternal()->method args;                             \
    return;                                                                   \
  }                                                                           \
  PR_END_MACRO

// Same as FORWARD_TO_INNER, but this will create a fresh inner if an
// inner doesn't already exists.
#define FORWARD_TO_INNER_CREATE(method, args)                                 \
  PR_BEGIN_MACRO                                                              \
  if (IsOuterWindow()) {                                                      \
    if (!mInnerWindow) {                                                      \
      if (mIsClosed) {                                                        \
        return NS_ERROR_NOT_AVAILABLE;                                        \
      }                                                                       \
      nsCOMPtr<nsIDOMDocument> doc;                                           \
      nsresult fwdic_nr = GetDocument(getter_AddRefs(doc));                   \
      NS_ENSURE_SUCCESS(fwdic_nr, fwdic_nr);                                  \
      if (!mInnerWindow) {                                                    \
        return NS_ERROR_NOT_AVAILABLE;                                        \
      }                                                                       \
    }                                                                         \
    return GetCurrentInnerWindowInternal()->method args;                      \
  }                                                                           \
  PR_END_MACRO

// CIDs
#ifdef OJI
static NS_DEFINE_CID(kJVMServiceCID, NS_JVMMANAGER_CID);
#endif
static NS_DEFINE_CID(kXULControllersCID, NS_XULCONTROLLERS_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);

static const char sJSStackContractID[] = "@mozilla.org/js/xpc/ContextStack;1";

static const char kCryptoContractID[] = NS_CRYPTO_CONTRACTID;
static const char kPkcs11ContractID[] = NS_PKCS11_CONTRACTID;

static PRBool
IsAboutBlank(nsIURI* aURI)
{
  NS_PRECONDITION(aURI, "Must have URI");
    
  // GetSpec can be expensive for some URIs, so check the scheme first.
  PRBool isAbout = PR_FALSE;
  if (NS_FAILED(aURI->SchemeIs("about", &isAbout)) || !isAbout) {
    return PR_FALSE;
  }
    
  nsCAutoString str;
  aURI->GetSpec(str);
  return str.EqualsLiteral("about:blank");  
}

static void
StripNullChars(const nsAString& aInStr,
               nsAString& aOutStr)
{
  // In common cases where we don't have nulls in the
  // string we can simple simply bypass the checking code.
  PRInt32 firstNullPos = aInStr.FindChar('\0');
  if (firstNullPos == kNotFound) {
    aOutStr.Assign(aInStr);
    return;
  }
  
  nsAString::const_iterator start, end;
  aInStr.BeginReading(start); 
  aInStr.EndReading(end); 

  while (start != end) {
    if (*start != '\0')
      aOutStr.Append(*start);
    ++start;
  }
}

/**
 * An indirect observer object that means we don't have to implement nsIObserver
 * on nsGlobalWindow, where any script could see it.
 */
class nsGlobalWindowObserver : public nsIObserver {
public:
  nsGlobalWindowObserver(nsGlobalWindow* aWindow) : mWindow(aWindow) {}
  NS_DECL_ISUPPORTS
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
  {
    if (!mWindow)
      return NS_OK;
    return mWindow->Observe(aSubject, aTopic, aData);
  }
  void Forget() { mWindow = nsnull; }
private:
  nsGlobalWindow* mWindow;
};

NS_IMPL_ISUPPORTS1(nsGlobalWindowObserver, nsIObserver)

nsTimeout::nsTimeout()
{
#ifdef DEBUG_jst
  {
    extern int gTimeoutCnt;

    ++gTimeoutCnt;
  }
#endif

  memset(this, 0, sizeof(*this));

  MOZ_COUNT_CTOR(nsTimeout);
}

nsTimeout::~nsTimeout()
{
#ifdef DEBUG_jst
  {
    extern int gTimeoutCnt;

    --gTimeoutCnt;
  }
#endif

  MOZ_COUNT_DTOR(nsTimeout);
}


  
//*****************************************************************************
//***    nsGlobalWindow: Object Management
//*****************************************************************************

nsGlobalWindow::nsGlobalWindow(nsGlobalWindow *aOuterWindow)
  : nsPIDOMWindow(aOuterWindow),
    mIsFrozen(PR_FALSE),
    mFullScreen(PR_FALSE),
    mIsClosed(PR_FALSE), 
    mInClose(PR_FALSE), 
    mHavePendingClose(PR_FALSE),
    mHadOriginalOpener(PR_FALSE),
    mIsPopupSpam(PR_FALSE),
    mBlockScriptedClosingFlag(PR_FALSE),
    mFireOfflineStatusChangeEventOnThaw(PR_FALSE),
    mCreatingInnerWindow(PR_FALSE),
    mGlobalObjectOwner(nsnull),
    mTimeoutInsertionPoint(nsnull),
    mTimeoutPublicIdCounter(1),
    mTimeoutFiringDepth(0),
    mJSObject(nsnull),
    mPendingStorageEvents(nsnull)
#ifdef DEBUG
    , mSetOpenerWindowCalled(PR_FALSE)
#endif
{
  memset(mScriptGlobals, 0, sizeof(mScriptGlobals));
  nsLayoutStatics::AddRef();

  // Initialize the PRCList (this).
  PR_INIT_CLIST(this);

  // Initialize timeout storage
  PR_INIT_CLIST(&mTimeouts);

  if (aOuterWindow) {
    // |this| is an inner window, add this inner window to the outer
    // window list of inners.
    PR_INSERT_AFTER(this, aOuterWindow);

    mObserver = new nsGlobalWindowObserver(this);
    if (mObserver) {
      NS_ADDREF(mObserver);
      nsCOMPtr<nsIObserverService> os =
        do_GetService("@mozilla.org/observer-service;1");
      if (os) {
        // Watch for online/offline status changes so we can fire events. Use
        // a strong reference.
        os->AddObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                        PR_FALSE);

        // Watch for dom-storage-chaged so we can fire storage
        // events. Use a strong reference.
        os->AddObserver(mObserver, "dom-storage-changed", PR_FALSE);
      }
    }
  } else {
    // |this| is an outer window. Outer windows start out frozen and
    // remain frozen until they get an inner window, so freeze this
    // outer window here.
    Freeze();

    mObserver = nsnull;
  }

  // We could have failed the first time through trying
  // to create the entropy collector, so we should
  // try to get one until we succeed.
  if (gRefCnt++ == 0 || !gEntropyCollector) {
    CallGetService(NS_ENTROPYCOLLECTOR_CONTRACTID, &gEntropyCollector);
  }
#ifdef DEBUG
  printf("++DOMWINDOW == %d\n", gRefCnt);
#endif

#ifdef PR_LOGGING
  if (!gDOMLeakPRLog)
    gDOMLeakPRLog = PR_NewLogModule("DOMLeak");

  if (gDOMLeakPRLog)
    PR_LOG(gDOMLeakPRLog, PR_LOG_DEBUG,
           ("DOMWINDOW %p created outer=%p", this, aOuterWindow));
#endif
}

nsGlobalWindow::~nsGlobalWindow()
{
  if (!--gRefCnt) {
    NS_IF_RELEASE(gEntropyCollector);
  }
#ifdef DEBUG
  printf("--DOMWINDOW == %d\n", gRefCnt);
#endif

#ifdef PR_LOGGING
  if (gDOMLeakPRLog)
    PR_LOG(gDOMLeakPRLog, PR_LOG_DEBUG,
           ("DOMWINDOW %p destroyed", this));
#endif

  if (mObserver) {
    nsCOMPtr<nsIObserverService> os =
      do_GetService("@mozilla.org/observer-service;1");
    if (os) {
      os->RemoveObserver(mObserver, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      os->RemoveObserver(mObserver, "dom-storage-changed");
    }

    // Drop its reference to this dying window, in case for some bogus reason
    // the object stays around.
    mObserver->Forget();
    NS_RELEASE(mObserver);
  }

  if (IsOuterWindow()) {
    // An outer window is destroyed with inner windows still possibly
    // alive, iterate through the inner windows and null out their
    // back pointer to this outer, and pull them out of the list of
    // inner windows.

    nsGlobalWindow *w;
    while ((w = (nsGlobalWindow *)PR_LIST_HEAD(this)) != this) {
      NS_ASSERTION(w->mOuterWindow == this, "Uh, bad outer window pointer?");

      w->mOuterWindow = nsnull;

      PR_REMOVE_AND_INIT_LINK(w);
    }
  } else {
    if (mListenerManager) {
      mListenerManager->Disconnect();
      mListenerManager = nsnull;
    }

    // An inner window is destroyed, pull it out of the outer window's
    // list if inner windows.

    PR_REMOVE_LINK(this);

    // If our outer window's inner window is this window, null out the
    // outer window's reference to this window that's being deleted.
    nsGlobalWindow *outer = GetOuterWindowInternal();
    if (outer && outer->mInnerWindow == this) {
      outer->mInnerWindow = nsnull;
    }
  }

  mDocument = nsnull;           // Forces Release
  mDoc = nsnull;

  NS_ASSERTION(!mArguments, "mArguments wasn't cleaned up properly!");

  CleanUp();

  delete mPendingStorageEvents;

  nsLayoutStatics::Release();
}

// static
void
nsGlobalWindow::ShutDown()
{
  NS_IF_RELEASE(sComputedDOMStyleFactory);
}

void
nsGlobalWindow::CleanUp()
{
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

  ClearControllers();

  mOpener = nsnull;             // Forces Release
  if (mContext) {
    mContext->SetOwner(nsnull);
    mContext = nsnull;            // Forces Release
  }
  mChromeEventHandler = nsnull; // Forces Release

  if (IsOuterWindow() && IsPopupSpamWindow()) {
    SetPopupSpamWindow(PR_FALSE);
    --gOpenPopupSpamCount;
  }

  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();

  if (inner) {
    inner->CleanUp();
  }

  PRUint32 scriptIndex;
  NS_STID_FOR_INDEX(scriptIndex) {
    mInnerWindowHolders[scriptIndex] = nsnull;
  }
  mArguments = nsnull;
  mArgumentsLast = nsnull;
}

void
nsGlobalWindow::ClearControllers()
{
  if (mControllers) {
    PRUint32 count;
    mControllers->GetControllerCount(&count);

    while (count--) {
      nsCOMPtr<nsIController> controller;
      mControllers->GetControllerAt(count, getter_AddRefs(controller));

      nsCOMPtr<nsIControllerContext> context = do_QueryInterface(controller);
      if (context)
        context->SetCommandContext(nsnull);
    }

    mControllers = nsnull;
  }
}

void
nsGlobalWindow::FreeInnerObjects(PRBool aClearScope)
{
  NS_ASSERTION(IsInnerWindow(), "Don't free inner objects on an outer window");

  ClearAllTimeouts();

  mChromeEventHandler = nsnull;

  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nsnull;
  }

  if (mDocument) {
    NS_ASSERTION(mDoc, "Why is mDoc null?");

    // Remember the document's principal.
    mDocumentPrincipal = mDoc->NodePrincipal();
  }

  // Remove our reference to the document and the document principal.
  mDocument = nsnull;
  mDoc = nsnull;

  if (aClearScope) {
    PRUint32 lang_id;
    NS_STID_FOR_ID(lang_id) {
      // Note that scx comes from the outer window.  If this is an inner
      // window, it may not be the current inner for its outer.
      nsIScriptContext *scx = GetScriptContextInternal(lang_id);
      if (scx)
        scx->ClearScope(mScriptGlobals[NS_STID_INDEX(lang_id)], PR_TRUE);
    }
  }
}

//*****************************************************************************
// nsGlobalWindow::nsISupports
//*****************************************************************************

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalWindow)

// QueryInterface implementation for nsGlobalWindow
NS_INTERFACE_MAP_BEGIN(nsGlobalWindow)
  // Make sure this matches the cast in nsGlobalWindow::FromWrapper()
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowInternal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow2)
  NS_INTERFACE_MAP_ENTRY(nsIDOMJSWindow)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMViewCSS)
  NS_INTERFACE_MAP_ENTRY(nsIDOMAbstractView)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorageWindow)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION(nsGlobalWindow)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Window)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsGlobalWindow, nsIScriptGlobalObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsGlobalWindow, nsIScriptGlobalObject)


NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsGlobalWindow, nsIScriptGlobalObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContext)

  // Strange bug: If you uncomment this line you will find that
  // the cycle collector crashes when working with multiple open
  // top-level windows. It is as though the windows somehow
  // race with one another. How can this be? Curious.
  //
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOpener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mControllers)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mArguments)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mArgumentsLast)

  for (PRUint32 i = 0; i < NS_STID_ARRAY_UBOUND; ++i) {      
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptContexts[i])
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(gGlobalStorageList)

  for (PRUint32 i = 0; i < NS_STID_ARRAY_UBOUND; ++i) {      
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mInnerWindowHolders[i])
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOpenerScriptPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mListenerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDoc)

  // Traverse any associated preserved wrappers.
  {
    nsISupports *preservedWrapper = nsnull;
    if (tmp->mDoc) {
      preservedWrapper = tmp->mDoc->GetReference(tmp);
      if (preservedWrapper)
        cb.NoteXPCOMChild(preservedWrapper);
    }
  }

  // Traverse our possibly-inner window.
  if (tmp->IsOuterWindow()
      && tmp->GetCurrentInnerWindowInternal()) {    
    cb.NoteXPCOMChild(NS_STATIC_CAST(nsIScriptGlobalObject*, 
                                     tmp->GetCurrentInnerWindowInternal()));
  }

  // FIXME: somewhere in these commented lines lies a bug that causes
  // a segfault. So we have disabled them, even though it seems wrong
  // to do so. Other matters are more pressing at the moment.

  // Traverse stuff from nsPIDOMWindow
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChromeEventHandler)
  // NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGlobalWindow, nsIScriptGlobalObject)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContext)

  // See comment about traversing mOpener above.
  // NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOpener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mControllers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mArguments)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mArgumentsLast)

  for (PRUint32 i = 0; i < NS_STID_ARRAY_UBOUND; ++i) {      
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mScriptContexts[i])
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(gGlobalStorageList)

  for (PRUint32 i = 0; i < NS_STID_ARRAY_UBOUND; ++i) {      
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mInnerWindowHolders[i])
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOpenerScriptPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mListenerManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSessionStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocumentPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDoc)

  // Unlink any associated preserved wrapper.
  if (tmp->mDoc)
    tmp->mDoc->RemoveReference(tmp->mDoc.get());

  // Unlink stuff from nsPIDOMWindow
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChromeEventHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


//*****************************************************************************
// nsGlobalWindow::nsIScriptGlobalObject
//*****************************************************************************

nsresult
nsGlobalWindow::SetScriptContext(PRUint32 lang_id, nsIScriptContext *aScriptContext)
{
  nsresult rv;
  
  PRBool ok = NS_STID_VALID(lang_id);
  NS_ASSERTION(ok, "Invalid programming language ID requested");
  NS_ENSURE_TRUE(ok, NS_ERROR_INVALID_ARG);
  PRUint32 lang_ndx = NS_STID_INDEX(lang_id);

  NS_ASSERTION(IsOuterWindow(), "Uh, SetScriptContext() called on inner window!");

  if (!aScriptContext)
    NS_WARNING("Possibly early removal of script object, see bug #41608");
  else {
    // should probably assert the context is clean???
    aScriptContext->WillInitializeContext();
    // Bind the script context and the global object
    rv = aScriptContext->InitContext(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsIScriptContext *existing;
  existing = mScriptContexts[lang_ndx];
  NS_ASSERTION(!aScriptContext || !existing, "Bad call to SetContext()!");

  if (existing)
    existing->SetOwner(nsnull);
  void *script_glob = nsnull;

  if (aScriptContext) {
    if (IsFrame()) {
      // This window is a [i]frame, don't bother GC'ing when the
      // frame's context is destroyed since a GC will happen when the
      // frameset or host document is destroyed anyway.

      aScriptContext->SetGCOnDestruction(PR_FALSE);
    }

    aScriptContext->DidInitializeContext();
    script_glob = aScriptContext->GetNativeGlobal();
    NS_ASSERTION(script_glob, "GetNativeGlobal returned NULL!");
  }
  // for now, keep mContext real.
  if (lang_id==nsIProgrammingLanguage::JAVASCRIPT) {
    mContext = aScriptContext;
    mJSObject = (JSObject *)script_glob;
  }
  mScriptContexts[lang_ndx] = aScriptContext;
  mScriptGlobals[lang_ndx] = script_glob;
  return NS_OK;
}

nsresult
nsGlobalWindow::EnsureScriptEnvironment(PRUint32 aLangID)
{
  FORWARD_TO_OUTER(EnsureScriptEnvironment, (aLangID), NS_ERROR_NOT_INITIALIZED);
  nsresult rv;
 
  PRBool ok = NS_STID_VALID(aLangID);
  NS_ASSERTION(ok, "Invalid programming language ID requested");
  NS_ENSURE_TRUE(ok, NS_ERROR_INVALID_ARG);
  PRUint32 lang_ndx = NS_STID_INDEX(aLangID);

  if (mScriptGlobals[lang_ndx])
      return NS_OK; // already initialized for this language.
  nsCOMPtr<nsIScriptRuntime> scriptRuntime;
  rv = NS_GetScriptRuntimeByID(aLangID, getter_AddRefs(scriptRuntime));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptContext> context;
  rv = scriptRuntime->CreateContext(getter_AddRefs(context));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetScriptContext(aLangID, context);
  NS_ENSURE_SUCCESS(rv, rv);
  // Note that SetScriptContext has taken a reference to the context.
  // If we have arguments, tell the new language about it.
  nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();
  if (currentInner) {
    // We are being initialized after the document has been setup.
    // Do what would have been done in SetNewDocument had we been around then.
    NS_ASSERTION(!mInnerWindowHolders[lang_ndx], "already have a holder?");
    nsCOMPtr<nsISupports> &holder = mInnerWindowHolders[lang_ndx];
    PRBool isChrome = PR_FALSE; // xxxmarkh - what about this??
    void *&innerGlob = currentInner->mScriptGlobals[lang_ndx];
    rv = context->CreateNativeGlobalForInner(this, isChrome,
                                             &innerGlob,
                                             getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(innerGlob && holder, "Failed to get global and holder");
    rv = context->ConnectToInner(currentInner, mScriptGlobals[lang_ndx]);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(mDocument));
    if (doc)
      context->DidSetDocument(doc, innerGlob);

    if (mArgumentsLast != nsnull) {
      context->SetProperty(innerGlob, "arguments", mArgumentsLast);
    }
  }
  return NS_OK;
}

nsIScriptContext *
nsGlobalWindow::GetScriptContext(PRUint32 lang)
{
  FORWARD_TO_OUTER(GetScriptContext, (lang), nsnull);

  PRBool ok = NS_STID_VALID(lang);
  NS_ASSERTION(ok, "Invalid programming language ID requested");
  NS_ENSURE_TRUE(ok, nsnull);

  PRUint32 lang_ndx = NS_STID_INDEX(lang);

  // for now we are still storing the JS versions in members.  Check the
  // JS elements of our array are still in sync.  Do this each time we are
  // called, as much JS specific code still goes via GetContext
  NS_ASSERTION(mScriptContexts[NS_STID_INDEX(nsIProgrammingLanguage::JAVASCRIPT)] == mContext &&
               mScriptGlobals[NS_STID_INDEX(nsIProgrammingLanguage::JAVASCRIPT)] == mJSObject,
               "JS language contexts are confused");
  return mScriptContexts[lang_ndx];
}

void *
nsGlobalWindow::GetScriptGlobal(PRUint32 lang)
{
  PRBool ok = NS_STID_VALID(lang);
  NS_ASSERTION(ok, "Invalid programming language ID requested");
  NS_ENSURE_TRUE(ok, nsnull);

  PRUint32 lang_ndx = NS_STID_INDEX(lang);

  // for now we are still storing the JS versions in members.  Check the
  // JS elements of our array are still in sync.  Do this each time we are
  // called, as much JS specific code still goes via GetGlobalJSObject
  NS_ASSERTION(mScriptContexts[NS_STID_INDEX(nsIProgrammingLanguage::JAVASCRIPT)] == mContext &&
               mScriptGlobals[NS_STID_INDEX(nsIProgrammingLanguage::JAVASCRIPT)] == mJSObject,
               "JS language contexts are confused");
  return mScriptGlobals[lang_ndx];
}

nsIScriptContext *
nsGlobalWindow::GetContext()
{
  FORWARD_TO_OUTER(GetContext, (), nsnull);

  // check GetContext is indeed identical to GetScriptContext()
  NS_ASSERTION(mContext == GetScriptContext(nsIProgrammingLanguage::JAVASCRIPT),
               "GetContext confused?");
  return mContext;
}

JSObject *
nsGlobalWindow::GetGlobalJSObject()
{
  NS_ASSERTION(mJSObject == GetScriptGlobal(nsIProgrammingLanguage::JAVASCRIPT),
               "GetGlobalJSObject confused?");
  return mJSObject;
}


PRBool
nsGlobalWindow::WouldReuseInnerWindow(nsIDocument *aNewDocument)
{
  // We reuse the inner window when:
  // a. We are currently at our original document.
  // b. At least one of the following conditions are true:
  // -- We are not currently a content window (i.e., we're currently a chrome
  //    window).
  // -- The new document is the same as the old document. This means that we're
  //    getting called from document.open().
  // -- The new document has the same origin as what we have loaded right now.

  if (!mDoc || !aNewDocument) {
    return PR_FALSE;
  }

  if (!mDoc->IsInitialDocument()) {
    return PR_FALSE;
  }
  
  NS_ASSERTION(IsAboutBlank(mDoc->GetDocumentURI()),
               "How'd this happen?");
  
  // Great, we're the original document, check for one of the other
  // conditions.
  if (mDoc == aNewDocument) {
    // aClearScopeHint is false.
    return PR_TRUE;
  }

  if (nsContentUtils::GetSecurityManager() &&
      NS_SUCCEEDED(nsContentUtils::GetSecurityManager()->
        CheckSameOriginPrincipal(mDoc->NodePrincipal(),
                                 aNewDocument->NodePrincipal()))) {
    // The origin is the same.
    return PR_TRUE;
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));

  if (treeItem) {
    PRInt32 itemType = nsIDocShellTreeItem::typeContent;
    treeItem->GetItemType(&itemType);

    // If we're a chrome window, then we want to reuse the inner window.
    return itemType == nsIDocShellTreeItem::typeChrome;
  }

  // No treeItem: don't reuse the current inner window.
  return PR_FALSE;
}

void
nsGlobalWindow::SetOpenerScriptPrincipal(nsIPrincipal* aPrincipal)
{
  FORWARD_TO_OUTER_VOID(SetOpenerScriptPrincipal, (aPrincipal));

  if (mDoc) {
    if (!mDoc->IsInitialDocument()) {
      // We have a document already, and it's not the original one.  Bail out.
      // Do NOT set mOpenerScriptPrincipal in this case, just to be safe.
      return;
    }
    
#ifdef DEBUG
    // We better have an about:blank document loaded at this point.  Otherwise,
    // something is really weird.
    nsCOMPtr<nsIURI> uri;
    mDoc->NodePrincipal()->GetURI(getter_AddRefs(uri));
    NS_ASSERTION(uri && IsAboutBlank(uri) &&
                 IsAboutBlank(mDoc->GetDocumentURI()),
                 "Unexpected original document");
#endif
    
    // Set the opener principal on our document; given the above check, this
    // is safe.
    mDoc->SetPrincipal(aPrincipal);
  }
    
  mOpenerScriptPrincipal = aPrincipal;
}

nsIPrincipal*
nsGlobalWindow::GetOpenerScriptPrincipal()
{
  FORWARD_TO_OUTER(GetOpenerScriptPrincipal, (), nsnull);

  return mOpenerScriptPrincipal;
}

PopupControlState
PushPopupControlState(PopupControlState aState, PRBool aForce)
{
  PopupControlState oldState = gPopupControlState;

  if (aState < gPopupControlState || aForce) {
    gPopupControlState = aState;
  }

  return oldState;
}

void
PopPopupControlState(PopupControlState aState)
{
  gPopupControlState = aState;
}

PopupControlState
nsGlobalWindow::PushPopupControlState(PopupControlState aState,
                                      PRBool aForce) const
{
  return ::PushPopupControlState(aState, aForce);
}

void
nsGlobalWindow::PopPopupControlState(PopupControlState aState) const
{
  ::PopPopupControlState(aState);
}

PopupControlState
nsGlobalWindow::GetPopupControlState() const
{
  return gPopupControlState;
}

#define WINDOWSTATEHOLDER_IID \
{0x0b917c3e, 0xbd50, 0x4683, {0xaf, 0xc9, 0xc7, 0x81, 0x07, 0xae, 0x33, 0x26}}

class WindowStateHolder : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(WINDOWSTATEHOLDER_IID)
  NS_DECL_ISUPPORTS

  WindowStateHolder(nsGlobalWindow *aWindow,
                    nsCOMPtr<nsISupports> aHolders[],
                    nsNavigator *aNavigator,
                    nsLocation *aLocation,
                    nsIXPConnectJSObjectHolder *aOuterProto);

  // Get the contents of focus memory when the state was saved
  // (if the focus was inside of this window).
  nsIDOMElement* GetFocusedElement() { return mFocusedElement; }
  nsIDOMWindowInternal* GetFocusedWindow() { return mFocusedWindow; }

  nsGlobalWindow* GetInnerWindow() { return mInnerWindow; }
  nsISupports* GetInnerWindowHolder(PRUint32 aScriptTypeID)
  { return mInnerWindowHolders[NS_STID_INDEX(aScriptTypeID)]; }

  nsNavigator* GetNavigator() { return mNavigator; }
  nsLocation* GetLocation() { return mLocation; }
  nsIXPConnectJSObjectHolder* GetOuterProto() { return mOuterProto; }

  void DidRestoreWindow()
  {
    PRUint32 lang_ndx;
    mInnerWindow = nsnull;

    NS_STID_FOR_INDEX(lang_ndx) {
        mInnerWindowHolders[lang_ndx] = nsnull;
    }
    mNavigator = nsnull;
    mLocation = nsnull;
    mOuterProto = nsnull;
  }

protected:
  ~WindowStateHolder();

  nsGlobalWindow *mInnerWindow;
  // We hold onto this to make sure the inner window doesn't go away. The outer
  // window ends up recalculating it anyway.
  nsCOMPtr<nsISupports> mInnerWindowHolders[NS_STID_ARRAY_UBOUND];
  nsRefPtr<nsNavigator> mNavigator;
  nsRefPtr<nsLocation> mLocation;
  nsCOMPtr<nsIDOMElement> mFocusedElement;
  nsCOMPtr<nsIDOMWindowInternal> mFocusedWindow;
  nsCOMPtr<nsIXPConnectJSObjectHolder> mOuterProto;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WindowStateHolder, WINDOWSTATEHOLDER_IID)

WindowStateHolder::WindowStateHolder(nsGlobalWindow *aWindow,
                                     nsCOMPtr<nsISupports> aHolders[],
                                     nsNavigator *aNavigator,
                                     nsLocation *aLocation,
                                     nsIXPConnectJSObjectHolder *aOuterProto)
  : mInnerWindow(aWindow),
    mNavigator(aNavigator),
    mLocation(aLocation),
    mOuterProto(aOuterProto)
{
  NS_PRECONDITION(aWindow, "null window");
  NS_PRECONDITION(aWindow->IsInnerWindow(), "Saving an outer window");

  PRUint32 lang_ndx;
  NS_STID_FOR_INDEX(lang_ndx) {
    mInnerWindowHolders[lang_ndx] = aHolders[lang_ndx];
  }
  nsIFocusController *fc = aWindow->GetRootFocusController();
  NS_ASSERTION(fc, "null focus controller");

  // We want to save the focused element/window only if they are inside of
  // this window.

  nsCOMPtr<nsIDOMWindowInternal> focusWinInternal;
  fc->GetFocusedWindow(getter_AddRefs(focusWinInternal));

  nsCOMPtr<nsPIDOMWindow> focusedWindow = do_QueryInterface(focusWinInternal);

  // The outer window is used for focus purposes, so make sure that's what
  // we're looking for.
  nsPIDOMWindow *targetWindow = aWindow->GetOuterWindow();

  while (focusedWindow) {
    if (focusedWindow == targetWindow) {
      fc->GetFocusedWindow(getter_AddRefs(mFocusedWindow));
      fc->GetFocusedElement(getter_AddRefs(mFocusedElement));
      break;
    }

    focusedWindow =
      NS_STATIC_CAST(nsGlobalWindow*,
                     NS_STATIC_CAST(nsPIDOMWindow*,
                                    focusedWindow))->GetPrivateParent();
  }

  aWindow->SuspendTimeouts();
}

WindowStateHolder::~WindowStateHolder()
{
  if (mInnerWindow) {
    // This window was left in the bfcache and is now going away. We need to
    // free it up.
    // Note that FreeInnerObjects may already have been called on the
    // inner window if its outer has already had SetDocShell(null)
    // called.  In this case the contexts will all be null and the
    // PR_TRUE for aClearScope won't do anything; this is OK since
    // SetDocShell(null) already did it.
    mInnerWindow->FreeInnerObjects(PR_TRUE);
  }
}

NS_IMPL_ISUPPORTS1(WindowStateHolder, WindowStateHolder)

nsresult
nsGlobalWindow::SetNewDocument(nsIDocument* aDocument,
                               nsISupports* aState,
                               PRBool aClearScopeHint)
{
  return SetNewDocument(aDocument, aState, aClearScopeHint, PR_FALSE);
}

nsresult
nsGlobalWindow::SetNewDocument(nsIDocument* aDocument,
                               nsISupports* aState,
                               PRBool aClearScopeHint,
                               PRBool aIsInternalCall)
{
  NS_ASSERTION(mDocumentPrincipal == nsnull,
               "mDocumentPrincipal prematurely set!");
#ifdef PR_LOGGING
  if (IsInnerWindow() && aDocument && gDOMLeakPRLog &&
      PR_LOG_TEST(gDOMLeakPRLog, PR_LOG_DEBUG)) {
    nsIURI *uri = aDocument->GetDocumentURI();
    nsCAutoString spec;
    if (uri)
      uri->GetSpec(spec);
    PR_LogPrint("DOMWINDOW %p SetNewDocument %s", this, spec.get());
  }
#endif

  if (IsOuterWindow() && IsFrozen()) {
    // This outer is now getting its first inner, thaw the outer now
    // that it's ready and is getting an inner window.

    Thaw();
  }

  if (!aIsInternalCall && IsInnerWindow()) {
    if (!mOuterWindow) {
      return NS_ERROR_NOT_INITIALIZED;
    }

    // Refuse to set a new document if the call came from an inner
    // window that's not the current inner window.
    if (mOuterWindow->GetCurrentInnerWindow() != this) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    return GetOuterWindowInternal()->SetNewDocument(aDocument,
                                                    aState,
                                                    aClearScopeHint, PR_TRUE);
  }

  if (!aDocument) {
    NS_ERROR("SetNewDocument(null) called!");

    return NS_ERROR_INVALID_ARG;
  }

  NS_ASSERTION(!GetCurrentInnerWindow() ||
               GetCurrentInnerWindow()->GetExtantDocument() == mDocument,
               "Uh, mDocument doesn't match the current inner window "
               "document!");

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocument> oldDoc(do_QueryInterface(mDocument));

  nsIScriptContext *scx = GetContextInternal();
  NS_ENSURE_TRUE(scx, NS_ERROR_NOT_INITIALIZED);

  JSContext *cx = (JSContext *)scx->GetNativeContext();

  // clear smartcard events, our document has gone away.
  if (mCrypto) {
    mCrypto->SetEnableSmartCardEvents(PR_FALSE);
  }

  if (!mDocument) {
    // First document load.

    // Get our private root. If it is equal to us, then we need to
    // attach our global key bindings that handles browser scrolling
    // and other browser commands.
    nsIDOMWindowInternal *internal = nsGlobalWindow::GetPrivateRoot();

    if (internal == NS_STATIC_CAST(nsIDOMWindowInternal *, this)) {
      nsCOMPtr<nsIXBLService> xblService = do_GetService("@mozilla.org/xbl;1");
      if (xblService) {
        nsCOMPtr<nsIDOMEventReceiver> rec =
          do_QueryInterface(mChromeEventHandler);
        xblService->AttachGlobalKeyHandler(rec);
      }
    }
  }

  /* No mDocShell means we're either an inner window or we're already
     been partially closed down.  When that happens, setting status
     isn't a big requirement, so don't. (Doesn't happen under normal
     circumstances, but bug 49615 describes a case.) */

  SetStatus(EmptyString());
  SetDefaultStatus(EmptyString());

  // This code should not be called during shutdown any more (now that
  // we don't ever call SetNewDocument(nsnull), so no need to null
  // check xpc here.
  nsIXPConnect *xpc = nsContentUtils::XPConnect();

  PRBool reUseInnerWindow = WouldReuseInnerWindow(aDocument);

  // Remember the old document's principal.
  nsIPrincipal *oldPrincipal = nsnull;

  if (oldDoc) {
    oldPrincipal = oldDoc->NodePrincipal();
  }

  // Drop our reference to the navigator object unless we're reusing
  // the existing inner window or the new document is from the same
  // origin as the old document.
  if (!reUseInnerWindow && mNavigator && oldPrincipal) {
    rv = nsContentUtils::GetSecurityManager()->
      CheckSameOriginPrincipal(oldPrincipal, aDocument->NodePrincipal());

    if (NS_FAILED(rv)) {
      // Different origins.  Release the navigator object so it gets
      // recreated for the new document.  The plugins or mime types
      // arrays may have changed. See bug 150087.
      mNavigator->SetDocShell(nsnull);

      mNavigator = nsnull;
    }
  }

  if (mNavigator && aDocument != oldDoc) {
    // We didn't drop our reference to our old navigator object and
    // we're loading a new document. Notify the navigator object about
    // the new document load so that it can make sure it is ready for
    // the new document.

    mNavigator->LoadingNewDocument();
  }

  PRUint32 st_id, st_ndx; // we loop over all our context/globs lots!

  // Set mDocument even if this is an outer window to avoid
  // having to *always* reach into the inner window to find the
  // document.

  mDocument = do_QueryInterface(aDocument);
  mDoc = aDocument;

  if (IsOuterWindow()) {
    NS_STID_FOR_ID(st_id) {
      nsIScriptContext *langContext = GetScriptContextInternal(st_id);
      if (langContext)
          langContext->WillInitializeContext();
    }

    nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();

    nsRefPtr<nsGlobalWindow> newInnerWindow;

    nsCOMPtr<nsIDOMChromeWindow> thisChrome =
      do_QueryInterface(NS_STATIC_CAST(nsIDOMWindow *, this));
    nsCOMPtr<nsIXPConnectJSObjectHolder> navigatorHolder;

    PRBool isChrome = PR_FALSE;

    JSAutoRequest ar(cx);

    // Make sure to clear scope on the outer window *before* we
    // initialize the new inner window. If we don't, things
    // (Object.prototype etc) could leak from the old outer to the new
    // inner scope.
    NS_STID_FOR_ID(st_id) {
      nsIScriptContext *langContext = GetScriptContextInternal(st_id);
      if (langContext)
        langContext->ClearScope(mScriptGlobals[NS_STID_INDEX(st_id)],
                                PR_FALSE);
    }

    if (reUseInnerWindow) {
      // We're reusing the current inner window.
      NS_ASSERTION(!currentInner->IsFrozen(),
                   "We should never be reusing a shared inner window");
      newInnerWindow = currentInner;

      if (aDocument != oldDoc) {
        nsWindowSH::InvalidateGlobalScopePolluter(cx, currentInner->mJSObject);
      }
    } else {
      if (aState) {
        nsCOMPtr<WindowStateHolder> wsh = do_QueryInterface(aState);
        NS_ASSERTION(wsh, "What kind of weird state are you giving me here?");

        newInnerWindow = wsh->GetInnerWindow();
        NS_STID_FOR_ID(st_id) {
            mInnerWindowHolders[NS_STID_INDEX(st_id)] = wsh->GetInnerWindowHolder(st_id);
        }

        // These assignments addref.
        mNavigator = wsh->GetNavigator();
        mLocation = wsh->GetLocation();

        if (mNavigator) {
          // Update mNavigator's docshell pointer now.
          mNavigator->SetDocShell(mDocShell);
          mNavigator->LoadingNewDocument();
        }
      } else {
        if (thisChrome) {
          newInnerWindow = new nsGlobalChromeWindow(this);

          isChrome = PR_TRUE;
        } else {
          newInnerWindow = new nsGlobalWindow(this);
        }

        mLocation = nsnull;
      }

      if (!newInnerWindow) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (!aState) {
        // This is redundant if we're restoring from a previous inner window.
        nsIScriptGlobalObject *sgo =
          (nsIScriptGlobalObject *)newInnerWindow.get();

        // Freeze the outer window and null out the inner window so
        // that initializing classes on the new inner doesn't end up
        // reaching into the old inner window for classes etc.
        //
        // [This happens with Object.prototype when XPConnect creates
        // a temporary global while initializing classes; the reason
        // being that xpconnect creates the temp global w/o a parent
        // and proto, which makes the JS engine look up classes in
        // cx->globalObject, i.e. this outer window].

        mInnerWindow = nsnull;

        Freeze();
        mCreatingInnerWindow = PR_TRUE;
        // Every script context we are initialized with must create a
        // new global.
        rv = NS_OK;
        NS_STID_FOR_ID(st_id) {
          st_ndx = NS_STID_INDEX(st_id);
          nsIScriptContext *this_ctx = GetScriptContextInternal(st_id);
          if (this_ctx) {
            void *&newGlobal = newInnerWindow->mScriptGlobals[st_ndx];
            nsCOMPtr<nsISupports> &holder = mInnerWindowHolders[st_ndx];
            rv |= this_ctx->CreateNativeGlobalForInner(sgo, isChrome,
                                                       &newGlobal,
                                                       getter_AddRefs(holder));
            NS_ASSERTION(NS_SUCCEEDED(rv) && newGlobal && holder, 
                        "Failed to get script global and holder");
            if (st_id == nsIProgrammingLanguage::JAVASCRIPT) {
                newInnerWindow->mJSObject = (JSObject *)newGlobal;
            }
          }
        }
        mCreatingInnerWindow = PR_FALSE;
        Thaw();

        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentInner && currentInner->mJSObject) {
        if (mNavigator && !aState) {
          // Hold on to the navigator wrapper so that we can set
          // window.navigator in the new window to point to the same
          // object (assuming we didn't change origins etc). See bug
          // 163645 for more on why we need this.

          nsIDOMNavigator* navigator =
            NS_STATIC_CAST(nsIDOMNavigator*, mNavigator.get());
          xpc->WrapNative(cx, currentInner->mJSObject, navigator,
                          NS_GET_IID(nsIDOMNavigator),
                          getter_AddRefs(navigatorHolder));
        }

        PRBool termFuncSet = PR_FALSE;

        if (oldDoc == aDocument) {
          nsCOMPtr<nsIJSContextStack> stack =
            do_GetService(sJSStackContractID);

          JSContext *cx = nsnull;

          if (stack) {
            stack->Peek(&cx);
          }

          nsIScriptContext *callerScx;
          if (cx && (callerScx = GetScriptContextFromJSContext(cx))) {
            // We're called from document.open() (and document.open() is
            // called from JS), clear the scope etc in a termination
            // function on the calling context to prevent clearing the
            // calling scope.
            NS_ASSERTION(!currentInner->IsFrozen(),
                "How does this opened window get into session history");

            JSAutoRequest ar(cx);

            callerScx->SetTerminationFunction(ClearWindowScope,
                                              NS_STATIC_CAST(nsIDOMWindow *,
                                                             currentInner));

            termFuncSet = PR_TRUE;
          }
        }

        // Don't clear scope on our current inner window if it's going to be
        // held in the bfcache.
        if (!currentInner->IsFrozen()) {
          // Skip the ClearScope if we set a termination function to do
          // it ourselves, later.
          currentInner->FreeInnerObjects(!termFuncSet);
        }
      }

      mInnerWindow = newInnerWindow;
    }

    if (!aState && !reUseInnerWindow) {
      // Loading a new page and creating a new inner window, *not*
      // restoring from session history.

      // InitClassesWithNewWrappedGlobal() (via CreateNativeGlobalForInner)
      // for the new inner window
      // sets the global object in cx to be the new wrapped global. We
      // don't want that, but re-initializing the outer window will
      // fix that for us. And perhaps more importantly, this will
      // ensure that the outer window gets a new prototype so we don't
      // leak prototype properties from the old inner window to the
      // new one.
      NS_STID_FOR_ID(st_id) {
        nsIScriptContext *this_ctx = GetScriptContextInternal(st_id);
        if (st_id == nsIProgrammingLanguage::JAVASCRIPT)
            JS_BeginRequest((JSContext *)this_ctx->GetNativeContext());
        if (this_ctx) {
          this_ctx->InitContext(this);
          // Now that both the the inner and outer windows are initialized
          // let each script context do its magic to hook them together.
          void *glob = mScriptGlobals[NS_STID_INDEX(st_id)];
          this_ctx->ConnectToInner(newInnerWindow, glob);
        }
        if (st_id == nsIProgrammingLanguage::JAVASCRIPT)
            JS_EndRequest((JSContext *)this_ctx->GetNativeContext());
      }
    }
    // Tell the contexts we have completed setting up the doc.
    NS_STID_FOR_ID(st_id) {
      // Add an extra ref in case we release mContext during GC.
      nsCOMPtr<nsIScriptContext> this_ctx =
                                    GetScriptContextInternal(st_id);
      if (this_ctx) {
        nsCOMPtr<nsIDOMDocument> dd(do_QueryInterface(aDocument));
        this_ctx->DidSetDocument(dd, newInnerWindow->GetScriptGlobal(st_id));
      }
    }

    // Now that the prototype is all set up, install the global scope
    // polluter. This must happen after the above prototype fixup. If
    // the GSP was to be installed on the inner window's real
    // prototype (as it would be if this was done before the prototype
    // fixup above) we would end up holding the GSP alive (through
    // XPConnect's internal marking of wrapper prototypes) as long as
    // the inner window was around, and if the GSP had properties on
    // it that held an element alive we'd hold the document alive,
    // which could hold event handlers alive, which hold the context
    // alive etc.

    if ((!reUseInnerWindow || aDocument != oldDoc) && !aState) {
      nsCOMPtr<nsIHTMLDocument> html_doc(do_QueryInterface(mDocument));
      nsWindowSH::InstallGlobalScopePolluter(cx, newInnerWindow->mJSObject,
                                             html_doc);
    }

    if (aState) {
      // Restoring from session history.

      nsCOMPtr<WindowStateHolder> wsh = do_QueryInterface(aState);
      NS_ASSERTION(wsh, "What kind of weird state are you giving me here?");

      // Restore the prototype for the Window/ChromeWindow class in
      // the outer window scope.
      nsCOMPtr<nsIClassInfo> ci =
        do_QueryInterface((nsIScriptGlobalObject *)this);

      rv = xpc->RestoreWrappedNativePrototype(cx, mJSObject, ci,
                                              wsh->GetOuterProto());
      NS_ENSURE_SUCCESS(rv, rv);

      // Refresh the outer window's prototype to what it was when the
      // window state was saved. This will make the outer window
      // object (and wrapper) pick up the prototype it had when the
      // window state was saved. This means Object.prototype etc from
      // the old inner will again be on the outer window's prototype
      // chain.

      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      rv = xpc->GetWrappedNativeOfJSObject(cx, mJSObject,
                                           getter_AddRefs(wrapper));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = wrapper->RefreshPrototype();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (aDocument) {
      aDocument->SetScriptGlobalObject(newInnerWindow);
    }

    if (!aState) {
      if (reUseInnerWindow) {
        if (newInnerWindow->mDoc != aDocument) {
          newInnerWindow->mDocument = do_QueryInterface(aDocument);
          newInnerWindow->mDoc = aDocument;

          // We're reusing the inner window for a new document. In this
          // case we don't clear the inner window's scope, but we must
          // make sure the cached document property gets updated.

          // XXXmarkh - tell other languages about this?
          JSAutoRequest ar(cx);
          ::JS_DeleteProperty(cx, currentInner->mJSObject, "document");
        }
      } else {
        rv = newInnerWindow->SetNewDocument(aDocument, nsnull,
                                            aClearScopeHint, PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);

        NS_STID_FOR_ID(st_id) {
          nsIScriptContext *this_ctx = GetScriptContextInternal(st_id);
          if (this_ctx) {
            // Initialize DOM classes etc on the inner window.
            void *glob = newInnerWindow->mScriptGlobals[NS_STID_INDEX(st_id)];
            rv = this_ctx->InitClasses(glob);
            NS_ENSURE_SUCCESS(rv, rv);

            if (navigatorHolder &&
                st_id == nsIProgrammingLanguage::JAVASCRIPT) {
              // Restore window.navigator onto the new inner window.
              JSObject *nav;
              JSAutoRequest ar(cx);
              navigatorHolder->GetJSObject(&nav);

              ::JS_DefineProperty(cx, newInnerWindow->mJSObject, "navigator",
                                  OBJECT_TO_JSVAL(nav), nsnull, nsnull,
                                  JSPROP_ENUMERATE | JSPROP_PERMANENT |
                                  JSPROP_READONLY);
            }
          }
        }
      }

      if (mArguments) {
        newInnerWindow->SetNewArguments(mArguments);
        mArguments = nsnull;
      }

      // Give the new inner window our chrome event handler (since it
      // doesn't have one).
      newInnerWindow->mChromeEventHandler = mChromeEventHandler;
    }

    NS_STID_FOR_ID(st_id) {
      // Add an extra ref in case we release mContext during GC.
      nsCOMPtr<nsIScriptContext> this_ctx =
                                    GetScriptContextInternal(st_id);
      if (this_ctx) {
        this_ctx->GC();
        this_ctx->DidInitializeContext();
      }
    }
  }

  // Clear our mutation bitfield.
  mMutationBits = 0;

  return NS_OK;
}

void
nsGlobalWindow::SetDocShell(nsIDocShell* aDocShell)
{
  NS_ASSERTION(IsOuterWindow(), "Uh, SetDocShell() called on inner window!");

  if (aDocShell == mDocShell)
    return;

  PRUint32 lang_id;
  nsIScriptContext *langCtx;
  // SetDocShell(nsnull) means the window is being torn down. Drop our
  // reference to the script context, allowing it to be deleted
  // later. Meanwhile, keep our weak reference to the script object
  // (mJSObject) so that it can be retrieved later (until it is
  // finalized by the JS GC).

  if (!aDocShell) {
    NS_ASSERTION(PR_CLIST_IS_EMPTY(&mTimeouts),
                 "Uh, outer window holds timeouts!");

    // Call FreeInnerObjects on all inner windows, not just the current
    // one, since some could be held by WindowStateHolder objects that
    // are GC-owned.
    for (nsGlobalWindow *inner = (nsGlobalWindow *)PR_LIST_HEAD(this);
         inner != this;
         inner = (nsGlobalWindow*)PR_NEXT_LINK(inner)) {
      NS_ASSERTION(inner->mOuterWindow == this, "bad outer window pointer");
      inner->FreeInnerObjects(PR_TRUE);
    }

    nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();

    if (currentInner) {
      NS_ASSERTION(mDoc, "Must have doc!");
      
      // Remember the document's principal.
      mDocumentPrincipal = mDoc->NodePrincipal();

      // Release our document reference
      mDocument = nsnull;
      mDoc = nsnull;
    }

    // clear all scopes
    NS_STID_FOR_ID(lang_id) {
      langCtx = mScriptContexts[NS_STID_INDEX(lang_id)];
      if (langCtx)
        langCtx->ClearScope(mScriptGlobals[NS_STID_INDEX(lang_id)], PR_TRUE);
    }

    // if we are closing the window while in full screen mode, be sure
    // to restore os chrome
    if (mFullScreen) {
      nsIFocusController *focusController =
        nsGlobalWindow::GetRootFocusController();
      PRBool isActive = PR_FALSE;
      if (focusController)
        focusController->GetActive(&isActive);
      // only restore OS chrome if the closing window was active

      if (isActive) {
        nsCOMPtr<nsIFullScreen> fullScreen =
          do_GetService("@mozilla.org/browser/fullscreen;1");

        if (fullScreen)
          fullScreen->ShowAllOSChrome();
      }
    }

    ClearControllers();

    mChromeEventHandler = nsnull; // force release now

    if (mArguments) { 
      // We got no new document after someone called
      // SetNewArguments(), drop our reference to the arguments.
      mArguments = nsnull;
      // xxxmarkh - should we also drop mArgumentsLast?
    }

    PRUint32 st_ndx;

    // Drop holders and tell each context to cleanup and release them now.
    NS_ASSERTION(mContext == mScriptContexts[NS_STID_INDEX(nsIProgrammingLanguage::JAVASCRIPT)],
                 "Contexts confused");
    NS_STID_FOR_INDEX(st_ndx) {
      mInnerWindowHolders[st_ndx] = nsnull;
      langCtx = mScriptContexts[st_ndx];
      if (langCtx) {
        langCtx->GC();
        langCtx->SetOwner(nsnull);
        langCtx->FinalizeContext();
        mScriptContexts[st_ndx] = nsnull;
      }
    }
    mContext = nsnull; // we nuked it above also
  }

  mDocShell = aDocShell;        // Weak Reference

  if (mNavigator)
    mNavigator->SetDocShell(aDocShell);
  if (mLocation)
    mLocation->SetDocShell(aDocShell);
  if (mHistory)
    mHistory->SetDocShell(aDocShell);
  if (mFrames)
    mFrames->SetDocShell(aDocShell);
  if (mScreen)
    mScreen->SetDocShell(aDocShell);

  if (mDocShell) {
    // tell our member elements about the new browserwindow
    if (mMenubar) {
      nsCOMPtr<nsIWebBrowserChrome> browserChrome;
      GetWebBrowserChrome(getter_AddRefs(browserChrome));
      mMenubar->SetWebBrowserChrome(browserChrome);
    }

    // Get our enclosing chrome shell and retrieve its global window impl, so
    // that we can do some forwarding to the chrome document.
    mDocShell->GetChromeEventHandler(getter_AddRefs(mChromeEventHandler));
    if (!mChromeEventHandler) {
      // We have no chrome event handler. If we have a parent,
      // get our chrome event handler from the parent. If
      // we don't have a parent, then we need to make a new
      // window root object that will function as a chrome event
      // handler and receive all events that occur anywhere inside
      // our window.
      nsCOMPtr<nsIDOMWindow> parentWindow;
      GetParent(getter_AddRefs(parentWindow));
      if (parentWindow.get() != NS_STATIC_CAST(nsIDOMWindow*,this)) {
        nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(parentWindow));
        mChromeEventHandler = piWindow->GetChromeEventHandler();
      }
      else NS_NewWindowRoot(this, getter_AddRefs(mChromeEventHandler));
    }
  }
}

void
nsGlobalWindow::SetOpenerWindow(nsIDOMWindowInternal* aOpener,
                                PRBool aOriginalOpener)
{
  FORWARD_TO_OUTER_VOID(SetOpenerWindow, (aOpener, aOriginalOpener));

  NS_ASSERTION(!aOriginalOpener || !mSetOpenerWindowCalled,
               "aOriginalOpener is true, but not first call to "
               "SetOpenerWindow!");
  NS_ASSERTION(aOpener || !aOriginalOpener,
               "Shouldn't set mHadOriginalOpener if aOpener is null");

  mOpener = aOpener;
  if (aOriginalOpener) {
    mHadOriginalOpener = PR_TRUE;
  }

#ifdef DEBUG
  mSetOpenerWindowCalled = PR_TRUE;
#endif
}

void
nsGlobalWindow::SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner)
{
  FORWARD_TO_OUTER_VOID(SetGlobalObjectOwner, (aOwner));

  mGlobalObjectOwner = aOwner;  // Note this is supposed to be a weak ref.
}

nsIScriptGlobalObjectOwner *
nsGlobalWindow::GetGlobalObjectOwner()
{
  FORWARD_TO_OUTER(GetGlobalObjectOwner, (), nsnull);

  return mGlobalObjectOwner;
}

nsresult
nsGlobalWindow::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  FORWARD_TO_INNER(PreHandleEvent, (aVisitor), NS_OK);
  static PRUint32 count = 0;
  PRUint32 msg = aVisitor.mEvent->message;

  aVisitor.mCanHandle = PR_TRUE;
  aVisitor.mForceContentDispatch = PR_TRUE; //FIXME! Bug 329119
  if ((msg == NS_MOUSE_MOVE) && gEntropyCollector) {
    //Chances are this counter will overflow during the life of the
    //process, but that's OK for our case. Means we get a little
    //more entropy.
    if (count++ % 100 == 0) {
      //Since the high bits seem to be zero's most of the time,
      //let's only take the lowest half of the point structure.
      PRInt16 myCoord[2];

      myCoord[0] = aVisitor.mEvent->refPoint.x;
      myCoord[1] = aVisitor.mEvent->refPoint.y;
      gEntropyCollector->RandomUpdate((void*)myCoord, sizeof(myCoord));
      gEntropyCollector->RandomUpdate((void*)&(aVisitor.mEvent->time),
                                      sizeof(PRUint32));
    }
  } else if (msg == NS_RESIZE_EVENT) {
    mIsHandlingResizeEvent = PR_TRUE;
  }

  // Check chrome document capture here.
  if (mChromeEventHandler) {
    aVisitor.mParentTarget = mChromeEventHandler;
    aVisitor.mParentIsChromeHandler = PR_TRUE;
  }
  return NS_OK;
}

nsresult
nsGlobalWindow::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  FORWARD_TO_INNER(PostHandleEvent, (aVisitor), NS_OK);
  /* mChromeEventHandler and mContext go dangling in the middle of this
   function under some circumstances (events that destroy the window)
   without this addref. */
  nsCOMPtr<nsIChromeEventHandler> kungFuDeathGrip1(mChromeEventHandler);
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip2(GetContextInternal());
  nsGlobalWindow* outer = GetOuterWindowInternal();

  // if the window is deactivated while in full screen mode,
  // restore OS chrome, and hide it again upon re-activation
  if (outer && outer->mFullScreen) {
    if (aVisitor.mEvent->message == NS_DEACTIVATE ||
        aVisitor.mEvent->message == NS_ACTIVATE) {
      nsCOMPtr<nsIFullScreen> fullScreen =
        do_GetService("@mozilla.org/browser/fullscreen;1");
      if (fullScreen) {
        if (aVisitor.mEvent->message == NS_DEACTIVATE)
          fullScreen->ShowAllOSChrome();
        else
          fullScreen->HideAllOSChrome();
      }
    }
  }

  if (aVisitor.mEvent->message == NS_RESIZE_EVENT) {
    mIsHandlingResizeEvent = PR_FALSE;
  } else if (aVisitor.mEvent->message == NS_PAGE_UNLOAD &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    // Execute bindingdetached handlers before we tear ourselves
    // down.
    if (mDocument) {
      NS_ASSERTION(mDoc, "Must have doc");
      mDoc->BindingManager()->ExecuteDetachedHandlers();
    }
    mIsDocumentLoaded = PR_FALSE;
  } else if (aVisitor.mEvent->message == NS_LOAD &&
             NS_IS_TRUSTED_EVENT(aVisitor.mEvent)) {
    // This is page load event since load events don't propagate to |window|.
    // @see nsDocument::PreHandleEvent.
    mIsDocumentLoaded = PR_TRUE;

    nsCOMPtr<nsIContent> content(do_QueryInterface(GetFrameElementInternal()));
    nsCOMPtr<nsIDocShellTreeItem> treeItem =
      do_QueryInterface(GetDocShell());

    PRInt32 itemType = nsIDocShellTreeItem::typeChrome;

    if (treeItem) {
      treeItem->GetItemType(&itemType);
    }

    if (content && GetParentInternal() &&
        itemType != nsIDocShellTreeItem::typeChrome) {
      // If we're not in chrome, or at a chrome boundary, fire the
      // onload event for the frame element.

      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event(NS_IS_TRUSTED_EVENT(aVisitor.mEvent), NS_LOAD);
      event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;

      // Most of the time we could get a pres context to pass in here,
      // but not always (i.e. if this window is not shown there won't
      // be a pres context available). Since we're not firing a GUI
      // event we don't need a pres context anyway so we just pass
      // null as the pres context all the time here.

      nsEventDispatcher::Dispatch(content, nsnull, &event, nsnull, &status);
    }
  }

  return NS_OK;
}

nsresult
nsGlobalWindow::DispatchDOMEvent(nsEvent* aEvent,
                                 nsIDOMEvent* aDOMEvent,
                                 nsPresContext* aPresContext,
                                 nsEventStatus* aEventStatus)
{
  return
    nsEventDispatcher::DispatchDOMEvent(NS_STATIC_CAST(nsPIDOMWindow*, this),
                                       aEvent, aDOMEvent, aPresContext,
                                       aEventStatus);
}

void
nsGlobalWindow::OnFinalize(PRUint32 aLangID, void *aObject)
{
  if (!NS_STID_VALID(aLangID)) {
    NS_ERROR("Invalid language ID");
    return;
  }
  PRUint32 lang_ndx = NS_STID_INDEX(aLangID);
  if (aObject == mScriptGlobals[lang_ndx]) {
    mScriptGlobals[lang_ndx] = nsnull;
  } else if (mScriptGlobals[lang_ndx]) {
    NS_ERROR("Huh? Script language created more than one wrapper for this global!");
  } else {
    NS_WARNING("Weird, we're finalized with a null language global?");
  }
  if (aLangID==nsIProgrammingLanguage::JAVASCRIPT)
    mJSObject = nsnull; // all relevant assertions and nulling done above.
}

void
nsGlobalWindow::SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts)
{
  FORWARD_TO_INNER_VOID(SetScriptsEnabled, (aEnabled, aFireTimeouts));

  if (aEnabled && aFireTimeouts) {
    // Scripts are enabled (again?) on this context, run timeouts that
    // fired on this context while scripts were disabled.

    RunTimeout(nsnull);
  }
}

nsresult
nsGlobalWindow::SetNewArguments(nsIArray *aArguments)
{
  FORWARD_TO_OUTER(SetNewArguments, (aArguments), NS_ERROR_NOT_INITIALIZED);

  JSContext *cx;
  NS_ENSURE_TRUE(aArguments && mContext &&
                 (cx = (JSContext *)mContext->GetNativeContext()),
                 NS_ERROR_NOT_INITIALIZED);

  // Note that currentInner may be non-null if someone's doing a
  // window.open with an existing window name.
  nsGlobalWindow *currentInner = GetCurrentInnerWindowInternal();
  
  nsresult rv;

  if (currentInner) {
    PRUint32 langID;
    NS_STID_FOR_ID(langID) {
      void *glob = currentInner->GetScriptGlobal(langID);
      nsIScriptContext *ctx = GetScriptContext(langID);
      if (glob && ctx) {
        rv = ctx->SetProperty(glob, "arguments", aArguments);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  // Hold on to the arguments so that we can re-set them once the next
  // document is loaded.
  mArguments = aArguments;
  mArgumentsLast = aArguments;

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIScriptObjectPrincipal
//*****************************************************************************

nsIPrincipal*
nsGlobalWindow::GetPrincipal()
{
  if (mDoc) {
    // If we have a document, get the principal from the document
    return mDoc->NodePrincipal();
  }

  if (mDocumentPrincipal) {
    return mDocumentPrincipal;
  }

  // If we don't have a principal and we don't have a document we
  // ask the parent window for the principal. This can happen when
  // loading a frameset that has a <frame src="javascript:xxx">, in
  // that case the global window is used in JS before we've loaded
  // a document into the window.

  nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
    do_QueryInterface(GetParentInternal());

  if (objPrincipal) {
    return objPrincipal->GetPrincipal();
  }

  return nsnull;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMWindow
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetDocument(nsIDOMDocument** aDocument)
{
  // This method *should* forward calls to the outer window, but since
  // there's nothing here that *depends* on anything in the outer
  // (GetDocShell() eliminates that dependency), we won't do that to
  // avoid the extra virtual function call.

  // lazily instantiate an about:blank document if necessary, and if
  // we have what it takes to do so. Note that domdoc here is the same
  // thing as our mDocument, but we don't have to explicitly set the
  // member variable because the docshell has already called
  // SetNewDocument().
  nsIDocShell *docShell;
  if (!mDocument && (docShell = GetDocShell()))
    nsCOMPtr<nsIDOMDocument> domdoc(do_GetInterface(docShell));

  NS_IF_ADDREF(*aDocument = mDocument);

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMWindowInternal
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetWindow(nsIDOMWindowInternal** aWindow)
{
  FORWARD_TO_OUTER(GetWindow, (aWindow), NS_ERROR_NOT_INITIALIZED);

  *aWindow = NS_STATIC_CAST(nsIDOMWindowInternal *, this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetSelf(nsIDOMWindowInternal** aWindow)
{
  FORWARD_TO_OUTER(GetSelf, (aWindow), NS_ERROR_NOT_INITIALIZED);

  *aWindow = NS_STATIC_CAST(nsIDOMWindowInternal *, this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetNavigator(nsIDOMNavigator** aNavigator)
{
  FORWARD_TO_OUTER(GetNavigator, (aNavigator), NS_ERROR_NOT_INITIALIZED);

  *aNavigator = nsnull;

  if (!mNavigator) {
    mNavigator = new nsNavigator(mDocShell);
    if (!mNavigator) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aNavigator = mNavigator);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScreen(nsIDOMScreen** aScreen)
{
  FORWARD_TO_OUTER(GetScreen, (aScreen), NS_ERROR_NOT_INITIALIZED);

  *aScreen = nsnull;

  if (!mScreen && mDocShell) {
    mScreen = new nsScreen(mDocShell);
    if (!mScreen) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aScreen = mScreen);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetHistory(nsIDOMHistory** aHistory)
{
  FORWARD_TO_OUTER(GetHistory, (aHistory), NS_ERROR_NOT_INITIALIZED);

  *aHistory = nsnull;

  if (!mHistory && mDocShell) {
    mHistory = new nsHistory(mDocShell);
    if (!mHistory) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aHistory = mHistory);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetParent(nsIDOMWindow** aParent)
{
  FORWARD_TO_OUTER(GetParent, (aParent), NS_ERROR_NOT_INITIALIZED);

  *aParent = nsnull;
  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> parent;
  docShellAsItem->GetSameTypeParent(getter_AddRefs(parent));

  if (parent) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(parent));
    NS_ENSURE_SUCCESS(CallQueryInterface(globalObject.get(), aParent),
                      NS_ERROR_FAILURE);
  }
  else {
    *aParent = NS_STATIC_CAST(nsIDOMWindowInternal *, this);
    NS_ADDREF(*aParent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetTop(nsIDOMWindow** aTop)
{
  FORWARD_TO_OUTER(GetTop, (aTop), NS_ERROR_NOT_INITIALIZED);

  nsresult ret = NS_OK;

  *aTop = nsnull;
  if (mDocShell) {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
    nsCOMPtr<nsIDocShellTreeItem> root;
    docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(root));

    if (root) {
      nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(root));
      CallQueryInterface(globalObject.get(), aTop);
    }
  }

  return ret;
}

NS_IMETHODIMP
nsGlobalWindow::GetContent(nsIDOMWindow** aContent)
{
  FORWARD_TO_OUTER(GetContent, (aContent), NS_ERROR_NOT_INITIALIZED);

  *aContent = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> primaryContent;

  if (!nsContentUtils::IsCallerChrome()) {
    // If we're called by non-chrome code, make sure we don't return
    // the primary content window if the calling tab is hidden. In
    // such a case we return the same-type root in the hidden tab,
    // which is "good enough", for now.
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(mDocShell));

    if (baseWin) {
      PRBool visible = PR_FALSE;
      baseWin->GetVisibility(&visible);

      if (!visible) {
        nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));

        treeItem->GetSameTypeRootTreeItem(getter_AddRefs(primaryContent));
      }
    }
  }

  if (!primaryContent) {
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    GetTreeOwner(getter_AddRefs(treeOwner));
    NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

    treeOwner->GetPrimaryContentShell(getter_AddRefs(primaryContent));
  }

  nsCOMPtr<nsIDOMWindowInternal> domWindow(do_GetInterface(primaryContent));
  NS_IF_ADDREF(*aContent = domWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPrompter(nsIPrompt** aPrompt)
{
  FORWARD_TO_OUTER(GetPrompter, (aPrompt), NS_ERROR_NOT_INITIALIZED);

  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_NO_INTERFACE);

  NS_ADDREF(*aPrompt = prompter);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetMenubar(nsIDOMBarProp** aMenubar)
{
  FORWARD_TO_OUTER(GetMenubar, (aMenubar), NS_ERROR_NOT_INITIALIZED);

  *aMenubar = nsnull;

  if (!mMenubar) {
    mMenubar = new nsMenubarProp();
    if (!mMenubar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mMenubar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aMenubar = mMenubar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetToolbar(nsIDOMBarProp** aToolbar)
{
  FORWARD_TO_OUTER(GetToolbar, (aToolbar), NS_ERROR_NOT_INITIALIZED);

  *aToolbar = nsnull;

  if (!mToolbar) {
    mToolbar = new nsToolbarProp();
    if (!mToolbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mToolbar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aToolbar = mToolbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetLocationbar(nsIDOMBarProp** aLocationbar)
{
  FORWARD_TO_OUTER(GetLocationbar, (aLocationbar), NS_ERROR_NOT_INITIALIZED);

  *aLocationbar = nsnull;

  if (!mLocationbar) {
    mLocationbar = new nsLocationbarProp();
    if (!mLocationbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mLocationbar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aLocationbar = mLocationbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPersonalbar(nsIDOMBarProp** aPersonalbar)
{
  FORWARD_TO_OUTER(GetPersonalbar, (aPersonalbar), NS_ERROR_NOT_INITIALIZED);

  *aPersonalbar = nsnull;

  if (!mPersonalbar) {
    mPersonalbar = new nsPersonalbarProp();
    if (!mPersonalbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mPersonalbar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aPersonalbar = mPersonalbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetStatusbar(nsIDOMBarProp** aStatusbar)
{
  FORWARD_TO_OUTER(GetStatusbar, (aStatusbar), NS_ERROR_NOT_INITIALIZED);

  *aStatusbar = nsnull;

  if (!mStatusbar) {
    mStatusbar = new nsStatusbarProp();
    if (!mStatusbar) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mStatusbar->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aStatusbar = mStatusbar);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollbars(nsIDOMBarProp** aScrollbars)
{
  FORWARD_TO_OUTER(GetScrollbars, (aScrollbars), NS_ERROR_NOT_INITIALIZED);

  *aScrollbars = nsnull;

  if (!mScrollbars) {
    mScrollbars = new nsScrollbarsProp(this);
    if (!mScrollbars) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIWebBrowserChrome> browserChrome;
    GetWebBrowserChrome(getter_AddRefs(browserChrome));

    mScrollbars->SetWebBrowserChrome(browserChrome);
  }

  NS_ADDREF(*aScrollbars = mScrollbars);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetDirectories(nsIDOMBarProp** aDirectories)
{
  return GetPersonalbar(aDirectories);
}

NS_IMETHODIMP
nsGlobalWindow::GetClosed(PRBool* aClosed)
{
  FORWARD_TO_OUTER(GetClosed, (aClosed), NS_ERROR_NOT_INITIALIZED);

  // If someone called close(), or if we don't have a docshell, we're
  // closed.
  *aClosed = mIsClosed || !mDocShell;

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetFrames(nsIDOMWindowCollection** aFrames)
{
  FORWARD_TO_OUTER(GetFrames, (aFrames), NS_ERROR_NOT_INITIALIZED);

  *aFrames = nsnull;

  if (!mFrames && mDocShell) {
    mFrames = new nsDOMWindowList(mDocShell);
    if (!mFrames) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  *aFrames = NS_STATIC_CAST(nsIDOMWindowCollection *, mFrames);
  NS_IF_ADDREF(*aFrames);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetCrypto(nsIDOMCrypto** aCrypto)
{
  FORWARD_TO_OUTER(GetCrypto, (aCrypto), NS_ERROR_NOT_INITIALIZED);

  if (!mCrypto) {
    mCrypto = do_CreateInstance(kCryptoContractID);
  }

  NS_IF_ADDREF(*aCrypto = mCrypto);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPkcs11(nsIDOMPkcs11** aPkcs11)
{
  FORWARD_TO_OUTER(GetPkcs11, (aPkcs11), NS_ERROR_NOT_INITIALIZED);

  if (!mPkcs11) {
    mPkcs11 = do_CreateInstance(kPkcs11ContractID);
  }

  NS_IF_ADDREF(*aPkcs11 = mPkcs11);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetControllers(nsIControllers** aResult)
{
  FORWARD_TO_OUTER(GetControllers, (aResult), NS_ERROR_NOT_INITIALIZED);

  if (!mControllers) {
    nsresult rv;
    mControllers = do_CreateInstance(kXULControllersCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Add in the default controller
    nsCOMPtr<nsIController> controller = do_CreateInstance(
                               NS_WINDOWCONTROLLER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    mControllers->InsertControllerAt(0, controller);
    nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller);
    if (!controllerContext) return NS_ERROR_FAILURE;

    controllerContext->SetCommandContext(NS_STATIC_CAST(nsIDOMWindow*, this));
  }

  *aResult = mControllers;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOpener(nsIDOMWindowInternal** aOpener)
{
  FORWARD_TO_OUTER(GetOpener, (aOpener), NS_ERROR_NOT_INITIALIZED);

  *aOpener = nsnull;
  // First, check if we were called from a privileged chrome script

  if (nsContentUtils::IsCallerTrustedForRead()) {
    *aOpener = mOpener;
    NS_IF_ADDREF(*aOpener);
    return NS_OK;
  }

  // We don't want to reveal the opener if the opener is a mail window,
  // because opener can be used to spoof the contents of a message (bug 105050).
  // So, we look in the opener's root docshell to see if it's a mail window.
  nsCOMPtr<nsPIDOMWindow> openerPwin(do_QueryInterface(mOpener));
  if (openerPwin) {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
      do_QueryInterface(openerPwin->GetDocShell());

    if (docShellAsItem) {
      nsCOMPtr<nsIDocShellTreeItem> openerRootItem;
      docShellAsItem->GetRootTreeItem(getter_AddRefs(openerRootItem));
      nsCOMPtr<nsIDocShell> openerRootDocShell(do_QueryInterface(openerRootItem));
      if (openerRootDocShell) {
        PRUint32 appType;
        nsresult rv = openerRootDocShell->GetAppType(&appType);
        if (NS_SUCCEEDED(rv) && appType != nsIDocShell::APP_TYPE_MAIL) {
          *aOpener = mOpener;
        }
      }
    }
  }
  NS_IF_ADDREF(*aOpener);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetOpener(nsIDOMWindowInternal* aOpener)
{
  // check if we were called from a privileged chrome script.
  // If not, opener is settable only to null.
  if (aOpener && !nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_OK;
  }

  SetOpenerWindow(aOpener, PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetStatus(nsAString& aStatus)
{
  FORWARD_TO_OUTER(GetStatus, (aStatus), NS_ERROR_NOT_INITIALIZED);

  aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetStatus(const nsAString& aStatus)
{
  FORWARD_TO_OUTER(SetStatus, (aStatus), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_status_change is true,
   * prevent setting window.status by exiting early
   */

  if (!CanSetProperty("dom.disable_window_status_change")) {
    return NS_OK;
  }

  mStatus = aStatus;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  GetWebBrowserChrome(getter_AddRefs(browserChrome));
  if(browserChrome) {
    browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT,
                             PromiseFlatString(aStatus).get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetDefaultStatus(nsAString& aDefaultStatus)
{
  FORWARD_TO_OUTER(GetDefaultStatus, (aDefaultStatus),
                   NS_ERROR_NOT_INITIALIZED);

  aDefaultStatus = mDefaultStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetDefaultStatus(const nsAString& aDefaultStatus)
{
  FORWARD_TO_OUTER(SetDefaultStatus, (aDefaultStatus),
                   NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_status_change is true,
   * prevent setting window.defaultStatus by exiting early
   */

  if (!CanSetProperty("dom.disable_window_status_change")) {
    return NS_OK;
  }

  mDefaultStatus = aDefaultStatus;

  nsCOMPtr<nsIWebBrowserChrome> browserChrome;
  GetWebBrowserChrome(getter_AddRefs(browserChrome));
  if (browserChrome) {
    browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT_DEFAULT,
                             PromiseFlatString(aDefaultStatus).get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetName(nsAString& aName)
{
  FORWARD_TO_OUTER(GetName, (aName), NS_ERROR_NOT_INITIALIZED);

  nsXPIDLString name;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    docShellAsItem->GetName(getter_Copies(name));

  aName.Assign(name);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetName(const nsAString& aName)
{
  FORWARD_TO_OUTER(SetName, (aName), NS_ERROR_NOT_INITIALIZED);

  nsresult result = NS_OK;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    result = docShellAsItem->SetName(PromiseFlatString(aName).get());
  return result;
}

NS_IMETHODIMP
nsGlobalWindow::GetInnerWidth(PRInt32* aInnerWidth)
{
  FORWARD_TO_OUTER(GetInnerWidth, (aInnerWidth), NS_ERROR_NOT_INITIALIZED);

  EnsureSizeUpToDate();

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  *aInnerWidth = 0;
  PRInt32 notused;
  if (docShellWin)
    docShellWin->GetSize(aInnerWidth, &notused);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetInnerWidth(PRInt32 aInnerWidth)
{
  FORWARD_TO_OUTER(SetInnerWidth, (aInnerWidth), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent setting window.innerWidth by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize") || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aInnerWidth, nsnull),
                    NS_ERROR_FAILURE);

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  PRInt32 notused, height = 0;
  docShellAsWin->GetSize(&notused, &height);
  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(docShellAsItem, aInnerWidth, height),
                    NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetInnerHeight(PRInt32* aInnerHeight)
{
  FORWARD_TO_OUTER(GetInnerHeight, (aInnerHeight), NS_ERROR_NOT_INITIALIZED);

  EnsureSizeUpToDate();

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  *aInnerHeight = 0;
  PRInt32 notused;
  if (docShellWin)
    docShellWin->GetSize(&notused, aInnerHeight);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetInnerHeight(PRInt32 aInnerHeight)
{
  FORWARD_TO_OUTER(SetInnerHeight, (aInnerHeight), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent setting window.innerHeight by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize") || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(nsnull, &aInnerHeight),
                    NS_ERROR_FAILURE);

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  PRInt32 width = 0, notused;
  docShellAsWin->GetSize(&width, &notused);
  NS_ENSURE_SUCCESS(treeOwner->
                    SizeShellTo(docShellAsItem, width, aInnerHeight),
                    NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOuterWidth(PRInt32* aOuterWidth)
{
  FORWARD_TO_OUTER(GetOuterWidth, (aOuterWidth), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  nsGlobalWindow* rootWindow =
    NS_STATIC_CAST(nsGlobalWindow *, GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }
  PRInt32 notused;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(aOuterWidth, &notused),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterWidth(PRInt32 aOuterWidth)
{
  FORWARD_TO_OUTER(SetOuterWidth, (aOuterWidth), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent setting window.outerWidth by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize")) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aOuterWidth, nsnull),
                    NS_ERROR_FAILURE);

  PRInt32 notused, height;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&notused, &height), NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(aOuterWidth, height, PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetOuterHeight(PRInt32* aOuterHeight)
{
  FORWARD_TO_OUTER(GetOuterHeight, (aOuterHeight), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  nsGlobalWindow* rootWindow =
    NS_STATIC_CAST(nsGlobalWindow *, GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }

  PRInt32 notused;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&notused, aOuterHeight),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetOuterHeight(PRInt32 aOuterHeight)
{
  FORWARD_TO_OUTER(SetOuterHeight, (aOuterHeight), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent setting window.outerHeight by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize")) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(nsnull, &aOuterHeight),
                    NS_ERROR_FAILURE);

  PRInt32 width, notused;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&width, &notused), NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(width, aOuterHeight, PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenX(PRInt32* aScreenX)
{
  FORWARD_TO_OUTER(GetScreenX, (aScreenX), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 y;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(aScreenX, &y),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetScreenX(PRInt32 aScreenX)
{
  FORWARD_TO_OUTER(SetScreenX, (aScreenX), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent setting window.screenX by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize")) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&aScreenX, nsnull),
                    NS_ERROR_FAILURE);

  PRInt32 x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(aScreenX, y),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScreenY(PRInt32* aScreenY)
{
  FORWARD_TO_OUTER(GetScreenY, (aScreenY), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 x;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, aScreenY),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SetScreenY(PRInt32 aScreenY)
{
  FORWARD_TO_OUTER(SetScreenY, (aScreenY), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent setting window.screenY by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize")) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(nsnull, &aScreenY),
                    NS_ERROR_FAILURE);

  PRInt32 x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y),
                    NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(x, aScreenY),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
nsGlobalWindow::CheckSecurityWidthAndHeight(PRInt32* aWidth, PRInt32* aHeight)
{
  // This one is easy. Just ensure the variable is greater than 100;
  if ((aWidth && *aWidth < 100) || (aHeight && *aHeight < 100)) {
    // Check security state for use in determing window dimensions

    if (!nsContentUtils::IsCallerTrustedForWrite()) {
      //sec check failed
      if (aWidth && *aWidth < 100) {
        *aWidth = 100;
      }
      if (aHeight && *aHeight < 100) {
        *aHeight = 100;
      }
    }
  }

  return NS_OK;
}

nsresult
nsGlobalWindow::CheckSecurityLeftAndTop(PRInt32* aLeft, PRInt32* aTop)
{
  // This one is harder. We have to get the screen size and window dimensions.

  // Check security state for use in determing window dimensions

  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    PRInt32 screenLeft, screenTop, screenWidth, screenHeight;
    PRInt32 winLeft, winTop, winWidth, winHeight;

    nsGlobalWindow* rootWindow =
      NS_STATIC_CAST(nsGlobalWindow*, GetPrivateRoot());
    if (rootWindow) {
      rootWindow->FlushPendingNotifications(Flush_Layout);
    }

    // Get the window size
    nsCOMPtr<nsIBaseWindow> treeOwner;
    GetTreeOwner(getter_AddRefs(treeOwner));
    if (treeOwner)
      treeOwner->GetPositionAndSize(&winLeft, &winTop, &winWidth, &winHeight);

    // Get the screen dimensions
    // XXX This should use nsIScreenManager once it's fully fleshed out.
    nsCOMPtr<nsIDOMScreen> screen;
    GetScreen(getter_AddRefs(screen));
    if (screen) {
      screen->GetAvailLeft(&screenLeft);
      screen->GetAvailWidth(&screenWidth);
      screen->GetAvailHeight(&screenHeight);
#if defined(XP_MAC) || defined(XP_MACOSX)
      /* The mac's coordinate system is different from the assumed Windows'
         system. It offsets by the height of the menubar so that a window
         placed at (0,0) will be entirely visible. Unfortunately that
         correction is made elsewhere (in Widget) and the meaning of
         the Avail... coordinates is overloaded. Here we allow a window
         to be placed at (0,0) because it does make sense to do so.
      */
      screen->GetTop(&screenTop);
#else
      screen->GetAvailTop(&screenTop);
#endif
    }

    if (screen && treeOwner) {
      if (aLeft) {
        if (screenLeft+screenWidth < *aLeft+winWidth)
          *aLeft = screenLeft+screenWidth - winWidth;
        if (screenLeft > *aLeft)
          *aLeft = screenLeft;
      }
      if (aTop) {
        if (screenTop+screenHeight < *aTop+winHeight)
          *aTop = screenTop+screenHeight - winHeight;
        if (screenTop > *aTop)
          *aTop = screenTop;
      }
    } else {
      if (aLeft)
        *aLeft = 0;
      if (aTop)
        *aTop = 0;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetPageXOffset(PRInt32* aPageXOffset)
{
  return GetScrollX(aPageXOffset);
}

NS_IMETHODIMP
nsGlobalWindow::GetPageYOffset(PRInt32* aPageYOffset)
{
  return GetScrollY(aPageYOffset);
}

nsresult
nsGlobalWindow::GetScrollMaxXY(PRInt32* aScrollMaxX, PRInt32* aScrollMaxY)
{
  FORWARD_TO_OUTER(GetScrollMaxXY, (aScrollMaxX, aScrollMaxY),
                   NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  nsIScrollableView *view = nsnull;      // no addref/release for views

  FlushPendingNotifications(Flush_Layout);
  GetScrollInfo(&view);
  if (!view)
    return NS_OK;      // bug 230965 changed from NS_ERROR_FAILURE

  nsSize scrolledSize;
  rv = view->GetContainerSize(&scrolledSize.width, &scrolledSize.height);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRect portRect = view->View()->GetBounds();

  if (aScrollMaxX)
    *aScrollMaxX = PR_MAX(0,
      (PRInt32)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrolledSize.width - portRect.width)));
  if (aScrollMaxY)
    *aScrollMaxY = PR_MAX(0,
      (PRInt32)floor(nsPresContext::AppUnitsToFloatCSSPixels(scrolledSize.height - portRect.height)));

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxX(PRInt32* aScrollMaxX)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxX);
  *aScrollMaxX = 0;
  return GetScrollMaxXY(aScrollMaxX, nsnull);
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollMaxY(PRInt32* aScrollMaxY)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxY);
  *aScrollMaxY = 0;
  return GetScrollMaxXY(nsnull, aScrollMaxY);
}

nsresult
nsGlobalWindow::GetScrollXY(PRInt32* aScrollX, PRInt32* aScrollY,
                            PRBool aDoFlush)
{
  FORWARD_TO_OUTER(GetScrollXY, (aScrollX, aScrollY, aDoFlush),
                   NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  nsIScrollableView *view = nsnull;      // no addref/release for views

  if (aDoFlush) {
    FlushPendingNotifications(Flush_Layout);
  } else {
    EnsureSizeUpToDate();
  }
  
  GetScrollInfo(&view);
  if (!view)
    return NS_OK;      // bug 202206 changed from NS_ERROR_FAILURE

  nscoord xPos, yPos;
  rv = view->GetScrollPosition(xPos, yPos);
  NS_ENSURE_SUCCESS(rv, rv);

  if ((xPos != 0 || yPos != 0) && !aDoFlush) {
    // Oh, well.  This is the expensive case -- the window is scrolled and we
    // didn't actually flush yet.  Repeat, but with a flush, since the content
    // may get shorter and hence our scroll position may decrease.
    return GetScrollXY(aScrollX, aScrollY, PR_TRUE);
  }
  
  if (aScrollX)
    *aScrollX = nsPresContext::AppUnitsToIntCSSPixels(xPos);
  if (aScrollY)
    *aScrollY = nsPresContext::AppUnitsToIntCSSPixels(yPos);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollX(PRInt32* aScrollX)
{
  NS_ENSURE_ARG_POINTER(aScrollX);
  *aScrollX = 0;
  return GetScrollXY(aScrollX, nsnull, PR_FALSE);
}

NS_IMETHODIMP
nsGlobalWindow::GetScrollY(PRInt32* aScrollY)
{
  NS_ENSURE_ARG_POINTER(aScrollY);
  *aScrollY = 0;
  return GetScrollXY(nsnull, aScrollY, PR_FALSE);
}

NS_IMETHODIMP
nsGlobalWindow::GetLength(PRUint32* aLength)
{
  nsCOMPtr<nsIDOMWindowCollection> frames;
  if (NS_SUCCEEDED(GetFrames(getter_AddRefs(frames))) && frames) {
    return frames->GetLength(aLength);
  }
  return NS_ERROR_FAILURE;
}

PRBool
nsGlobalWindow::DispatchCustomEvent(const char *aEventName)
{
  PRBool defaultActionEnabled = PR_TRUE;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  nsContentUtils::DispatchTrustedEvent(doc,
                                       NS_STATIC_CAST(
                                         nsIScriptGlobalObject*, this),
                                       NS_ConvertASCIItoUTF16(aEventName),
                                       PR_TRUE, PR_TRUE, &defaultActionEnabled);

  return defaultActionEnabled;
}

static already_AddRefed<nsIDocShellTreeItem>
GetCallerDocShellTreeItem()
{
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService(sJSStackContractID);

  JSContext *cx = nsnull;

  if (stack) {
    stack->Peek(&cx);
  }

  nsIDocShellTreeItem *callerItem = nsnull;

  if (cx) {
    nsCOMPtr<nsIWebNavigation> callerWebNav =
      do_GetInterface(nsJSUtils::GetDynamicScriptGlobal(cx));

    if (callerWebNav) {
      CallQueryInterface(callerWebNav, &callerItem);
    }
  }

  return callerItem;
}

PRBool
nsGlobalWindow::WindowExists(const nsAString& aName,
                             PRBool aLookForCallerOnJSStack)
{
  NS_PRECONDITION(IsOuterWindow(), "Must be outer window");
  NS_PRECONDITION(mDocShell, "Must have docshell");

  nsCOMPtr<nsIDocShellTreeItem> caller;
  if (aLookForCallerOnJSStack) {
    caller = GetCallerDocShellTreeItem();
  }

  nsCOMPtr<nsIDocShellTreeItem> docShell = do_QueryInterface(mDocShell);
  NS_ASSERTION(docShell,
               "Docshell doesn't implement nsIDocShellTreeItem?");

  if (!caller) {
    caller = docShell;
  }

  nsCOMPtr<nsIDocShellTreeItem> namedItem;
  docShell->FindItemWithName(PromiseFlatString(aName).get(), nsnull, caller,
                             getter_AddRefs(namedItem));
  return namedItem != nsnull;
}

already_AddRefed<nsIWidget>
nsGlobalWindow::GetMainWidget()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  nsIWidget *widget = nsnull;

  if (treeOwnerAsWin) {
    treeOwnerAsWin->GetMainWidget(&widget);
  }

  return widget;
}

NS_IMETHODIMP
nsGlobalWindow::SetFullScreen(PRBool aFullScreen)
{
  FORWARD_TO_OUTER(SetFullScreen, (aFullScreen), NS_ERROR_NOT_INITIALIZED);

  // Only chrome can change our fullScreen mode.
  if (aFullScreen == mFullScreen || 
      !nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_OK;
  }

  // SetFullScreen needs to be called on the root window, so get that
  // via the DocShell tree, and if we are not already the root,
  // call SetFullScreen on that window instead.
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(mDocShell);
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsIDOMWindowInternal> window = do_GetInterface(rootItem);
  if (!window)
    return NS_ERROR_FAILURE;
  if (rootItem != treeItem)
    return window->SetFullScreen(aFullScreen);

  // make sure we don't try to set full screen on a non-chrome window,
  // which might happen in embedding world
  PRInt32 itemType;
  treeItem->GetItemType(&itemType);
  if (itemType != nsIDocShellTreeItem::typeChrome)
    return NS_ERROR_FAILURE;

  // dispatch a "fullscreen" DOM event so that XUL apps can
  // respond visually if we are kicked into full screen mode
  if (!DispatchCustomEvent("fullscreen")) {
    // event handlers can prevent us from going into full-screen mode

    return NS_OK;
  }

  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  if (widget)
    widget->MakeFullScreen(aFullScreen);

  mFullScreen = aFullScreen;

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetFullScreen(PRBool* aFullScreen)
{
  FORWARD_TO_OUTER(GetFullScreen, (aFullScreen), NS_ERROR_NOT_INITIALIZED);

  *aFullScreen = mFullScreen;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::Dump(const nsAString& aStr)
{
#if !(defined(NS_DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
  {
    // In optimized builds we check a pref that controls if we should
    // enable output from dump() or not, in debug builds it's always
    // enabled.

    // if pref doesn't exist, disable dump output.
    PRBool enable_dump =
      nsContentUtils::GetBoolPref("browser.dom.window.dump.enabled");

    if (!enable_dump) {
      return NS_OK;
    }
  }
#endif

  char *cstr = ToNewUTF8String(aStr);

#if defined(XP_MAC) || defined(XP_MACOSX)
  // have to convert \r to \n so that printing to the console works
  char *c = cstr, *cEnd = cstr + aStr.Length();
  while (c < cEnd) {
    if (*c == '\r')
      *c = '\n';
    c++;
  }
#endif

  if (cstr) {
    printf("%s", cstr);
    nsMemory::Free(cstr);
  }

  return NS_OK;
}

void
nsGlobalWindow::EnsureReflowFlushAndPaint()
{
  NS_ASSERTION(mDocShell, "EnsureReflowFlushAndPaint() called with no "
               "docshell!");

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell)
    return;

  // Flush pending reflows.
  if (mDoc) {
    mDoc->FlushPendingNotifications(Flush_Layout);
  }

  // Unsuppress painting.
  presShell->UnsuppressPainting();
}

NS_IMETHODIMP
nsGlobalWindow::GetTextZoom(float *aZoom)
{
  FORWARD_TO_OUTER(GetTextZoom, (aZoom), NS_ERROR_NOT_INITIALIZED);

  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(contentViewer));

    if (markupViewer) {
      return markupViewer->GetTextZoom(aZoom);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::SetTextZoom(float aZoom)
{
  FORWARD_TO_OUTER(SetTextZoom, (aZoom), NS_ERROR_NOT_INITIALIZED);

  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(contentViewer));

    if (markupViewer)
      return markupViewer->SetTextZoom(aZoom);
  }
  return NS_ERROR_FAILURE;
}

// static
void
nsGlobalWindow::MakeScriptDialogTitle(nsAString &aOutTitle)
{
  aOutTitle.Truncate();

  // Try to get a host from the running principal -- this will do the
  // right thing for javascript: and data: documents.

  nsresult rv = NS_OK;
  NS_ASSERTION(nsContentUtils::GetSecurityManager(),
    "Global Window has no security manager!");
  if (nsContentUtils::GetSecurityManager()) {
    nsCOMPtr<nsIPrincipal> principal;
    rv = nsContentUtils::GetSecurityManager()->
      GetSubjectPrincipal(getter_AddRefs(principal));

    if (NS_SUCCEEDED(rv) && principal) {
      nsCOMPtr<nsIURI> uri;
      rv = principal->GetURI(getter_AddRefs(uri));

      if (NS_SUCCEEDED(rv) && uri) {
        // remove user:pass for privacy and spoof prevention

        nsCOMPtr<nsIURIFixup> fixup(do_GetService(NS_URIFIXUP_CONTRACTID));
        if (fixup) {
          nsCOMPtr<nsIURI> fixedURI;
          rv = fixup->CreateExposableURI(uri, getter_AddRefs(fixedURI));
          if (NS_SUCCEEDED(rv) && fixedURI) {
            nsCAutoString host;
            fixedURI->GetHost(host);

            if (!host.IsEmpty()) {
              // if this URI has a host we'll show it. For other
              // schemes (e.g. file:) we fall back to the localized
              // generic string

              nsCAutoString prepath;
              fixedURI->GetPrePath(prepath);

              NS_ConvertUTF8toUTF16 ucsPrePath(prepath);
              const PRUnichar *formatStrings[] = { ucsPrePath.get() };
              nsXPIDLString tempString;
              nsContentUtils::FormatLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                                    "ScriptDlgHeading",
                                                    formatStrings, NS_ARRAY_LENGTH(formatStrings),
                                                    tempString);
              aOutTitle = tempString;
            }
          }
        }
      }
    }
    else { // failed to get subject principal
      NS_WARNING("No script principal? Who is calling alert/confirm/prompt?!");
    }
  }

  if (aOutTitle.IsEmpty()) {
    // We didn't find a host so use the generic heading
    nsXPIDLString tempString;
    nsContentUtils::GetLocalizedString(nsContentUtils::eCOMMON_DIALOG_PROPERTIES,
                                       "ScriptDlgGenericHeading",
                                       tempString);
    aOutTitle = tempString;
  }

  // Just in case
  if (aOutTitle.IsEmpty()) {
    NS_WARNING("could not get ScriptDlgGenericHeading string from string bundle");
    aOutTitle.AssignLiteral("[Script]");
  }
}

NS_IMETHODIMP
nsGlobalWindow::Alert(const nsAString& aString)
{
  FORWARD_TO_OUTER(Alert, (aString), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

  // Special handling for alert(null) in JS for backwards
  // compatibility.

  NS_NAMED_LITERAL_STRING(null_str, "null");

  const nsAString *str = DOMStringIsNull(aString) ? &null_str : &aString;

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title);

  // Remove non-terminating null characters from the 
  // string. See bug #310037. 
  nsAutoString final;
  StripNullChars(*str, final);

  return prompter->Alert(title.get(), final.get());
}

NS_IMETHODIMP
nsGlobalWindow::Confirm(const nsAString& aString, PRBool* aReturn)
{
  FORWARD_TO_OUTER(Confirm, (aString, aReturn), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

  *aReturn = PR_FALSE;

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title);

  // Remove non-terminating null characters from the 
  // string. See bug #310037. 
  nsAutoString final;
  StripNullChars(aString, final);

  return prompter->Confirm(title.get(), final.get(),
                           aReturn);
}

NS_IMETHODIMP
nsGlobalWindow::Prompt(const nsAString& aMessage, const nsAString& aInitial,
                       const nsAString& aTitle, PRUint32 aSavePassword,
                       nsAString& aReturn)
{
  // We don't use "aTitle" because we ignore the 3rd (title) argument to
  // prompt(). IE and Opera ignore it too. See Mozilla bug 334893.
  SetDOMStringToNull(aReturn);

  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> wwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAuthPrompt> prompter;
  wwatch->GetNewAuthPrompter(this, getter_AddRefs(prompter));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  // Reset popup state while opening a modal dialog, and firing events
  // about the dialog, to prevent the current state from being active
  // the whole time a modal dialog is open.
  nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

  PRBool b;
  nsXPIDLString uniResult;

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  nsAutoString title;
  MakeScriptDialogTitle(title);
  
  // Remove non-terminating null characters from the 
  // string. See bug #310037. 
  nsAutoString fixedMessage, fixedInitial;
  StripNullChars(aMessage, fixedMessage);
  StripNullChars(aInitial, fixedInitial);

  rv = prompter->Prompt(title.get(), fixedMessage.get(), nsnull,
                        aSavePassword, fixedInitial.get(),
                        getter_Copies(uniResult), &b);
  NS_ENSURE_SUCCESS(rv, rv);

  if (uniResult && b) {
    aReturn.Assign(uniResult);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Prompt(nsAString& aReturn)
{
  FORWARD_TO_OUTER(Prompt, (aReturn), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_STATE(mDocShell);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString message, initial, title;

  PRUint32 argc;
  jsval *argv = nsnull;

  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  PRUint32 savePassword = nsIAuthPrompt::SAVE_PASSWORD_NEVER;

  if (argc > 0) {
    JSAutoRequest ar(cx);
    switch (argc) {
      default:
      case 4:
        nsJSUtils::ConvertJSValToUint32(&savePassword, cx, argv[3]);
      case 3:
        nsJSUtils::ConvertJSValToString(title, cx, argv[2]);
      case 2:
        nsJSUtils::ConvertJSValToString(initial, cx, argv[1]);
      case 1:
        nsJSUtils::ConvertJSValToString(message, cx, argv[0]);
        break;
    }
  }

  return Prompt(message, initial, title, savePassword, aReturn);
}

NS_IMETHODIMP
nsGlobalWindow::Focus()
{
  FORWARD_TO_OUTER(Focus, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);

  PRBool isVisible = PR_FALSE;
  if (baseWin) {
    baseWin->GetVisibility(&isVisible);
  }

  if (!isVisible) {
    // A hidden tab is being focused, ignore this call.
    return NS_OK;
  }

  /*
   * If caller is not chrome and dom.disable_window_flip is true,
   * prevent bringing a window to the front if the window is not the
   * currently active window, but do change the currently focused
   * window in the focus controller so that focus is in the right
   * place when the window is activated again.
   */

  PRBool canFocus =
    CanSetProperty("dom.disable_window_flip") ||
    CheckOpenAllow(CheckForAbusePoint()) == allowNoAbuse;

  PRBool isActive = PR_FALSE;
  nsIFocusController *focusController =
    nsGlobalWindow::GetRootFocusController();
  if (focusController) {
    focusController->GetActive(&isActive);
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  if (treeOwnerAsWin && (canFocus || isActive)) {
    PRBool isEnabled = PR_TRUE;
    if (NS_SUCCEEDED(treeOwnerAsWin->GetEnabled(&isEnabled)) && !isEnabled) {
      NS_WARNING( "Should not try to set the focus on a disabled window" );
      return NS_OK;
    }

    treeOwnerAsWin->SetVisibility(PR_TRUE);
    nsCOMPtr<nsIEmbeddingSiteWindow> embeddingWin(do_GetInterface(treeOwnerAsWin));
    if (embeddingWin)
      embeddingWin->SetFocus();
  }

  nsCOMPtr<nsIPresShell> presShell;
  if (mDocShell) {
    // Don't look for a presshell if we're a root chrome window that's got
    // about:blank loaded.  We don't want to focus our widget in that case.
    // XXXbz should we really be checking for IsInitialDocument() instead?
    PRBool lookForPresShell = PR_TRUE;
    PRInt32 itemType = nsIDocShellTreeItem::typeContent;
    nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));
    NS_ASSERTION(treeItem, "What happened?");
    treeItem->GetItemType(&itemType);
    if (itemType == nsIDocShellTreeItem::typeChrome &&
        GetPrivateRoot() == NS_STATIC_CAST(nsIDOMWindowInternal*, this) &&
        mDocument) {
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
      NS_ASSERTION(doc, "Bogus doc?");
      nsIURI* ourURI = doc->GetDocumentURI();
      if (ourURI) {
        lookForPresShell = !IsAboutBlank(ourURI);
      }
    }
      
    if (lookForPresShell) {
      mDocShell->GetEldestPresShell(getter_AddRefs(presShell));
    }
  }

  nsresult result = NS_OK;
  if (presShell && (canFocus || isActive)) {
    nsIViewManager* vm = presShell->GetViewManager();
    if (vm) {
      nsCOMPtr<nsIWidget> widget;
      vm->GetWidget(getter_AddRefs(widget));
      if (widget)
        // raise the window since this was a focus call on the window.
        result = widget->SetFocus(PR_TRUE);
    }
  }
  else {
    if (focusController) {
      focusController->SetFocusedWindow(this);
    }
  }

  return result;
}

NS_IMETHODIMP
nsGlobalWindow::Blur()
{
  FORWARD_TO_OUTER(Blur, (), NS_ERROR_NOT_INITIALIZED);

  // If embedding apps don't implement nsIEmbeddingSiteWindow2, we
  // shouldn't throw exceptions to web content.
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIEmbeddingSiteWindow2> siteWindow(do_GetInterface(treeOwner));
  if (siteWindow) {
    // This method call may cause mDocShell to become nsnull.
    rv = siteWindow->Blur();

    if (NS_SUCCEEDED(rv) && mDocShell)
      mDocShell->SetHasFocus(PR_FALSE);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Back()
{
  FORWARD_TO_OUTER(Back, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoBack();
}

NS_IMETHODIMP
nsGlobalWindow::Forward()
{
  FORWARD_TO_OUTER(Forward, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoForward();
}

NS_IMETHODIMP
nsGlobalWindow::Home()
{
  FORWARD_TO_OUTER(Home, (), NS_ERROR_NOT_INITIALIZED);

  if (!mDocShell)
    return NS_OK;

  nsAdoptingString homeURL =
    nsContentUtils::GetLocalizedStringPref(PREF_BROWSER_STARTUP_HOMEPAGE);

  if (homeURL.IsEmpty()) {
    // if all else fails, use this
#ifdef DEBUG_seth
    printf("all else failed.  using %s as the home page\n", DEFAULT_HOME_PAGE);
#endif
    CopyASCIItoUTF16(DEFAULT_HOME_PAGE, homeURL);
  }

  nsresult rv;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
  rv = webNav->LoadURI(homeURL.get(),
                       nsIWebNavigation::LOAD_FLAGS_NONE,
                       nsnull,
                       nsnull,
                       nsnull);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::Stop()
{
  FORWARD_TO_OUTER(Stop, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  if (!webNav)
    return NS_OK;

  return webNav->Stop(nsIWebNavigation::STOP_ALL);
}

NS_IMETHODIMP
nsGlobalWindow::Print()
{
#ifdef NS_PRINTING
  FORWARD_TO_OUTER(Print, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint;
  if (NS_SUCCEEDED(GetInterface(NS_GET_IID(nsIWebBrowserPrint),
                                getter_AddRefs(webBrowserPrint)))) {

    nsCOMPtr<nsIPrintSettingsService> printSettingsService = 
      do_GetService("@mozilla.org/gfx/printsettings-service;1");

    nsCOMPtr<nsIPrintSettings> printSettings;
    if (printSettingsService) {
      PRBool printSettingsAreGlobal =
        nsContentUtils::GetBoolPref("print.use_global_printsettings", PR_FALSE);

      if (printSettingsAreGlobal) {
        printSettingsService->GetGlobalPrintSettings(getter_AddRefs(printSettings));

        nsXPIDLString printerName;
        printSettingsService->GetDefaultPrinterName(getter_Copies(printerName));
        if (printerName)
          printSettingsService->InitPrintSettingsFromPrinter(printerName, printSettings);
        printSettingsService->InitPrintSettingsFromPrefs(printSettings, 
                                                         PR_TRUE, 
                                                         nsIPrintSettings::kInitSaveAll);
      } else {
        printSettingsService->GetNewPrintSettings(getter_AddRefs(printSettings));
      }

      webBrowserPrint->Print(printSettings, nsnull);

      PRBool savePrintSettings =
        nsContentUtils::GetBoolPref("print.save_print_settings", PR_FALSE);
      if (printSettingsAreGlobal && savePrintSettings) {
        printSettingsService->
          SavePrintSettingsToPrefs(printSettings,
                                   PR_TRUE,
                                   nsIPrintSettings::kInitSaveAll);
        printSettingsService->
          SavePrintSettingsToPrefs(printSettings,
                                   PR_FALSE,
                                   nsIPrintSettings::kInitSavePrinterName);
      }
    } else {
      webBrowserPrint->GetGlobalPrintSettings(getter_AddRefs(printSettings));
      webBrowserPrint->Print(printSettings, nsnull);
    }
  } 

  return NS_OK;
#else
  return NS_ERROR_NOT_AVAILABLE;
#endif
}

NS_IMETHODIMP
nsGlobalWindow::MoveTo(PRInt32 aXPos, PRInt32 aYPos)
{
  FORWARD_TO_OUTER(MoveTo, (aXPos, aYPos), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent window.moveTo() by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize") || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&aXPos, &aYPos),
                    NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(aXPos, aYPos),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::MoveBy(PRInt32 aXDif, PRInt32 aYDif)
{
  FORWARD_TO_OUTER(MoveBy, (aXDif, aYDif), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent window.moveBy() by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize") || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 x, y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, &y), NS_ERROR_FAILURE);

  PRInt32 newX = x + aXDif;
  PRInt32 newY = y + aYDif;
  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&newX, &newY), NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(newX, newY),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ResizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  FORWARD_TO_OUTER(ResizeTo, (aWidth, aHeight), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent window.resizeTo() by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize") || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aWidth, &aHeight),
                    NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(aWidth, aHeight, PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ResizeBy(PRInt32 aWidthDif, PRInt32 aHeightDif)
{
  FORWARD_TO_OUTER(ResizeBy, (aWidthDif, aHeightDif), NS_ERROR_NOT_INITIALIZED);

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent window.resizeBy() by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize") || IsFrame()) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 width, height;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&width, &height), NS_ERROR_FAILURE);

  PRInt32 newWidth = width + aWidthDif;
  PRInt32 newHeight = height + aHeightDif;
  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&newWidth, &newHeight),
                    NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(newWidth, newHeight,
                                            PR_TRUE), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::SizeToContent()
{
  FORWARD_TO_OUTER(SizeToContent, (), NS_ERROR_NOT_INITIALIZED);

  if (!mDocShell) {
    return NS_OK;
  }

  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * block window.SizeToContent() by exiting
   */

  if (!CanSetProperty("dom.disable_window_move_resize") || IsFrame()) {
    return NS_OK;
  }

  // The content viewer does a check to make sure that it's a content
  // viewer for a toplevel docshell.
  
  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(cv));
  NS_ENSURE_TRUE(markupViewer, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(markupViewer->SizeToContent(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetWindowRoot(nsIDOMEventTarget **aWindowRoot)
{
  *aWindowRoot = nsnull;

  nsIDOMWindowInternal *rootWindow = nsGlobalWindow::GetPrivateRoot();
  nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));
  if (!piWin) {
    return NS_OK;
  }

  nsIChromeEventHandler *chromeHandler = piWin->GetChromeEventHandler();
  if (!chromeHandler) {
    return NS_OK;
  }

  return CallQueryInterface(chromeHandler, aWindowRoot);
}

NS_IMETHODIMP
nsGlobalWindow::Scroll(PRInt32 aXScroll, PRInt32 aYScroll)
{
  return ScrollTo(aXScroll, aYScroll);
}

NS_IMETHODIMP
nsGlobalWindow::ScrollTo(PRInt32 aXScroll, PRInt32 aYScroll)
{
  nsresult result;
  nsIScrollableView *view = nsnull;      // no addref/release for views

  FlushPendingNotifications(Flush_Layout);
  result = GetScrollInfo(&view);

  if (view) {
    // Here we calculate what the max pixel value is that we can
    // scroll to, we do this by dividing maxint with the pixel to
    // twips conversion factor, and substracting 4, the 4 comes from
    // experimenting with this value, anything less makes the view
    // code not scroll correctly, I have no idea why. -- jst
    const PRInt32 maxpx = nsPresContext::AppUnitsToIntCSSPixels(0x7fffffff) - 4;

    if (aXScroll > maxpx) {
      aXScroll = maxpx;
    }

    if (aYScroll > maxpx) {
      aYScroll = maxpx;
    }

    result = view->ScrollTo(nsPresContext::CSSPixelsToAppUnits(aXScroll),
                            nsPresContext::CSSPixelsToAppUnits(aYScroll),
                            NS_VMREFRESH_IMMEDIATE);
  }

  return result;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollBy(PRInt32 aXScrollDif, PRInt32 aYScrollDif)
{
  nsresult result;
  nsIScrollableView *view = nsnull;      // no addref/release for views

  FlushPendingNotifications(Flush_Layout);
  result = GetScrollInfo(&view);

  if (view) {
    nscoord xPos, yPos;
    result = view->GetScrollPosition(xPos, yPos);
    if (NS_SUCCEEDED(result)) {
      result = ScrollTo(nsPresContext::AppUnitsToIntCSSPixels(xPos) + aXScrollDif,
                        nsPresContext::AppUnitsToIntCSSPixels(yPos) + aYScrollDif);
    }
  }

  return result;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollByLines(PRInt32 numLines)
{
  nsresult result;
  nsIScrollableView *view = nsnull;   // no addref/release for views

  FlushPendingNotifications(Flush_Layout);
  result = GetScrollInfo(&view);
  if (view) {
    result = view->ScrollByLines(0, numLines);
  }

  return result;
}

NS_IMETHODIMP
nsGlobalWindow::ScrollByPages(PRInt32 numPages)
{
  nsresult result;
  nsIScrollableView *view = nsnull;   // no addref/release for views

  FlushPendingNotifications(Flush_Layout);
  result = GetScrollInfo(&view);
  if (view) {
    result = view->ScrollByPages(0, numPages);
  }

  return result;
}

NS_IMETHODIMP
nsGlobalWindow::ClearTimeout()
{
  return ClearTimeoutOrInterval();
}

NS_IMETHODIMP
nsGlobalWindow::ClearInterval()
{
  return ClearTimeoutOrInterval();
}

NS_IMETHODIMP
nsGlobalWindow::SetTimeout(PRBool *_retval)
{
  return SetTimeoutOrInterval(PR_FALSE, _retval);
}

NS_IMETHODIMP
nsGlobalWindow::SetInterval(PRBool *_retval)
{
  return SetTimeoutOrInterval(PR_TRUE, _retval);
}

NS_IMETHODIMP
nsGlobalWindow::SetResizable(PRBool aResizable)
{
  // nop

  return NS_OK;
}

static void
ReportUseOfDeprecatedMethod(nsGlobalWindow* aWindow, const char* aWarning)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aWindow->GetExtantDocument());
  nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                  aWarning,
                                  nsnull, 0,
                                  doc ? doc->GetDocumentURI() : nsnull,
                                  EmptyString(), 0, 0,
                                  nsIScriptError::warningFlag,
                                  "DOM Events");
}

NS_IMETHODIMP
nsGlobalWindow::CaptureEvents(PRInt32 aEventFlags)
{
  ReportUseOfDeprecatedMethod(this, "UseOfCaptureEventsWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::ReleaseEvents(PRInt32 aEventFlags)
{
  ReportUseOfDeprecatedMethod(this, "UseOfReleaseEventsWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::RouteEvent(nsIDOMEvent* aEvt)
{
  ReportUseOfDeprecatedMethod(this, "UseOfRouteEventWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::EnableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::DisableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

static
PRBool IsPopupBlocked(nsIDOMDocument* aDoc)
{
  nsCOMPtr<nsIPopupWindowManager> pm =
    do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID);

  if (!pm) {
    return PR_FALSE;
  }

  PRBool blocked = PR_TRUE;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));

  if (doc) {
    PRUint32 permission = nsIPopupWindowManager::ALLOW_POPUP;
    pm->TestPermission(doc->GetDocumentURI(), &permission);
    blocked = (permission == nsIPopupWindowManager::DENY_POPUP);
  }
  return blocked;
}

static
void FirePopupBlockedEvent(nsIDOMDocument* aDoc,
                           nsIDOMWindow *aRequestingWindow, nsIURI *aPopupURI,
                           const nsAString &aPopupWindowName,
                           const nsAString &aPopupWindowFeatures)
{
  if (aDoc) {
    // Fire a "DOMPopupBlocked" event so that the UI can hear about
    // blocked popups.
    nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(aDoc));
    nsCOMPtr<nsIDOMEvent> event;
    docEvent->CreateEvent(NS_LITERAL_STRING("PopupBlockedEvents"),
                          getter_AddRefs(event));
    if (event) {
      nsCOMPtr<nsIDOMPopupBlockedEvent> pbev(do_QueryInterface(event));
      pbev->InitPopupBlockedEvent(NS_LITERAL_STRING("DOMPopupBlocked"),
                                  PR_TRUE, PR_TRUE, aRequestingWindow,
                                  aPopupURI, aPopupWindowName,
                                  aPopupWindowFeatures);
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
      privateEvent->SetTrusted(PR_TRUE);

      nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(aDoc));
      PRBool defaultActionEnabled;
      targ->DispatchEvent(event, &defaultActionEnabled);
    }
  }
}

void FirePopupWindowEvent(nsIDOMDocument* aDoc)
{
  // Fire a "PopupWindow" event
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));
  nsContentUtils::DispatchTrustedEvent(doc, aDoc,
                                       NS_LITERAL_STRING("PopupWindow"),
                                       PR_TRUE, PR_TRUE);
}

// static
PRBool
nsGlobalWindow::CanSetProperty(const char *aPrefName)
{
  // Chrome can set any property.
  if (nsContentUtils::IsCallerTrustedForWrite()) {
    return PR_TRUE;
  }

  // If the pref is set to true, we can not set the property
  // and vice versa.
  return !nsContentUtils::GetBoolPref(aPrefName, PR_TRUE);
}


/*
 * Examine the current document state to see if we're in a way that is
 * typically abused by web designers. The window.open code uses this
 * routine to determine whether to allow the new window.
 * Returns a value from the CheckForAbusePoint enum.
 */
PopupControlState
nsGlobalWindow::CheckForAbusePoint()
{
  FORWARD_TO_OUTER(CheckForAbusePoint, (), openAbused);

  NS_ASSERTION(mDocShell, "Must have docshell");
  
  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(mDocShell));

  NS_ASSERTION(item, "Docshell doesn't implenent nsIDocShellTreeItem?");

  PRInt32 type = nsIDocShellTreeItem::typeChrome;
  item->GetItemType(&type);
  if (type != nsIDocShellTreeItem::typeContent)
    return openAllowed;

  // level of abuse we've detected, initialized to the current popup
  // state
  PopupControlState abuse = gPopupControlState;

  // limit the number of simultaneously open popups
  if (abuse == openAbused || abuse == openControlled) {
    PRInt32 popupMax = nsContentUtils::GetIntPref("dom.popup_maximum", -1);
    if (popupMax >= 0 && gOpenPopupSpamCount >= popupMax)
      abuse = openOverridden;
  }

  return abuse;
}

/* Allow or deny a window open based on whether popups are suppressed.
   A popup generally will be allowed if it's from a white-listed domain.
   Returns a value from the CheckOpenAllow enum. */
OpenAllowValue
nsGlobalWindow::CheckOpenAllow(PopupControlState aAbuseLevel)
{
  NS_PRECONDITION(GetDocShell(), "Must have docshell");

  OpenAllowValue allowWindow = allowNoAbuse; // (also used for openControlled)
  
  if (aAbuseLevel >= openAbused) {
    allowWindow = allowNot;

    // However it might still not be blocked. For now we use both our
    // location and the top window's location when determining whether
    // a popup open request is whitelisted or not. This isn't ideal
    // when dealing with iframe/frame documents, but it'll do for
    // now. Getting the iframe/frame case right would require some
    // changes to the frontend's handling of popup events etc.
    if (aAbuseLevel == openAbused) {
      nsCOMPtr<nsIDOMWindow> topWindow;
      GetTop(getter_AddRefs(topWindow));

      nsCOMPtr<nsPIDOMWindow> topPIWin(do_QueryInterface(topWindow));

      if (topPIWin && (!IsPopupBlocked(topPIWin->GetExtantDocument()) ||
                       !IsPopupBlocked(mDocument))) {
        allowWindow = allowWhitelisted;
      }
    }
  }

  return allowWindow;
}

/* If a window open is blocked, fire the appropriate DOM events.
   aBlocked signifies we just blocked a popup.
   aWindow signifies we just opened what is probably a popup.
*/
void
nsGlobalWindow::FireAbuseEvents(PRBool aBlocked, PRBool aWindow,
                                const nsAString &aPopupURL,
                                const nsAString &aPopupWindowName,
                                const nsAString &aPopupWindowFeatures)
{
  // fetch the URI of the window requesting the opened window

  nsCOMPtr<nsIDOMWindow> topWindow;
  GetTop(getter_AddRefs(topWindow));
  if (!topWindow)
    return;

  nsCOMPtr<nsIDOMDocument> topDoc;
  topWindow->GetDocument(getter_AddRefs(topDoc));

  nsCOMPtr<nsIURI> popupURI;

  // build the URI of the would-have-been popup window
  // (see nsWindowWatcher::URIfromURL)

  // first, fetch the opener's base URI

  nsIURI *baseURL = 0;

  nsCOMPtr<nsIJSContextStack> stack = do_GetService(sJSStackContractID);
  nsCOMPtr<nsIDOMWindow> contextWindow;
  if (stack) {
    JSContext *cx = nsnull;
    stack->Peek(&cx);
    if (cx) {
      nsIScriptContext *currentCX = nsJSUtils::GetDynamicScriptContext(cx);
      if (currentCX) {
        contextWindow = do_QueryInterface(currentCX->GetGlobalObject());
      }
    }
  }
  if (!contextWindow)
    contextWindow = NS_STATIC_CAST(nsIDOMWindow*,this);

  nsCOMPtr<nsIDOMDocument> domdoc;
  contextWindow->GetDocument(getter_AddRefs(domdoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domdoc));
  if (doc)
    baseURL = doc->GetBaseURI();

  // use the base URI to build what would have been the popup's URI
  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (ios)
    ios->NewURI(NS_ConvertUTF16toUTF8(aPopupURL), 0, baseURL,
                getter_AddRefs(popupURI));

  // fire an event chock full of informative URIs
  if (aBlocked)
    FirePopupBlockedEvent(topDoc, this, popupURI, aPopupWindowName,
                          aPopupWindowFeatures);
  if (aWindow)
    FirePopupWindowEvent(topDoc);
}

NS_IMETHODIMP
nsGlobalWindow::Open(const nsAString& aUrl, const nsAString& aName,
                     const nsAString& aOptions, nsIDOMWindow **_retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      PR_FALSE,          // aDialog
                      PR_TRUE,           // aCalledNoScript
                      PR_FALSE,          // aDoJSFixups
                      nsnull, nsnull,    // No args
                      GetPrincipal(),    // aCalleePrincipal
                      nsnull,            // aJSCallerContext
                      _retval);
}

NS_IMETHODIMP
nsGlobalWindow::Open(nsIDOMWindow **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  nsresult rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString url, name, options;

  PRUint32 argc;
  jsval *argv = nsnull;

  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  if (argc > 0) {
    JSAutoRequest ar(cx);
    switch (argc) {
      default:
      case 3:
        nsJSUtils::ConvertJSValToString(options, cx, argv[2]);
      case 2:
        nsJSUtils::ConvertJSValToString(name, cx, argv[1]);
      case 1:
        nsJSUtils::ConvertJSValToString(url, cx, argv[0]);
        break;
    }
  }

  return OpenInternal(url, name, options,
                      PR_FALSE,          // aDialog
                      PR_FALSE,          // aCalledNoScript
                      PR_TRUE,           // aDoJSFixups
                      nsnull, nsnull,    // No args
                      GetPrincipal(),    // aCalleePrincipal
                      cx,                // aJSCallerContext
                      _retval);
}

// like Open, but attaches to the new window any extra parameters past
// [features] as a JS property named "arguments"
NS_IMETHODIMP
nsGlobalWindow::OpenDialog(const nsAString& aUrl, const nsAString& aName,
                           const nsAString& aOptions,
                           nsISupports* aExtraArgument, nsIDOMWindow** _retval)
{
  return OpenInternal(aUrl, aName, aOptions,
                      PR_TRUE,                    // aDialog
                      PR_TRUE,                    // aCalledNoScript
                      PR_FALSE,                   // aDoJSFixups
                      nsnull, aExtraArgument,     // Arguments
                      GetPrincipal(),             // aCalleePrincipal
                      nsnull,                     // aJSCallerContext
                      _retval);
}

NS_IMETHODIMP
nsGlobalWindow::OpenDialog(nsIDOMWindow** _retval)
{
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIXPCNativeCallContext> ncc;
  nsresult rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString url, name, options;

  PRUint32 argc;
  jsval *argv = nsnull;

  // XXX - need to get this as nsISupports?
  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  if (argc > 0) {
    JSAutoRequest ar(cx);
    switch (argc) {
      default:
      case 3:
        nsJSUtils::ConvertJSValToString(options, cx, argv[2]);
      case 2:
        nsJSUtils::ConvertJSValToString(name, cx, argv[1]);
      case 1:
        nsJSUtils::ConvertJSValToString(url, cx, argv[0]);
        break;
    }
  }

  // Strip the url, name and options from the args seen by scripts.
  PRUint32 argOffset = argc < 3 ? argc : 3;
  nsCOMPtr<nsIArray> argvArray;
  rv = NS_CreateJSArgv(cx, argc-argOffset, argv+argOffset, getter_AddRefs(argvArray));
  NS_ENSURE_SUCCESS(rv, rv);

  return OpenInternal(url, name, options,
                      PR_TRUE,             // aDialog
                      PR_FALSE,            // aCalledNoScript
                      PR_FALSE,            // aDoJSFixups
                      argvArray, nsnull,   // Arguments
                      GetPrincipal(),      // aCalleePrincipal
                      cx,                  // aJSCallerContext
                      _retval);
}

NS_IMETHODIMP
nsGlobalWindow::GetFrames(nsIDOMWindow** aFrames)
{
  FORWARD_TO_OUTER(GetFrames, (aFrames), NS_ERROR_NOT_INITIALIZED);

  *aFrames = this;
  NS_ADDREF(*aFrames);

  FlushPendingNotifications(Flush_ContentAndNotify);

  return NS_OK;
}

class nsCloseEvent : public nsRunnable {
public:
  nsCloseEvent (nsGlobalWindow *aWindow)
    : mWindow(aWindow)
  {
  }
 
  NS_IMETHOD Run() {
    if (mWindow)
      mWindow->ReallyCloseWindow();
    return NS_OK;
  }

  nsRefPtr<nsGlobalWindow> mWindow;
};

NS_IMETHODIMP
nsGlobalWindow::Close()
{
  FORWARD_TO_OUTER(Close, (), NS_ERROR_NOT_INITIALIZED);

  if (IsFrame() || !mDocShell || IsInModalState()) {
    // window.close() is called on a frame in a frameset, on a window
    // that's already closed, or on a window for which there's
    // currently a modal dialog open. Ignore such calls.

    return NS_OK;
  }

  if (mHavePendingClose) {
    // We're going to be closed anyway; do nothing since we don't want
    // to double-close
    return NS_OK;
  }

  if (mBlockScriptedClosingFlag)
  {
    // A script's popup has been blocked and we don't want
    // the window to be closed directly after this event,
    // so the user can see that there was a blocked popup.
    return NS_OK;
  }

  // Don't allow scripts from content to close windows
  // that were not opened by script
  nsresult rv = NS_OK;
  if (!mHadOriginalOpener && !nsContentUtils::IsCallerTrustedForWrite()) {
    PRBool allowClose =
      nsContentUtils::GetBoolPref("dom.allow_scripts_to_close_windows",
                                  PR_TRUE);
    if (!allowClose) {
      // We're blocking the close operation
      // report localized error msg in JS console
      nsContentUtils::ReportToConsole(
          nsContentUtils::eDOM_PROPERTIES,
          "WindowCloseBlockedWarning",
          nsnull, 0, // No params
          nsnull, // No URI.  Not clear which URI we should be using
                  // here anyway
          EmptyString(), 0, 0, // No source, or column/line number
          nsIScriptError::warningFlag,
          "DOM Window");  // Better name for the category?

      return NS_OK;
    }
  }

  // Ask the content viewer whether the toplevel window can close.
  // If the content viewer returns false, it is responsible for calling
  // Close() as soon as it is possible for the window to close.
  // This allows us to not close the window while printing is happening.

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (!mInClose && !mIsClosed && cv) {
    PRBool canClose;

    rv = cv->PermitUnload(&canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return NS_OK;

    rv = cv->RequestWindowClose(&canClose);
    if (NS_SUCCEEDED(rv) && !canClose)
      return NS_OK;
  }

  // Fire a DOM event notifying listeners that this window is about to
  // be closed. The tab UI code may choose to cancel the default
  // action for this event, if so, we won't actually close the window
  // (since the tab UI code will close the tab in stead). Sure, this
  // could be abused by content code, but do we care? I don't think
  // so...

  PRBool wasInClose = mInClose;
  mInClose = PR_TRUE;

  if (!DispatchCustomEvent("DOMWindowClose")) {
    // Someone chose to prevent the default action for this event, if
    // so, let's not close this window after all...

    mInClose = wasInClose;
    return NS_OK;
  }

  // Flag that we were closed.
  mIsClosed = PR_TRUE;

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService(sJSStackContractID);

  JSContext *cx = nsnull;

  if (stack) {
    stack->Peek(&cx);
  }

  if (cx) {
    nsIScriptContext *currentCX = nsJSUtils::GetDynamicScriptContext(cx);

    if (currentCX && currentCX == mContext) {
      // We ignore the return value here.  If setting the termination function
      // fails, it's better to fail to close the window than it is to crash
      // (which is what would tend to happen if we did this synchronously
      // here).
      rv = currentCX->SetTerminationFunction(CloseWindow,
                                             NS_STATIC_CAST(nsIDOMWindow *,
                                                            this));
      if (NS_SUCCEEDED(rv)) {
        mHavePendingClose = PR_TRUE;
      }
      return NS_OK;
    }
  }

  
  // We may have plugins on the page that have issued this close from their
  // event loop and because we currently destroy the plugin window with
  // frames, we crash. So, if we are called from Javascript, post an event
  // to really close the window.
  rv = NS_ERROR_FAILURE;
  if (!nsContentUtils::IsCallerChrome()) {
    nsCOMPtr<nsIRunnable> ev = new nsCloseEvent(this);
    rv = NS_DispatchToCurrentThread(ev);
  }
  
  if (NS_FAILED(rv)) {
    ReallyCloseWindow();
    rv = NS_OK;
  } else {
    mHavePendingClose = PR_TRUE;
  }
  
  return rv;
}


void
nsGlobalWindow::ReallyCloseWindow()
{
  FORWARD_TO_OUTER_VOID(ReallyCloseWindow, ());

  // Make sure we never reenter this method.
  mHavePendingClose = PR_TRUE;

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  // If there's no treeOwnerAsWin, this window must already be closed.

  if (treeOwnerAsWin) {

    // but if we're a browser window we could be in some nasty
    // self-destroying cascade that we should mostly ignore

    nsCOMPtr<nsIDocShellTreeItem> docItem(do_QueryInterface(mDocShell));
    if (docItem) {
      nsCOMPtr<nsIBrowserDOMWindow> bwin;
      nsCOMPtr<nsIDocShellTreeItem> rootItem;
      docItem->GetRootTreeItem(getter_AddRefs(rootItem));
      nsCOMPtr<nsIDOMWindow> rootWin(do_GetInterface(rootItem));
      nsCOMPtr<nsIDOMChromeWindow> chromeWin(do_QueryInterface(rootWin));
      if (chromeWin)
        chromeWin->GetBrowserDOMWindow(getter_AddRefs(bwin));

      if (rootWin) {
        /* Normally we destroy the entire window, but not if
           this DOM window belongs to a tabbed browser and doesn't
           correspond to a tab. This allows a well-behaved tab
           to destroy the container as it should but is a final measure
           to prevent an errant tab from doing so when it shouldn't.
           This works because we reach this code when we shouldn't only
           in the particular circumstance that we belong to a tab
           that has just been closed (and is therefore already missing
           from the list of browsers) (and has an unload handler
           that closes the window). */
        // XXXbz now that we have mHavePendingClose, is this needed?
        PRBool isTab = PR_FALSE;
        if (rootWin == this ||
            !bwin || (bwin->IsTabContentWindow(GetOuterWindowInternal(),
                                               &isTab), isTab))
          treeOwnerAsWin->Destroy();
      }
    }

    CleanUp();
  }
}

void
nsGlobalWindow::EnterModalState()
{
  nsCOMPtr<nsIDOMWindow> top;
  GetTop(getter_AddRefs(top));

  if (!top) {
    NS_ERROR("Uh, EnterModalState() called w/o a reachable top window?");

    return;
  }

  NS_STATIC_CAST(nsGlobalWindow *,
                 NS_STATIC_CAST(nsIDOMWindow *,
                                top.get()))->mModalStateDepth++;
}

// static
void
nsGlobalWindow::RunPendingTimeoutsRecursive(nsGlobalWindow *aTopWindow,
                                            nsGlobalWindow *aWindow)
{
  nsGlobalWindow *inner;

  // Return early if we're frozen or have no inner window.
  if (!(inner = aWindow->GetCurrentInnerWindowInternal()) ||
      inner->IsFrozen()) {
    return;
  }

  inner->RunTimeout(nsnull);

  // Check again if we're frozen since running pending timeouts
  // could've frozen us.
  if (inner->IsFrozen()) {
    return;
  }

  nsCOMPtr<nsIDOMWindowCollection> frames;
  aWindow->GetFrames(getter_AddRefs(frames));

  if (!frames) {
    return;
  }

  PRUint32 i, length;
  if (NS_FAILED(frames->GetLength(&length)) || !length) {
    return;
  }

  for (i = 0; i < length && aTopWindow->mModalStateDepth == 0; i++) {
    nsCOMPtr<nsIDOMWindow> child;
    frames->Item(i, getter_AddRefs(child));

    if (!child) {
      return;
    }

    nsGlobalWindow *childWin =
      NS_STATIC_CAST(nsGlobalWindow *,
                     NS_STATIC_CAST(nsIDOMWindow *,
                                    child.get()));

    RunPendingTimeoutsRecursive(aTopWindow, childWin);
  }
}

class nsPendingTimeoutRunner : public nsRunnable
{
public:
  nsPendingTimeoutRunner(nsGlobalWindow *aWindow)
    : mWindow(aWindow)
  {
    NS_ASSERTION(mWindow, "mWindow is null.");
  }

  NS_IMETHOD Run()
  {
    nsGlobalWindow::RunPendingTimeoutsRecursive(mWindow, mWindow);

    return NS_OK;
  }

private:
  nsRefPtr<nsGlobalWindow> mWindow;
};

void
nsGlobalWindow::LeaveModalState()
{
  nsCOMPtr<nsIDOMWindow> top;
  GetTop(getter_AddRefs(top));

  if (!top) {
    NS_ERROR("Uh, LeaveModalState() called w/o a reachable top window?");

    return;
  }

  nsGlobalWindow *topWin =
    NS_STATIC_CAST(nsGlobalWindow *,
                   NS_STATIC_CAST(nsIDOMWindow *,
                                  top.get()));

  topWin->mModalStateDepth--;

  if (topWin->mModalStateDepth == 0) {
    nsCOMPtr<nsIRunnable> runner = new nsPendingTimeoutRunner(topWin);
    if (NS_FAILED(NS_DispatchToCurrentThread(runner)))
      NS_WARNING("failed to dispatch pending timeout runnable");
  }
}

PRBool
nsGlobalWindow::IsInModalState()
{
  nsCOMPtr<nsIDOMWindow> top;
  GetTop(getter_AddRefs(top));

  if (!top) {
    NS_ERROR("Uh, IsInModalState() called w/o a reachable top window?");

    return PR_FALSE;
  }

  return NS_STATIC_CAST(nsGlobalWindow *,
                        NS_STATIC_CAST(nsIDOMWindow *,
                                       top.get()))->mModalStateDepth != 0;
}

NS_IMETHODIMP
nsGlobalWindow::GetFrameElement(nsIDOMElement** aFrameElement)
{
  FORWARD_TO_OUTER(GetFrameElement, (aFrameElement), NS_ERROR_NOT_INITIALIZED);

  *aFrameElement = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> docShellTI(do_QueryInterface(mDocShell));

  if (!docShellTI) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> parent;
  docShellTI->GetSameTypeParent(getter_AddRefs(parent));

  if (!parent || parent == docShellTI) {
    // We're at a chrome boundary, don't expose the chrome iframe
    // element to content code.

    return NS_OK;
  }

  *aFrameElement = mFrameElement;
  NS_IF_ADDREF(*aFrameElement);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::UpdateCommands(const nsAString& anAction)
{
  nsPIDOMWindow *rootWindow = nsGlobalWindow::GetPrivateRoot();
  if (!rootWindow)
    return NS_OK;

  nsCOMPtr<nsIDOMXULDocument> xulDoc =
    do_QueryInterface(rootWindow->GetExtantDocument());
  // See if we contain a XUL document.
  if (xulDoc) {
    // Retrieve the command dispatcher and call updateCommands on it.
    nsCOMPtr<nsIDOMXULCommandDispatcher> xulCommandDispatcher;
    xulDoc->GetCommandDispatcher(getter_AddRefs(xulCommandDispatcher));
    xulCommandDispatcher->UpdateCommands(anAction);
  }

  return NS_OK;
}

nsresult
nsGlobalWindow::ConvertCharset(const nsAString& aStr, char** aDest)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIUnicodeEncoder> encoder;

  nsCOMPtr<nsICharsetConverterManager>
    ccm(do_GetService(kCharsetConverterManagerCID));
  NS_ENSURE_TRUE(ccm, NS_ERROR_FAILURE);

  // Get the document character set
  nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8")); // default to utf-8
  if (mDoc) {
    charset = mDoc->GetDocumentCharacterSet();
  }

  // Get an encoder for the character set
  result = ccm->GetUnicodeEncoderRaw(charset.get(),
                                     getter_AddRefs(encoder));
  if (NS_FAILED(result))
    return result;

  result = encoder->Reset();
  if (NS_FAILED(result))
    return result;

  PRInt32 maxByteLen, srcLen;
  srcLen = aStr.Length();

  const nsPromiseFlatString& flatSrc = PromiseFlatString(aStr);
  const PRUnichar* src = flatSrc.get();

  // Get the expected length of result string
  result = encoder->GetMaxLength(src, srcLen, &maxByteLen);
  if (NS_FAILED(result))
    return result;

  // Allocate a buffer of the maximum length
  *aDest = (char *) nsMemory::Alloc(maxByteLen + 1);
  PRInt32 destLen2, destLen = maxByteLen;
  if (!*aDest)
    return NS_ERROR_OUT_OF_MEMORY;

  // Convert from unicode to the character set
  result = encoder->Convert(src, &srcLen, *aDest, &destLen);
  if (NS_FAILED(result)) {    
    nsMemory::Free(*aDest);
    *aDest = nsnull;
    return result;
  }

  // Allow the encoder to finish the conversion
  destLen2 = maxByteLen - destLen;
  encoder->Finish(*aDest + destLen, &destLen2);
  (*aDest)[destLen + destLen2] = '\0';

  return result;
}

PRBool
nsGlobalWindow::GetBlurSuppression()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  PRBool suppress = PR_FALSE;
  if (treeOwnerAsWin)
    treeOwnerAsWin->GetBlurSuppression(&suppress);
  return suppress;
}

NS_IMETHODIMP
nsGlobalWindow::GetSelection(nsISelection** aSelection)
{
  FORWARD_TO_OUTER(GetSelection, (aSelection), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aSelection);
  *aSelection = nsnull;

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell)
    return NS_OK;
    
  *aSelection = presShell->GetCurrentSelection(nsISelectionController::SELECTION_NORMAL);
  
  NS_IF_ADDREF(*aSelection);

  return NS_OK;
}

// Non-scriptable version of window.find(), part of nsIDOMWindowInternal
NS_IMETHODIMP
nsGlobalWindow::Find(const nsAString& aStr, PRBool aCaseSensitive,
                     PRBool aBackwards, PRBool aWrapAround, PRBool aWholeWord,
                     PRBool aSearchInFrames, PRBool aShowDialog,
                     PRBool *aDidFind)
{
  return FindInternal(aStr, aCaseSensitive, aBackwards, aWrapAround,
                      aWholeWord, aSearchInFrames, aShowDialog, aDidFind);
}

// Scriptable version of window.find() which takes a variable number of
// arguments, part of nsIDOMJSWindow.
NS_IMETHODIMP
nsGlobalWindow::Find(PRBool *aDidFind)
{
  nsresult rv = NS_OK;

  // We get the arguments passed to the function using the XPConnect native
  // call context.
  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(ncc, "No Native Call Context."
                    "Please don't call this method from C++.");
  if (!ncc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc;
  jsval *argv = nsnull;

  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  // Parse the arguments passed to the function
  nsAutoString searchStr;
  PRBool caseSensitive  = PR_FALSE;
  PRBool backwards      = PR_FALSE;
  PRBool wrapAround     = PR_FALSE;
  PRBool showDialog     = PR_FALSE;
  PRBool wholeWord      = PR_FALSE;
  PRBool searchInFrames = PR_FALSE;

  if (argc > 0) {
    JSAutoRequest ar(cx);
    switch (argc) {
      default:
      case 7:
        if (!JS_ValueToBoolean(cx, argv[6], &showDialog)) {
          // Seventh arg specifies whether we should search in all frames
          showDialog = PR_FALSE;
        }
      case 6:
        if (!JS_ValueToBoolean(cx, argv[5], &searchInFrames)) {
          // Sixth arg specifies whether we should search only for whole words
          searchInFrames = PR_FALSE;
        }
      case 5:
        if (!JS_ValueToBoolean(cx, argv[4], &wholeWord)) {
          // Fifth arg specifies whether we should show the Find dialog
          wholeWord = PR_FALSE;
        }
      case 4:
        if (!JS_ValueToBoolean(cx, argv[3], &wrapAround)) {
          // Fourth arg specifies whether we should wrap the search
          wrapAround = PR_FALSE;
        }
      case 3:
        if (!JS_ValueToBoolean(cx, argv[2], &backwards)) {
          // Third arg specifies whether to search backwards
          backwards = PR_FALSE;
        }
      case 2:
        if (!JS_ValueToBoolean(cx, argv[1], &caseSensitive)) {
          // Second arg is the case sensitivity
          caseSensitive = PR_FALSE;
        }
      case 1:
        // First arg is the search pattern
        nsJSUtils::ConvertJSValToString(searchStr, cx, argv[0]);
        break;
    }
  }

  return FindInternal(searchStr, caseSensitive, backwards, wrapAround,
                      wholeWord, searchInFrames, showDialog, aDidFind);
}

nsresult
nsGlobalWindow::FindInternal(const nsAString& aStr, PRBool caseSensitive,
                             PRBool backwards, PRBool wrapAround,
                             PRBool wholeWord, PRBool searchInFrames,
                             PRBool showDialog, PRBool *aDidFind)
{
  FORWARD_TO_OUTER(FindInternal, (aStr, caseSensitive, backwards, wrapAround,
                                  wholeWord, searchInFrames, showDialog,
                                  aDidFind), NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aDidFind);
  nsresult rv = NS_OK;
  *aDidFind = PR_FALSE;

  nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mDocShell));

  // Set the options of the search
  rv = finder->SetSearchString(PromiseFlatString(aStr).get());
  NS_ENSURE_SUCCESS(rv, rv);
  finder->SetMatchCase(caseSensitive);
  finder->SetFindBackwards(backwards);
  finder->SetWrapFind(wrapAround);
  finder->SetEntireWord(wholeWord);
  finder->SetSearchFrames(searchInFrames);

  // the nsIWebBrowserFind is initialized to use this window
  // as the search root, but uses focus to set the current search
  // frame. If we're being called from JS (as here), this window
  // should be the current search frame.
  nsCOMPtr<nsIWebBrowserFindInFrames> framesFinder(do_QueryInterface(finder));
  if (framesFinder) {
    framesFinder->SetRootSearchFrame(this);   // paranoia
    framesFinder->SetCurrentSearchFrame(this);
  }
  
  // The Find API does not accept empty strings. Launch the Find Dialog.
  if (aStr.IsEmpty() || showDialog) {
    // See if the find dialog is already up using nsIWindowMediator
    nsCOMPtr<nsIWindowMediator> windowMediator =
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);

    nsCOMPtr<nsIDOMWindowInternal> findDialog;

    if (windowMediator) {
      windowMediator->GetMostRecentWindow(NS_LITERAL_STRING("findInPage").get(),
                                          getter_AddRefs(findDialog));
    }

    if (findDialog) {
      // The Find dialog is already open, bring it to the top.
      rv = findDialog->Focus();
    } else { // Open a Find dialog
      if (finder) {
        nsCOMPtr<nsIDOMWindow> dialog;
        rv = OpenDialog(NS_LITERAL_STRING("chrome://global/content/finddialog.xul"),
                        NS_LITERAL_STRING("_blank"),
                        NS_LITERAL_STRING("chrome, resizable=no, dependent=yes"),
                        finder, getter_AddRefs(dialog));
      }
    }
  } else {
    // Launch the search with the passed in search string
    rv = finder->FindNext(aDidFind);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

static PRBool
Is8bit(const nsAString& aString)
{
  static const PRUnichar EIGHT_BIT = PRUnichar(~0x00FF);

  nsAString::const_iterator done_reading;
  aString.EndReading(done_reading);

  // for each chunk of |aString|...
  PRUint32 fragmentLength = 0;
  nsAString::const_iterator iter;
  for (aString.BeginReading(iter); iter != done_reading;
       iter.advance(PRInt32(fragmentLength))) {
    fragmentLength = PRUint32(iter.size_forward());
    const PRUnichar* c = iter.get();
    const PRUnichar* fragmentEnd = c + fragmentLength;

    // for each character in this chunk...
    while (c < fragmentEnd)
      if (*c++ & EIGHT_BIT)
        return PR_FALSE;
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsGlobalWindow::Atob(const nsAString& aAsciiBase64String,
                     nsAString& aBinaryData)
{
  aBinaryData.Truncate();

  if (!Is8bit(aAsciiBase64String)) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  PRInt32 dataLen = aAsciiBase64String.Length();

  NS_LossyConvertUTF16toASCII base64(aAsciiBase64String);
  if (base64.Length() != dataLen) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 resultLen = dataLen;
  if (!base64.IsEmpty() && base64[dataLen - 1] == '=') {
    if (base64.Length() > 1 && base64[dataLen - 2] == '=') {
      resultLen = dataLen - 2;
    } else {
      resultLen = dataLen - 1;
    }
  }

  resultLen = ((resultLen * 3) / 4);
  // Add 4 extra bytes (one is needed for sure for null termination)
  // to the malloc size just to make sure we don't end up writing past
  // the allocated memory (the PL_Base64Decode API should really
  // provide a guaranteed way to figure this out w/o needing to do the
  // above yourself).
  char *dest = NS_STATIC_CAST(char *, nsMemory::Alloc(resultLen + 4));
  if (!dest) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char *bin_data = PL_Base64Decode(base64.get(), dataLen, dest);

  nsresult rv = NS_OK;

  if (bin_data) {
    CopyASCIItoUTF16(Substring(bin_data, bin_data + resultLen), aBinaryData);
  } else {
    rv = NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  nsMemory::Free(dest);

  return rv;
}

NS_IMETHODIMP
nsGlobalWindow::Btoa(const nsAString& aBinaryData,
                     nsAString& aAsciiBase64String)
{
  aAsciiBase64String.Truncate();

  if (!Is8bit(aBinaryData)) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  char *bin_data = ToNewCString(aBinaryData);
  if (!bin_data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 resultLen = ((aBinaryData.Length() + 2) / 3) * 4;

  char *base64 = PL_Base64Encode(bin_data, aBinaryData.Length(), nsnull);
  if (!base64) {
    nsMemory::Free(bin_data);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  CopyASCIItoUTF16(nsDependentCString(base64, resultLen), aAsciiBase64String);

  PR_Free(base64);
  nsMemory::Free(bin_data);

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMEventTarget
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::AddEventListener(const nsAString& aType,
                                 nsIDOMEventListener* aListener,
                                 PRBool aUseCapture)
{
  FORWARD_TO_INNER_CREATE(AddEventListener, (aType, aListener, aUseCapture));

  return AddEventListener(aType, aListener, aUseCapture,
                          !nsContentUtils::IsChromeDoc(mDoc));
}

NS_IMETHODIMP
nsGlobalWindow::RemoveEventListener(const nsAString& aType,
                                    nsIDOMEventListener* aListener,
                                    PRBool aUseCapture)
{
  return RemoveGroupedEventListener(aType, aListener, aUseCapture, nsnull);
}

NS_IMETHODIMP
nsGlobalWindow::DispatchEvent(nsIDOMEvent* aEvent, PRBool* _retval)
{
  FORWARD_TO_INNER(DispatchEvent, (aEvent, _retval), NS_OK);

  if (!mDoc) {
    return NS_ERROR_FAILURE;
  }

  // Obtain a presentation shell
  nsIPresShell *shell = mDoc->GetShellAt(0);
  nsCOMPtr<nsPresContext> presContext;
  if (shell) {
    // Retrieve the context
    presContext = shell->GetPresContext();
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    nsEventDispatcher::DispatchDOMEvent(GetOuterWindow(), nsnull, aEvent,
                                        presContext, &status);

  *_retval = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOM3EventTarget
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::AddGroupedEventListener(const nsAString & aType,
                                        nsIDOMEventListener *aListener,
                                        PRBool aUseCapture,
                                        nsIDOMEventGroup *aEvtGrp)
{
  FORWARD_TO_INNER_CREATE(AddGroupedEventListener,
                          (aType, aListener, aUseCapture, aEvtGrp));

  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED(GetListenerManager(PR_TRUE, getter_AddRefs(manager)))) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags, aEvtGrp);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::RemoveGroupedEventListener(const nsAString & aType,
                                           nsIDOMEventListener *aListener,
                                           PRBool aUseCapture,
                                           nsIDOMEventGroup *aEvtGrp)
{
  FORWARD_TO_INNER(RemoveGroupedEventListener,
                   (aType, aListener, aUseCapture, aEvtGrp),
                   NS_ERROR_NOT_INITIALIZED);

  if (mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags,
                                                aEvtGrp);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::CanTrigger(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalWindow::IsRegisteredHere(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalWindow::AddEventListener(const nsAString& aType,
                                 nsIDOMEventListener *aListener,
                                 PRBool aUseCapture, PRBool aWantsUntrusted)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  nsresult rv = GetListenerManager(PR_TRUE, getter_AddRefs(manager));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  if (aWantsUntrusted) {
    flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
  }

  return manager->AddEventListenerByType(aListener, aType, flags, nsnull);
}


//*****************************************************************************
// nsGlobalWindow::nsIDOMEventReceiver
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::AddEventListenerByIID(nsIDOMEventListener* aListener,
                                      const nsIID& aIID)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED(GetListenerManager(PR_TRUE, getter_AddRefs(manager)))) {
    manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::RemoveEventListenerByIID(nsIDOMEventListener* aListener,
                                         const nsIID& aIID)
{
  FORWARD_TO_INNER(RemoveEventListenerByIID, (aListener, aIID),
                   NS_ERROR_NOT_INITIALIZED);

  if (mListenerManager) {
    mListenerManager->RemoveEventListenerByIID(aListener, aIID,
                                               NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalWindow::GetListenerManager(PRBool aCreateIfNotFound,
                                   nsIEventListenerManager** aResult)
{
  FORWARD_TO_INNER_CREATE(GetListenerManager, (aCreateIfNotFound, aResult));

  if (!mListenerManager) {
    if (!aCreateIfNotFound) {
      *aResult = nsnull;
      return NS_OK;
    }

    static NS_DEFINE_CID(kEventListenerManagerCID,
                         NS_EVENTLISTENERMANAGER_CID);
    nsresult rv;

    mListenerManager = do_CreateInstance(kEventListenerManagerCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    mListenerManager->SetListenerTarget(
      NS_STATIC_CAST(nsIDOMEventReceiver*, this));
  }

  NS_ADDREF(*aResult = mListenerManager);

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::HandleEvent(nsIDOMEvent *aEvent)
{
  PRBool defaultActionEnabled;
  return DispatchEvent(aEvent, &defaultActionEnabled);
}

NS_IMETHODIMP
nsGlobalWindow::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  if (NS_SUCCEEDED(GetListenerManager(PR_TRUE, getter_AddRefs(manager))) &&
      manager) {
    return manager->GetSystemEventGroupLM(aGroup);
  }
  return NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsGlobalWindow::nsPIDOMWindow
//*****************************************************************************

nsPIDOMWindow*
nsGlobalWindow::GetPrivateParent()
{
  FORWARD_TO_OUTER(GetPrivateParent, (), nsnull);

  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (NS_STATIC_CAST(nsIDOMWindow *, this) == parent.get()) {
    nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
    if (!chromeElement)
      return nsnull;             // This is ok, just means a null parent.

    nsIDocument* doc = chromeElement->GetDocument();
    if (!doc)
      return nsnull;             // This is ok, just means a null parent.

    nsIScriptGlobalObject *globalObject = doc->GetScriptGlobalObject();
    if (!globalObject)
      return nsnull;             // This is ok, just means a null parent.

    parent = do_QueryInterface(globalObject);
  }

  if (parent) {
    return NS_STATIC_CAST(nsGlobalWindow *,
                          NS_STATIC_CAST(nsIDOMWindow*, parent.get()));
  }

  return nsnull;
}

nsPIDOMWindow*
nsGlobalWindow::GetPrivateRoot()
{
  FORWARD_TO_OUTER(GetPrivateRoot, (), nsnull);

  nsCOMPtr<nsIDOMWindow> top;
  GetTop(getter_AddRefs(top));

  nsCOMPtr<nsPIDOMWindow> ptop = do_QueryInterface(top);
  NS_ASSERTION(ptop, "cannot get ptop");
  if (!ptop)
    return nsnull;

  nsIDocShell *docShell = ptop->GetDocShell();

  // Get the chrome event handler from the doc shell, since we only
  // want to deal with XUL chrome handlers and not the new kind of
  // window root handler.
  nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;
  docShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));

  nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
  if (chromeElement) {
    nsIDocument* doc = chromeElement->GetDocument();
    if (doc) {
      nsIDOMWindow *parent = doc->GetWindow();
      if (parent) {
        parent->GetTop(getter_AddRefs(top));
      }
    }
  }

  return NS_STATIC_CAST(nsGlobalWindow *,
                        NS_STATIC_CAST(nsIDOMWindow *, top));
}


NS_IMETHODIMP
nsGlobalWindow::GetLocation(nsIDOMLocation ** aLocation)
{
  FORWARD_TO_OUTER(GetLocation, (aLocation), NS_ERROR_NOT_INITIALIZED);

  *aLocation = nsnull;

  if (!mLocation && mDocShell) {
    mLocation = new nsLocation(mDocShell);
    if (!mLocation) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aLocation = mLocation);

  return NS_OK;
}

static nsresult
FireWidgetEvent(nsIDocShell *aDocShell, PRUint32 aMsg)
{
  nsCOMPtr<nsIPresShell> presShell;
  aDocShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsIViewManager* vm = presShell->GetViewManager();
  NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

  nsIView *rootView;
  vm->GetRootView(rootView);
  NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

  // We're holding a STRONG REF to the widget to ensure it doesn't go away
  // during event processing
  nsCOMPtr<nsIWidget> widget = rootView->GetWidget();
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  nsEventStatus status;

  nsGUIEvent guiEvent(PR_TRUE, aMsg, widget);
  guiEvent.time = PR_IntervalNow();

  vm->DispatchEvent(&guiEvent, &status);

  return NS_OK;
}

nsresult
nsGlobalWindow::Activate()
{
  FORWARD_TO_OUTER(Activate, (), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  if (treeOwnerAsWin) {
    PRBool isEnabled = PR_TRUE;
    if (NS_SUCCEEDED(treeOwnerAsWin->GetEnabled(&isEnabled)) && !isEnabled) {
      NS_WARNING( "Should not try to activate a disabled window" );
      return NS_ERROR_FAILURE;
    }

    treeOwnerAsWin->SetVisibility(PR_TRUE);
  }

  return FireWidgetEvent(mDocShell, NS_ACTIVATE);
}

nsresult
nsGlobalWindow::Deactivate()
{
  FORWARD_TO_OUTER(Deactivate, (), NS_ERROR_NOT_INITIALIZED);

  return FireWidgetEvent(mDocShell, NS_DEACTIVATE);
}

nsIFocusController*
nsGlobalWindow::GetRootFocusController()
{
  nsIDOMWindowInternal *rootWindow = nsGlobalWindow::GetPrivateRoot();
  nsCOMPtr<nsIFocusController> fc;

  if (rootWindow) {
    // Obtain the chrome event handler.
    nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));
    nsIChromeEventHandler *chromeHandler = piWin->GetChromeEventHandler();
    if (chromeHandler) {
      nsCOMPtr<nsPIWindowRoot> windowRoot(do_QueryInterface(chromeHandler));
      if (windowRoot) {
        windowRoot->GetFocusController(getter_AddRefs(fc));
      }
    }
  }

  // this reference is going away, but the root (if found) holds onto
  // it
  return fc;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMViewCSS
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetComputedStyle(nsIDOMElement* aElt,
                                 const nsAString& aPseudoElt,
                                 nsIDOMCSSStyleDeclaration** aReturn)
{
  FORWARD_TO_OUTER(GetComputedStyle, (aElt, aPseudoElt, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (!aElt) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if (!mDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell) {
    return NS_OK;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIComputedDOMStyle> compStyle;

  if (!sComputedDOMStyleFactory) {
    rv = CallGetClassObject("@mozilla.org/DOM/Level2/CSS/computedStyleDeclaration;1",
                            &sComputedDOMStyleFactory);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv =
    sComputedDOMStyleFactory->CreateInstance(nsnull,
                                             NS_GET_IID(nsIComputedDOMStyle),
                                             getter_AddRefs(compStyle));

  NS_ENSURE_SUCCESS(rv, rv);

  rv = compStyle->Init(aElt, aPseudoElt, presShell);
  NS_ENSURE_SUCCESS(rv, rv);

  return compStyle->QueryInterface(NS_GET_IID(nsIDOMCSSStyleDeclaration),
                                   (void **) aReturn);
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMAbstractView
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetDocument(nsIDOMDocumentView ** aDocumentView)
{
  NS_ENSURE_ARG_POINTER(aDocumentView);

  nsresult rv = NS_OK;

  if (mDocument) {
    rv = CallQueryInterface(mDocument, aDocumentView);
  }
  else {
    *aDocumentView = nsnull;
  }

  return rv;
}

//*****************************************************************************
// nsGlobalWindow::nsIDOMStorageWindow
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetSessionStorage(nsIDOMStorage ** aSessionStorage)
{
  FORWARD_TO_INNER(GetSessionStorage, (aSessionStorage), NS_ERROR_UNEXPECTED);

  *aSessionStorage = nsnull;

  nsIPrincipal *principal = GetPrincipal();
  nsIDocShell *docShell = GetDocShell();

  if (!principal || !docShell) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> codebase;
  nsresult rv = principal->GetURI(getter_AddRefs(codebase));

  if (NS_FAILED(rv) || !codebase) {
    return NS_FAILED(rv) ? rv : NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  return docShell->GetSessionStorageForURI(codebase, aSessionStorage);
}

NS_IMETHODIMP
nsGlobalWindow::GetGlobalStorage(nsIDOMStorageList ** aGlobalStorage)
{
  NS_ENSURE_ARG_POINTER(aGlobalStorage);

#ifdef MOZ_STORAGE
  if (!gGlobalStorageList) {
    nsresult rv = NS_NewDOMStorageList(getter_AddRefs(gGlobalStorageList));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aGlobalStorage = gGlobalStorageList;
  NS_IF_ADDREF(*aGlobalStorage);

  return NS_OK;
#else
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
#endif
}

//*****************************************************************************
// nsGlobalWindow::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP
nsGlobalWindow::GetInterface(const nsIID & aIID, void **aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);
  *aSink = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIDocCharset))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIDocCharset> docCharset(do_QueryInterface(mDocShell));
      if (docCharset) {
        *aSink = docCharset;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebNavigation))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
      if (webNav) {
        *aSink = webNav;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
#ifdef NS_PRINTING
  else if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    if (mDocShell) {
      nsCOMPtr<nsIContentViewer> viewer;
      mDocShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
        if (webBrowserPrint) {
          *aSink = webBrowserPrint;
          NS_ADDREF(((nsISupports *) *aSink));
        }
      }
    }
  }
#endif
  else if (aIID.Equals(NS_GET_IID(nsIScriptEventManager))) {
    if (mDoc) {
      nsIScriptEventManager* mgr = mDoc->GetScriptEventManager();
      if (mgr) {
        *aSink = mgr;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIDOMWindowUtils))) {
    FORWARD_TO_OUTER(GetInterface, (aIID, aSink), NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsISupports> utils(do_QueryReferent(mWindowUtils));
    if (utils) {
      *aSink = utils;
      NS_ADDREF(((nsISupports *) *aSink));
    } else {
      nsDOMWindowUtils *utilObj = new nsDOMWindowUtils(this);
      nsCOMPtr<nsISupports> utilsIfc =
                              NS_ISUPPORTS_CAST(nsIDOMWindowUtils *, utilObj);
      if (utilsIfc) {
        mWindowUtils = do_GetWeakReference(utilsIfc);
        *aSink = utilsIfc;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else {
    return QueryInterface(aIID, aSink);
  }

  return *aSink ? NS_OK : NS_ERROR_NO_INTERFACE;
}

void
nsGlobalWindow::FireOfflineStatusEvent()
{
  if (!mDocument)
    return;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  nsAutoString name;
  if (NS_IsOffline()) {
    name.AssignLiteral("offline");
  } else {
    name.AssignLiteral("online");
  }
  // The event is fired at the body element, or if there is no body element,
  // at the document.
  nsCOMPtr<nsISupports> eventTarget = doc.get();
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(doc);
  if (htmlDoc) {
    nsCOMPtr<nsIDOMHTMLElement> body;
    htmlDoc->GetBody(getter_AddRefs(body));
    if (body) {
      eventTarget = body;
    }
  }
  nsContentUtils::DispatchTrustedEvent(doc, eventTarget, name, PR_TRUE, PR_FALSE);
}

nsresult
nsGlobalWindow::Observe(nsISupports* aSubject, const char* aTopic,
                        const PRUnichar* aData)
{
  if (!nsCRT::strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC)) {
    if (IsFrozen()) {
      // if an even number of notifications arrive while we're frozen,
      // we don't need to fire.
      mFireOfflineStatusChangeEventOnThaw = !mFireOfflineStatusChangeEventOnThaw;
    } else {
      FireOfflineStatusEvent();
    }
    return NS_OK;
  }

  if (IsInnerWindow() && !nsCRT::strcmp(aTopic, "dom-storage-changed")) {
    nsIPrincipal *principal;
    nsresult rv;

    if (!aData) {
      nsCOMPtr<nsIDOMStorage> storage;
      GetSessionStorage(getter_AddRefs(storage));

      if (storage != aSubject && !aData) {
        // A sessionStorage object changed, but not our session storage
        // object.
        return NS_OK;
      }
    } else if ((principal = GetPrincipal())) {
      // A global storage object changed, check to see if it's one
      // this window can access.

      nsCOMPtr<nsIURI> codebase;
      principal->GetURI(getter_AddRefs(codebase));

      if (!codebase) {
        return NS_OK;
      }

      nsCAutoString currentDomain;
      rv = codebase->GetAsciiHost(currentDomain);
      if (NS_FAILED(rv)) {
        return NS_OK;
      }

      if (!nsDOMStorageList::CanAccessDomain(nsDependentString(aData),
                                             NS_ConvertASCIItoUTF16(currentDomain))) {
        // This window can't reach the global storage object for the
        // domain for which the change happened, so don't fire any
        // events in this window.

        return NS_OK;
      }
    }

    nsAutoString domain(aData);

    if (IsFrozen()) {
      // This window is frozen, rather than firing the events here,
      // store the domain in which the change happened and fire the
      // events if we're ever thawed.

      if (!mPendingStorageEvents) {
        mPendingStorageEvents = new nsDataHashtable<nsStringHashKey, PRBool>;
        NS_ENSURE_TRUE(mPendingStorageEvents, NS_ERROR_OUT_OF_MEMORY);

        rv = mPendingStorageEvents->Init();
        NS_ENSURE_SUCCESS(rv, rv);
      }

      mPendingStorageEvents->Put(domain, PR_TRUE);

      return NS_OK;
    }

    nsRefPtr<nsDOMStorageEvent> event = new nsDOMStorageEvent(domain);
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

    rv = event->Init();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(mDocument));

    nsCOMPtr<nsIDOMEventTarget> target;

    if (htmlDoc) {
      nsCOMPtr<nsIDOMHTMLElement> body;
      htmlDoc->GetBody(getter_AddRefs(body));

      target = do_QueryInterface(body);
    }

    if (!target) {
      target = this;
    }

    PRBool defaultActionEnabled;
    target->DispatchEvent((nsIDOMStorageEvent *)event, &defaultActionEnabled);

    return NS_OK;
  }

  NS_WARNING("unrecognized topic in nsGlobalWindow::Observe");
  return NS_ERROR_FAILURE;
}

PR_STATIC_CALLBACK(PLDHashOperator)
FirePendingStorageEvents(const nsAString& aKey, PRBool aData, void *userArg)
{
  nsGlobalWindow *win = NS_STATIC_CAST(nsGlobalWindow *, userArg);

  nsCOMPtr<nsIDOMStorage> storage;
  win->GetSessionStorage(getter_AddRefs(storage));

  if (storage) {
    win->Observe(storage, "dom-storage-changed",
                 aKey.IsEmpty() ? nsnull : PromiseFlatString(aKey).get());
  }

  return PL_DHASH_NEXT;
}

nsresult
nsGlobalWindow::FireDelayedDOMEvents()
{
  FORWARD_TO_INNER(FireDelayedDOMEvents, (), NS_ERROR_UNEXPECTED);

  if (mPendingStorageEvents) {
    // Fire pending storage events.
    mPendingStorageEvents->EnumerateRead(FirePendingStorageEvents, this);

    delete mPendingStorageEvents;
    mPendingStorageEvents = nsnull;
  }

  if (mFireOfflineStatusChangeEventOnThaw) {
    mFireOfflineStatusChangeEventOnThaw = PR_FALSE;
    FireOfflineStatusEvent();
  }

  nsCOMPtr<nsIDocShellTreeNode> node =
    do_QueryInterface(GetDocShell());
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      NS_ASSERTION(childShell, "null child shell");

      nsCOMPtr<nsPIDOMWindow> pWin = do_GetInterface(childShell);
      if (pWin) {
        nsGlobalWindow *win =
          NS_STATIC_CAST(nsGlobalWindow*,
                         NS_STATIC_CAST(nsPIDOMWindow*, pWin));
        win->FireDelayedDOMEvents();
      }
    }
  }

  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow: Window Control Functions
//*****************************************************************************

nsIDOMWindowInternal *
nsGlobalWindow::GetParentInternal()
{
  FORWARD_TO_OUTER(GetParentInternal, (), nsnull);

  nsIDOMWindowInternal *parentInternal = nsnull;

  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (parent && parent != NS_STATIC_CAST(nsIDOMWindow *, this)) {
    nsCOMPtr<nsIDOMWindowInternal> tmp(do_QueryInterface(parent));
    NS_ASSERTION(parent, "Huh, parent not an nsIDOMWindowInternal?");

    parentInternal = tmp;
  }

  return parentInternal;
}

// static
void
nsGlobalWindow::CloseBlockScriptTerminationFunc(nsISupports *aRef)
{
  nsGlobalWindow* pwin = NS_STATIC_CAST(nsGlobalWindow*,
                                        NS_STATIC_CAST(nsPIDOMWindow*, aRef));
  pwin->mBlockScriptedClosingFlag = PR_FALSE;
}

nsresult
nsGlobalWindow::OpenInternal(const nsAString& aUrl, const nsAString& aName,
                             const nsAString& aOptions, PRBool aDialog,
                             PRBool aCalledNoScript, PRBool aDoJSFixups,
                             nsIArray *argv,
                             nsISupports *aExtraArgument,
                             nsIPrincipal *aCalleePrincipal,
                             JSContext *aJSCallerContext,
                             nsIDOMWindow **aReturn)
{
  FORWARD_TO_OUTER(OpenInternal, (aUrl, aName, aOptions, aDialog,
                                  aCalledNoScript, aDoJSFixups,
                                  argv, aExtraArgument, aCalleePrincipal,
                                  aJSCallerContext, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

#ifdef NS_DEBUG
  PRUint32 argc = 0;
  if (argv)
      argv->GetLength(&argc);
#endif
  NS_PRECONDITION(!aExtraArgument || (!argv && argc == 0),
                  "Can't pass in arguments both ways");
  NS_PRECONDITION(!aCalledNoScript || (!argv && argc == 0),
                  "Can't pass JS args when called via the noscript methods");
  NS_PRECONDITION(!aDoJSFixups || !aCalledNoScript,
                  "JS fixups should not be done when called noscript");
  NS_PRECONDITION(!aJSCallerContext || !aCalledNoScript,
                  "Shouldn't have caller context when called noscript");

  *aReturn = nsnull;
  
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  GetWebBrowserChrome(getter_AddRefs(chrome));
  if (!chrome) {
    // No chrome means we don't want to go through with this open call
    // -- see nsIWindowWatcher.idl
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(mDocShell, "Must have docshell here");

  const PRBool checkForPopup =
    !aDialog && !WindowExists(aName, !aCalledNoScript);

  nsCOMPtr<nsIURI> currentCodebase;

  if (aCalleePrincipal) {
    aCalleePrincipal->GetURI(getter_AddRefs(currentCodebase));
  }

  // Note: it's very important that this be an nsXPIDLCString, since we want
  // .get() on it to return nsnull until we write stuff to it.  The window
  // watcher expects a null URL string if there is no URL to load.
  nsXPIDLCString url;
  nsresult rv = NS_OK;

  // It's important to do this security check before determining whether this
  // window opening should be blocked, to ensure that we don't FireAbuseEvents
  // for a window opening that wouldn't have succeeded in the first place.
  if (!aUrl.IsEmpty()) {
    AppendUTF16toUTF8(aUrl, url);

    /* Check whether the URI is allowed, but not for dialogs --
       see bug 56851. The security of this function depends on
       window.openDialog being inaccessible from web scripts */
    if (url.get() && !aDialog)
      rv = SecurityCheckURL(url.get());
  }

  if (NS_FAILED(rv))
    return rv;

  // These next two variables are only accessed when checkForPopup is true
  PopupControlState abuseLevel;
  OpenAllowValue allowReason;
  if (checkForPopup) {
    abuseLevel = CheckForAbusePoint();
    allowReason = CheckOpenAllow(abuseLevel);
    if (allowReason == allowNot) {
      if (aJSCallerContext) {
        // If script in some other window is doing a window.open on us and
        // it's being blocked, then it's OK to close us afterwards, probably.
        // But if we're doing a window.open on ourselves and block the popup,
        // prevent this window from closing until after this script terminates
        // so that whatever popup blocker UI the app has will be visible.
        if (mContext == GetScriptContextFromJSContext(aJSCallerContext)) {
          mBlockScriptedClosingFlag = PR_TRUE;
          mContext->SetTerminationFunction(CloseBlockScriptTerminationFunc,
                                           NS_STATIC_CAST(nsPIDOMWindow*,
                                                          this));
        }
      }

      FireAbuseEvents(PR_TRUE, PR_FALSE, aUrl, aName, aOptions);
      return aDoJSFixups ? NS_OK : NS_ERROR_FAILURE;
    }
  }    

  nsCOMPtr<nsIDOMWindow> domReturn;

  nsCOMPtr<nsIWindowWatcher> wwatch =
    do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_TRUE(wwatch, rv);

  NS_ConvertUTF16toUTF8 options(aOptions);
  NS_ConvertUTF16toUTF8 name(aName);

  const char *options_ptr = aOptions.IsEmpty() ? nsnull : options.get();
  const char *name_ptr = aName.IsEmpty() ? nsnull : name.get();

  {
    // Reset popup state while opening a window to prevent the
    // current state from being active the whole time a modal
    // dialog is open.
    nsAutoPopupStatePusher popupStatePusher(openAbused, PR_TRUE);

    if (!aCalledNoScript) {
      nsCOMPtr<nsPIWindowWatcher> pwwatch(do_QueryInterface(wwatch));
      NS_ASSERTION(pwwatch,
                   "Unable to open windows from JS because window watcher "
                   "is broken");
      NS_ENSURE_TRUE(pwwatch, NS_ERROR_UNEXPECTED);
        
      rv = pwwatch->OpenWindowJS(this, url.get(), name_ptr, options_ptr,
                                 aDialog, argv,
                                 getter_AddRefs(domReturn));
    } else {
      // Push a null JSContext here so that the window watcher won't screw us
      // up.  We do NOT want this case looking at the JS context on the stack
      // when searching.  Compare comments on
      // nsIDOMWindowInternal::OpenWindow and nsIWindowWatcher::OpenWindow.
      nsCOMPtr<nsIJSContextStack> stack =
        do_GetService(sJSStackContractID);

      if (stack) {
        rv = stack->Push(nsnull);
        NS_ENSURE_SUCCESS(rv, rv);
      }
        
      rv = wwatch->OpenWindow(this, url.get(), name_ptr, options_ptr,
                              aExtraArgument, getter_AddRefs(domReturn));

      if (stack) {
        JSContext* cx;
        stack->Pop(&cx);
        NS_ASSERTION(!cx, "Unexpected JSContext popped!");
      }
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // success!

  domReturn.swap(*aReturn);

  if (aDoJSFixups) {      
    nsCOMPtr<nsIDOMChromeWindow> chrome_win(do_QueryInterface(*aReturn));
    if (!chrome_win) {
      // A new non-chrome window was created from a call to
      // window.open() from JavaScript, make sure there's a document in
      // the new window. We do this by simply asking the new window for
      // its document, this will synchronously create an empty document
      // if there is no document in the window.
      // XXXbz should this just use EnsureInnerWindow()?
#ifdef DEBUG_jst
      {
        nsCOMPtr<nsPIDOMWindow> pidomwin(do_QueryInterface(*aReturn));

        nsIDOMDocument *temp = pidomwin->GetExtantDocument();

        NS_ASSERTION(temp, "No document in new window!!!");
      }
#endif

      nsCOMPtr<nsIDOMDocument> doc;
      (*aReturn)->GetDocument(getter_AddRefs(doc));
    }
  }
    
  if (checkForPopup) {
    if (abuseLevel >= openControlled) {
      nsGlobalWindow *opened = NS_STATIC_CAST(nsGlobalWindow *, *aReturn);
      if (!opened->IsPopupSpamWindow()) {
        opened->SetPopupSpamWindow(PR_TRUE);
        ++gOpenPopupSpamCount;
      }
    }
    if (abuseLevel >= openAbused)
      FireAbuseEvents(PR_FALSE, PR_TRUE, aUrl, aName, aOptions);
  }

  // copy the session storage data over to the new window if
  // necessary.  If the new window has the same domain as this window
  // did at the beginning of this function, the session storage data
  // for that domain, and only that domain, is copied over.
  nsGlobalWindow *opened = NS_STATIC_CAST(nsGlobalWindow *, *aReturn);
  nsIDocShell* newDocShell = opened->GetDocShell();

  if (currentCodebase && newDocShell && mDocShell && url.get()) {
    nsCOMPtr<nsIURI> newURI;

    JSContext       *cx;
    PRBool           freePass;
    BuildURIfromBase(url, getter_AddRefs(newURI), &freePass, &cx);

    if (newURI) {
      nsCAutoString thisDomain, newDomain;
      nsresult gethostrv = currentCodebase->GetAsciiHost(thisDomain);
      gethostrv |= newURI->GetAsciiHost(newDomain);

      if (NS_SUCCEEDED(gethostrv) && thisDomain.Equals(newDomain)) {
        nsCOMPtr<nsIDOMStorage> storage;
        mDocShell->GetSessionStorageForURI(currentCodebase,
                                           getter_AddRefs(storage));
        nsCOMPtr<nsPIDOMStorage> piStorage = do_QueryInterface(storage);
        if (piStorage) {
          nsCOMPtr<nsIDOMStorage> newstorage = piStorage->Clone(newURI);
          newDocShell->AddSessionStorage(thisDomain, newstorage);
        }
      }
    }
  }
  
  return rv;
}

// static
void
nsGlobalWindow::CloseWindow(nsISupports *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));

  nsGlobalWindow* globalWin =
    NS_STATIC_CAST(nsGlobalWindow *,
                   NS_STATIC_CAST(nsPIDOMWindow*, win));

  // Need to post an event for closing, otherwise window and 
  // presshell etc. may get destroyed while creating frames, bug 338897.
  nsCOMPtr<nsIRunnable> ev = new nsCloseEvent(globalWin);
  if (ev) {
    NS_DispatchToCurrentThread(ev);
  }
  // else if OOM, better not to close. That might cause a crash.
}

// static
void
nsGlobalWindow::ClearWindowScope(nsISupports *aWindow)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aWindow));
  PRUint32 lang_id;
  NS_STID_FOR_ID(lang_id) {
    nsIScriptContext *scx = sgo->GetScriptContext(lang_id);
    if (scx) {
      void *global = sgo->GetScriptGlobal(lang_id);
      scx->ClearScope(global, PR_TRUE);
    }
  }
}

//*****************************************************************************
// nsGlobalWindow: Timeout Functions
//*****************************************************************************

nsresult
nsGlobalWindow::SetTimeoutOrInterval(nsIScriptTimeoutHandler *aHandler,
                                     PRInt32 interval,
                                     PRBool aIsInterval, PRInt32 *aReturn)
{
  FORWARD_TO_INNER(SetTimeoutOrInterval, (aHandler, interval, aIsInterval, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

  if (interval < DOM_MIN_TIMEOUT_VALUE) {
    // Don't allow timeouts less than DOM_MIN_TIMEOUT_VALUE from
    // now...

    interval = DOM_MIN_TIMEOUT_VALUE;
  }

  NS_ASSERTION(interval >= 0, "DOM_MIN_TIMEOUT_VALUE lies");
  PRUint32 realInterval = interval;

  // Make sure we don't proceed with a interval larger than our timer
  // code can handle.
  if (realInterval > PR_IntervalToMilliseconds(DOM_MAX_TIMEOUT_VALUE)) {
    realInterval = PR_IntervalToMilliseconds(DOM_MAX_TIMEOUT_VALUE);
  }

  nsTimeout *timeout = new nsTimeout();
  if (!timeout)
    return NS_ERROR_OUT_OF_MEMORY;

  // Increment the timeout's reference count to represent this function's hold
  // on the timeout.
  timeout->AddRef();

  if (aIsInterval) {
    timeout->mInterval = realInterval;
  }
  timeout->mScriptHandler = aHandler;

  // Get principal of currently executing code, save for execution of timeout.
  // If either our principals subsume the subject principal, or we're from the
  // same origin, then use the subject principal. Otherwise, use our principal
  // to avoid running script in elevated principals.

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  nsresult rv;
  rv = nsContentUtils::GetSecurityManager()->
    GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
  if (NS_FAILED(rv)) {
    timeout->Release();

    return NS_ERROR_FAILURE;
  }

  PRBool subsumes = PR_FALSE;
  nsCOMPtr<nsIPrincipal> ourPrincipal = GetPrincipal();

  // Note the direction of this test: We don't allow chrome setTimeouts on
  // content windows, but we do allow content setTimeouts on chrome windows.
  rv = ourPrincipal->Subsumes(subjectPrincipal, &subsumes);
  if (NS_FAILED(rv)) {
    timeout->Release();

    return NS_ERROR_FAILURE;
  }

  if (subsumes) {
    timeout->mPrincipal = subjectPrincipal;
  } else {
    // Subsumes does a very strict equality test. Allow sites of the same origin
    // to set timeouts on each other.

    rv = nsContentUtils::GetSecurityManager()->
      CheckSameOriginPrincipal(subjectPrincipal, ourPrincipal);
    timeout->mPrincipal = NS_SUCCEEDED(rv) ? subjectPrincipal : ourPrincipal;
    rv = NS_OK;
  }

  PRTime delta = (PRTime)realInterval * PR_USEC_PER_MSEC;

  if (!IsFrozen()) {
    // If we're not currently frozen, then we set timeout->mWhen to be the
    // actual firing time of the timer (i.e., now + delta). We also actually
    // create a timer and fire it off.

    timeout->mWhen = PR_Now() + delta;

    timeout->mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) {
      timeout->Release();

      return rv;
    }

    rv = timeout->mTimer->InitWithFuncCallback(TimerCallback, timeout,
                                               realInterval,
                                               nsITimer::TYPE_ONE_SHOT);
    if (NS_FAILED(rv)) {
      timeout->Release();

      return rv;
    }

    // The timeout is now also held in the timer's closure.
    timeout->AddRef();
  } else {
    // If we are frozen, however, then we instead simply set timeout->mWhen to
    // be the "time remaining" in the timeout (i.e., the interval itself). We
    // don't create a timer for it, since that will happen when we are thawed
    // and the timeout will then get a timer and run to completion.

    timeout->mWhen = delta;
  }

  timeout->mWindow = this;

  // No popups from timeouts by default
  timeout->mPopupState = openAbused;

  if (gRunningTimeoutDepth == 0 && gPopupControlState < openAbused) {
    // This timeout is *not* set from another timeout and it's set
    // while popups are enabled. Propagate the state to the timeout if
    // its delay (interval) is equal to or less than what
    // "dom.disable_open_click_delay" is set to (in ms).

    PRInt32 delay =
      nsContentUtils::GetIntPref("dom.disable_open_click_delay");

    if (interval <= delay) {
      timeout->mPopupState = gPopupControlState;
    }
  }

  InsertTimeoutIntoList(timeout);

  timeout->mPublicId = ++mTimeoutPublicIdCounter;
  *aReturn = timeout->mPublicId;

  // Our hold on the timeout is expiring. Note that this should not actually
  // free the timeout (since the list should have taken ownership as well).
  timeout->Release();

  return NS_OK;

}

nsresult
nsGlobalWindow::SetTimeoutOrInterval(PRBool aIsInterval, PRInt32 *aReturn)
{
  FORWARD_TO_INNER(SetTimeoutOrInterval, (aIsInterval, aReturn),
                   NS_ERROR_NOT_INITIALIZED);

  PRInt32 interval = 0;
  nsCOMPtr<nsIScriptTimeoutHandler> handler;
  nsresult rv = NS_CreateJSTimeoutHandler(GetContextInternal(),
                                          aIsInterval,
                                          &interval,
                                          getter_AddRefs(handler));
  if (NS_FAILED(rv))
    return (rv == NS_ERROR_DOM_TYPE_ERR) ? NS_OK : rv;

  return SetTimeoutOrInterval(handler, interval, aIsInterval, aReturn);
}

// static
void
nsGlobalWindow::RunTimeout(nsTimeout *aTimeout)
{
  // If a modal dialog is open for this window, return early. Pending
  // timeouts will run when the modal dialog is dismissed.
  if (IsInModalState()) {
    return;
  }

  NS_ASSERTION(IsInnerWindow(), "Timeout running on outer window!");
  NS_ASSERTION(!IsFrozen(), "Timeout running on a window in the bfcache!");

  nsTimeout *nextTimeout, *timeout;
  nsTimeout *last_expired_timeout, *last_insertion_point;
  nsTimeout dummy_timeout;
  PRUint32 firingDepth = mTimeoutFiringDepth + 1;

  // Make sure that the window and the script context don't go away as
  // a result of running timeouts
  nsCOMPtr<nsIScriptGlobalObject> windowKungFuDeathGrip(this);

  // A native timer has gone off. See which of our timeouts need
  // servicing
  PRTime now = PR_Now();
  PRTime deadline;

  if (aTimeout && aTimeout->mWhen > now) {
    // The OS timer fired early (yikes!), and possibly out of order
    // too. Set |deadline| to be the time when the OS timer *should*
    // have fired so that any timers that *should* have fired before
    // aTimeout *will* be fired now. This happens most of the time on
    // Win2k.

    deadline = aTimeout->mWhen;
  } else {
    deadline = now;
  }

  // The timeout list is kept in deadline order. Discover the latest
  // timeout whose deadline has expired. On some platforms, native
  // timeout events fire "early", so we need to test the timer as well
  // as the deadline.
  last_expired_timeout = nsnull;
  for (timeout = FirstTimeout(); IsTimeout(timeout); timeout = timeout->Next()) {
    if (((timeout == aTimeout) || (timeout->mWhen <= deadline)) &&
        (timeout->mFiringDepth == 0)) {
      // Mark any timeouts that are on the list to be fired with the
      // firing depth so that we can reentrantly run timeouts
      timeout->mFiringDepth = firingDepth;
      last_expired_timeout = timeout;
    }
  }

  // Maybe the timeout that the event was fired for has been deleted
  // and there are no others timeouts with deadlines that make them
  // eligible for execution yet. Go away.
  if (!last_expired_timeout) {
    return;
  }

  // Insert a dummy timeout into the list of timeouts between the
  // portion of the list that we are about to process now and those
  // timeouts that will be processed in a future call to
  // win_run_timeout(). This dummy timeout serves as the head of the
  // list for any timeouts inserted as a result of running a timeout.
  dummy_timeout.mFiringDepth = firingDepth;
  PR_INSERT_AFTER(&dummy_timeout, last_expired_timeout);

  // Don't let ClearWindowTimeouts throw away our stack-allocated
  // dummy timeout.
  dummy_timeout.AddRef();
  dummy_timeout.AddRef();

  last_insertion_point = mTimeoutInsertionPoint;
  mTimeoutInsertionPoint = &dummy_timeout;

  for (timeout = FirstTimeout();
       timeout != &dummy_timeout && !IsFrozen();
       timeout = nextTimeout) {
    nextTimeout = timeout->Next();

    if (timeout->mFiringDepth != firingDepth) {
      // We skip the timeout since it's on the list to run at another
      // depth.

      continue;
    }

    // The timeout is on the list to run at this depth, go ahead and
    // process it.

    // Get the script context (a strong ref to prevent it going away)
    // for this timeout and ensure the script language is enabled.
    nsCOMPtr<nsIScriptContext> scx = GetScriptContextInternal(
                                timeout->mScriptHandler->GetScriptTypeID());

    if (!scx) {
      // No context means this window was closed or never properly
      // initialized for this language.
      continue;
    }

    // The "scripts disabled" concept is still a little vague wrt
    // multiple languages.  Prepare for the day when languages can be
    // disabled independently of the other languages...
    if (!scx->GetScriptsEnabled()) {
      // Scripts were enabled once in this window (unless aTimeout ==
      // nsnull) but now scripts are disabled (we might be in
      // print-preview, for instance), this means we shouldn't run any
      // timeouts at this point.
      //
      // If scripts are enabled for this language in this window again
      // we'll fire the timeouts that are due at that point.
      continue;
    }

    // This timeout is good to run
    nsTimeout *last_running_timeout = mRunningTimeout;
    mRunningTimeout = timeout;
    timeout->mRunning = PR_TRUE;

    // Push this timeout's popup control state, which should only be
    // eabled the first time a timeout fires that was created while
    // popups were enabled and with a delay less than
    // "dom.disable_open_click_delay".
    nsAutoPopupStatePusher popupStatePusher(timeout->mPopupState);

    // Clear the timeout's popup state, if any, to prevent interval
    // timeouts from repeatedly opening poups.
    timeout->mPopupState = openAbused;

    // Hold on to the timeout in case mExpr or mFunObj releases its
    // doc.
    timeout->AddRef();

    ++gRunningTimeoutDepth;
    ++mTimeoutFiringDepth;

    nsCOMPtr<nsIScriptTimeoutHandler> handler(timeout->mScriptHandler);
    void *scriptObject = handler->GetScriptObject();
    if (!scriptObject) {
      // Evaluate the timeout expression.
      const PRUnichar *script = handler->GetHandlerText();
      NS_ASSERTION(script, "timeout has no script nor handler text!");

      const char *filename = nsnull;
      PRUint32 lineNo = 0;
      handler->GetLocation(&filename, &lineNo);

      PRBool is_undefined;
      scx->EvaluateString(nsDependentString(script), 
                          GetScriptGlobal(handler->GetScriptTypeID()),
                          timeout->mPrincipal, filename, lineNo,
                          handler->GetScriptVersion(), nsnull,
                          &is_undefined);
    } else {
      // Let the script handler know about the "secret" final argument that
      // indicates timeout lateness in milliseconds
      PRTime lateness = now - timeout->mWhen;

      // Make sure to cast the unsigned PR_USEC_PER_MSEC to signed
      // PRTime to make the division do the right thing on 64-bit
      // platforms whether lateness is positive or negative.
      handler->SetLateness((PRIntervalTime)(lateness /
                                            (PRTime)PR_USEC_PER_MSEC));

      nsCOMPtr<nsIVariant> dummy;
      nsCOMPtr<nsISupports> me(NS_STATIC_CAST(nsIDOMWindow *, this));
      scx->CallEventHandler(me,
                            GetScriptGlobal(handler->GetScriptTypeID()),
                            scriptObject, handler->GetArgv(),
                            // XXXmarkh - consider allowing CallEventHandler to
                            // accept nsnull?
                            getter_AddRefs(dummy));

    }
    handler = nsnull; // drop reference before dropping timeout refs.

    --mTimeoutFiringDepth;
    --gRunningTimeoutDepth;

    mRunningTimeout = last_running_timeout;
    timeout->mRunning = PR_FALSE;

    // We ignore any failures from calling EvaluateString() or
    // CallEventHandler() on the context here since we're in a loop
    // where we're likely to be running timeouts whose OS timers
    // didn't fire in time and we don't want to not fire those timers
    // now just because execution of one timer failed. We can't
    // propagate the error to anyone who cares about it from this
    // point anyway, and the script context should have already reported
    // the script error in the usual way - so we just drop it.

    // If all timeouts were cleared and |timeout != aTimeout| then
    // |timeout| may be the last reference to the timeout so check if
    // it was cleared before releasing it.
    PRBool timeout_was_cleared = timeout->mCleared;

    timeout->Release();

    if (timeout_was_cleared) {
      // The running timeout's window was cleared, this means that
      // ClearAllTimeouts() was called from a *nested* call, possibly
      // through a timeout that fired while a modal (to this window)
      // dialog was open or through other non-obvious paths.

      mTimeoutInsertionPoint = last_insertion_point;

      return;
    }

    PRBool isInterval = PR_FALSE;

    // If we have a regular interval timer, we re-schedule the
    // timeout, accounting for clock drift.
    if (timeout->mInterval) {
      // Compute time to next timeout for interval timer. If we're
      // running pending timeouts because they've been temporarily
      // disabled (!aTimeout), set the next interval to be relative to
      // "now", and not to when the timeout that was pending should
      // have fired.
      PRTime nextInterval = (aTimeout ? timeout->mWhen : now) +
        ((PRTime)timeout->mInterval * PR_USEC_PER_MSEC);
      PRTime delay = nextInterval - PR_Now();

      // If the next interval timeout is already supposed to have
      // happened then run the timeout as soon as we can (meaning
      // after DOM_MIN_TIMEOUT_VALUE time has passed).

      // Note: We must cast the rhs expression to PRTime to work
      // around what looks like a compiler bug on x86_64.
      if (delay < (PRTime)(DOM_MIN_TIMEOUT_VALUE * PR_USEC_PER_MSEC)) {
        delay = DOM_MIN_TIMEOUT_VALUE * PR_USEC_PER_MSEC;
      }

      if (timeout->mTimer) {
        timeout->mWhen = nextInterval;

        // Reschedule the OS timer. Don't bother returning any error
        // codes if this fails since the callers of this method
        // doesn't care about them nobody who cares about them
        // anyways.

        // Make sure to cast the unsigned PR_USEC_PER_MSEC to signed
        // PRTime to make the division do the right thing on 64-bit
        // platforms whether delay is positive or negative (which we
        // know is always positive here, but cast anyways for
        // consistency).
        nsresult rv = timeout->mTimer->
          InitWithFuncCallback(TimerCallback, timeout,
                               (PRInt32)(delay / (PRTime)PR_USEC_PER_MSEC),
                               nsITimer::TYPE_ONE_SHOT);

        if (NS_FAILED(rv)) {
          NS_ERROR("Error initializing timer for DOM timeout!");

          // We failed to initialize the new OS timer, this timer does
          // us no good here so we just cancel it (just in case) and
          // null out the pointer to the OS timer, this will release the
          // OS timer. As we continue executing the code below we'll end
          // up deleting the timeout since it's not an interval timeout
          // any more (since timeout->mTimer == nsnull).
          timeout->mTimer->Cancel();
          timeout->mTimer = nsnull;

          // Now that the OS timer no longer has a reference to the
          // timeout we need to drop that reference.
          timeout->Release();
        }
      } else {
        NS_ASSERTION(IsFrozen(), "How'd our timer end up null if we're not frozen?");

        timeout->mWhen = delay;
        isInterval = PR_TRUE;
      }
    }

    if (timeout->mTimer) {
      if (timeout->mInterval) {
        isInterval = PR_TRUE;
      } else {
        // The timeout still has an OS timer, and it's not an
        // interval, that means that the OS timer could still fire (if
        // it didn't already, i.e. aTimeout == timeout), cancel the OS
        // timer and release it's reference to the timeout.
        timeout->mTimer->Cancel();
        timeout->mTimer = nsnull;

        timeout->Release();
      }
    }

    // Running a timeout can cause another timeout to be deleted, so
    // we need to reset the pointer to the following timeout.
    nextTimeout = timeout->Next();

    PR_REMOVE_LINK(timeout);

    // Release the timeout struct since it's out of the list
    timeout->Release();

    if (isInterval) {
      // Reschedule an interval timeout. Insert interval timeout
      // onto list sorted in deadline order.

      InsertTimeoutIntoList(timeout);
    }
  }

  // Take the dummy timeout off the head of the list
  PR_REMOVE_LINK(&dummy_timeout);

  mTimeoutInsertionPoint = last_insertion_point;
}

nsrefcnt
nsTimeout::Release()
{
  if (--mRefCnt > 0)
    return mRefCnt;

  // language specific cleanup done as mScriptHandler destructs...

  // Kill the timer if it is still alive.
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  delete this;
  return 0;
}

nsrefcnt
nsTimeout::AddRef()
{
  return ++mRefCnt;
}


nsresult
nsGlobalWindow::ClearTimeoutOrInterval(PRInt32 aTimerID)
{
  FORWARD_TO_INNER(ClearTimeoutOrInterval, (aTimerID), NS_ERROR_NOT_INITIALIZED);

  PRUint32 public_id = (PRUint32)aTimerID;
  nsTimeout *timeout;

  for (timeout = FirstTimeout();
       IsTimeout(timeout);
       timeout = timeout->Next()) {
    if (timeout->mPublicId == public_id) {
      if (timeout->mRunning) {
        /* We're running from inside the timeout. Mark this
           timeout for deferred deletion by the code in
           RunTimeout() */
        timeout->mInterval = 0;
      }
      else {
        /* Delete the timeout from the pending timeout list */
        PR_REMOVE_LINK(timeout);

        if (timeout->mTimer) {
          timeout->mTimer->Cancel();
          timeout->mTimer = nsnull;
          timeout->Release();
        }
        timeout->Release();
      }
      break;
    }
  }

  return NS_OK;
}

// A JavaScript specific version.
nsresult
nsGlobalWindow::ClearTimeoutOrInterval()
{
  FORWARD_TO_INNER(ClearTimeoutOrInterval, (), NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc;

  ncc->GetArgc(&argc);

  if (argc < 1) {
    // No arguments, return early.

    return NS_OK;
  }

  jsval *argv = nsnull;

  ncc->GetArgvPtr(&argv);

  int32 timer_id;

  JSAutoRequest ar(cx);

  if (argv[0] == JSVAL_VOID || !::JS_ValueToInt32(cx, argv[0], &timer_id) ||
      timer_id <= 0) {
    // Undefined or non-positive number passed as argument, return
    // early. Make sure that JS_ValueToInt32 didn't set an exception.

    ::JS_ClearPendingException(cx);
    return NS_OK;
  }

  return ClearTimeoutOrInterval(timer_id);
}

void
nsGlobalWindow::ClearAllTimeouts()
{
  nsTimeout *timeout, *nextTimeout;

  for (timeout = FirstTimeout(); IsTimeout(timeout); timeout = nextTimeout) {
    /* If RunTimeout() is higher up on the stack for this
       window, e.g. as a result of document.write from a timeout,
       then we need to reset the list insertion point for
       newly-created timeouts in case the user adds a timeout,
       before we pop the stack back to RunTimeout. */
    if (mRunningTimeout == timeout)
      mTimeoutInsertionPoint = nsnull;

    nextTimeout = timeout->Next();

    if (timeout->mTimer) {
      timeout->mTimer->Cancel();
      timeout->mTimer = nsnull;

      // Drop the count since the timer isn't going to hold on
      // anymore.
      timeout->Release();
    }

    // Set timeout->mCleared to true to indicate that the timeout was
    // cleared and taken out of the list of timeouts
    timeout->mCleared = PR_TRUE;

    // Drop the count since we're removing it from the list.
    timeout->Release();
  }

  // Clear out our list
  PR_INIT_CLIST(&mTimeouts);
}

void
nsGlobalWindow::InsertTimeoutIntoList(nsTimeout *aTimeout)
{
  NS_ASSERTION(IsInnerWindow(),
               "InsertTimeoutIntoList() called on outer window!");

  // Start at mLastTimeout and go backwards.  Don't go further than
  // mTimeoutInsertionPoint, though.  This optimizes for the common case of
  // insertion at the end.
  nsTimeout* prevSibling;
  for (prevSibling = LastTimeout();
       IsTimeout(prevSibling) && prevSibling != mTimeoutInsertionPoint &&
         prevSibling->mWhen > aTimeout->mWhen;
       prevSibling = prevSibling->Prev()) {
    /* Do nothing; just searching */
  }

  // Now link in aTimeout after prevSibling.
  PR_INSERT_AFTER(aTimeout, prevSibling);

  aTimeout->mFiringDepth = 0;

  // Increment the timeout's reference count since it's now held on to
  // by the list
  aTimeout->AddRef();
}

// static
void
nsGlobalWindow::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsTimeout *timeout = (nsTimeout *)aClosure;

  // Hold on to the timeout to ensure it doesn't go away while it's
  // being handled (aka kungFuDeathGrip).
  timeout->AddRef();

  timeout->mWindow->RunTimeout(timeout);

  // Drop our reference to the timeout now that we're done with it.
  timeout->Release();
}

//*****************************************************************************
// nsGlobalWindow: Helper Functions
//*****************************************************************************

nsresult
nsGlobalWindow::GetTreeOwner(nsIDocShellTreeOwner **aTreeOwner)
{
  FORWARD_TO_OUTER(GetTreeOwner, (aTreeOwner), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));

  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.

  if (!docShellAsItem) {
    *aTreeOwner = nsnull;

    return NS_OK;
  }

  return docShellAsItem->GetTreeOwner(aTreeOwner);
}

nsresult
nsGlobalWindow::GetTreeOwner(nsIBaseWindow **aTreeOwner)
{
  FORWARD_TO_OUTER(GetTreeOwner, (aTreeOwner), NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;

  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.

  if (docShellAsItem) {
    docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  }

  if (!treeOwner) {
    *aTreeOwner = nsnull;
    return NS_OK;
  }

  return CallQueryInterface(treeOwner, aTreeOwner);
}

nsresult
nsGlobalWindow::GetWebBrowserChrome(nsIWebBrowserChrome **aBrowserChrome)
{
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
  NS_IF_ADDREF(*aBrowserChrome = browserChrome);

  return NS_OK;
}

nsresult
nsGlobalWindow::GetScrollInfo(nsIScrollableView **aScrollableView)
{
  FORWARD_TO_OUTER(GetScrollInfo, (aScrollableView),
                   NS_ERROR_NOT_INITIALIZED);

  *aScrollableView = nsnull;

  if (!mDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (presContext) {
    nsIViewManager* vm = presContext->GetViewManager();
    if (vm)
      return vm->GetRootScrollableView(aScrollableView);
  }
  return NS_OK;
}

nsresult
nsGlobalWindow::BuildURIfromBase(const char *aURL, nsIURI **aBuiltURI,
                                 PRBool *aFreeSecurityPass,
                                 JSContext **aCXused)
{
  nsIScriptContext *scx = GetContextInternal();
  JSContext *cx = nsnull;

  *aBuiltURI = nsnull;
  *aFreeSecurityPass = PR_FALSE;
  if (aCXused)
    *aCXused = nsnull;

  // get JSContext
  NS_ASSERTION(scx, "opening window missing its context");
  NS_ASSERTION(mDocument, "opening window missing its document");
  if (!scx || !mDocument)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMChromeWindow> chrome_win =
    do_QueryInterface(NS_STATIC_CAST(nsIDOMWindow *, this));

  if (nsContentUtils::IsCallerChrome() && !chrome_win) {
    // If open() is called from chrome on a non-chrome window, we'll
    // use the context from the window on which open() is being called
    // to prevent giving chrome priveleges to new windows opened in
    // such a way. This also makes us get the appropriate base URI for
    // the below URI resolution code.

    cx = (JSContext *)scx->GetNativeContext();
  } else {
    // get the JSContext from the call stack
    nsCOMPtr<nsIThreadJSContextStack> stack(do_GetService(sJSStackContractID));
    if (stack)
      stack->Peek(&cx);
  }

  /* resolve the URI, which could be relative to the calling window
     (note the algorithm to get the base URI should match the one
     used to actually kick off the load in nsWindowWatcher.cpp). */
  nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8")); // default to utf-8
  nsIURI* baseURI = nsnull;
  nsCOMPtr<nsIURI> uriToLoad;
  nsCOMPtr<nsIDOMWindow> sourceWindow;

  if (cx) {
    nsIScriptContext *scriptcx = nsJSUtils::GetDynamicScriptContext(cx);
    if (scriptcx)
      sourceWindow = do_QueryInterface(scriptcx->GetGlobalObject());
  }

  if (!sourceWindow) {
    sourceWindow = do_QueryInterface(NS_ISUPPORTS_CAST(nsIDOMWindow *, this));
    *aFreeSecurityPass = PR_TRUE;
  }

  if (sourceWindow) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    sourceWindow->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
    if (doc) {
      baseURI = doc->GetBaseURI();
      charset = doc->GetDocumentCharacterSet();
    }
  }

  if (aCXused)
    *aCXused = cx;
  return NS_NewURI(aBuiltURI, nsDependentCString(aURL), charset.get(), baseURI);
}

nsresult
nsGlobalWindow::SecurityCheckURL(const char *aURL)
{
  JSContext       *cx;
  PRBool           freePass;
  nsCOMPtr<nsIURI> uri;

  if (NS_FAILED(BuildURIfromBase(aURL, getter_AddRefs(uri), &freePass, &cx)))
    return NS_ERROR_FAILURE;

  if (!freePass && NS_FAILED(nsContentUtils::GetSecurityManager()->
        CheckLoadURIFromScript(cx, uri)))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

void
nsGlobalWindow::FlushPendingNotifications(mozFlushType aType)
{
  if (mDoc) {
    mDoc->FlushPendingNotifications(aType);
  }
}

void
nsGlobalWindow::EnsureSizeUpToDate()
{
  // If we're a subframe, make sure our size is up to date.  It's OK that this
  // crosses the content/chrome boundary, since chrome can have pending reflows
  // too.
  nsGlobalWindow *parent =
    NS_STATIC_CAST(nsGlobalWindow *, GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(Flush_Layout);
  }
}

nsresult
nsGlobalWindow::SaveWindowState(nsISupports **aState)
{
  NS_PRECONDITION(IsOuterWindow(), "Can't save the inner window's state");

  *aState = nsnull;

  if (!mContext || !mJSObject) {
    // The window may be getting torn down; don't bother saving state.
    return NS_OK;
  }

  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();
  NS_ASSERTION(inner, "No inner window to save");

  // Don't do anything else to this inner window! After this point, all
  // calls to SetTimeoutOrInterval will create entries in the timeout
  // list that will only run after this window has come out of the bfcache.
  // Also, while we're frozen, we won't dispatch online/offline events
  // to the page.
  inner->Freeze();

  // Remember the outer window's XPConnect prototype.
  nsCOMPtr<nsIClassInfo> ci =
    do_QueryInterface((nsIScriptGlobalObject *)this);
  nsCOMPtr<nsIXPConnectJSObjectHolder> proto;
  nsresult rv = nsContentUtils::XPConnect()->
    GetWrappedNativePrototype((JSContext *)mContext->GetNativeContext(),
                              mJSObject, ci, getter_AddRefs(proto));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISupports> state = new WindowStateHolder(inner,
                                                      mInnerWindowHolders,
                                                      mNavigator,
                                                      mLocation,
                                                      proto);
  NS_ENSURE_TRUE(state, NS_ERROR_OUT_OF_MEMORY);

#ifdef DEBUG_PAGE_CACHE
  printf("saving window state, state = %p\n", (void*)state);
#endif

  state.swap(*aState);
  return NS_OK;
}

nsresult
nsGlobalWindow::RestoreWindowState(nsISupports *aState)
{
  NS_ASSERTION(IsOuterWindow(), "Cannot restore an inner window");

  if (!mContext || !mJSObject) {
    // The window may be getting torn down; don't bother restoring state.
    return NS_OK;
  }

  nsCOMPtr<WindowStateHolder> holder = do_QueryInterface(aState);
  NS_ENSURE_TRUE(holder, NS_ERROR_FAILURE);

#ifdef DEBUG_PAGE_CACHE
  printf("restoring window state, state = %p\n", (void*)holder);
#endif

  nsGlobalWindow *inner = GetCurrentInnerWindowInternal();

  nsIDOMElement *focusedElement = holder->GetFocusedElement();
  nsIDOMWindowInternal *focusedWindow = holder->GetFocusedWindow();

  // If the toplevel window isn't focused, just update the focus controller.
  nsIFocusController *fc = nsGlobalWindow::GetRootFocusController();
  NS_ENSURE_TRUE(fc, NS_ERROR_UNEXPECTED);

  PRBool active;
  fc->GetActive(&active);
  if (active) {
    PRBool didFocusContent = PR_FALSE;
    nsCOMPtr<nsIContent> focusedContent = do_QueryInterface(focusedElement);

    if (focusedContent) {
      // We don't bother checking whether the element or frame is focusable.
      // If it was focusable when we stored the presentation, it must be
      // focusable now.
      nsIDocument *doc = focusedContent->GetCurrentDoc();
      if (doc) {
        nsIPresShell *shell = doc->GetShellAt(0);
        if (shell) {
          nsPresContext *pc = shell->GetPresContext();
          if (pc) {
            pc->EventStateManager()->SetContentState(focusedContent,
                                                     NS_EVENT_STATE_FOCUS);
            didFocusContent = PR_TRUE;
          }
        }
      }
    }

    if (!didFocusContent && focusedWindow) {
      // Clear the focus controller's memory of any focused element so that
      // the element does not get inadvertently focused again.
      fc->ResetElementFocus();

      focusedWindow->Focus();
    }
  } else if (focusedWindow) {
    // Just update the saved focus memory.
    fc->SetFocusedWindow(focusedWindow);
    fc->SetFocusedElement(focusedElement);
  }

  // And we're ready to go!
  inner->Thaw();

  holder->DidRestoreWindow();

  return NS_OK;
}

void
nsGlobalWindow::SuspendTimeouts()
{
  FORWARD_TO_INNER_VOID(SuspendTimeouts, ());

  PRTime now = PR_Now();
  for (nsTimeout *t = FirstTimeout(); IsTimeout(t); t = t->Next()) {
    // Change mWhen to be the time remaining for this timer.    
    if (t->mWhen > now)
      t->mWhen -= now;
    else
      t->mWhen = 0;

    // Drop the XPCOM timer; we'll reschedule when restoring the state.
    if (t->mTimer) {
      t->mTimer->Cancel();
      t->mTimer = nsnull;

      // Drop the reference that the timer's closure had on this timeout, we'll
      // add it back in ResumeTimeouts. Note that it shouldn't matter that we're
      // passing null for the context, since this shouldn't actually release this
      // timeout.
      t->Release();
    }
  }

  // Suspend our children as well.
  nsCOMPtr<nsIDocShellTreeNode> node(do_QueryInterface(GetDocShell()));
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      NS_ASSERTION(childShell, "null child shell");

      nsCOMPtr<nsPIDOMWindow> pWin = do_GetInterface(childShell);
      if (pWin) {
        nsGlobalWindow *win =
          NS_STATIC_CAST(nsGlobalWindow*,
                         NS_STATIC_CAST(nsPIDOMWindow*, pWin));

        win->SuspendTimeouts();

        NS_ASSERTION(win->IsOuterWindow(), "Expected outer window");
        nsGlobalWindow* inner = win->GetCurrentInnerWindowInternal();
        if (inner) {
          inner->Freeze();
        }
      }
    }
  }
}

nsresult
nsGlobalWindow::ResumeTimeouts()
{
  FORWARD_TO_INNER(ResumeTimeouts, (), NS_ERROR_NOT_INITIALIZED);

  // Restore all of the timeouts, using the stored time remaining
  // (stored in timeout->mWhen).

  PRTime now = PR_Now();
  nsresult rv;

  for (nsTimeout *t = FirstTimeout(); IsTimeout(t); t = t->Next()) {
    // Make sure to cast the unsigned PR_USEC_PER_MSEC to signed
    // PRTime to make the division do the right thing on 64-bit
    // platforms whether t->mWhen is positive or negative (which is
    // likely to always be positive here, but cast anyways for
    // consistency).
    PRUint32 delay =
      PR_MAX(((PRUint32)(t->mWhen / (PRTime)PR_USEC_PER_MSEC)),
              DOM_MIN_TIMEOUT_VALUE);

    // Set mWhen back to the time when the timer is supposed to
    // fire.
    t->mWhen += now;

    t->mTimer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ENSURE_TRUE(t->mTimer, NS_ERROR_OUT_OF_MEMORY);

    rv = t->mTimer->InitWithFuncCallback(TimerCallback, t, delay,
                                         nsITimer::TYPE_ONE_SHOT);
    if (NS_FAILED(rv)) {
      t->mTimer = nsnull;
      return rv;
    }

    // Add a reference for the new timer's closure.
    t->AddRef();
  }

  // Resume our children as well.
  nsCOMPtr<nsIDocShellTreeNode> node =
    do_QueryInterface(GetDocShell());
  if (node) {
    PRInt32 childCount = 0;
    node->GetChildCount(&childCount);

    for (PRInt32 i = 0; i < childCount; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> childShell;
      node->GetChildAt(i, getter_AddRefs(childShell));
      NS_ASSERTION(childShell, "null child shell");

      nsCOMPtr<nsPIDOMWindow> pWin = do_GetInterface(childShell);
      if (pWin) {
        nsGlobalWindow *win =
          NS_STATIC_CAST(nsGlobalWindow*,
                         NS_STATIC_CAST(nsPIDOMWindow*, pWin));

        NS_ASSERTION(win->IsOuterWindow(), "Expected outer window");
        nsGlobalWindow* inner = win->GetCurrentInnerWindowInternal();
        if (inner) {
          inner->Thaw();
        }

        rv = win->ResumeTimeouts();
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalWindow::GetScriptTypeID(PRUint32 *aScriptType)
{
    NS_ERROR("No default script type here - ask some element");
    return nsIProgrammingLanguage::UNKNOWN;
}

NS_IMETHODIMP
nsGlobalWindow::SetScriptTypeID(PRUint32 aScriptType)
{
    NS_ERROR("Can't change default script type for a document");
    return NS_ERROR_NOT_IMPLEMENTED;
}

// QueryInterface implementation for nsGlobalChromeWindow
NS_INTERFACE_MAP_BEGIN(nsGlobalChromeWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMChromeWindow)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ChromeWindow)
NS_INTERFACE_MAP_END_INHERITING(nsGlobalWindow)

NS_IMPL_ADDREF_INHERITED(nsGlobalChromeWindow, nsGlobalWindow)
NS_IMPL_RELEASE_INHERITED(nsGlobalChromeWindow, nsGlobalWindow)

// nsGlobalChromeWindow implementation

static void TitleConsoleWarning()
{
  nsCOMPtr<nsIConsoleService> console(do_GetService("@mozilla.org/consoleservice;1"));
  if (console)
    console->LogStringMessage(NS_LITERAL_STRING("Deprecated property window.title used.  Please use document.title instead.").get());
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetTitle(nsAString& aTitle)
{
  NS_ERROR("nsIDOMChromeWindow::GetTitle is deprecated, use nsIDOMNSDocument instead");
  TitleConsoleWarning();

  nsresult rv;
  nsCOMPtr<nsIDOMNSDocument> nsdoc(do_QueryInterface(mDocument, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return nsdoc->GetTitle(aTitle);
}

NS_IMETHODIMP
nsGlobalChromeWindow::SetTitle(const nsAString& aTitle)
{
  NS_ERROR("nsIDOMChromeWindow::SetTitle is deprecated, use nsIDOMNSDocument instead");
  TitleConsoleWarning();

  nsresult rv;
  nsCOMPtr<nsIDOMNSDocument> nsdoc(do_QueryInterface(mDocument, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return nsdoc->SetTitle(aTitle);
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetWindowState(PRUint16* aWindowState)
{
  *aWindowState = nsIDOMChromeWindow::STATE_NORMAL;

  nsCOMPtr<nsIWidget> widget = GetMainWidget();

  PRInt32 aMode = 0;

  if (widget) {
    nsresult rv = widget->GetSizeMode(&aMode);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  switch (aMode) {
    case nsSizeMode_Minimized:
      *aWindowState = nsIDOMChromeWindow::STATE_MINIMIZED;
      break;
    case nsSizeMode_Maximized:
      *aWindowState = nsIDOMChromeWindow::STATE_MAXIMIZED;
      break;
    case nsSizeMode_Normal:
      *aWindowState = nsIDOMChromeWindow::STATE_NORMAL;
      break;
    default:
      NS_WARNING("Illegal window state for this chrome window");
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Maximize()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    rv = widget->SetSizeMode(nsSizeMode_Maximized);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Minimize()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    // minimize doesn't send deactivate events on windows,
    // so we need to forcefully restore the os chrome
    nsCOMPtr<nsIFullScreen> fullScreen =
      do_GetService("@mozilla.org/browser/fullscreen;1");
    if (fullScreen)
      fullScreen->ShowAllOSChrome();

    rv = widget->SetSizeMode(nsSizeMode_Minimized);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Restore()
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    rv = widget->SetSizeMode(nsSizeMode_Normal);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttention()
{
  return GetAttentionWithCycleCount(-1);
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetAttentionWithCycleCount(PRInt32 aCycleCount)
{
  nsCOMPtr<nsIWidget> widget = GetMainWidget();
  nsresult rv = NS_OK;

  if (widget) {
    rv = widget->GetAttention(aCycleCount);
  }

  return rv;
}

//Note: This call will lock the cursor, it will not change as it moves.
//To unlock, the cursor must be set back to CURSOR_AUTO.
NS_IMETHODIMP
nsGlobalChromeWindow::SetCursor(const nsAString& aCursor)
{
  FORWARD_TO_OUTER_CHROME(SetCursor, (aCursor), NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  PRInt32 cursor;

  // use C strings to keep the code/data size down
  NS_ConvertUTF16toUTF8 cursorString(aCursor);

  if (cursorString.Equals("auto"))
    cursor = NS_STYLE_CURSOR_AUTO;
  else {
    nsCSSKeyword keyword = nsCSSKeywords::LookupKeyword(aCursor);
    if (eCSSKeyword_UNKNOWN == keyword ||
        !nsCSSProps::FindKeyword(keyword, nsCSSProps::kCursorKTable, cursor)) {
      // XXX remove the following three values (leave return NS_OK) after 1.8
      // XXX since they should have been -moz- prefixed (covered by FindKeyword).
      // XXX (also remove |cursorString| at that point?).
      if (cursorString.Equals("grab"))
        cursor = NS_STYLE_CURSOR_GRAB;
      else if (cursorString.Equals("grabbing"))
        cursor = NS_STYLE_CURSOR_GRABBING;
      else if (cursorString.Equals("spinning"))
        cursor = NS_STYLE_CURSOR_SPINNING;
      else
        return NS_OK;
    }
  }

  nsCOMPtr<nsPresContext> presContext;
  if (mDocShell) {
    mDocShell->GetPresContext(getter_AddRefs(presContext));
  }

  if (presContext) {
    // Need root widget.
    nsCOMPtr<nsIPresShell> presShell;
    mDocShell->GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    nsIViewManager* vm = presShell->GetViewManager();
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

    nsIView *rootView;
    vm->GetRootView(rootView);
    NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

    nsIWidget* widget = rootView->GetWidget();
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

    // Call esm and set cursor.
    rv = presContext->EventStateManager()->SetCursor(cursor, nsnull,
                                                     PR_FALSE, 0.0f, 0.0f,
                                                     widget, PR_TRUE);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetBrowserDOMWindow(nsIBrowserDOMWindow **aBrowserWindow)
{
  FORWARD_TO_OUTER_CHROME(GetBrowserDOMWindow, (aBrowserWindow),
                          NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aBrowserWindow);

  *aBrowserWindow = mBrowserDOMWindow;
  NS_IF_ADDREF(*aBrowserWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::SetBrowserDOMWindow(nsIBrowserDOMWindow *aBrowserWindow)
{
  FORWARD_TO_OUTER_CHROME(SetBrowserDOMWindow, (aBrowserWindow),
                          NS_ERROR_NOT_INITIALIZED);

  mBrowserDOMWindow = aBrowserWindow;
  return NS_OK;
}

//*****************************************************************************
// nsGlobalWindow: Creator Function (This should go away)
//*****************************************************************************

nsresult
NS_NewScriptGlobalObject(PRBool aIsChrome, nsIScriptGlobalObject **aResult)
{
  *aResult = nsnull;

  nsGlobalWindow *global;

  if (aIsChrome) {
    global = new nsGlobalChromeWindow(nsnull);
  } else {
    global = new nsGlobalWindow(nsnull);
  }

  NS_ENSURE_TRUE(global, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(NS_STATIC_CAST(nsIScriptGlobalObject *, global),
                            aResult);
}

//*****************************************************************************
//***    nsNavigator: Object Management
//*****************************************************************************

nsNavigator::nsNavigator(nsIDocShell *aDocShell)
  : mDocShell(aDocShell)
{
}

nsNavigator::~nsNavigator()
{
  sPrefInternal_id = JSVAL_VOID;
}

//*****************************************************************************
//    nsNavigator::nsISupports
//*****************************************************************************


// QueryInterface implementation for nsNavigator
NS_INTERFACE_MAP_BEGIN(nsNavigator)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMJSNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMClientInformation)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Navigator)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsNavigator)
NS_IMPL_RELEASE(nsNavigator)


void
nsNavigator::SetDocShell(nsIDocShell *aDocShell)
{
  mDocShell = aDocShell;
  if (mPlugins)
    mPlugins->SetDocShell(aDocShell);
}

//*****************************************************************************
//    nsNavigator::nsIDOMNavigator
//*****************************************************************************

NS_IMETHODIMP
nsNavigator::GetUserAgent(nsAString& aUserAgent)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString ua;
    rv = service->GetUserAgent(ua);
    CopyASCIItoUTF16(ua, aUserAgent);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetAppCodeName(nsAString& aAppCodeName)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString appName;
    rv = service->GetAppName(appName);
    CopyASCIItoUTF16(appName, aAppCodeName);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetAppVersion(nsAString& aAppVersion)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingCString& override = 
      nsContentUtils::GetCharPref("general.appversion.override");

    if (override) {
      CopyUTF8toUTF16(override, aAppVersion);
      return NS_OK;
    }
  }

  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString str;
    rv = service->GetAppVersion(str);
    CopyASCIItoUTF16(str, aAppVersion);
    if (NS_FAILED(rv))
      return rv;

    aAppVersion.AppendLiteral(" (");

    rv = service->GetPlatform(str);
    if (NS_FAILED(rv))
      return rv;

    AppendASCIItoUTF16(str, aAppVersion);

    aAppVersion.AppendLiteral("; ");

    rv = service->GetLanguage(str);
    if (NS_FAILED(rv))
      return rv;
    AppendASCIItoUTF16(str, aAppVersion);

    aAppVersion.Append(PRUnichar(')'));
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetAppName(nsAString& aAppName)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingCString& override =
      nsContentUtils::GetCharPref("general.appname.override");

    if (override) {
      CopyUTF8toUTF16(override, aAppName);
      return NS_OK;
    }
  }

  aAppName.AssignLiteral("Netscape");
  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetLanguage(nsAString& aLanguage)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString lang;
    rv = service->GetLanguage(lang);
    CopyASCIItoUTF16(lang, aLanguage);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetPlatform(nsAString& aPlatform)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingCString& override =
      nsContentUtils::GetCharPref("general.platform.override");

    if (override) {
      CopyUTF8toUTF16(override, aPlatform);
      return NS_OK;
    }
  }

  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    // sorry for the #if platform ugliness, but Communicator is
    // likewise hardcoded and we're seeking backward compatibility
    // here (bug 47080)
#if defined(WIN32)
    aPlatform.AssignLiteral("Win32");
#elif defined(XP_MACOSX) && defined(__ppc__)
    aPlatform.AssignLiteral("MacPPC");
#elif defined(XP_MACOSX) && defined(__i386__)
    aPlatform.AssignLiteral("MacIntel");
#elif defined(XP_OS2)
    aPlatform.AssignLiteral("OS/2");
#else
    // XXX Communicator uses compiled-in build-time string defines
    // to indicate the platform it was compiled *for*, not what it is
    // currently running *on* which is what this does.
    nsCAutoString plat;
    rv = service->GetOscpu(plat);
    CopyASCIItoUTF16(plat, aPlatform);
#endif
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetOscpu(nsAString& aOSCPU)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString oscpu;
    rv = service->GetOscpu(oscpu);
    CopyASCIItoUTF16(oscpu, aOSCPU);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetVendor(nsAString& aVendor)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString vendor;
    rv = service->GetVendor(vendor);
    CopyASCIItoUTF16(vendor, aVendor);
  }

  return rv;
}


NS_IMETHODIMP
nsNavigator::GetVendorSub(nsAString& aVendorSub)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString vendor;
    rv = service->GetVendorSub(vendor);
    CopyASCIItoUTF16(vendor, aVendorSub);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetProduct(nsAString& aProduct)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString product;
    rv = service->GetProduct(product);
    CopyASCIItoUTF16(product, aProduct);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetProductSub(nsAString& aProductSub)
{
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString productSub;
    rv = service->GetProductSub(productSub);
    CopyASCIItoUTF16(productSub, aProductSub);
  }

  return rv;
}

NS_IMETHODIMP
nsNavigator::GetSecurityPolicy(nsAString& aSecurityPolicy)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetMimeTypes(nsIDOMMimeTypeArray **aMimeTypes)
{
  if (!mMimeTypes) {
    mMimeTypes = new nsMimeTypeArray(this);
    if (!mMimeTypes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aMimeTypes = mMimeTypes);

  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetPlugins(nsIDOMPluginArray **aPlugins)
{
  if (!mPlugins) {
    mPlugins = new nsPluginArray(this, mDocShell);
    if (!mPlugins) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aPlugins = mPlugins);

  return NS_OK;
}

// values for the network.cookie.cookieBehavior pref are documented in
// nsCookieService.cpp.
#define COOKIE_BEHAVIOR_REJECT 2

NS_IMETHODIMP
nsNavigator::GetCookieEnabled(PRBool *aCookieEnabled)
{
  *aCookieEnabled =
    (nsContentUtils::GetIntPref("network.cookie.cookieBehavior",
                                COOKIE_BEHAVIOR_REJECT) !=
     COOKIE_BEHAVIOR_REJECT);

  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetOnLine(PRBool* aOnline)
{
  NS_PRECONDITION(aOnline, "Null out param");
  
  *aOnline = !NS_IsOffline();
  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::GetBuildID(nsAString& aBuildID)
{
  aBuildID = NS_LITERAL_STRING(NS_STRINGIFY(NS_BUILD_ID));

  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::JavaEnabled(PRBool *aReturn)
{
  nsresult rv = NS_OK;
  *aReturn = PR_FALSE;

#ifdef OJI
  // determine whether user has enabled java.
  // if pref doesn't exist, map result to false.
  *aReturn = nsContentUtils::GetBoolPref("security.enable_java");

  // if Java is not enabled, result is false and return reight away
  if (!*aReturn)
    return NS_OK;

  // Ask the nsIJVMManager if Java is enabled
  nsCOMPtr<nsIJVMManager> jvmService = do_GetService(kJVMServiceCID);
  if (jvmService) {
    jvmService->GetJavaEnabled(aReturn);
  }
  else {
    *aReturn = PR_FALSE;
  }
#endif
  return rv;
}

NS_IMETHODIMP
nsNavigator::TaintEnabled(PRBool *aReturn)
{
  *aReturn = PR_FALSE;
  return NS_OK;
}

jsval
nsNavigator::sPrefInternal_id = JSVAL_VOID;

NS_IMETHODIMP
nsNavigator::Preference()
{
  nsCOMPtr<nsIXPCNativeCallContext> ncc;
  nsresult rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  PRUint32 argc;

  ncc->GetArgc(&argc);

  if (argc == 0) {
    // No arguments means there's nothing to be done here.

    return NS_OK;
  }

  jsval *argv = nsnull;

  ncc->GetArgvPtr(&argv);
  NS_ENSURE_TRUE(argv, NS_ERROR_UNEXPECTED);

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  //--Check to see if the caller is allowed to access prefs
  if (sPrefInternal_id == JSVAL_VOID) {
    sPrefInternal_id =
      STRING_TO_JSVAL(::JS_InternString(cx, "preferenceinternal"));
  }

  PRUint32 action;
  if (argc == 1) {
    action = nsIXPCSecurityManager::ACCESS_GET_PROPERTY;
  } else {
    action = nsIXPCSecurityManager::ACCESS_SET_PROPERTY;
  }

  rv = nsContentUtils::GetSecurityManager()->
    CheckPropertyAccess(cx, nsnull, "Navigator", sPrefInternal_id, action);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  nsIPrefBranch *prefBranch = nsContentUtils::GetPrefBranch();
  NS_ENSURE_STATE(prefBranch);

  JSString *str = ::JS_ValueToString(cx, argv[0]);
  NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

  jsval *retval = nsnull;

  rv = ncc->GetRetValPtr(&retval);
  NS_ENSURE_SUCCESS(rv, rv);

  char *prefStr = ::JS_GetStringBytes(str);
  if (argc == 1) {
    PRInt32 prefType;

    prefBranch->GetPrefType(prefStr, &prefType);

    switch (prefType) {
    case nsIPrefBranch::PREF_STRING:
      {
        nsXPIDLCString prefCharVal;
        rv = prefBranch->GetCharPref(prefStr, getter_Copies(prefCharVal));
        NS_ENSURE_SUCCESS(rv, rv);

        JSString *retStr = ::JS_NewStringCopyZ(cx, prefCharVal);
        NS_ENSURE_TRUE(retStr, NS_ERROR_OUT_OF_MEMORY);

        *retval = STRING_TO_JSVAL(retStr);

        break;
      }

    case nsIPrefBranch::PREF_INT:
      {
        PRInt32 prefIntVal;
        rv = prefBranch->GetIntPref(prefStr, &prefIntVal);
        NS_ENSURE_SUCCESS(rv, rv);

        *retval = INT_TO_JSVAL(prefIntVal);

        break;
      }

    case nsIPrefBranch::PREF_BOOL:
      {
        PRBool prefBoolVal;

        rv = prefBranch->GetBoolPref(prefStr, &prefBoolVal);
        NS_ENSURE_SUCCESS(rv, rv);

        *retval = BOOLEAN_TO_JSVAL(prefBoolVal);

        break;
      }
    default:
      {
        // Nothing we can do here...

        return ncc->SetReturnValueWasSet(PR_FALSE);
      }
    }

    ncc->SetReturnValueWasSet(PR_TRUE);
  } else {
    if (JSVAL_IS_STRING(argv[1])) {
      JSString *valueJSStr = ::JS_ValueToString(cx, argv[1]);
      NS_ENSURE_TRUE(valueJSStr, NS_ERROR_OUT_OF_MEMORY);

      rv = prefBranch->SetCharPref(prefStr, ::JS_GetStringBytes(valueJSStr));
    } else if (JSVAL_IS_INT(argv[1])) {
      jsint valueInt = JSVAL_TO_INT(argv[1]);

      rv = prefBranch->SetIntPref(prefStr, (PRInt32)valueInt);
    } else if (JSVAL_IS_BOOLEAN(argv[1])) {
      JSBool valueBool = JSVAL_TO_BOOLEAN(argv[1]);

      rv = prefBranch->SetBoolPref(prefStr, (PRBool)valueBool);
    } else if (JSVAL_IS_NULL(argv[1])) {
      rv = prefBranch->DeleteBranch(prefStr);
    }
  }

  return rv;
}

void
nsNavigator::LoadingNewDocument()
{
  // Release these so that they will be recreated for the
  // new document (if requested).  The plugins or mime types
  // arrays may have changed.  See bug 150087.
  mMimeTypes = nsnull;
  mPlugins = nsnull;
}

nsresult
nsNavigator::RefreshMIMEArray()
{
  nsresult rv = NS_OK;
  if (mMimeTypes)
    rv = mMimeTypes->Refresh();
  return rv;
}

//*****************************************************************************
//    nsNavigator::nsIDOMClientInformation
//*****************************************************************************

NS_IMETHODIMP
nsNavigator::RegisterContentHandler(const nsAString& aMIMEType, 
                                    const nsAString& aURI, 
                                    const nsAString& aTitle)
{
  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar = 
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (registrar && mDocShell) {
    nsCOMPtr<nsIDOMWindow> contentDOMWindow(do_GetInterface(mDocShell));
    if (contentDOMWindow)
      return registrar->RegisterContentHandler(aMIMEType, aURI, aTitle,
                                               contentDOMWindow);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavigator::RegisterProtocolHandler(const nsAString& aProtocol, 
                                     const nsAString& aURI, 
                                     const nsAString& aTitle)
{
  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar = 
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (registrar && mDocShell) {
    nsCOMPtr<nsIDOMWindow> contentDOMWindow(do_GetInterface(mDocShell));
    if (contentDOMWindow)
      return registrar->RegisterProtocolHandler(aProtocol, aURI, aTitle,
                                                contentDOMWindow);
  }

  return NS_OK;
}
