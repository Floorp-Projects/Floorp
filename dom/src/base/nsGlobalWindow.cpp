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
 *   Brendan Eich <brendan@mozilla.org>
 *   David Hyatt (hyatt@netscape.com)
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

// Local Includes
#include "nsGlobalWindow.h"
#include "nsScreen.h"
#include "nsHistory.h"
#include "nsBarProps.h"

// Helper Classes
#include "nsXPIDLString.h"
#include "nsJSUtils.h"
#include "prmem.h"
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

// Interfaces Needed
#include "nsIWidget.h"
#include "nsIBaseWindow.h"
#include "nsICharsetConverterManager.h"
#include "nsIContent.h"
#include "nsIWebBrowserPrint.h"
#include "nsIContentViewerEdit.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocCharset.h"
#include "nsIDocument.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMPopupBlockedEvent.h"
#include "nsIDOMPkcs11.h"
#include "nsDOMString.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsIEventQueueService.h"
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
#include "nsISelectionController.h"
#include "nsISelection.h"
#include "nsIFrameSelection.h"
#include "nsIPrompt.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserFind.h"  // For window.find()
#include "nsIWebShell.h"
#include "nsIWindowMediator.h"  // For window.find()
#include "nsIComputedDOMStyle.h"
#include "nsIEntropyCollector.h"
#include "nsDOMCID.h"
#include "nsDOMError.h"
#include "nsDOMWindowUtils.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIContentViewer.h"
#include "nsISupportsPrimitives.h"
#include "nsDOMClassInfo.h"
#include "nsIJSNativeInitializer.h"
#include "nsIFullScreen.h"
#include "nsIStringBundle.h"
#include "nsIScriptEventManager.h" // For GetInterface()
#include "nsIConsoleService.h"
#include "nsIControllerContext.h"
#include "nsGlobalWindowCommands.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"

#include "plbase64.h"

#include "nsIPrintSettings.h"

#include "nsWindowRoot.h"
#include "nsNetCID.h"

// XXX An unfortunate dependency exists here (two XUL files).
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"

#include "nsIBindingManager.h"
#include "nsIXBLService.h"

// used for popup blocking, needs to be converted to something
// belonging to the back-end like nsIContentPolicy
#include "nsIPopupWindowManager.h"

static nsIEntropyCollector *gEntropyCollector          = nsnull;
static PRInt32              gRefCnt                    = 0;
static PRInt32              gOpenPopupSpamCount        = 0;
nsIXPConnect *GlobalWindowImpl::sXPConnect             = nsnull;
nsIScriptSecurityManager *GlobalWindowImpl::sSecMan    = nsnull;
nsIFactory *GlobalWindowImpl::sComputedDOMStyleFactory = nsnull;
#ifdef DEBUG_jst
PRInt32 gTimeoutCnt                                    = 0;
#endif

#define DOM_MIN_TIMEOUT_VALUE 10 // 10ms

// CIDs
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
#ifdef OJI
static NS_DEFINE_CID(kJVMServiceCID, NS_JVMMANAGER_CID);
#endif
static NS_DEFINE_CID(kHTTPHandlerCID, NS_HTTPPROTOCOLHANDLER_CID);
static NS_DEFINE_CID(kXULControllersCID, NS_XULCONTROLLERS_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID); // For window.find()
static NS_DEFINE_CID(kCStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

static const char sJSStackContractID[] = "@mozilla.org/js/xpc/ContextStack;1";

static const char kDOMBundleURL[] = "chrome://global/locale/commonDialogs.properties";
static const char kDOMSecurityWarningsBundleURL[] = "chrome://communicator/locale/dom/dom.properties";

static const char kCryptoContractID[] = NS_CRYPTO_CONTRACTID;
static const char kPkcs11ContractID[] = NS_PKCS11_CONTRACTID;

// CheckForAbusePoint return values:
enum {
  openAllowed = 0,  // open that window without worries
  openControlled,   // it's a popup, but allow it
  openAbused,       // it's a popup. disallow it, but allow domain override.
  openOverridden    // disallow window open
};
// CheckOpenAllow return values:
enum {
  allowNot = 0,     // the window opening was denied
  allowNoAbuse,     // allowed: not a popup
  allowSelf,        // allowed: it's the same window (_self, _top, et.al.)
  allowExtant,      // allowed: an already open window
  allowWhitelisted  // allowed: it's whitelisted or popup blocking is disabled
};


// return true if eventName is contained within events, delimited by spaces
static PRBool
ContainsEventName(const char *eventName, const nsAFlatCString& events)
{
  nsAFlatCString::const_iterator start, end;
  events.BeginReading(start);
  events.EndReading(end);

  nsAFlatCString::const_iterator startiter(start);

  while (startiter != end) {
    nsAFlatCString::const_iterator enditer(end);

    if (!FindInReadable(nsDependentCString(eventName), startiter, enditer))
      return PR_FALSE;

    // the match is surrounded by spaces, or at a string boundary
    if ((startiter == start || *--startiter == ' ') &&
        (enditer == end || *enditer == ' '))
      return PR_TRUE;

    /* Move on and see if there are other matches. (The delimitation
       requirement makes it pointless to begin the next search before
       the end of the invalid match just found.) */
    startiter = enditer;
  }

  return PR_FALSE;
}

//*****************************************************************************
//***    GlobalWindowImpl: Object Management
//*****************************************************************************

GlobalWindowImpl::GlobalWindowImpl()
  : mFirstDocumentLoad(PR_TRUE),
    mIsScopeClear(PR_TRUE),
    mFullScreen(PR_FALSE),
    mIsClosed(PR_FALSE), 
    mOpenerWasCleared(PR_FALSE),
    mIsPopupSpam(PR_FALSE),
    mJSObject(nsnull),
    mTimeouts(nsnull),
    mTimeoutInsertionPoint(&mTimeouts),
    mTimeoutPublicIdCounter(1),
    mTimeoutFiringDepth(0),
    mLastMouseButtonAction(LL_ZERO),
    mGlobalObjectOwner(nsnull),
    mDocShell(nsnull),
    mCurrentEvent(0)
{
  // nsPIDOMWindow initializers
  mFrameElement = nsnull;
  mRunningTimeout = nsnull;
  mMutationBits = 0;
  mIsDocumentLoaded = PR_FALSE;

  // We could have failed the first time through trying
  // to create the entropy collector, so we should
  // try to get one until we succeed.
  if (gRefCnt++ == 0 || !gEntropyCollector) {
    CallGetService(NS_ENTROPYCOLLECTOR_CONTRACTID, &gEntropyCollector);
  }
#ifdef DEBUG
  printf("++DOMWINDOW == %d\n", gRefCnt);
#endif

  if (!sXPConnect) {
    CallGetService(nsIXPConnect::GetCID(), &sXPConnect);
  }

  if (!sSecMan) {
    CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &sSecMan);
  }
}

GlobalWindowImpl::~GlobalWindowImpl()
{
  if (!--gRefCnt) {
    NS_IF_RELEASE(gEntropyCollector);
  }
#ifdef DEBUG
  printf("--DOMWINDOW == %d\n", gRefCnt);
#endif

  mDocument = nsnull;           // Forces Release

  CleanUp();
}

// static
void
GlobalWindowImpl::ShutDown()
{
  NS_IF_RELEASE(sXPConnect);
  NS_IF_RELEASE(sSecMan);
  NS_IF_RELEASE(sComputedDOMStyleFactory);

#ifdef DEBUG_jst
  printf ("---- Leaked %d GlobalWindowImpl's\n", gRefCnt);
  printf ("---- Leaked %d nsTimeoutImpl's\n", gTimeoutCnt);
#endif
}

void
GlobalWindowImpl::CleanUp()
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
  mContext = nsnull;            // Forces Release
  mChromeEventHandler = nsnull; // Forces Release

  if (IsPopupSpamWindow()) {
    SetPopupSpamWindow(PR_FALSE);
    --gOpenPopupSpamCount;
  }
}

void
GlobalWindowImpl::ClearControllers()
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

//*****************************************************************************
// GlobalWindowImpl::nsISupports
//*****************************************************************************


// QueryInterface implementation for GlobalWindowImpl
NS_INTERFACE_MAP_BEGIN(GlobalWindowImpl)
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
  NS_INTERFACE_MAP_ENTRY(nsPIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMViewCSS)
  NS_INTERFACE_MAP_ENTRY(nsIDOMAbstractView)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Window)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(GlobalWindowImpl)
NS_IMPL_RELEASE(GlobalWindowImpl)


//*****************************************************************************
// GlobalWindowImpl::nsIScriptGlobalObject
//*****************************************************************************

void
GlobalWindowImpl::SetContext(nsIScriptContext* aContext)
{
  // if setting the context to null, then we won't get to clean up the
  // named reference, so do it now
  if (!aContext) {
    NS_WARNING("Possibly early removal of script object, see bug #41608");
  } else {
    JSContext *cx = (JSContext *)aContext->GetNativeContext();

    mJSObject = ::JS_GetGlobalObject(cx);
  }

  mContext = aContext;

  if (mContext) {
    if (IsFrame()) {
      // This window is a [i]frame, don't bother GC'ing when the
      // frame's context is destroyed since a GC will happen when the
      // frameset or host document is destroyed anyway.

      mContext->SetGCOnDestruction(PR_FALSE);
    }
  }
}

nsIScriptContext *
GlobalWindowImpl::GetContext()
{
  return mContext;
}

void
GlobalWindowImpl::SetOpenerScriptURL(nsIURI* aURI)
{
  mOpenerScriptURL = aURI;
}

nsresult
GlobalWindowImpl::SetNewDocument(nsIDOMDocument* aDocument,
                                 PRBool aRemoveEventListeners,
                                 PRBool aClearScopeHint)
{
  if (!aDocument && mDocument) {
    // Cache the old principal now that the document is being removed.
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

    mDocumentPrincipal = doc->GetPrincipal();
  }

  // Always clear watchpoints, to deal with two cases:
  // 1.  The first document for this window is loading, and a miscreant has
  //     preset watchpoints on the window object in order to attack the new
  //     document's privileged information.
  // 2.  A document loaded and used watchpoints on its own window, leaving
  //     them set until the next document loads. We must clean up window
  //     watchpoints here.
  // Watchpoints set on document and subordinate objects are all cleared
  // when those sub-window objects are finalized, after JS_ClearScope and
  // a GC run that finds them to be garbage.

  JSContext *cx = nsnull;

  if (mContext) {
    cx = (JSContext *)mContext->GetNativeContext();

    if (mJSObject) {
      ::JS_ClearWatchPointsForObject(cx, mJSObject);
    }
  }

  if (aDocument) {
    if (mNavigator && mDocumentPrincipal) {
      // Loading a new document.  Compare its principal against the
      // old principal to see whether we are allowed to keep the old
      // navigator object.
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
      NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

      nsIPrincipal *newPrincipal = doc->GetPrincipal();
      nsresult rv = NS_ERROR_FAILURE;
      if (newPrincipal) {
        rv = sSecMan->CheckSameOriginPrincipal(mDocumentPrincipal,
                                               newPrincipal);
      }

      if (NS_SUCCEEDED(rv)) {
        // Same origins.  Notify the navigator object that we are
        // loading a new document, so it can make sure it is ready
        // for the new document.
        mNavigator->LoadingNewDocument();
      } else {
        // Different origins.  Destroy the navigator object so it gets
        // recreated for the new document.  The plugins or mime types
        // arrays may have changed. See bug 150087.
        mNavigatorHolder = nsnull;

        mNavigator->SetDocShell(nsnull);

        mNavigator = nsnull;
      }
    }

    // Let go of the cached principal since we no longer need it.
    mDocumentPrincipal = nsnull;
  }

  if (mFirstDocumentLoad) {
    if (aDocument) {
      mFirstDocumentLoad = PR_FALSE;
    }

    mDocument = aDocument;

    if (mDocument) {
      // Get our private root. If it is equal to us, then we
      // need to attach our global key bindings that handle
      // browser scrolling and other browser commands.
      nsIDOMWindowInternal *internal = GlobalWindowImpl::GetPrivateRoot();

      if (internal == NS_STATIC_CAST(nsIDOMWindowInternal *, this)) {
        nsCOMPtr<nsIXBLService> xblService =
          do_GetService("@mozilla.org/xbl;1");
        if (xblService) {
          nsCOMPtr<nsIDOMEventReceiver> rec =
            do_QueryInterface(mChromeEventHandler);
          xblService->AttachGlobalKeyHandler(rec);

          // for now, the only way to get drag/drop is to use the XUL
          // wrapper. There are just too many things that need to be
          // added to hookup DnD with XBL (pinkerton)
          //xblService->AttachGlobalDragHandler(rec);
        }
      }
    }

    return NS_OK;
  }

  /* No mDocShell means we've already been partially closed down.
     When that happens, setting status isn't a big requirement,
     so don't. (Doesn't happen under normal circumstances, but
     bug 49615 describes a case.) */
  /* We only want to do this when we're setting a new document rather
     than going away. See bug 61840.  */

  if (mDocShell && aDocument) {
    SetStatus(EmptyString());
    SetDefaultStatus(EmptyString());
  }

  if (mDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    nsIURI *docURL;

    // If we had a document in this window the document most likely
    // made our scope "unclear"

    mIsScopeClear = PR_FALSE;

    if (doc) {
      docURL = doc->GetDocumentURI();
    } else {
      docURL = nsnull;
    }

    if (docURL) {
      nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));
      PRBool isContentWindow = PR_FALSE;

      if (treeItem) {
        PRInt32 itemType = nsIDocShellTreeItem::typeContent;
        treeItem->GetItemType(&itemType);

        isContentWindow = itemType != nsIDocShellTreeItem::typeChrome;
      }

      nsCAutoString url;
      docURL->GetSpec(url);
      PRBool isAboutBlank = url.EqualsLiteral("about:blank");

      PRBool isSameOrigin = PR_FALSE;
      if (isAboutBlank && mOpenerScriptURL) {
        nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
        if (webNav) {
          nsCOMPtr<nsIURI> newDocURL;
          webNav->GetCurrentURI(getter_AddRefs(newDocURL));
          if (newDocURL && sSecMan) {
            sSecMan->SecurityCompareURIs(mOpenerScriptURL,
                                         newDocURL,
                                         &isSameOrigin);
          }
        }
      }

      if (!isAboutBlank ||
          (isContentWindow && aClearScopeHint && !isSameOrigin)) {
        // the current document is *not* about:blank,
        // or aClearScopeHint is true and the new document
        // has a different origin than the calling script.
        // clear timeouts and clear the scope
        ClearAllTimeouts();

        if (mContext && mJSObject) {
          if (mNavigator) {
            nsIDOMNavigator* navigator =
              NS_STATIC_CAST(nsIDOMNavigator*, mNavigator.get());
            sXPConnect->WrapNative(cx, mJSObject, navigator,
                                   NS_GET_IID(nsIDOMNavigator),
                                   getter_AddRefs(mNavigatorHolder));
          }

          ::JS_ClearScope(cx, mJSObject);
          ::JS_ClearRegExpStatics(cx);

          mIsScopeClear = PR_TRUE;
        }
      }

      // Don't remove event listeners in similar conditions
      aRemoveEventListeners = aRemoveEventListeners &&
        (!isAboutBlank || (isContentWindow && !isSameOrigin));

    }
    
    if (aRemoveEventListeners && mListenerManager) {
      mListenerManager->RemoveAllListeners(PR_FALSE);
      mListenerManager = nsnull;
    }
  }

  if (mContext && aDocument) {
    if (mNavigator && mJSObject) {
      // Make our JS wrapper hold on to the navigator object so
      // that it doesn't go away when transitioning from page to
      // page. We do this by just asking for the navigator
      // property on the global object. That causes the global
      // object's resolve hook to cache the property on the
      // global, and thus root it which prevents GC from getting
      // rid of the object.
      jsval dummy;
      ::JS_GetProperty(cx, mJSObject, "navigator", &dummy);
    }

    // Add an extra ref in case we release mContext during GC.
    nsCOMPtr<nsIScriptContext> kungFuDeathGrip = mContext;
    kungFuDeathGrip->GC();
  }

  mDocument = aDocument;

  if (mDocument && mContext) {

    if (mIsScopeClear) {
      mContext->InitContext(this);
    } else if (mJSObject) {
      // If we didn't clear the scope (i.e. the old document was
      // about:blank) then we need to update the cached document
      // property on the window to reflect the new document and not
      // the old one.

      nsWindowSH::CacheDocumentProperty(cx, mJSObject, this);
    }
  }

  // Clear our mutation bitfield.
  mMutationBits = 0;

  return NS_OK;
}

void
GlobalWindowImpl::SetDocShell(nsIDocShell* aDocShell)
{
  if (aDocShell == mDocShell)
    return;

  // SetDocShell(nsnull) means the window is being torn down. Drop our
  // reference to the script context, allowing it to be deleted
  // later. Meanwhile, keep our weak reference to the script object
  // (mJSObject) so that it can be retrieved later (until it is
  // finalized by the JS GC).

  if (!aDocShell && mContext) {
    ClearAllTimeouts();

    // if we are closing the window while in full screen mode, be sure
    // to restore os chrome
    if (mFullScreen) {
      nsIFocusController *focusController =
        GlobalWindowImpl::GetRootFocusController();
      PRBool isActive = PR_FALSE;
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

    mNavigatorHolder = nsnull;

    mContext->GC();

    mContext = nsnull;          // force release now
    mChromeEventHandler = nsnull; // force release now
  }

  mDocShell = aDocShell;        // Weak Reference

  if (mLocation)
    mLocation->SetDocShell(aDocShell);
  if (mNavigator)
    mNavigator->SetDocShell(aDocShell);
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

nsIDocShell *
GlobalWindowImpl::GetDocShell()
{
  return mDocShell;
}

void
GlobalWindowImpl::SetOpenerWindow(nsIDOMWindowInternal* aOpener)
{
  mOpener = aOpener;
}

void
GlobalWindowImpl::SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner)
{
  mGlobalObjectOwner = aOwner;  // Note this is supposed to be a weak ref.
}

nsIScriptGlobalObjectOwner *
GlobalWindowImpl::GetGlobalObjectOwner()
{
  return mGlobalObjectOwner;
}

nsresult
GlobalWindowImpl::HandleDOMEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIDOMEvent** aDOMEvent,
                                 PRUint32 aFlags,
                                 nsEventStatus* aEventStatus)
{
  nsresult ret = NS_OK;
  PRBool externalDOMEvent = PR_FALSE;
  nsIDOMEvent *domEvent = nsnull;
  static PRUint32 count = 0;

  nsEvent *oldEvent = mCurrentEvent;
  mCurrentEvent = aEvent;

  /* mChromeEventHandler and mContext go dangling in the middle of this
     function under some circumstances (events that destroy the window)
     without this addref. */
  nsCOMPtr<nsIChromeEventHandler> kungFuDeathGrip1(mChromeEventHandler);
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip2(mContext);

  /* If this is a mouse event, use the struct to provide entropy for
   * the system.
   */
  if (gEntropyCollector &&
      (NS_EVENT_FLAG_CAPTURE & aFlags) &&
      (aEvent->message == NS_MOUSE_MOVE)) {
    //I'd like to not come in here if there is a mChromeEventHandler
    //present, but there is always one when the message is
    //NS_MOUSE_MOVE.
    //
    //Chances are this counter will overflow during the life of the
    //process, but that's OK for our case. Means we get a little
    //more entropy.
    if (count++ % 100 == 0) {
      //Since the high bits seem to be zero's most of the time,
      //let's only take the lowest half of the point structure.
      PRInt16 myCoord[4];

      myCoord[0] = aEvent->point.x;
      myCoord[1] = aEvent->point.y;
      myCoord[2] = aEvent->refPoint.x;
      myCoord[3] = aEvent->refPoint.y;
      gEntropyCollector->RandomUpdate((void*)myCoord, sizeof(myCoord));
      gEntropyCollector->RandomUpdate((void*)&aEvent->time, sizeof(PRUint32));
    }
  }

  // if the window is deactivated while in full screen mode,
  // restore OS chrome, and hide it again upon re-activation
  if (mFullScreen && (NS_EVENT_FLAG_BUBBLE & aFlags)) {
    if (aEvent->message == NS_DEACTIVATE || aEvent->message == NS_ACTIVATE) {
      nsCOMPtr<nsIFullScreen> fullScreen =
        do_GetService("@mozilla.org/browser/fullscreen;1");
      if (fullScreen) {
        if (aEvent->message == NS_DEACTIVATE)
          fullScreen->ShowAllOSChrome();
        else
          fullScreen->HideAllOSChrome();
      }
    }
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    if (aDOMEvent) {
      if (*aDOMEvent) {
        externalDOMEvent = PR_TRUE;
      }
    }
    else {
      aDOMEvent = &domEvent;
    }
    aEvent->flags |= aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
    aFlags |= NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_CAPTURE;

    // Execute bindingdetached handlers before we tear ourselves
    // down.
    if (aEvent->message == NS_PAGE_UNLOAD && mDocument && !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT)) {
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
      nsIBindingManager *bindingManager = doc->GetBindingManager();
      if (bindingManager)
        bindingManager->ExecuteDetachedHandlers();
    }
  }

  if (aEvent->message == NS_PAGE_UNLOAD) {
    mIsDocumentLoaded = PR_FALSE;
  } else if ((aEvent->message >= NS_MOUSE_LEFT_BUTTON_UP &&
              aEvent->message <= NS_MOUSE_RIGHT_BUTTON_DOWN) ||
             (aEvent->message >= NS_MOUSE_LEFT_DOUBLECLICK &&
              aEvent->message <= NS_MOUSE_RIGHT_CLICK)) {
    mLastMouseButtonAction = PR_Now();
  }

  // Capturing stage
  if ((NS_EVENT_FLAG_CAPTURE & aFlags) && mChromeEventHandler) {
    // Check chrome document capture here.
    // XXX The chrome can not handle this, see bug 51211
    if (aEvent->message != NS_IMAGE_LOAD) {
      mChromeEventHandler->HandleChromeEvent(aPresContext, aEvent, aDOMEvent,
                                             aFlags & NS_EVENT_CAPTURE_MASK,
                                             aEventStatus);
    }
  }

  // Local handling stage
  if ((aEvent->message != NS_BLUR_CONTENT || !GetBlurSuppression()) &&
      mListenerManager &&
      !(NS_EVENT_FLAG_CANT_BUBBLE & aEvent->flags && NS_EVENT_FLAG_BUBBLE & aFlags && !(NS_EVENT_FLAG_INIT & aFlags))) {
    aEvent->flags |= aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, this,
                                  aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  if (aEvent->message == NS_PAGE_LOAD)
    mIsDocumentLoaded = PR_TRUE;

  // Bubbling stage
  if ((NS_EVENT_FLAG_BUBBLE & aFlags) && mChromeEventHandler) {
    // Bubble to a chrome document if it exists
    // XXX Need a way to know if an event should really bubble or not.
    // For now filter out load and unload, since they cause problems.
    if ((aEvent->message != NS_PAGE_LOAD) &&
        (aEvent->message != NS_PAGE_UNLOAD) &&
        (aEvent->message != NS_IMAGE_LOAD) &&
        (aEvent->message != NS_FOCUS_CONTENT) &&
        (aEvent->message != NS_BLUR_CONTENT)) {
      mChromeEventHandler->HandleChromeEvent(aPresContext, aEvent,
                                             aDOMEvent, aFlags & NS_EVENT_BUBBLE_MASK,
                                             aEventStatus);
    }
  }

  if (aEvent->message == NS_PAGE_LOAD) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(mFrameElement));

    nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));

    PRInt32 itemType = nsIDocShellTreeItem::typeChrome;

    if (treeItem) {
      treeItem->GetItemType(&itemType);
    }

    if (content && GetParentInternal() &&
        itemType != nsIDocShellTreeItem::typeChrome) {
      // If we're not in chrome, or at a chrome boundary, fire the
      // onload event for the frame element.

      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event(NS_PAGE_LOAD);

      // Most of the time we could get a pres context to pass in here,
      // but not always (i.e. if this window is not shown there won't
      // be a pres context available). Since we're not firing a GUI
      // event we don't need a pres context anyway so we just pass
      // null as the pres context all the time here.

      ret = content->HandleDOMEvent(nsnull, &event, nsnull,
                                    NS_EVENT_FLAG_INIT, &status);
    }
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created an event,
    // release here.
    if (*aDOMEvent && !externalDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (rc) {
        // Okay, so someone in the DOM loop (a listener, JS object) still has
        // a ref to the DOM Event but the internal data hasn't been malloc'd.
        // Force a copy of the data here so the DOM Event is still valid.
        nsCOMPtr<nsIPrivateDOMEvent>
          privateEvent(do_QueryInterface(*aDOMEvent));
        if (privateEvent)
          privateEvent->DuplicatePrivateData();
      }
      aDOMEvent = nsnull;
    }
  }

  mCurrentEvent = oldEvent;
  return ret;
}

JSObject *
GlobalWindowImpl::GetGlobalJSObject()
{
  return mJSObject;
}

void
GlobalWindowImpl::OnFinalize(JSObject *aJSObject)
{
  if (aJSObject == mJSObject) {
    mJSObject = nsnull;
  } else if (mJSObject) {
    NS_ERROR("Huh? XPConnect created more than one wrapper for this global!");
  } else {
    NS_WARNING("Weird, we're finalized with a null mJSObject?");
  }
}

void
GlobalWindowImpl::SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts)
{
  if (aEnabled && aFireTimeouts) {
    // Scripts are enabled (again?) on this context, run timeouts that
    // fired on this context while scripts were disabled.

    RunTimeout(nsnull);
  }
}


//*****************************************************************************
// GlobalWindowImpl::nsIScriptObjectPrincipal
//*****************************************************************************

nsIPrincipal*
GlobalWindowImpl::GetPrincipal()
{
  if (mDocument) {
    // If we have a document, get the principal from the document
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    NS_ENSURE_TRUE(doc, nsnull);

    return doc->GetPrincipal();
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
// GlobalWindowImpl::nsIDOMWindow
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::GetDocument(nsIDOMDocument** aDocument)
{
  // lazily instantiate an about:blank document if necessary, and if
  // we have what it takes to do so. Note that domdoc here is the same
  // thing as our mDocument, but we don't have to explicitly set the
  // member variable because the docshell has already called
  // SetNewDocument().
  if (!mDocument && mDocShell)
    nsCOMPtr<nsIDOMDocument> domdoc(do_GetInterface(mDocShell));

  NS_IF_ADDREF(*aDocument = mDocument);

  return NS_OK;
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOMWindowInternal
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::GetWindow(nsIDOMWindowInternal** aWindow)
{
  *aWindow = NS_STATIC_CAST(nsIDOMWindowInternal *, this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetSelf(nsIDOMWindowInternal** aWindow)
{
  *aWindow = NS_STATIC_CAST(nsIDOMWindowInternal *, this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetNavigator(nsIDOMNavigator** aNavigator)
{
  *aNavigator = nsnull;

  if (!mNavigator) {
    mNavigator = new NavigatorImpl(mDocShell);
    if (!mNavigator) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aNavigator = mNavigator);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScreen(nsIDOMScreen** aScreen)
{
  *aScreen = nsnull;

  if (!mScreen && mDocShell) {
    mScreen = new ScreenImpl(mDocShell);
    if (!mScreen) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aScreen = mScreen);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetHistory(nsIDOMHistory** aHistory)
{
  *aHistory = nsnull;

  if (!mHistory && mDocShell) {
    mHistory = new HistoryImpl(mDocShell);
    if (!mHistory) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aHistory = mHistory);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetParent(nsIDOMWindow** aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
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
GlobalWindowImpl::GetTop(nsIDOMWindow** aTop)
{
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
GlobalWindowImpl::GetContent(nsIDOMWindow** aContent)
{
  *aContent = nsnull;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> primaryContent;
  treeOwner->GetPrimaryContentShell(getter_AddRefs(primaryContent));

  nsCOMPtr<nsIDOMWindowInternal> domWindow(do_GetInterface(primaryContent));
  NS_IF_ADDREF(*aContent = domWindow);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetPrompter(nsIPrompt** aPrompt)
{
  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_NO_INTERFACE);

  NS_ADDREF(*aPrompt = prompter);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetMenubar(nsIDOMBarProp** aMenubar)
{
  *aMenubar = nsnull;

  if (!mMenubar) {
    mMenubar = new MenubarPropImpl();
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
GlobalWindowImpl::GetToolbar(nsIDOMBarProp** aToolbar)
{
  *aToolbar = nsnull;

  if (!mToolbar) {
    mToolbar = new ToolbarPropImpl();
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
GlobalWindowImpl::GetLocationbar(nsIDOMBarProp** aLocationbar)
{
  *aLocationbar = nsnull;

  if (!mLocationbar) {
    mLocationbar = new LocationbarPropImpl();
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
GlobalWindowImpl::GetPersonalbar(nsIDOMBarProp** aPersonalbar)
{
  *aPersonalbar = nsnull;

  if (!mPersonalbar) {
    mPersonalbar = new PersonalbarPropImpl();
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
GlobalWindowImpl::GetStatusbar(nsIDOMBarProp** aStatusbar)
{
  *aStatusbar = nsnull;

  if (!mStatusbar) {
    mStatusbar = new StatusbarPropImpl();
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
GlobalWindowImpl::GetScrollbars(nsIDOMBarProp** aScrollbars)
{
  *aScrollbars = nsnull;

  if (!mScrollbars) {
    mScrollbars = new ScrollbarsPropImpl(this);
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
GlobalWindowImpl::GetDirectories(nsIDOMBarProp** aDirectories)
{
  return GetPersonalbar(aDirectories);
}

NS_IMETHODIMP
GlobalWindowImpl::GetClosed(PRBool* aClosed)
{
  // If someone called close(), or if we don't have a docshell, we're
  // closed.
  *aClosed = mIsClosed || !mDocShell;

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetFrames(nsIDOMWindowCollection** aFrames)
{
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
GlobalWindowImpl::GetCrypto(nsIDOMCrypto** aCrypto)
{
  if (!mCrypto) {
    mCrypto = do_CreateInstance(kCryptoContractID);
  }

  NS_IF_ADDREF(*aCrypto = mCrypto);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetPkcs11(nsIDOMPkcs11** aPkcs11)
{
  if (!mPkcs11) {
    mPkcs11 = do_CreateInstance(kPkcs11ContractID);
  }

  NS_IF_ADDREF(*aPkcs11 = mPkcs11);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetControllers(nsIControllers** aResult)
{
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
GlobalWindowImpl::GetOpener(nsIDOMWindowInternal** aOpener)
{
  *aOpener = nsnull;
  // First, check if we were called from a privileged chrome script

  NS_ENSURE_TRUE(sSecMan, NS_ERROR_FAILURE);
  PRBool inChrome;
  nsresult rv = sSecMan->SubjectPrincipalIsSystem(&inChrome);
  if (NS_SUCCEEDED(rv) && inChrome) {
    *aOpener = mOpener;
    NS_IF_ADDREF(*aOpener);
    return NS_OK;
  }

  // We don't want to reveal the opener if the opener is a mail window,
  // because opener can be used to spoof the contents of a message (bug 105050).
  // So, we look in the opener's root docshell to see if it's a mail window.
  nsCOMPtr<nsIScriptGlobalObject> openerSGO(do_QueryInterface(mOpener));
  if (openerSGO) {
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
      do_QueryInterface(openerSGO->GetDocShell());

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
GlobalWindowImpl::SetOpener(nsIDOMWindowInternal* aOpener)
{
  // check if we were called from a privileged chrome script.
  // If not, opener is settable only to null.
  if (aOpener && !IsCallerChrome()) {
    return NS_OK;
  }
  if (mOpener && !aOpener)
    mOpenerWasCleared = PR_TRUE;

  mOpener = aOpener;

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetStatus(nsAString& aStatus)
{
  aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetStatus(const nsAString& aStatus)
{
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
GlobalWindowImpl::GetDefaultStatus(nsAString& aDefaultStatus)
{
  aDefaultStatus = mDefaultStatus;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetDefaultStatus(const nsAString& aDefaultStatus)
{
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
GlobalWindowImpl::GetName(nsAString& aName)
{
  nsXPIDLString name;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    docShellAsItem->GetName(getter_Copies(name));

  aName.Assign(name);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetName(const nsAString& aName)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    result = docShellAsItem->SetName(PromiseFlatString(aName).get());
  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::GetInnerWidth(PRInt32* aInnerWidth)
{
  // If we're a subframe, make sure our size is up to date.  It's OK that this
  // crosses the content/chrome boundary, since chrome can have pending reflows
  // too.
  GlobalWindowImpl* parent = NS_STATIC_CAST(GlobalWindowImpl*,
                                            GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(Flush_Layout);
  }

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  *aInnerWidth = 0;
  PRInt32 notused;
  if (docShellWin)
    docShellWin->GetSize(aInnerWidth, &notused);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetInnerWidth(PRInt32 aInnerWidth)
{
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
  PRInt32 notused, cy = 0;
  docShellAsWin->GetSize(&notused, &cy);
  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(docShellAsItem, aInnerWidth, cy),
                    NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetInnerHeight(PRInt32* aInnerHeight)
{
  // If we're a subframe, make sure our size is up to date.  It's OK that this
  // crosses the content/chrome boundary, since chrome can have pending reflows
  // too.
  GlobalWindowImpl* parent = NS_STATIC_CAST(GlobalWindowImpl*,
                                            GetPrivateParent());
  if (parent) {
    parent->FlushPendingNotifications(Flush_Layout);
  }

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  *aInnerHeight = 0;
  PRInt32 notused;
  if (docShellWin)
    docShellWin->GetSize(&notused, aInnerHeight);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetInnerHeight(PRInt32 aInnerHeight)
{
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
  PRInt32 cx = 0, notused;
  docShellAsWin->GetSize(&cx, &notused);
  NS_ENSURE_SUCCESS(treeOwner->
                    SizeShellTo(docShellAsItem, cx, aInnerHeight),
                    NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetOuterWidth(PRInt32* aOuterWidth)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  GlobalWindowImpl* rootWindow = NS_STATIC_CAST(GlobalWindowImpl*,
                                                GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }
  PRInt32 notused;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(aOuterWidth, &notused),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetOuterWidth(PRInt32 aOuterWidth)
{
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

  PRInt32 notused, cy;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&notused, &cy), NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(aOuterWidth, cy, PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetOuterHeight(PRInt32* aOuterHeight)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  GlobalWindowImpl* rootWindow = NS_STATIC_CAST(GlobalWindowImpl*,
                                                GetPrivateRoot());
  if (rootWindow) {
    rootWindow->FlushPendingNotifications(Flush_Layout);
  }

  PRInt32 notused;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&notused, aOuterHeight),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetOuterHeight(PRInt32 aOuterHeight)
{
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

  PRInt32 cx, notused;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&cx, &notused), NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(cx, aOuterHeight, PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScreenX(PRInt32* aScreenX)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 y;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(aScreenX, &y),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetScreenX(PRInt32 aScreenX)
{
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
GlobalWindowImpl::GetScreenY(PRInt32* aScreenY)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  PRInt32 x;

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, aScreenY),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetScreenY(PRInt32 aScreenY)
{
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
GlobalWindowImpl::CheckSecurityWidthAndHeight(PRInt32* aWidth,
                                              PRInt32* aHeight)
{
  // This one is easy. Just ensure the variable is greater than 100;
  if ((aWidth && *aWidth < 100) || (aHeight && *aHeight < 100)) {
    // Check security state for use in determing window dimensions

    NS_ENSURE_TRUE(sSecMan, NS_ERROR_FAILURE);

    PRBool enabled;
    nsresult res = sSecMan->IsCapabilityEnabled("UniversalBrowserWrite",
                                                &enabled);

    if (NS_FAILED(res) || !enabled) {
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
GlobalWindowImpl::CheckSecurityLeftAndTop(PRInt32* aLeft, PRInt32* aTop)
{
  // This one is harder. We have to get the screen size and window dimensions.

  // Check security state for use in determing window dimensions

  NS_ENSURE_TRUE(sSecMan, NS_ERROR_FAILURE);

  PRBool enabled;
  nsresult res = sSecMan->IsCapabilityEnabled("UniversalBrowserWrite",
                                              &enabled);
  if (NS_FAILED(res)) {
    enabled = PR_FALSE;
  }

  if (!enabled) {
    PRInt32 screenLeft, screenTop, screenWidth, screenHeight;
    PRInt32 winLeft, winTop, winWidth, winHeight;

    GlobalWindowImpl* rootWindow = NS_STATIC_CAST(GlobalWindowImpl*,
                                                  GetPrivateRoot());
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
GlobalWindowImpl::GetPageXOffset(PRInt32* aPageXOffset)
{
  return GetScrollX(aPageXOffset);
}

NS_IMETHODIMP
GlobalWindowImpl::GetPageYOffset(PRInt32* aPageYOffset)
{
  return GetScrollY(aPageYOffset);
}

nsresult
GlobalWindowImpl::GetScrollMaxXY(PRInt32* aScrollMaxX, PRInt32* aScrollMaxY)
{
  nsresult rv;
  nsIScrollableView *view = nsnull;      // no addref/release for views
  float p2t, t2p;

  GetScrollInfo(&view, &p2t, &t2p);
  if (!view)
    return NS_OK;      // bug 230965 changed from NS_ERROR_FAILURE

  nsSize scrolledSize;
  rv = view->GetContainerSize(&scrolledSize.width, &scrolledSize.height);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIView* portView;
  rv = CallQueryInterface(view, &portView);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRect portRect = portView->GetBounds();

  if (aScrollMaxX)
    *aScrollMaxX = PR_MAX(0,
      (PRInt32)floor(t2p*(scrolledSize.width - portRect.width)));
  if (aScrollMaxY)
    *aScrollMaxY = PR_MAX(0,
      (PRInt32)floor(t2p*(scrolledSize.height - portRect.height)));

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScrollMaxX(PRInt32* aScrollMaxX)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxX);
  *aScrollMaxX = 0;
  return GetScrollMaxXY(aScrollMaxX, nsnull);
}

NS_IMETHODIMP
GlobalWindowImpl::GetScrollMaxY(PRInt32* aScrollMaxY)
{
  NS_ENSURE_ARG_POINTER(aScrollMaxY);
  *aScrollMaxY = 0;
  return GetScrollMaxXY(nsnull, aScrollMaxY);
}

nsresult
GlobalWindowImpl::GetScrollXY(PRInt32* aScrollX, PRInt32* aScrollY)
{
  nsresult rv;
  nsIScrollableView *view = nsnull;      // no addref/release for views
  float p2t, t2p;

  GetScrollInfo(&view, &p2t, &t2p);
  if (!view)
    return NS_OK;      // bug 202206 changed from NS_ERROR_FAILURE

  nscoord xPos, yPos;
  rv = view->GetScrollPosition(xPos, yPos);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aScrollX)
    *aScrollX = NSTwipsToIntPixels(xPos, t2p);
  if (aScrollY)
    *aScrollY = NSTwipsToIntPixels(yPos, t2p);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScrollX(PRInt32* aScrollX)
{
  NS_ENSURE_ARG_POINTER(aScrollX);
  *aScrollX = 0;
  return GetScrollXY(aScrollX, nsnull);
}

NS_IMETHODIMP
GlobalWindowImpl::GetScrollY(PRInt32* aScrollY)
{
  NS_ENSURE_ARG_POINTER(aScrollY);
  *aScrollY = 0;
  return GetScrollXY(nsnull, aScrollY);
}

NS_IMETHODIMP
GlobalWindowImpl::GetLength(PRUint32* aLength)
{
  nsCOMPtr<nsIDOMWindowCollection> frames;
  if (NS_SUCCEEDED(GetFrames(getter_AddRefs(frames))) && frames) {
    return frames->GetLength(aLength);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GlobalWindowImpl::SetFullScreen(PRBool aFullScreen)
{
  // Only chrome can change our fullScreen mode.
  if (aFullScreen == mFullScreen || !IsCallerChrome()) {
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

  // dispatch an onfullscreen DOM event so that XUL apps can
  // respond visually if we are kicked into full screen mode
  nsCOMPtr<nsIDOMDocumentEvent> doc(do_QueryInterface(mDocument));
  nsCOMPtr<nsIDOMEvent> event;
  doc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  if (!event)
    return NS_ERROR_FAILURE;

  event->InitEvent(NS_LITERAL_STRING("fullscreen"), PR_FALSE, PR_TRUE);
  PRBool allowDefault;
  DispatchEvent(event, &allowDefault);

  // event handlers can prevent us from going into full-screen mode
  if (!allowDefault)
    return NS_OK;

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWidget> widget;
  treeOwnerAsWin->GetMainWidget(getter_AddRefs(widget));
  if (widget)
    widget->MakeFullScreen(aFullScreen);

  mFullScreen = aFullScreen;

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetFullScreen(PRBool* aFullScreen)
{
  *aFullScreen = mFullScreen;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Dump(const nsAString& aStr)
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
GlobalWindowImpl::EnsureReflowFlushAndPaint()
{
  NS_ASSERTION(mDocShell, "EnsureReflowFlushAndPaint() called with no "
               "docshell!");

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell)
    return;

  // Flush pending reflows.
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));

  if (doc) {
    doc->FlushPendingNotifications(Flush_Layout);
  }

  // Unsuppress painting.
  presShell->UnsuppressPainting();
}

NS_IMETHODIMP
GlobalWindowImpl::GetTextZoom(float *aZoom)
{
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
GlobalWindowImpl::SetTextZoom(float aZoom)
{
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
PRBool
GlobalWindowImpl::IsCallerChrome()
{
  NS_ENSURE_TRUE(sSecMan, PR_FALSE);

  PRBool isChrome = PR_FALSE;
  nsresult rv = sSecMan->SubjectPrincipalIsSystem(&isChrome);

  return NS_SUCCEEDED(rv) ? isChrome : PR_FALSE;
}

// static
void
GlobalWindowImpl::MakeScriptDialogTitle(const nsAString &aInTitle,
                                        nsAString &aOutTitle)
{
  aOutTitle.Truncate();

  // Load the string to be prepended to titles for script
  // confirm/alert/prompt boxes.

  nsCOMPtr<nsIStringBundleService> stringBundleService =
     do_GetService(kCStringBundleServiceCID);

  if (stringBundleService) {
    nsCOMPtr<nsIStringBundle> stringBundle;
    stringBundleService->CreateBundle(kDOMBundleURL,
                                      getter_AddRefs(stringBundle));

    if (stringBundle) {
      nsAutoString inTitle(aInTitle);
      nsXPIDLString tempString;
      const PRUnichar *formatStrings[1];
      formatStrings[0] = inTitle.get();
      stringBundle->FormatStringFromName(NS_LITERAL_STRING("ScriptDlgTitle").get(),
                                         formatStrings, 1,
                                         getter_Copies(tempString));

      aOutTitle = tempString;
    }
  }

  // Just in case
  if (aOutTitle.IsEmpty()) {
    NS_WARNING("could not get ScriptDlgTitle string from string bundle");
    aOutTitle.AssignLiteral("[Script] ");
    aOutTitle.Append(aInTitle);
  }
}

NS_IMETHODIMP
GlobalWindowImpl::Alert(const nsAString& aString)
{
  NS_ENSURE_STATE(mDocShell);

  // Special handling for alert(null) in JS for backwards
  // compatibility.

  NS_NAMED_LITERAL_STRING(null_str, "null");

  const nsAString *str = DOMStringIsNull(aString) ? &null_str : &aString;

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  // Test whether title needs to prefixed with [script]
  nsAutoString newTitle;
  const PRUnichar *title = nsnull;
  if (!IsCallerChrome()) {
      MakeScriptDialogTitle(EmptyString(), newTitle);
      title = newTitle.get();
  }
  else {
      NS_WARNING("chrome shouldn't be calling alert(), use the prompt "
                 "service");
  }

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  return prompter->Alert(title, PromiseFlatString(*str).get());
}

NS_IMETHODIMP
GlobalWindowImpl::Confirm(const nsAString& aString, PRBool* aReturn)
{
  NS_ENSURE_STATE(mDocShell);

  *aReturn = PR_FALSE;

  // Test whether title needs to prefixed with [script]
  nsAutoString newTitle;
  const PRUnichar *title = nsnull;
  if (!IsCallerChrome()) {
      MakeScriptDialogTitle(EmptyString(), newTitle);
      title = newTitle.get();
  }
  else {
      NS_WARNING("chrome shouldn't be calling confirm(), use the prompt "
                 "service");
  }

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  return prompter->Confirm(title, PromiseFlatString(aString).get(), aReturn);
}

NS_IMETHODIMP
GlobalWindowImpl::Prompt(const nsAString& aMessage,
                         const nsAString& aInitial,
                         const nsAString& aTitle,
                         PRUint32 aSavePassword,
                         nsAString& aReturn)
{
  SetDOMStringToNull(aReturn);

  NS_ENSURE_STATE(mDocShell);

  nsCOMPtr<nsIAuthPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  PRBool b;
  nsXPIDLString uniResult;

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  // Test whether title needs to prefixed with [script]
  nsAutoString title;
  if (!IsCallerChrome()) {
      MakeScriptDialogTitle(aTitle, title);
  } else {
      NS_WARNING("chrome shouldn't be calling prompt(), use the prompt "
                 "service");
      title.Assign(aTitle);
  }

  nsresult rv = prompter->Prompt(title.get(),
                                 PromiseFlatString(aMessage).get(), nsnull,
                                 aSavePassword,
                                 PromiseFlatString(aInitial).get(),
                                 getter_Copies(uniResult), &b);
  NS_ENSURE_SUCCESS(rv, rv);

  if (uniResult && b) {
    aReturn.Assign(uniResult);
  }

  return rv;
}

NS_IMETHODIMP
GlobalWindowImpl::Prompt(nsAString& aReturn)
{
  NS_ENSURE_STATE(mDocShell);
  NS_ENSURE_STATE(sXPConnect);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = sXPConnect->GetCurrentNativeCallContext(getter_AddRefs(ncc));
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
    nsJSUtils::ConvertJSValToString(message, cx, argv[0]);

    if (argc > 1) {
      nsJSUtils::ConvertJSValToString(initial, cx, argv[1]);

      if (argc > 2) {
        nsJSUtils::ConvertJSValToString(title, cx, argv[2]);

        if (argc > 3) {
          nsJSUtils::ConvertJSValToUint32(&savePassword, cx, argv[3]);
        }
      }
    }
  }

  return Prompt(message, initial, title, savePassword, aReturn);
}

NS_IMETHODIMP
GlobalWindowImpl::Focus()
{
  /*
   * If caller is not chrome and dom.disable_window_flip is true,
   * prevent setting window.focus() by exiting early
   */

  if (!CanSetProperty("dom.disable_window_flip")) {
    return NS_OK;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  if (treeOwnerAsWin) {
    PRBool isEnabled = PR_TRUE;
    if (NS_SUCCEEDED(treeOwnerAsWin->GetEnabled(&isEnabled)) && !isEnabled) {
      NS_WARNING( "Should not try to set the focus on a disabled window" );
      return NS_ERROR_FAILURE;
    }

    treeOwnerAsWin->SetVisibility(PR_TRUE);
    nsCOMPtr<nsIEmbeddingSiteWindow> embeddingWin(do_GetInterface(treeOwnerAsWin));
    if (embeddingWin)
      embeddingWin->SetFocus();
  }

  nsCOMPtr<nsIPresShell> presShell;
  if (mDocShell) {
    mDocShell->GetEldestPresShell(getter_AddRefs(presShell));
  }

  nsresult result = NS_OK;
  if (presShell) {
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
    nsIFocusController *focusController =
      GlobalWindowImpl::GetRootFocusController();
    if (focusController)
      focusController->SetFocusedWindow(this);
  }

  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::Blur()
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIEmbeddingSiteWindow2> siteWindow(do_GetInterface(treeOwner));
  if (siteWindow)
    rv = siteWindow->Blur();

  if (NS_SUCCEEDED(rv))
    mDocShell->SetHasFocus(PR_FALSE);

  return rv;
}

NS_IMETHODIMP
GlobalWindowImpl::Back()
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoBack();
}

NS_IMETHODIMP
GlobalWindowImpl::Forward()
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoForward();
}

NS_IMETHODIMP
GlobalWindowImpl::Home()
{
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
GlobalWindowImpl::Stop()
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  return webNav->Stop(nsIWebNavigation::STOP_ALL);
}

NS_IMETHODIMP
GlobalWindowImpl::Print()
{
  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint;
  if (NS_SUCCEEDED(GetInterface(NS_GET_IID(nsIWebBrowserPrint),
                                getter_AddRefs(webBrowserPrint)))) {
    nsCOMPtr<nsIPrintSettings> printSettings;
    webBrowserPrint->GetGlobalPrintSettings(getter_AddRefs(printSettings));
    webBrowserPrint->Print(printSettings, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::MoveTo(PRInt32 aXPos, PRInt32 aYPos)
{
  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent window.moveTo() by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize")) {
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
GlobalWindowImpl::MoveBy(PRInt32 aXDif, PRInt32 aYDif)
{
  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * prevent window.moveBy() by exiting early
   */

  if (!CanSetProperty("dom.disable_window_move_resize")) {
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
GlobalWindowImpl::ResizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
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
GlobalWindowImpl::ResizeBy(PRInt32 aWidthDif, PRInt32 aHeightDif)
{
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

  PRInt32 cx, cy;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&cx, &cy), NS_ERROR_FAILURE);

  PRInt32 newCX = cx + aWidthDif;
  PRInt32 newCY = cy + aHeightDif;
  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&newCX, &newCY),
                    NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(newCX, newCY,
                                            PR_TRUE), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SizeToContent()
{
  /*
   * If caller is not chrome and dom.disable_window_move_resize is true,
   * block window.SizeToContent() by exiting
   */

  if (!CanSetProperty("dom.disable_window_move_resize")) {
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
GlobalWindowImpl::GetWindowRoot(nsIDOMEventTarget **aWindowRoot)
{
  *aWindowRoot = nsnull;

  nsIDOMWindowInternal *rootWindow = GlobalWindowImpl::GetPrivateRoot();
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
GlobalWindowImpl::Scroll(PRInt32 aXScroll, PRInt32 aYScroll)
{
  return ScrollTo(aXScroll, aYScroll);
}

NS_IMETHODIMP
GlobalWindowImpl::ScrollTo(PRInt32 aXScroll, PRInt32 aYScroll)
{
  nsresult result;
  nsIScrollableView *view = nsnull;      // no addref/release for views
  float p2t, t2p;

  result = GetScrollInfo(&view, &p2t, &t2p);

  if (view) {
    // Here we calculate what the max pixel value is that we can
    // scroll to, we do this by dividing maxint with the pixel to
    // twips conversion factor, and substracting 4, the 4 comes from
    // experimenting with this value, anything less makes the view
    // code not scroll correctly, I have no idea why. -- jst
    const PRInt32 maxpx = (PRInt32)((float)0x7fffffff / p2t) - 4;

    if (aXScroll > maxpx) {
      aXScroll = maxpx;
    }

    if (aYScroll > maxpx) {
      aYScroll = maxpx;
    }

    result = view->ScrollTo(NSIntPixelsToTwips(aXScroll, p2t),
                            NSIntPixelsToTwips(aYScroll, p2t),
                            NS_VMREFRESH_IMMEDIATE);
  }

  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::ScrollBy(PRInt32 aXScrollDif,
                           PRInt32 aYScrollDif)
{
  nsresult result;
  nsIScrollableView *view = nsnull;      // no addref/release for views
  float p2t, t2p;

  result = GetScrollInfo(&view, &p2t, &t2p);

  if (view) {
    nscoord xPos, yPos;
    result = view->GetScrollPosition(xPos, yPos);
    if (NS_SUCCEEDED(result)) {
      result = ScrollTo(NSTwipsToIntPixels(xPos, t2p) + aXScrollDif,
                        NSTwipsToIntPixels(yPos, t2p) + aYScrollDif);
    }
  }

  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::ScrollByLines(PRInt32 numLines)
{
  nsresult result;
  nsIScrollableView *view = nsnull;   // no addref/release for views
  float p2t, t2p;

  result = GetScrollInfo(&view, &p2t, &t2p);
  if (view) {
    result = view->ScrollByLines(0, numLines);
  }

  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::ScrollByPages(PRInt32 numPages)
{
  nsresult result;
  nsIScrollableView *view = nsnull;   // no addref/release for views
  float p2t, t2p;

  result = GetScrollInfo(&view, &p2t, &t2p);
  if (view) {
    result = view->ScrollByPages(0, numPages);
  }

  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::ClearTimeout()
{
  return ClearTimeoutOrInterval();
}

NS_IMETHODIMP
GlobalWindowImpl::ClearInterval()
{
  return ClearTimeoutOrInterval();
}

NS_IMETHODIMP
GlobalWindowImpl::SetTimeout(PRBool *_retval)
{
  return SetTimeoutOrInterval(PR_FALSE, _retval);
}

NS_IMETHODIMP
GlobalWindowImpl::SetInterval(PRBool *_retval)
{
  return SetTimeoutOrInterval(PR_TRUE, _retval);
}

NS_IMETHODIMP
GlobalWindowImpl::SetResizable(PRBool aResizable)
{
  // nop

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::CaptureEvents(PRInt32 aEventFlags)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(manager)))) {
    manager->CaptureEvent(aEventFlags);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::ReleaseEvents(PRInt32 aEventFlags)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(manager)))) {
    manager->ReleaseEvent(aEventFlags);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::RouteEvent(nsIDOMEvent* aEvt)
{
  //XXX Not the best solution -joki
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::EnableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::DisableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

static
PRBool IsPopupBlocked(nsIDOMDocument* aDoc)
{
  PRBool blocked = PR_FALSE;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));
  nsCOMPtr<nsIPopupWindowManager> pm(do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID));
  if (pm && doc) {
    PRUint32 permission = nsIPopupWindowManager::ALLOW_POPUP;
    pm->TestPermission(doc->GetDocumentURI(), &permission);
    blocked = (permission == nsIPopupWindowManager::DENY_POPUP);
  }
  return blocked;
}

static
void FirePopupBlockedEvent(nsIDOMDocument* aDoc,
                           nsIURI *aRequestingURI, nsIURI *aPopupURI,
                           const nsAString &aPopupWindowFeatures)
{
  if (aDoc) {
    // Fire a "DOMPopupBlocked" event so that the UI can hear about blocked popups.
    nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(aDoc));
    nsCOMPtr<nsIDOMEvent> event;
    docEvent->CreateEvent(NS_LITERAL_STRING("PopupBlockedEvents"), getter_AddRefs(event));
    if (event) {
      nsCOMPtr<nsIDOMPopupBlockedEvent> pbev(do_QueryInterface(event));
      pbev->InitPopupBlockedEvent(NS_LITERAL_STRING("DOMPopupBlocked"),
              PR_TRUE, PR_TRUE, aRequestingURI, aPopupURI, aPopupWindowFeatures);
      PRBool noDefault;
      nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(aDoc));
      targ->DispatchEvent(event, &noDefault);
    }
  }
}

void FirePopupWindowEvent(nsIDOMDocument* aDoc)
{
  if (aDoc) {
    // Fire a "PopupWindow" event
    nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(aDoc));
    nsCOMPtr<nsIDOMEvent> event;
    docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
    if (event) {
      event->InitEvent(NS_LITERAL_STRING("PopupWindow"), PR_TRUE, PR_TRUE);
      PRBool noDefault;
      nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(aDoc));
      targ->DispatchEvent(event, &noDefault);
    }
  }
}

// static
PRBool
GlobalWindowImpl::CanSetProperty(const char *aPrefName)
{
  // Chrome can set any property.
  if (IsCallerChrome()) {
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
PRUint32
GlobalWindowImpl::CheckForAbusePoint()
{
  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(mDocShell));

  if (item) {
    PRInt32 type = nsIDocShellTreeItem::typeChrome;

    item->GetItemType(&type);
    if (type != nsIDocShellTreeItem::typeContent)
      return openAllowed;
  }

  PRUint32 abuse = openAllowed; // level of abuse we've detected

  // is the document being loaded or unloaded?
  if (!mIsDocumentLoaded)
    abuse = openAbused;

  /* Disallow windows after a user-defined click delay.
     This is a consideration secondary to document load because
     of its stronger response (openOverridden). See bug 247017. */
  if (abuse == openAllowed) {
    PRInt32 delay = nsContentUtils::GetIntPref("dom.disable_open_click_delay");
    if (delay != 0) {
      PRTime now = PR_Now();
      PRTime ll_delta;
      PRUint32 delta;
      LL_SUB(ll_delta, now, mLastMouseButtonAction);
      LL_L2UI(delta, ll_delta);
      if (delta/1000 > (PRUint32) delay)
        abuse = openOverridden;
    }
  }

  /* Is a timer running?
     This is a consideration secondary to the user-defined click delay
     because that seemed the Right Thing. See bug 197919 comment 45. */
  if (abuse == openAllowed && mRunningTimeout)
    abuse = openAbused;

  if (abuse == openAllowed) {
    // we'll need to know what DOM event is being processed now, if any
    nsEvent *currentEvent = mCurrentEvent;
    if (!currentEvent && mDocShell) {
      /* The DOM window's current event is accurate for events that make it
        all the way to the window. But it doesn't see events handled directly
        by a target element. For those, check the EventStateManager. */
      nsCOMPtr<nsIPresShell> presShell;
      mDocShell->GetPresShell(getter_AddRefs(presShell));
      if (presShell) {
        nsCOMPtr<nsIPresContext> presContext;
        presShell->GetPresContext(getter_AddRefs(presContext));
        if (presContext)
          presContext->EventStateManager()->GetCurrentEvent(&currentEvent);
      }
    }

    // fetch pref string detailing which events are allowed
    const nsAdoptingCString& eventPref =
      nsContentUtils::GetCharPref("dom.popup_allowed_events");

    // generally if an event handler is running, new windows are disallowed.
    // check for exceptions:
    if (currentEvent) {
      abuse = openAbused;
      switch(currentEvent->eventStructType) {
        case NS_EVENT :
          switch(currentEvent->message) {
            case NS_FORM_SELECTED :
              if (::ContainsEventName("select", eventPref))
                abuse = openControlled;
              break;
            case NS_FORM_CHANGE :
              if (::ContainsEventName("change", eventPref))
                abuse = openControlled;
              break;
            case NS_RESIZE_EVENT :
              if (::ContainsEventName("resize", eventPref))
                abuse = openControlled;
              break;
          }
          break;
        case NS_GUI_EVENT :
          switch(currentEvent->message) {
            case NS_FORM_INPUT :
              if (::ContainsEventName("input", eventPref))
                abuse = openControlled;
              break;
          }
          break;
        case NS_INPUT_EVENT :
          switch(currentEvent->message) {
            case NS_FORM_CHANGE :
              if (::ContainsEventName("change", eventPref))
                abuse = openControlled;
              break;
          }
          break;
        case NS_KEY_EVENT : {
          PRUint32 key = NS_STATIC_CAST(nsKeyEvent *, currentEvent)->keyCode;
          switch(currentEvent->message) {
            case NS_KEY_PRESS :
              // return key on focused button. see note at NS_MOUSE_LEFT_CLICK.
              if (key == nsIDOMKeyEvent::DOM_VK_RETURN)
                abuse = openAllowed;
              else if (::ContainsEventName("keypress", eventPref))
                abuse = openControlled;
              break;
            case NS_KEY_UP :
              // space key on focused button. see note at NS_MOUSE_LEFT_CLICK.
              if (key == nsIDOMKeyEvent::DOM_VK_SPACE)
                abuse = openAllowed;
              else if (::ContainsEventName("keyup", eventPref))
                abuse = openControlled;
              break;
            case NS_KEY_DOWN :
              if (::ContainsEventName("keydown", eventPref))
                abuse = openControlled;
              break;
          }
          break;
        }
        case NS_MOUSE_EVENT :
          switch(currentEvent->message) {
            case NS_MOUSE_LEFT_BUTTON_UP :
              if (::ContainsEventName("mouseup", eventPref))
                abuse = openControlled;
              break;
            case NS_MOUSE_LEFT_BUTTON_DOWN :
              if (::ContainsEventName("mousedown", eventPref))
                abuse = openControlled;
              break;
            case NS_MOUSE_LEFT_CLICK :
              /* Click events get special treatment because of their
                 historical status as a more legitimate event handler.
                 If click popups are enabled in the prefs, clear the
                 popup status completely. */
              if (::ContainsEventName("click", eventPref))
                abuse = openAllowed;
              break;
            case NS_MOUSE_LEFT_DOUBLECLICK :
              if (::ContainsEventName("dblclick", eventPref))
                abuse = openControlled;
              break;
          }
          break;
        case NS_SCRIPT_ERROR_EVENT :
          switch(currentEvent->message) {
            case NS_SCRIPT_ERROR :
              if (::ContainsEventName("error", eventPref))
                abuse = openControlled;
              break;
          }
          break;
        case NS_FORM_EVENT :
          switch(currentEvent->message) {
            case NS_FORM_SUBMIT :
              if (::ContainsEventName("submit", eventPref))
                abuse = openControlled;
              break;
            case NS_FORM_RESET :
              if (::ContainsEventName("reset", eventPref))
                abuse = openControlled;
              break;
          }
          break;
      } // switch
    } // currentEvent
  } // abuse == openAllowed

  // limit the number of simultaneously open popups
  if (abuse == openAbused || abuse == openControlled) {
    PRInt32 popupMax = nsContentUtils::GetIntPref("dom.popup_maximum", -1);
    if (popupMax >= 0 && gOpenPopupSpamCount >= popupMax)
      abuse = openOverridden;
  }

  return abuse;
}

/* Allow or deny a window open based on whether popups are suppressed.
   A popup generally will be allowed if it's from a white-listed domain,
   or if its target is an extant window.
   Returns a value from the CheckOpenAllow enum. */
PRUint32
GlobalWindowImpl::CheckOpenAllow(PRUint32 aAbuseLevel,
                                 const nsAString &aName)
{
  PRUint32 allowWindow = allowNoAbuse; // (also used for openControlled)
  
  if (aAbuseLevel >= openAbused) {
    allowWindow = allowNot;

    // However it might still not be blocked.
    if (aAbuseLevel == openAbused && !IsPopupBlocked(mDocument))
      allowWindow = allowWhitelisted;
    else {
      // Special case items that don't actually open new windows.
      nsAutoString name(aName);
      if (!name.IsEmpty()) {
        // _main is an IE target which should be case-insensitive but isn't
        // see bug 217886 for details
        if (name.LowerCaseEqualsLiteral("_top") ||
            name.LowerCaseEqualsLiteral("_self") ||
            name.LowerCaseEqualsLiteral("_content") ||
            name.EqualsLiteral("_main"))
          allowWindow = allowSelf;
        else {
          nsCOMPtr<nsIWindowWatcher> wwatch =
              do_GetService(NS_WINDOWWATCHER_CONTRACTID);
          if (wwatch) {
            nsCOMPtr<nsIDOMWindow> namedWindow;
            wwatch->GetWindowByName(PromiseFlatString(aName).get(), this,
                                    getter_AddRefs(namedWindow));
            if (namedWindow)
              allowWindow = allowExtant;
          }
        }
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
GlobalWindowImpl::FireAbuseEvents(PRBool aBlocked, PRBool aWindow,
                                  const nsAString &aPopupURL,
                                  const nsAString &aPopupWindowFeatures)
{
  // fetch the URI of the window requesting the opened window

  nsCOMPtr<nsIDOMWindow> topWindow;
  GetTop(getter_AddRefs(topWindow));
  if (!topWindow)
    return;

  nsCOMPtr<nsIDOMDocument> topDoc;
  topWindow->GetDocument(getter_AddRefs(topDoc));

  nsCOMPtr<nsIURI> requestingURI;
  nsCOMPtr<nsIURI> popupURI;
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(topWindow));
  if (webNav)
    webNav->GetCurrentURI(getter_AddRefs(requestingURI));

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
    ios->NewURI(NS_ConvertUCS2toUTF8(aPopupURL), 0, baseURL,
                getter_AddRefs(popupURI));

  // fire an event chock full of informative URIs
  if (aBlocked)
    FirePopupBlockedEvent(topDoc, requestingURI, popupURI, aPopupWindowFeatures);
  if (aWindow)
    FirePopupWindowEvent(topDoc);
}

NS_IMETHODIMP
GlobalWindowImpl::Open(const nsAString& aUrl,
                       const nsAString& aName,
                       const nsAString& aOptions,
                       nsIDOMWindow **_retval)
{
  nsresult rv;

  PRUint32 abuseLevel = CheckForAbusePoint();
  PRUint32 allowReason = CheckOpenAllow(abuseLevel, aName);
  if (allowReason == allowNot) {
    FireAbuseEvents(PR_TRUE, PR_FALSE, aUrl, aOptions);
    return NS_ERROR_FAILURE; // unlike the public Open method, return an error
  }

  rv = OpenInternal(aUrl, aName, aOptions, PR_FALSE, nsnull, 0, nsnull,
                    _retval);
  if (NS_SUCCEEDED(rv)) {
    if (abuseLevel >= openControlled && allowReason != allowSelf) {
      GlobalWindowImpl *opened = NS_STATIC_CAST(GlobalWindowImpl*, *_retval);
      if (!opened->IsPopupSpamWindow()) {
        opened->SetPopupSpamWindow(PR_TRUE);
        ++gOpenPopupSpamCount;
      }
    }
    if (abuseLevel >= openAbused)
      FireAbuseEvents(PR_FALSE, PR_TRUE, aUrl, aOptions);
  }
  return rv;
}

NS_IMETHODIMP
GlobalWindowImpl::Open(nsIDOMWindow **_retval)
{
  *_retval = nsnull;

  NS_ENSURE_STATE(sXPConnect);
  nsresult rv = NS_OK;

  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = sXPConnect->GetCurrentNativeCallContext(getter_AddRefs(ncc));
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
    nsJSUtils::ConvertJSValToString(url, cx, argv[0]);

    if (argc > 1) {
      nsJSUtils::ConvertJSValToString(name, cx, argv[1]);

      if (argc > 2) {
        nsJSUtils::ConvertJSValToString(options, cx, argv[2]);
      }
    }
  }

  PRUint32 abuseLevel = CheckForAbusePoint();
  PRUint32 allowReason = CheckOpenAllow(abuseLevel, name);
  if (allowReason == allowNot) {
    FireAbuseEvents(PR_TRUE, PR_FALSE, url, options);
    return NS_OK; // don't open the window, but also don't throw a JS exception
  }

  rv = OpenInternal(url, name, options, PR_FALSE, nsnull, 0, nsnull, _retval);

  nsCOMPtr<nsIDOMChromeWindow> chrome_win(do_QueryInterface(*_retval));

  if (NS_SUCCEEDED(rv)) {
    if (!chrome_win) {
    // A new non-chrome window was created from a call to
    // window.open() from JavaScript, make sure there's a document in
    // the new window. We do this by simply asking the new window for
    // its document, this will synchronously create an empty document
    // if there is no document in the window.

#ifdef DEBUG_jst
    {
      nsCOMPtr<nsPIDOMWindow> pidomwin(do_QueryInterface(*_retval));

      nsIDOMDocument *temp = pidomwin->GetExtantDocument();

      NS_ASSERTION(temp, "No document in new window!!!");
    }
#endif

      nsCOMPtr<nsIDOMDocument> doc;
      (*_retval)->GetDocument(getter_AddRefs(doc));
    }
    
    if (abuseLevel >= openControlled && allowReason != allowSelf) {
      GlobalWindowImpl *opened = NS_STATIC_CAST(GlobalWindowImpl*, *_retval);
      if (!opened->IsPopupSpamWindow()) {
        opened->SetPopupSpamWindow(PR_TRUE);
        ++gOpenPopupSpamCount;
      }
    }
    if (abuseLevel >= openAbused)
      FireAbuseEvents(PR_FALSE, PR_TRUE, url, options);
  }

  return rv;
}

// like Open, but attaches to the new window any extra parameters past
// [features] as a JS property named "arguments"
NS_IMETHODIMP
GlobalWindowImpl::OpenDialog(const nsAString& aUrl,
                             const nsAString& aName,
                             const nsAString& aOptions,
                             nsISupports* aExtraArgument,
                             nsIDOMWindow** _retval)
{
  return OpenInternal(aUrl, aName, aOptions, PR_TRUE, nsnull, 0,
                      aExtraArgument, _retval);
}

NS_IMETHODIMP
GlobalWindowImpl::OpenDialog(nsIDOMWindow** _retval)
{
  if (!IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  NS_ENSURE_STATE(sXPConnect);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = sXPConnect->GetCurrentNativeCallContext(getter_AddRefs(ncc));
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
    nsJSUtils::ConvertJSValToString(url, cx, argv[0]);

    if (argc > 1) {
      nsJSUtils::ConvertJSValToString(name, cx, argv[1]);

      if (argc > 2) {
        nsJSUtils::ConvertJSValToString(options, cx, argv[2]);
      }
    }
  }

  return OpenInternal(url, name, options, PR_TRUE, argv, argc, nsnull,
                      _retval);
}

NS_IMETHODIMP
GlobalWindowImpl::GetFrames(nsIDOMWindow** aFrames)
{
  *aFrames = this;
  NS_ADDREF(*aFrames);

  FlushPendingNotifications(Flush_ContentAndNotify);

  return NS_OK;
}

struct nsCloseEvent : public PLEvent {
  nsCloseEvent (GlobalWindowImpl *aWindow)
    : mWindow(aWindow)
  {
  }
 
  void HandleEvent() {
    if (mWindow)
      mWindow->ReallyCloseWindow();
  }

  nsresult PostCloseEvent();

  nsRefPtr<GlobalWindowImpl> mWindow;
};

static void PR_CALLBACK HandleCloseEvent(nsCloseEvent* aEvent)
{
  aEvent->HandleEvent();
}
static void PR_CALLBACK DestroyCloseEvent(nsCloseEvent* aEvent)
{
  delete aEvent;
}

nsresult
nsCloseEvent::PostCloseEvent()
{
  nsCOMPtr<nsIEventQueueService> eventService(do_GetService(kEventQueueServiceCID));
  if (eventService) {
    nsCOMPtr<nsIEventQueue> eventQueue;  
    eventService->GetThreadEventQueue(PR_GetCurrentThread(), getter_AddRefs(eventQueue));
    if (eventQueue) {

      PL_InitEvent(this, nsnull, (PLHandleEventProc) ::HandleCloseEvent, (PLDestroyEventProc) ::DestroyCloseEvent);
      return eventQueue->PostEvent(this);
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::Close()
{
  if (IsFrame() || !mDocShell) {
    // window.close() is called on a frame in a frameset, or on a
    // window that's already closed. Ignore such calls.

    return NS_OK;
  }

  // Don't allow scripts from content to close windows
  // that were not opened by script
  nsresult rv;
  if (!mOpener && !mOpenerWasCleared) {
    nsCOMPtr<nsIScriptSecurityManager> secMan(
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) {
      PRBool inChrome = PR_TRUE;
      rv = secMan->SubjectPrincipalIsSystem(&inChrome);
      if (NS_SUCCEEDED(rv) && !inChrome) {
        PRBool allowClose =
          nsContentUtils::GetBoolPref("dom.allow_scripts_to_close_windows",
                                      PR_TRUE);
        if (!allowClose) {
          // We're blocking the close operation
          // report localized error msg in JS console
          nsCOMPtr<nsIStringBundleService> stringBundleService(
            do_GetService(kCStringBundleServiceCID));
          if (stringBundleService) {
            nsCOMPtr<nsIStringBundle> stringBundle;
            rv = stringBundleService->CreateBundle(
                                        kDOMSecurityWarningsBundleURL,
                                        getter_AddRefs(stringBundle));
            if (NS_SUCCEEDED(rv) && stringBundle) {
              nsXPIDLString errorMsg;
              rv = stringBundle->GetStringFromName(
                                   NS_LITERAL_STRING("WindowCloseBlockedWarning").get(),
                                   getter_Copies(errorMsg));
              if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsIConsoleService> console(
                  do_GetService("@mozilla.org/consoleservice;1"));
                if (console)
                  console->LogStringMessage(errorMsg.get());
              }
            }
          }

          return NS_OK;
        }
      }
    }
  }

  // Ask the content viewer whether the toplevel window can close.
  // If the content viewer returns false, it is responsible for calling
  // Close() as soon as it is possible for the window to close.
  // This allows us to not close the window while printing is happening.

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
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

  nsCOMPtr<nsIDOMDocumentEvent> doc(do_QueryInterface(mDocument));
  nsCOMPtr<nsIDOMEvent> event;

  if (doc) {
    doc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  }

  if (event) {
    event->InitEvent(NS_LITERAL_STRING("DOMWindowClose"), PR_TRUE, PR_TRUE);

    PRBool executeDefault = PR_TRUE;
    DispatchEvent(event, &executeDefault);

    if (!executeDefault) {
      // Someone chose to prevent the default action for this event,
      // if so, let's not close this window after all...

      return NS_OK;
    }
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
      currentCX->SetTerminationFunction(CloseWindow,
                                        NS_STATIC_CAST(nsIDOMWindow *,
                                                       this));
      return NS_OK;
    }
  }

  
  // We may have plugins on the page that have issued this close from their
  // event loop and because we currently destroy the plugin window with
  // frames, we crash. So, if we are called from Javascript, post an event
  // to really close the window.
  rv = NS_ERROR_FAILURE;
  if (!IsCallerChrome()) {
    nsCloseEvent *ev = new nsCloseEvent(this);

    if (ev) {
      rv = ev->PostCloseEvent();

      if (NS_FAILED(rv)) {
        PL_DestroyEvent(ev);
      }
    } else rv = NS_ERROR_OUT_OF_MEMORY;
  }
  
  if (NS_FAILED(rv)) {
    ReallyCloseWindow();
    rv = NS_OK;
  }
  
  return rv;
}


void
GlobalWindowImpl::ReallyCloseWindow()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  // If there's no treeOwnerAsWin, this window must already be closed.

  if (treeOwnerAsWin) {
    treeOwnerAsWin->Destroy();
    CleanUp();
  }
}

NS_IMETHODIMP
GlobalWindowImpl::GetFrameElement(nsIDOMElement** aFrameElement)
{
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
GlobalWindowImpl::UpdateCommands(const nsAString& anAction)
{
  nsIDOMWindowInternal *rootWindow = GlobalWindowImpl::GetPrivateRoot();
  if (!rootWindow)
    return NS_OK;

  nsCOMPtr<nsIDOMDocument> document;
  rootWindow->GetDocument(getter_AddRefs(document));

  if (document) {
    // See if we contain a XUL document.
    nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(document);
    if (xulDoc) {
      // Retrieve the command dispatcher and call updateCommands on it.
      nsCOMPtr<nsIDOMXULCommandDispatcher> xulCommandDispatcher;
      xulDoc->GetCommandDispatcher(getter_AddRefs(xulCommandDispatcher));
      xulCommandDispatcher->UpdateCommands(anAction);
    }
  }

  return NS_OK;
}

nsresult
GlobalWindowImpl::ConvertCharset(const nsAString& aStr,
                                 char** aDest)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIUnicodeEncoder> encoder;

  nsCOMPtr<nsICharsetConverterManager>
    ccm(do_GetService(kCharsetConverterManagerCID));
  NS_ENSURE_TRUE(ccm, NS_ERROR_FAILURE);

  // Get the document character set
  nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8")); // default to utf-8
  if (mDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));

    if (doc)
      charset = doc->GetDocumentCharacterSet();
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
GlobalWindowImpl::GetBlurSuppression()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  PRBool suppress = PR_FALSE;
  if (treeOwnerAsWin)
    treeOwnerAsWin->GetBlurSuppression(&suppress);
  return suppress;
}

NS_IMETHODIMP
GlobalWindowImpl::GetSelection(nsISelection** aSelection)
{
  NS_ENSURE_ARG_POINTER(aSelection);
  *aSelection = nsnull;

  if (!mDocShell)
    return NS_OK;

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));

  if (!presShell)
    return NS_OK;

  nsCOMPtr<nsIFrameSelection> selection;
  presShell->GetFrameSelection(getter_AddRefs(selection));

  if (!selection)
    return NS_OK;

  return selection->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                 aSelection);
}

// Non-scriptable version of window.find(), part of nsIDOMWindowInternal
NS_IMETHODIMP
GlobalWindowImpl::Find(const nsAString& aStr,
                       PRBool aCaseSensitive,
                       PRBool aBackwards,
                       PRBool aWrapAround,
                       PRBool aWholeWord,
                       PRBool aSearchInFrames,
                       PRBool aShowDialog,
                       PRBool *aDidFind)
{
  return FindInternal(aStr, aCaseSensitive, aBackwards, aWrapAround,
                      aWholeWord, aSearchInFrames, aShowDialog, aDidFind);
}

// Scriptable version of window.find() which takes a variable number of
// arguments, part of nsIDOMJSWindow.
NS_IMETHODIMP
GlobalWindowImpl::Find(PRBool *aDidFind)
{
  NS_ENSURE_STATE(sXPConnect);
  nsresult rv = NS_OK;

  // We get the arguments passed to the function using the XPConnect native
  // call context.
  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = sXPConnect->GetCurrentNativeCallContext(getter_AddRefs(ncc));
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
    // First arg is the search pattern
    nsJSUtils::ConvertJSValToString(searchStr, cx, argv[0]);
  }

  if (argc > 1 && !JS_ValueToBoolean(cx, argv[1], &caseSensitive)) {
    // Second arg is the case sensitivity
    caseSensitive = PR_FALSE;
  }

  if (argc > 2 && !JS_ValueToBoolean(cx, argv[2], &backwards)) {
    // Third arg specifies whether to search backwards
    backwards = PR_FALSE;
  }

  if (argc > 3 && !JS_ValueToBoolean(cx, argv[3], &wrapAround)) {
    // Fourth arg specifies whether we should wrap the search
    wrapAround = PR_FALSE;
  }

  if (argc > 4 && !JS_ValueToBoolean(cx, argv[4], &wholeWord)) {
    // Fifth arg specifies whether we should show the Find dialog
    wholeWord = PR_FALSE;
  }

  if (argc > 5 && !JS_ValueToBoolean(cx, argv[5], &searchInFrames)) {
    // Sixth arg specifies whether we should search only for whole words
    searchInFrames = PR_FALSE;
  }

  if (argc > 6 && !JS_ValueToBoolean(cx, argv[6], &showDialog)) {
    // Seventh arg specifies whether we should search in all frames
    showDialog = PR_FALSE;
  }

  return FindInternal(searchStr, caseSensitive, backwards, wrapAround,
                      wholeWord, searchInFrames, showDialog, aDidFind);
}

nsresult
GlobalWindowImpl::FindInternal(const nsAString& aStr,
                               PRBool caseSensitive,
                               PRBool backwards,
                               PRBool wrapAround,
                               PRBool wholeWord,
                               PRBool searchInFrames,
                               PRBool showDialog,
                               PRBool *aDidFind)
{
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
      do_GetService(kWindowMediatorCID);

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
GlobalWindowImpl::Atob(const nsAString& aAsciiBase64String,
                       nsAString& aBinaryData)
{
  aBinaryData.Truncate();

  if (!Is8bit(aAsciiBase64String)) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  char *base64 = ToNewCString(aAsciiBase64String);
  if (!base64) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 dataLen = aAsciiBase64String.Length();

  PRInt32 resultLen = dataLen;
  if (base64[dataLen - 1] == '=') {
    if (base64[dataLen - 2] == '=') {
      resultLen = dataLen - 2;
    } else {
      resultLen = dataLen - 1;
    }
  }

  resultLen = ((resultLen * 3) / 4);

  char *bin_data = PL_Base64Decode(base64, aAsciiBase64String.Length(),
                                   nsnull);
  if (!bin_data) {
    nsMemory::Free(base64);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  CopyASCIItoUCS2(Substring(bin_data, bin_data + resultLen), aBinaryData);

  nsMemory::Free(base64);
  PR_Free(bin_data);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Btoa(const nsAString& aBinaryData,
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

  CopyASCIItoUCS2(nsDependentCString(base64, resultLen), aAsciiBase64String);

  PR_Free(base64);
  nsMemory::Free(bin_data);

  return NS_OK;
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOMEventTarget
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::AddEventListener(const nsAString& aType,
                                   nsIDOMEventListener* aListener,
                                   PRBool aUseCapture)
{
  return AddGroupedEventListener(aType, aListener, aUseCapture, nsnull);
}

NS_IMETHODIMP
GlobalWindowImpl::RemoveEventListener(const nsAString& aType,
                                      nsIDOMEventListener* aListener,
                                      PRBool aUseCapture)
{
  return RemoveGroupedEventListener(aType, aListener, aUseCapture, nsnull);
}

NS_IMETHODIMP
GlobalWindowImpl::DispatchEvent(nsIDOMEvent* aEvent, PRBool* _retval)
{
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  if (doc) {
    // Obtain a presentation context
    nsIPresShell *shell = doc->GetShellAt(0);
    if (!shell) {
      return NS_OK;
    }

    // Retrieve the context
    nsCOMPtr<nsIPresContext> aPresContext;
    shell->GetPresContext(getter_AddRefs(aPresContext));
    aPresContext->EventStateManager()->
      DispatchNewEvent(NS_STATIC_CAST(nsIScriptGlobalObject*, this),
                       aEvent, _retval);
  }

  return NS_ERROR_FAILURE;
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOM3EventTarget
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::AddGroupedEventListener(const nsAString & aType,
                                          nsIDOMEventListener *aListener,
                                          PRBool aUseCapture,
                                          nsIDOMEventGroup *aEvtGrp)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(manager)))) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags, aEvtGrp);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::RemoveGroupedEventListener(const nsAString & aType,
                                             nsIDOMEventListener *aListener,
                                             PRBool aUseCapture,
                                             nsIDOMEventGroup *aEvtGrp)
{
  if (mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags, aEvtGrp);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::CanTrigger(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GlobalWindowImpl::IsRegisteredHere(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOMEventReceiver
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::AddEventListenerByIID(nsIDOMEventListener* aListener,
                                        const nsIID& aIID)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED (GetListenerManager(getter_AddRefs(manager)))) {
    manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::RemoveEventListenerByIID(nsIDOMEventListener* aListener,
                                           const nsIID& aIID)
{
  if (mListenerManager) {
    mListenerManager->RemoveEventListenerByIID(aListener, aIID,
                                               NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::GetListenerManager(nsIEventListenerManager **aResult)
{
  if (!mListenerManager) {
    static NS_DEFINE_CID(kEventListenerManagerCID,
                         NS_EVENTLISTENERMANAGER_CID);
    nsresult rv;

    mListenerManager = do_CreateInstance(kEventListenerManagerCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return CallQueryInterface(mListenerManager, aResult);
}

NS_IMETHODIMP
GlobalWindowImpl::HandleEvent(nsIDOMEvent *aEvent)
{
  PRBool noDefault;
  return DispatchEvent(aEvent, &noDefault);
}

NS_IMETHODIMP
GlobalWindowImpl::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(manager))) && manager) {
    return manager->GetSystemEventGroupLM(aGroup);
  }
  return NS_ERROR_FAILURE;
}

//*****************************************************************************
// GlobalWindowImpl::nsPIDOMWindow
//*****************************************************************************

nsPIDOMWindow*
GlobalWindowImpl::GetPrivateParent()
{
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
    return NS_STATIC_CAST(GlobalWindowImpl*,
                          NS_STATIC_CAST(nsIDOMWindow*, parent.get()));
  }

  return nsnull;
}

nsPIDOMWindow*
GlobalWindowImpl::GetPrivateRoot()
{
  nsCOMPtr<nsIDOMWindow> parent;
  GetTop(getter_AddRefs(parent));

  nsCOMPtr<nsIScriptGlobalObject> parentTop = do_QueryInterface(parent);
  NS_ASSERTION(parentTop, "cannot get parentTop");
  if (!parentTop)
    return nsnull;

  nsIDocShell *docShell = parentTop->GetDocShell();

  // Get the chrome event handler from the doc shell, since we only
  // want to deal with XUL chrome handlers and not the new kind of
  // window root handler.
  nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;
  docShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));

  nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
  if (chromeElement) {
    nsIDocument* doc = chromeElement->GetDocument();
    if (doc) {
      parent = do_QueryInterface(doc->GetScriptGlobalObject());
      nsCOMPtr<nsIDOMWindow> tempParent;
      parent->GetTop(getter_AddRefs(tempParent));
      return NS_STATIC_CAST(GlobalWindowImpl*,
                            NS_STATIC_CAST(nsIDOMWindow*, tempParent));
    }
  }

  if (parent) {
    return NS_STATIC_CAST(GlobalWindowImpl*,
                          NS_STATIC_CAST(nsIDOMWindow*, parent));
  }

  return nsnull;
}


NS_IMETHODIMP
GlobalWindowImpl::GetLocation(nsIDOMLocation ** aLocation)
{
  *aLocation = nsnull;

  if (!mLocation && mDocShell) {
    mLocation = new LocationImpl(mDocShell);
    if (!mLocation) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_IF_ADDREF(*aLocation = mLocation);
  
  return NS_OK;
}

nsresult
GlobalWindowImpl::GetObjectProperty(const PRUnichar *aProperty,
                                    nsISupports ** aObject)
{
  NS_ENSURE_TRUE(mJSObject, NS_ERROR_NOT_AVAILABLE);

  // Get JSContext from stack.
  nsCOMPtr<nsIThreadJSContextStack> stack(do_GetService(sJSStackContractID));
  NS_ENSURE_TRUE(stack, NS_ERROR_FAILURE);

  JSContext *cx;
  NS_ENSURE_SUCCESS(stack->Peek(&cx), NS_ERROR_FAILURE);

  if (!cx) {
    stack->GetSafeJSContext(&cx);
    NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);
  }

  jsval propertyVal;

  if (!::JS_LookupUCProperty(cx, mJSObject,
                             NS_REINTERPRET_CAST(const jschar *, aProperty),
                             nsCRT::strlen(aProperty), &propertyVal)) {
    return NS_ERROR_FAILURE;
  }

  if (!nsJSUtils::ConvertJSValToXPCObject(aObject, NS_GET_IID(nsISupports),
                                          cx, propertyVal)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
GlobalWindowImpl::Activate()
{
/*
   nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
   GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
   if(treeOwnerAsWin)
      treeOwnerAsWin->SetVisibility(PR_TRUE);

   nsCOMPtr<nsIPresShell> presShell;
   mDocShell->GetPresShell(getter_AddRefs(presShell));
   NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

   nsIViewManager* vm = presShell->GetViewManager();
   NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

   nsIView* rootView;
   vm->GetRootView(rootView);
   NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

   nsIWidget* widget = rootView->GetWidget();
   NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

   return widget->SetFocus();

 */
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

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell) {
    NS_WARNING( "no preshell for window" );
    return NS_ERROR_FAILURE;
  }

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

  nsGUIEvent guiEvent(NS_ACTIVATE, widget);
  guiEvent.time = PR_IntervalNow();

  vm->DispatchEvent(&guiEvent, &status);

  return NS_OK;
}

nsresult
GlobalWindowImpl::Deactivate()
{
  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsIViewManager* vm = presShell->GetViewManager();
  NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

  nsIView *rootView;
  vm->GetRootView(rootView);
  NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

  // Hold a STRONG REF to keep the widget around during event processing
  nsCOMPtr<nsIWidget> widget = rootView->GetWidget();
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  nsEventStatus status;

  nsGUIEvent guiEvent(NS_DEACTIVATE, widget);
  guiEvent.time = PR_IntervalNow();

  vm->DispatchEvent(&guiEvent, &status);

  return NS_OK;
}

nsIFocusController*
GlobalWindowImpl::GetRootFocusController()
{
  nsIDOMWindowInternal *rootWindow = GlobalWindowImpl::GetPrivateRoot();
  if (rootWindow) {
    // Obtain the chrome event handler.
    nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));
    nsIChromeEventHandler *chromeHandler = piWin->GetChromeEventHandler();
    if (chromeHandler) {
      nsCOMPtr<nsPIWindowRoot> windowRoot(do_QueryInterface(chromeHandler));
      if (windowRoot) {
        nsCOMPtr<nsIFocusController> fc;
        windowRoot->GetFocusController(getter_AddRefs(fc));
        return fc;  // this reference is going away, but the root holds onto it
      }
    }
  }
  return nsnull;
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOMViewCSS
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::GetComputedStyle(nsIDOMElement* aElt,
                                   const nsAString& aPseudoElt,
                                   nsIDOMCSSStyleDeclaration** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (!aElt) {
    NS_ERROR("Don't call getComputedStyle with a null DOMElement reference!");
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
    nsCID cid;
    rv = nsComponentManager::
      ContractIDToClassID("@mozilla.org/DOM/Level2/CSS/computedStyleDeclaration;1",
                          &cid);

    NS_ENSURE_SUCCESS(rv, rv);

    rv = nsComponentManager::
      GetClassObject(cid, NS_GET_IID(nsIFactory),
                     (void **)&sComputedDOMStyleFactory);

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
// GlobalWindowImpl::nsIDOMAbstractView
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::GetDocument(nsIDOMDocumentView ** aDocumentView)
{
  NS_ENSURE_ARG_POINTER(aDocumentView);

  nsresult rv = NS_OK;

  if (mDocument) {
    rv = mDocument->QueryInterface(NS_GET_IID(nsIDOMDocumentView),
                                   (void **) aDocumentView);
  }
  else {
    *aDocumentView = nsnull;
  }

  return rv;
}

///*****************************************************************************
// GlobalWindowImpl::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::GetInterface(const nsIID & aIID, void **aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);
  *aSink = nsnull;

  if (aIID.Equals(NS_GET_IID(nsIDocCharset))) {
    if (mDocShell) {
      nsCOMPtr<nsIDocCharset> docCharset(do_QueryInterface(mDocShell));
      if (docCharset) {
        *aSink = docCharset;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebNavigation))) {
    if (mDocShell) {
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
      if (webNav) {
        *aSink = webNav;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
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
  else if (aIID.Equals(NS_GET_IID(nsIScriptEventManager))) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    if (doc) {
      nsIScriptEventManager* mgr = doc->GetScriptEventManager();
      if (mgr) {
        *aSink = mgr;
        NS_ADDREF(((nsISupports *) *aSink));
      }
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIDOMWindowUtils))) {
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

  return NS_OK;
}

//*****************************************************************************
// GlobalWindowImpl: Window Control Functions
//*****************************************************************************

nsIDOMWindowInternal *
GlobalWindowImpl::GetParentInternal()
{
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

NS_IMETHODIMP
GlobalWindowImpl::OpenInternal(const nsAString& aUrl,
                               const nsAString& aName,
                               const nsAString& aOptions,
                               PRBool aDialog, jsval *argv, PRUint32 argc,
                               nsISupports *aExtraArgument,
                               nsIDOMWindow **aReturn)
{
  nsXPIDLCString url;
  nsresult rv = NS_OK;  

  *aReturn = nsnull;

  if (!aUrl.IsEmpty()) {
    // fix bug 35076
    // Escape all non ASCII characters in the url.
    // Earlier, this code used to call Escape() and would escape characters like
    // '?', '&', and '=' in the url.  This caused bug 174628.
    if (IsASCII(aUrl)) {
      AppendUTF16toUTF8(aUrl, url);
    }
    else {
      nsXPIDLCString dest;
      rv = ConvertCharset(aUrl, getter_Copies(dest));
      if (NS_SUCCEEDED(rv))
        NS_EscapeURL(dest, esc_AlwaysCopy | esc_OnlyNonASCII, url);      
      else
        AppendUTF16toUTF8(aUrl, url);
    }

    /* Check whether the URI is allowed, but not for dialogs --
       see bug 56851. The security of this function depends on
       window.openDialog being inaccessible from web scripts */
    if (url.get() && !aDialog)
      rv = SecurityCheckURL(url.get());
  }

  nsCOMPtr<nsIDOMWindow> domReturn;

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWindowWatcher> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);

    if (wwatch) {
      NS_ConvertUCS2toUTF8 options(aOptions);
      NS_ConvertUCS2toUTF8 name(aName);

      const char *options_ptr = aOptions.IsEmpty() ? nsnull : options.get();
      const char *name_ptr = aName.IsEmpty() ? nsnull : name.get();

      if (argc) {
        nsCOMPtr<nsPIWindowWatcher> pwwatch(do_QueryInterface(wwatch));
        NS_ENSURE_TRUE(pwwatch, NS_ERROR_UNEXPECTED);

        PRUint32 extraArgc = argc >= 3 ? argc - 3 : 0;
        rv = pwwatch->OpenWindowJS(this, url.get(), name_ptr, options_ptr,
                                   aDialog, extraArgc, argv + 3,
                                   getter_AddRefs(domReturn));
      } else {
        rv = wwatch->OpenWindow(this, url.get(), name_ptr, options_ptr,
                                aExtraArgument, getter_AddRefs(domReturn));
      }

      if (domReturn) {
        CallQueryInterface(domReturn, aReturn);

        // Save the prinicpal of the calling script
        // We need it to decide whether to clear the scope in SetNewDocument
        NS_ASSERTION(sSecMan, "No Security Manager Found!");
        if (sSecMan) {
          nsCOMPtr<nsIPrincipal> principal;
          sSecMan->GetSubjectPrincipal(getter_AddRefs(principal));
          if (principal) {
            nsCOMPtr<nsIURI> subjectURI;
            principal->GetURI(getter_AddRefs(subjectURI));
            if (subjectURI) {
              nsCOMPtr<nsPIDOMWindow> domReturnPrivate(do_QueryInterface(domReturn));
              domReturnPrivate->SetOpenerScriptURL(subjectURI);
            }
          }
        }
      }
    }
  }

  return rv;
}

void
GlobalWindowImpl::CloseWindow(nsISupports *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));

  NS_STATIC_CAST(GlobalWindowImpl*,
                 NS_STATIC_CAST(nsPIDOMWindow*, win))->ReallyCloseWindow();
}

//*****************************************************************************
// GlobalWindowImpl: Timeout Functions
//*****************************************************************************

static const char kSetIntervalStr[] = "setInterval";
static const char kSetTimeoutStr[] = "setTimeout";

nsresult
GlobalWindowImpl::SetTimeoutOrInterval(PRBool aIsInterval, PRInt32 *aReturn)
{
  if (!mContext) {
    // This window was already closed, or never properly initialized,
    // don't let a timer be scheduled on such a window.

    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ENSURE_STATE(sXPConnect);

  nsCOMPtr<nsIXPCNativeCallContext> ncc;
  nsresult rv = sXPConnect->GetCurrentNativeCallContext(getter_AddRefs(ncc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc;
  jsval *argv = nsnull;

  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  JSString *expr = nsnull;
  JSObject *funobj = nsnull;
  nsTimeoutImpl *timeout;
  jsdouble interval = 0.0;
  PRInt64 now, delta;

  if (argc < 1) {
    ::JS_ReportError(cx, "Function %s requires at least 1 parameter",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);

    return ncc->SetExceptionWasThrown(PR_TRUE);
  }

  if (argc > 1 && !::JS_ValueToNumber(cx, argv[1], &interval)) {
    ::JS_ReportError(cx,
                     "Second argument to %s must be a millisecond interval",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);

    return ncc->SetExceptionWasThrown(PR_TRUE);
  }

  switch (::JS_TypeOfValue(cx, argv[0])) {
  case JSTYPE_FUNCTION:
    funobj = JSVAL_TO_OBJECT(argv[0]);
    break;

  case JSTYPE_STRING:
  case JSTYPE_OBJECT:
    expr = ::JS_ValueToString(cx, argv[0]);
    if (!expr)
      return NS_ERROR_OUT_OF_MEMORY;
    argv[0] = STRING_TO_JSVAL(expr);
    break;

  default:
    ::JS_ReportError(cx, "useless %s call (missing quotes around argument?)",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);

    return ncc->SetExceptionWasThrown(PR_TRUE);
  }

  if (interval < DOM_MIN_TIMEOUT_VALUE) {
    // Don't allow timeouts less than DOM_MIN_TIMEOUT_VALUE from
    // now...

    interval = DOM_MIN_TIMEOUT_VALUE;
  }

  timeout = new nsTimeoutImpl();
  if (!timeout)
    return NS_ERROR_OUT_OF_MEMORY;

  // Increment the timeout's reference count to indicate that this
  // timeout struct will be held as the closure of a timer.
  timeout->AddRef();
  if (aIsInterval) {
    timeout->mInterval = (PRInt32)interval;
  }

  if (expr) {
    if (!::JS_AddNamedRoot(cx, &timeout->mExpr, "timeout.mExpr")) {
      timeout->Release(mContext);

      return NS_ERROR_OUT_OF_MEMORY;
    }

    timeout->mExpr = expr;
  } else if (funobj) {
    /* Leave an extra slot for a secret final argument that
       indicates to the called function how "late" the timeout is. */
    timeout->mArgv = (jsval *) PR_MALLOC((argc - 1) * sizeof(jsval));

    if (!timeout->mArgv) {
      timeout->Release(mContext);

      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!::JS_AddNamedRoot(cx, &timeout->mFunObj, "timeout.mFunObj")) {
      timeout->Release(mContext);

      return NS_ERROR_FAILURE;
    }

    timeout->mFunObj = funobj;
    timeout->mArgc = 0;

    for (PRInt32 i = 2; (PRUint32)i < argc; ++i) {
      timeout->mArgv[i - 2] = argv[i];

      if (!::JS_AddNamedRoot(cx, &timeout->mArgv[i - 2], "timeout.mArgv[i]")) {
        timeout->Release(mContext);

        return NS_ERROR_FAILURE;
      }

      timeout->mArgc++;
    }
  }

  const char *filename;
  if (nsJSUtils::GetCallingLocation(cx, &filename, &timeout->mLineNo)) {
    timeout->mFileName = PL_strdup(filename);

    if (!timeout->mFileName) {
      timeout->Release(mContext);

      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  timeout->mVersion = ::JS_VersionToString(::JS_GetVersion(cx));

  // Get principal of currently executing code, save for execution of timeout

  rv = sSecMan->GetSubjectPrincipal(getter_AddRefs(timeout->mPrincipal));

  if (NS_FAILED(rv)) {
    timeout->Release(mContext);

    return NS_ERROR_FAILURE;
  }

  LL_I2L(now, PR_IntervalNow());
  LL_D2L(delta, PR_MillisecondsToInterval((PRUint32)interval));
  LL_ADD(timeout->mWhen, now, delta);

  timeout->mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) {
    timeout->Release(mContext);

    return rv;
  }

  rv = timeout->mTimer->InitWithFuncCallback(TimerCallback, timeout,
                                             (PRInt32)interval,
                                             nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    timeout->Release(mContext);

    return rv;
  }

  timeout->mWindow = this;
  NS_ADDREF(timeout->mWindow);

  InsertTimeoutIntoList(mTimeoutInsertionPoint, timeout);
  timeout->mPublicId = ++mTimeoutPublicIdCounter;
  *aReturn = timeout->mPublicId;

  return NS_OK;
}

// static
void
GlobalWindowImpl::RunTimeout(nsTimeoutImpl *aTimeout)
{
  if (!mContext) {
    // No context means this window was closed or never properly
    // initialized.

    return;
  }

  if (!mContext->GetScriptsEnabled()) {
    // Scripts were enabled once in this window (unless aTimeout ==
    // nsnull) but now scripts are disabled (we might be in
    // print-preview, for instance), this means we shouldn't run any
    // timeouts at this point.
    //
    // If scrips are enabled in this window again we'll fire the
    // timeouts that are due at that point.

    return;
  }

  nsTimeoutImpl *next, *prev, *timeout;
  nsTimeoutImpl *last_expired_timeout, **last_insertion_point;
  nsTimeoutImpl dummy_timeout;
  JSContext *cx;
  PRInt64 now, deadline;
  PRUint32 firingDepth = mTimeoutFiringDepth + 1;

  // Make sure that the window and the script context don't go away as
  // a result of running timeouts
  nsCOMPtr<nsIScriptGlobalObject> windowKungFuDeathGrip(this);
  nsCOMPtr<nsIScriptContext> contextKungFuDeathGrip(mContext);

  cx = (JSContext *)mContext->GetNativeContext();

  // A native timer has gone off. See which of our timeouts need
  // servicing
  LL_I2L(now, PR_IntervalNow());

  if (aTimeout && LL_CMP(aTimeout->mWhen, >, now)) {
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
  for (timeout = mTimeouts; timeout; timeout = timeout->mNext) {
    if (((timeout == aTimeout) || !LL_CMP(timeout->mWhen, >, deadline)) &&
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
  dummy_timeout.mNext = last_expired_timeout->mNext;
  last_expired_timeout->mNext = &dummy_timeout;

  // Don't let ClearWindowTimeouts throw away our stack-allocated
  // dummy timeout.
  dummy_timeout.AddRef();
  dummy_timeout.AddRef();

  last_insertion_point = mTimeoutInsertionPoint;
  mTimeoutInsertionPoint = &dummy_timeout.mNext;

  prev = nsnull;
  for (timeout = mTimeouts; timeout != &dummy_timeout; timeout = next) {
    next = timeout->mNext;

    if (timeout->mFiringDepth != firingDepth) {
      // We skip the timeout since it's on the list to run at another
      // depth.

      prev = timeout;

      continue;
    }

    // The timeout is on the list to run at this depth, go ahead and
    // process it.

    nsTimeoutImpl *last_running_timeout = mRunningTimeout;
    mRunningTimeout = timeout;
    timeout->mRunning = PR_TRUE;

    // Hold on to the timeout in case mExpr or mFunObj releases its
    // doc.
    timeout->AddRef();

    ++mTimeoutFiringDepth;

    if (timeout->mExpr) {
      // Evaluate the timeout expression.
      const PRUnichar *script =
        NS_REINTERPRET_CAST(const PRUnichar *,
                            ::JS_GetStringChars(timeout->mExpr));

      PRBool is_undefined;
      mContext->EvaluateString(nsDependentString(script), mJSObject,
                               timeout->mPrincipal, timeout->mFileName,
                               timeout->mLineNo, timeout->mVersion, nsnull,
                               &is_undefined);
    } else {
      PRInt64 lateness64;
      PRInt32 lateness;

      // Add a "secret" final argument that indicates timeout lateness
      // in milliseconds
      LL_SUB(lateness64, now, timeout->mWhen);
      LL_L2I(lateness, lateness64);
      lateness = PR_IntervalToMilliseconds(lateness);
      timeout->mArgv[timeout->mArgc] = INT_TO_JSVAL((jsint) lateness);

      jsval dummy;
      mContext->CallEventHandler(mJSObject, timeout->mFunObj,
                                 timeout->mArgc + 1, timeout->mArgv, &dummy);
    }

    --mTimeoutFiringDepth;
    mRunningTimeout = last_running_timeout;
    timeout->mRunning = PR_FALSE;

    // We ignore any failures from calling EvaluateString() or
    // CallEventHandler() on the context here since we're in a loop
    // where we're likely to be running timeouts whose OS timers
    // didn't fire in time and we don't want to not fire those timers
    // now just because execution of one timer failed. We can't
    // propagate the error to anyone who cares about it from this
    // point anyway so we just drop it.

    // If all timeouts were cleared and |timeout != aTimeout| then
    // |timeout| may be the last reference to the timeout so check if
    // it was cleared before releasing it.
    PRBool timeout_was_cleared = timeout->mCleared;

    timeout->Release(contextKungFuDeathGrip);

    if (timeout_was_cleared) {
      // The running timeout's window was cleared, this means that
      // ClearAllTimeouts() was called from a *nested* call, possibly
      // through a timeout that fired while a modal (to this window)
      // dialog was open or through other non-obvious paths.

      mTimeoutInsertionPoint = last_insertion_point;

      return;
    }

    // If we have a regular interval timer, we re-schedule the
    // timeout, accounting for clock drift.
    if (timeout->mInterval) {
      // Compute time to next timeout for interval timer.
      PRInt32 delay32;

      {
        PRInt64 interval, delay;

        LL_I2L(interval, PR_MillisecondsToInterval(timeout->mInterval));
        LL_ADD(timeout->mWhen, timeout->mWhen, interval);
        LL_I2L(now, PR_IntervalNow());
        LL_SUB(delay, timeout->mWhen, now);
        LL_L2I(delay32, delay);
      }

      // If the next interval timeout is already supposed to have
      // happened then run the timeout immediately.
      if (delay32 < 0) {
        delay32 = 0;
      }

      delay32 = PR_IntervalToMilliseconds(delay32);

      if (delay32 < DOM_MIN_TIMEOUT_VALUE) {
        // Don't let intervals starve the message pump, no matter
        // what. Just like we do for non-interval timeouts.

        delay32 = DOM_MIN_TIMEOUT_VALUE;
      }

      // Reschedule the OS timer. Don't bother returning any error
      // codes if this fails since nobody who cares about them is
      // listening anyways.
      nsresult rv =
        timeout->mTimer->InitWithFuncCallback(TimerCallback, timeout, delay32,
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
        timeout->Release(contextKungFuDeathGrip);
      }
    }

    PRBool isInterval = PR_FALSE;

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

        timeout->Release(mContext);
      }
    }

    // Running a timeout can cause another timeout to be deleted, so
    // we need to reset the pointer to the following timeout.
    next = timeout->mNext;

    if (!prev) {
      mTimeouts = next;
    } else {
      prev->mNext = next;
    }

    // Release the timeout struct since it's out of the list
    timeout->Release(contextKungFuDeathGrip);

    if (isInterval) {
      // Reschedule an interval timeout. Insert interval timeout
      // onto list sorted in deadline order.

      InsertTimeoutIntoList(mTimeoutInsertionPoint, timeout);
    }
  }

  // Take the dummy timeout off the head of the list
  if (!prev) {
    mTimeouts = dummy_timeout.mNext;
  } else {
    prev->mNext = dummy_timeout.mNext;
  }

  mTimeoutInsertionPoint = last_insertion_point;
}

void
nsTimeoutImpl::Release(nsIScriptContext *aContext)
{
  if (--mRefCnt > 0)
    return;

  if (mExpr || mFunObj) {
    nsIScriptContext *scx = aContext;
    JSRuntime *rt = nsnull;

    if (!scx && mWindow) {
      scx = mWindow->GetContext();
    }

    if (scx) {
      JSContext *cx;
      cx = (JSContext *)scx->GetNativeContext();
      rt = ::JS_GetRuntime(cx);
    } else {
      // XXX The timeout *must* be unrooted, even if !scx. This can be
      // done without a JS context using the JSRuntime. This is safe
      // enough, but it would be better to drop all a window's
      // timeouts before its context is cleared. Bug 50705 describes a
      // situation where we're not. In that case, at the time the
      // context is cleared, a timeout (actually an Interval) is still
      // active, but temporarily removed from the window's list of
      // timers (placed instead on the timer manager's list). This
      // makes the nearly handy ClearAllTimeouts routine useless, so
      // we settled on using the JSRuntime rather than relying on the
      // window having a context. It would be good to remedy this
      // workable but clumsy situation someday.

      NS_WARNING("nsTimeoutImpl::Release() proceeding without context.");
      nsCOMPtr<nsIJSRuntimeService> rtsvc =
        do_GetService("@mozilla.org/js/xpc/RuntimeService;1");

      if (rtsvc)
        rtsvc->GetRuntime(&rt);
    }

    if (!rt) {
      // most unexpected. not much choice but to bail.

      NS_ERROR("nsTimeoutImpl::Release() with no JSRuntime. eek!");

      return;
    }

    if (mExpr) {
      ::JS_RemoveRootRT(rt, &mExpr);
    } else {
      ::JS_RemoveRootRT(rt, &mFunObj);

      if (mArgv) {
        for (PRInt32 i = 0; i < mArgc; ++i) {
          ::JS_RemoveRootRT(rt, &mArgv[i]);
        }

        PR_FREEIF(mArgv);
      }
    }
  }

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  if (mFileName) {
    PL_strfree(mFileName);
  }

  NS_IF_RELEASE(mWindow);

  delete this;
}

void
nsTimeoutImpl::AddRef()
{
  ++mRefCnt;
}

nsresult
GlobalWindowImpl::ClearTimeoutOrInterval()
{
  NS_ENSURE_STATE(sXPConnect);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = sXPConnect->GetCurrentNativeCallContext(getter_AddRefs(ncc));
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

  if (argv[0] == JSVAL_VOID || !::JS_ValueToInt32(cx, argv[0], &timer_id) ||
      timer_id <= 0) {
    // Undefined or non-positive number passed as argument, return
    // early.

    return NS_OK;
  }

  PRUint32 public_id = (PRUint32)timer_id;
  nsTimeoutImpl **top, *timeout;

  for (top = &mTimeouts; (timeout = *top) != NULL; top = &timeout->mNext) {
    if (timeout->mPublicId == public_id) {
      if (timeout->mRunning) {
        /* We're running from inside the timeout. Mark this
           timeout for deferred deletion by the code in
           RunTimeout() */
        timeout->mInterval = 0;
      }
      else {
        /* Delete the timeout from the pending timeout list */
        *top = timeout->mNext;

        if (timeout->mTimer) {
          timeout->mTimer->Cancel();
          timeout->mTimer = nsnull;
          timeout->Release(mContext);
        }
        timeout->Release(mContext);
      }
      break;
    }
  }

  return NS_OK;
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
      mTimeoutInsertionPoint = &mTimeouts;

    next = timeout->mNext;

    if (timeout->mTimer) {
      timeout->mTimer->Cancel();
      timeout->mTimer = nsnull;

      // Drop the count since the timer isn't going to hold on
      // anymore.
      timeout->Release(mContext);
    }

    // Set timeout->mCleared to true to indicate that the timeout was
    // cleared and taken out of the list of timeouts
    timeout->mCleared = PR_TRUE;

    // Drop the count since we're removing it from the list.
    timeout->Release(mContext);
  }

  mTimeouts = NULL;
}

void
GlobalWindowImpl::InsertTimeoutIntoList(nsTimeoutImpl **aList,
                                        nsTimeoutImpl *aTimeout)
{
  nsTimeoutImpl *to;

  NS_ASSERTION(aList,
               "GlobalWindowImpl::InsertTimeoutIntoList null timeoutList");
  while ((to = *aList) != nsnull) {
    if (LL_CMP(to->mWhen, >, aTimeout->mWhen))
      break;
    aList = &to->mNext;
  }
  aTimeout->mFiringDepth = 0;
  aTimeout->mNext = to;
  *aList = aTimeout;

  // Increment the timeout's reference count since it's now held on to
  // by the list
  aTimeout->AddRef();
}

// static
void
GlobalWindowImpl::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsTimeoutImpl *timeout = (nsTimeoutImpl *)aClosure;

  // Hold on to the timeout to ensure it doesn't go away while it's
  // being handled (aka kungFuDeathGrip).
  timeout->AddRef();

  timeout->mWindow->RunTimeout(timeout);

  // Drop our reference to the timeout now that we're done with it.
  timeout->Release(nsnull);
}

//*****************************************************************************
// GlobalWindowImpl: Helper Functions
//*****************************************************************************

nsresult
GlobalWindowImpl::GetTreeOwner(nsIDocShellTreeOwner **aTreeOwner)
{
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
GlobalWindowImpl::GetTreeOwner(nsIBaseWindow **aTreeOwner)
{
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
GlobalWindowImpl::GetWebBrowserChrome(nsIWebBrowserChrome **aBrowserChrome)
{
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
  NS_IF_ADDREF(*aBrowserChrome = browserChrome);

  return NS_OK;
}

nsresult
GlobalWindowImpl::GetScrollInfo(nsIScrollableView **aScrollableView,
                                float *aP2T, float *aT2P)
{
  *aScrollableView = nsnull;
  *aP2T = 0.0f;
  *aT2P = 0.0f;

  if (!mDocShell) {
    return NS_OK;
  }

  // Flush pending notifications so that the presentation is up to
  // date.
  FlushPendingNotifications(Flush_Layout);

  nsCOMPtr<nsIPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (presContext) {
    *aP2T = presContext->PixelsToTwips();
    *aT2P = presContext->TwipsToPixels();

    nsIViewManager* vm = presContext->GetViewManager();
    if (vm)
      return vm->GetRootScrollableView(aScrollableView);
  }
  return NS_OK;
}

nsresult
GlobalWindowImpl::SecurityCheckURL(const char *aURL)
{
  nsresult   rv;
  JSContext *cx = nsnull;

  // get JSContext
  NS_ASSERTION(mContext, "opening window missing its context");
  NS_ASSERTION(mDocument, "opening window missing its document");
  if (!mContext || !mDocument || !sSecMan)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMChromeWindow> chrome_win =
    do_QueryInterface(NS_STATIC_CAST(nsIDOMWindow *, this));

  if (IsCallerChrome() && !chrome_win) {
    // If open() is called from chrome on a non-chrome window, we'll
    // use the context from the window on which open() is being called
    // to prevent giving chrome priveleges to new windows opened in
    // such a way. This also makes us get the appropriate base URI for
    // the below URI resolution code.

    cx = (JSContext *)mContext->GetNativeContext();
  } else {
    // get the JSContext from the call stack
    nsCOMPtr<nsIThreadJSContextStack> stack(do_GetService(sJSStackContractID));
    if (stack)
      stack->Peek(&cx);
    if (!cx) {
      // if there's no JS on the call stack, then we should pass the
      // security check.
      return NS_OK;
    }
  }

  /* resolve the URI, which could be relative to the calling window
     (note the algorithm to get the base URI should match the one
     used to actually kick off the load in nsWindowWatcher.cpp). */
  nsIURI* baseURI = nsnull;
  nsCOMPtr<nsIURI> uriToLoad;

  nsIScriptContext *scriptcx = nsJSUtils::GetDynamicScriptContext(cx);

  if (scriptcx) {
    nsCOMPtr<nsIDOMWindow> caller =
      do_QueryInterface(scriptcx->GetGlobalObject());

    if (caller) {
      nsCOMPtr<nsIDOMDocument> callerDOMdoc;
      caller->GetDocument(getter_AddRefs(callerDOMdoc));
      nsCOMPtr<nsIDocument> callerDoc(do_QueryInterface(callerDOMdoc));
      if (callerDoc)
        baseURI = callerDoc->GetBaseURI();
    }
  }

  rv = NS_NewURI(getter_AddRefs(uriToLoad), nsDependentCString(aURL), nsnull,
                 baseURI);
  if (NS_FAILED(rv))
    return rv;

  if (NS_FAILED(sSecMan->CheckLoadURIFromScript(cx, uriToLoad))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
GlobalWindowImpl::FlushPendingNotifications(mozFlushType aType)
{
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  if (doc) {
    doc->FlushPendingNotifications(aType);
  }
}

// QueryInterface implementation for nsGlobalChromeWindow
NS_INTERFACE_MAP_BEGIN(nsGlobalChromeWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMChromeWindow)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ChromeWindow)
NS_INTERFACE_MAP_END_INHERITING(GlobalWindowImpl)

NS_IMPL_ADDREF_INHERITED(nsGlobalChromeWindow, GlobalWindowImpl)
NS_IMPL_RELEASE_INHERITED(nsGlobalChromeWindow, GlobalWindowImpl)

// nsGlobalChromeWindow implementation

NS_IMETHODIMP
nsGlobalChromeWindow::GetTitle(nsAString& aTitle)
{
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::SetTitle(const nsAString& aTitle)
{
  mTitle = aTitle;
  if(mDocShell) {
    nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
    if(docShellAsWin) {
      docShellAsWin->SetTitle(PromiseFlatString(mTitle).get());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalChromeWindow::GetWindowState(PRUint16* aWindowState)
{
  *aWindowState = nsIDOMChromeWindow::STATE_NORMAL;

  nsCOMPtr<nsIWidget> widget;
  nsresult rv = GetMainWidget(getter_AddRefs(widget));

  PRInt32 aMode = 0;

  if (widget) {
    rv = widget->GetSizeMode(&aMode);
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

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Maximize()
{
  nsCOMPtr<nsIWidget> widget;
  nsresult rv = GetMainWidget(getter_AddRefs(widget));

  if (widget) {
    rv = widget->SetSizeMode(nsSizeMode_Maximized);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalChromeWindow::Minimize()
{
  nsCOMPtr<nsIWidget> widget;
  nsresult rv = GetMainWidget(getter_AddRefs(widget));

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
  nsCOMPtr<nsIWidget> widget;
  nsresult rv = GetMainWidget(getter_AddRefs(widget));

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
  nsCOMPtr<nsIWidget> widget;
  nsresult rv = GetMainWidget(getter_AddRefs(widget));

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
  nsresult rv = NS_OK;
  PRInt32 cursor;

  // use C strings to keep the code/data size down
  NS_ConvertUCS2toUTF8 cursorString(aCursor);
  
  if (cursorString.Equals("auto"))
    cursor = NS_STYLE_CURSOR_AUTO;
  else if (cursorString.Equals("default"))
    cursor = NS_STYLE_CURSOR_DEFAULT;
  else if (cursorString.Equals("pointer"))
    cursor = NS_STYLE_CURSOR_POINTER;
  else if (cursorString.Equals("crosshair"))
    cursor = NS_STYLE_CURSOR_CROSSHAIR;
  else if (cursorString.Equals("move"))
    cursor = NS_STYLE_CURSOR_MOVE;
  else if (cursorString.Equals("text"))
    cursor = NS_STYLE_CURSOR_TEXT;
  else if (cursorString.Equals("wait"))
    cursor = NS_STYLE_CURSOR_WAIT;
  else if (cursorString.Equals("help"))
    cursor = NS_STYLE_CURSOR_HELP;
  else if (cursorString.Equals("n-resize"))
    cursor = NS_STYLE_CURSOR_N_RESIZE;
  else if (cursorString.Equals("s-resize"))
    cursor = NS_STYLE_CURSOR_S_RESIZE;
  else if (cursorString.Equals("w-resize"))
    cursor = NS_STYLE_CURSOR_W_RESIZE;
  else if (cursorString.Equals("e-resize"))
    cursor = NS_STYLE_CURSOR_E_RESIZE;
  else if (cursorString.Equals("ne-resize"))
    cursor = NS_STYLE_CURSOR_NE_RESIZE;
  else if (cursorString.Equals("nw-resize"))
    cursor = NS_STYLE_CURSOR_NW_RESIZE;
  else if (cursorString.Equals("se-resize"))
    cursor = NS_STYLE_CURSOR_SE_RESIZE;
  else if (cursorString.Equals("sw-resize"))
    cursor = NS_STYLE_CURSOR_SW_RESIZE;
  else if (cursorString.Equals("copy"))
    cursor = NS_STYLE_CURSOR_COPY;      // CSS3
  else if (cursorString.Equals("alias"))
    cursor = NS_STYLE_CURSOR_ALIAS;
  else if (cursorString.Equals("context-menu"))
    cursor = NS_STYLE_CURSOR_CONTEXT_MENU;
  else if (cursorString.Equals("cell"))
    cursor = NS_STYLE_CURSOR_CELL;
  else if (cursorString.Equals("grab"))
    cursor = NS_STYLE_CURSOR_GRAB;
  else if (cursorString.Equals("grabbing"))
    cursor = NS_STYLE_CURSOR_GRABBING;
  else if (cursorString.Equals("spinning"))
    cursor = NS_STYLE_CURSOR_SPINNING;
  else if (cursorString.Equals("count-up"))
    cursor = NS_STYLE_CURSOR_COUNT_UP;
  else if (cursorString.Equals("count-down"))
    cursor = NS_STYLE_CURSOR_COUNT_DOWN;
  else if (cursorString.Equals("count-up-down"))
    cursor = NS_STYLE_CURSOR_COUNT_UP_DOWN;
  else if (cursorString.Equals("-moz-zoom-in"))
    cursor = NS_STYLE_CURSOR_MOZ_ZOOM_IN;
  else if (cursorString.Equals("-moz-zoom-out"))
    cursor = NS_STYLE_CURSOR_MOZ_ZOOM_OUT;
  else
    return NS_OK;

  nsCOMPtr<nsIPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
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
    rv = presContext->EventStateManager()->SetCursor(cursor, widget, PR_TRUE);
  }

  return rv;
}

nsresult
nsGlobalChromeWindow::GetMainWidget(nsIWidget** aMainWidget)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  nsresult rv = GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  if (treeOwnerAsWin) {
    rv = treeOwnerAsWin->GetMainWidget(aMainWidget);
  }

  return rv;
}


//*****************************************************************************
// GlobalWindowImpl: Creator Function (This should go away)
//*****************************************************************************

nsresult
NS_NewScriptGlobalObject(PRBool aIsChrome, nsIScriptGlobalObject **aResult)
{
  *aResult = nsnull;

  GlobalWindowImpl *global;

  if (aIsChrome) {
    global = new nsGlobalChromeWindow();
  } else {
    global = new GlobalWindowImpl();
  }

  NS_ENSURE_TRUE(global, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(NS_STATIC_CAST(nsIScriptGlobalObject *, global),
                            aResult);
}

//*****************************************************************************
//***    NavigatorImpl: Object Management
//*****************************************************************************

NavigatorImpl::NavigatorImpl(nsIDocShell *aDocShell) :
  mDocShell(aDocShell)
{
}

NavigatorImpl::~NavigatorImpl()
{
  sPrefInternal_id = JSVAL_VOID;
}

//*****************************************************************************
//    NavigatorImpl::nsISupports
//*****************************************************************************


// QueryInterface implementation for NavigatorImpl
NS_INTERFACE_MAP_BEGIN(NavigatorImpl)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMJSNavigator)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Navigator)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(NavigatorImpl)
NS_IMPL_RELEASE(NavigatorImpl)


void NavigatorImpl::SetDocShell(nsIDocShell *aDocShell)
{
  mDocShell = aDocShell;
  if (mPlugins)
    mPlugins->SetDocShell(aDocShell);
}

//*****************************************************************************
//    NavigatorImpl::nsIDOMNavigator
//*****************************************************************************

NS_IMETHODIMP
NavigatorImpl::GetUserAgent(nsAString& aUserAgent)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    nsCAutoString ua;
    res = service->GetUserAgent(ua);
    CopyASCIItoUCS2(ua, aUserAgent);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppCodeName(nsAString& aAppCodeName)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    nsCAutoString appName;
    res = service->GetAppName(appName);
    CopyASCIItoUCS2(appName, aAppCodeName);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppVersion(nsAString& aAppVersion)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    nsCAutoString str;
    res = service->GetAppVersion(str);
    CopyASCIItoUCS2(str, aAppVersion);

    aAppVersion.AppendLiteral(" (");
    res = service->GetPlatform(str);
    if (NS_FAILED(res))
      return res;

    AppendASCIItoUTF16(str, aAppVersion);

    aAppVersion.AppendLiteral("; ");
    res = service->GetLanguage(str);
    if (NS_FAILED(res))
      return res;

    AppendASCIItoUTF16(str, aAppVersion);

    aAppVersion.Append(PRUnichar(')'));
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppName(nsAString& aAppName)
{
  aAppName.AssignLiteral("Netscape");
  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::GetLanguage(nsAString& aLanguage)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    nsCAutoString lang;
    res = service->GetLanguage(lang);
    CopyASCIItoUCS2(lang, aLanguage);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetPlatform(nsAString& aPlatform)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    // sorry for the #if platform ugliness, but Communicator is
    // likewise hardcoded and we're seeking backward compatibility
    // here (bug 47080)
#if defined(WIN32)
    aPlatform.AssignLiteral("Win32");
#elif defined(XP_MAC) || defined(XP_MACOSX)
    // XXX not sure what to do about Mac OS X on non-PPC, but since Comm 4.x
    // doesn't know about it this will actually be backward compatible
    aPlatform.AssignLiteral("MacPPC");
#elif defined(XP_OS2)
    aPlatform.AssignLiteral("OS/2");
#else
    // XXX Communicator uses compiled-in build-time string defines
    // to indicate the platform it was compiled *for*, not what it is
    // currently running *on* which is what this does.
    nsCAutoString plat;
    res = service->GetOscpu(plat);
    CopyASCIItoUCS2(plat, aPlatform);
#endif
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetOscpu(nsAString& aOSCPU)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    nsCAutoString oscpu;
    res = service->GetOscpu(oscpu);
    CopyASCIItoUCS2(oscpu, aOSCPU);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetVendor(nsAString& aVendor)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    nsCAutoString vendor;
    res = service->GetVendor(vendor);
    CopyASCIItoUCS2(vendor, aVendor);
  }

  return res;
}


NS_IMETHODIMP
NavigatorImpl::GetVendorSub(nsAString& aVendorSub)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    nsCAutoString vendor;
    res = service->GetVendorSub(vendor);
    CopyASCIItoUCS2(vendor, aVendorSub);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetProduct(nsAString& aProduct)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    nsCAutoString product;
    res = service->GetProduct(product);
    CopyASCIItoUCS2(product, aProduct);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetProductSub(nsAString& aProductSub)
{
  nsresult res;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && service) {
    nsCAutoString productSub;
    res = service->GetProductSub(productSub);
    CopyASCIItoUCS2(productSub, aProductSub);
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetSecurityPolicy(nsAString& aSecurityPolicy)
{
  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::GetMimeTypes(nsIDOMMimeTypeArray **aMimeTypes)
{
  if (!mMimeTypes) {
    mMimeTypes = new MimeTypeArrayImpl(this);
    if (!mMimeTypes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aMimeTypes = mMimeTypes);

  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::GetPlugins(nsIDOMPluginArray **aPlugins)
{
  if (!mPlugins) {
    mPlugins = new PluginArrayImpl(this, mDocShell);
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
NavigatorImpl::GetCookieEnabled(PRBool *aCookieEnabled)
{
  *aCookieEnabled =
    (nsContentUtils::GetIntPref("network.cookie.cookieBehavior",
                                COOKIE_BEHAVIOR_REJECT) !=
     COOKIE_BEHAVIOR_REJECT);

  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::GetOnLine(PRBool* aOnline)
{
  NS_PRECONDITION(aOnline, "Null out param");
  
  *aOnline = PR_FALSE;  // No ioservice would mean this is the case
  
  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (ios) {
    ios->GetOffline(aOnline);
    *aOnline = !*aOnline;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::JavaEnabled(PRBool *aReturn)
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
NavigatorImpl::TaintEnabled(PRBool *aReturn)
{
  *aReturn = PR_FALSE;
  return NS_OK;
}

jsval
NavigatorImpl::sPrefInternal_id = JSVAL_VOID;

NS_IMETHODIMP
NavigatorImpl::Preference()
{
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPCNativeCallContext> ncc;

  rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(ncc));
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

  nsCOMPtr<nsIScriptSecurityManager> secMan =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = secMan->CheckPropertyAccess(cx, nsnull, "Navigator", sPrefInternal_id,
                                   action);
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
NavigatorImpl::LoadingNewDocument()
{
  // Release these so that they will be recreated for the
  // new document (if requested).  The plugins or mime types
  // arrays may have changed.  See bug 150087.
  mMimeTypes = nsnull;
  mPlugins = nsnull;
}

nsresult
NavigatorImpl::RefreshMIMEArray()
{
  nsresult rv = NS_OK;
  if (mMimeTypes)
    rv = mMimeTypes->Refresh();
  return rv;
}
