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
 *   Brendan Eich <brendan@mozilla.org>
 *   David Hyatt (hyatt@netscape.com)
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

// Local Includes
#include "nsGlobalWindow.h"
#include "nsScreen.h"
#include "nsHistory.h"
#include "nsBarProps.h"

// Helper Classes
#include "nsCOMPtr.h"
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
#include "nsIJVMManager.h"
#include "nsContentCID.h"

// Interfaces Needed
#include "nsIWidget.h"
#include "nsIBaseWindow.h"
#include "nsICharsetConverterManager.h"
#include "nsICodebasePrincipal.h"
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
#include "nsIDOMPkcs11.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsIEventQueueService.h"
#include "nsIEventStateManager.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIPref.h"
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
#include "nsISidebar.h"         // XXX for sidebar HACK, see bug 20721
#include "nsIPrompt.h"
#include "nsIStyleContext.h"
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
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIContentViewer.h"
#include "nsISupportsPrimitives.h"
#include "nsDOMClassInfo.h"
#include "nsIJSNativeInitializer.h"
#include "nsIFullScreen.h"
#include "nsIStringBundle.h"

#include "plbase64.h"

#include "nsIPrintSettings.h"

#include "nsWindowRoot.h"

// XXX An unfortunate dependency exists here (two XUL files).
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"

#include "nsIBindingManager.h"
#include "nsIXBLService.h"

#ifdef MOZ_PHOENIX
// Used by phoenix to do popup whitelisting
#include "nsIPopupWindowManager.h"
#endif

static nsIEntropyCollector* gEntropyCollector          = nsnull;
static PRInt32              gRefCnt                    = 0;
nsIXPConnect *GlobalWindowImpl::sXPConnect             = nsnull;
nsIScriptSecurityManager *GlobalWindowImpl::sSecMan    = nsnull;
nsIFactory *GlobalWindowImpl::sComputedDOMStyleFactory = nsnull;

#define DOM_MIN_TIMEOUT_VALUE 10 // 10ms

// CIDs
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kJVMServiceCID, NS_JVMMANAGER_CID);
static NS_DEFINE_CID(kHTTPHandlerCID, NS_HTTPPROTOCOLHANDLER_CID);
static NS_DEFINE_CID(kXULControllersCID, NS_XULCONTROLLERS_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID); // For window.find()
static NS_DEFINE_CID(kCStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

static const char *sJSStackContractID = "@mozilla.org/js/xpc/ContextStack;1";

static const char *kDOMBundleURL = "chrome://global/locale/commonDialogs.properties";



static const char * const kCryptoContractID = NS_CRYPTO_CONTRACTID;
static const char * const kPkcs11ContractID = NS_PKCS11_CONTRACTID;

static PRBool CanSetProperty(const char * prefName)
{
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
  if (!prefs) {
    return PR_FALSE;
  }

  PRBool prefValue = PR_TRUE;
  // if pref is set to true, we can't set the property
  prefs->GetBoolPref(prefName, &prefValue);

  return !prefValue;
}

//*****************************************************************************
//***    GlobalWindowImpl: Object Management
//*****************************************************************************

GlobalWindowImpl::GlobalWindowImpl() :
  mJSObject(nsnull), mNavigator(nsnull), mScreen(nsnull), mHistory(nsnull),
  mFrames(nsnull), mLocation(nsnull), mMenubar(nsnull), mToolbar(nsnull),
  mLocationbar(nsnull), mPersonalbar(nsnull), mStatusbar(nsnull),
  mScrollbars(nsnull), mTimeouts(nsnull), mTimeoutInsertionPoint(&mTimeouts),
  mRunningTimeout(nsnull), mTimeoutPublicIdCounter(1), mTimeoutFiringDepth(0),
  mTimeoutsWereCleared(PR_FALSE), mFirstDocumentLoad(PR_TRUE),
  mIsScopeClear(PR_TRUE), mIsDocumentLoaded(PR_FALSE),
  mLastMouseButtonAction(LL_ZERO), mFullScreen(PR_FALSE),
  mOriginalPos(nsnull), mOriginalSize(nsnull),
  mGlobalObjectOwner(nsnull),
  mDocShell(nsnull), mMutationBits(0), mChromeEventHandler(nsnull),
  mFrameElement(nsnull)
{
  NS_INIT_ISUPPORTS();
  // We could have failed the first time through trying
  // to create the entropy collector, so we should
  // try to get one until we succeed.
  if (gRefCnt++ == 0 || !gEntropyCollector) {
    nsCOMPtr<nsIEntropyCollector> enCol =
      do_GetService(NS_ENTROPYCOLLECTOR_CONTRACTID);

    if (enCol) {
      gEntropyCollector = enCol;
      NS_ADDREF(gEntropyCollector);
    }
  }

  if (!sXPConnect) {
    nsServiceManager::GetService(nsIXPConnect::GetCID(),
                                 nsIXPConnect::GetIID(),
                                 (nsISupports **)&sXPConnect);
  }

  if (!sSecMan) {
    nsServiceManager::GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID,
                                 NS_GET_IID(nsIScriptSecurityManager),
                                 (nsISupports **)&sSecMan);
  }
}

GlobalWindowImpl::~GlobalWindowImpl()
{
  if (!--gRefCnt) {
    NS_IF_RELEASE(gEntropyCollector);
  }

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
#endif
}

void GlobalWindowImpl::CleanUp()
{
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
  mOpener = nsnull;             // Forces Release
  mControllers = nsnull;        // Forces Release
  mContext = nsnull;            // Forces Release
  mChromeEventHandler = nsnull; // Forces Release
}

//*****************************************************************************
// GlobalWindowImpl::nsISupports
//*****************************************************************************


// QueryInterface implementation for GlobalWindowImpl
NS_INTERFACE_MAP_BEGIN(GlobalWindowImpl)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowInternal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMJSWindow)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
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

NS_IMETHODIMP
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
    nsCOMPtr<nsIDOMWindowInternal> parent;
    GetParentInternal(getter_AddRefs(parent));

    if (parent) {
      // This window is a [i]frame, don't bother GC'ing when the
      // frame's context is destroyed since a GC will happen when the
      // frameset or host document is destroyed anyway.

      mContext->SetGCOnDestruction(PR_FALSE);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetContext(nsIScriptContext ** aContext)
{
  *aContext = mContext;
  NS_IF_ADDREF(*aContext);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetNewDocument(nsIDOMDocument* aDocument,
                                 PRBool aRemoveEventListeners,
                                 PRBool aClearScopeHint)
{
  if (!aDocument) {
    if (mDocument) {
      // Cache the old principal now that the document is being removed.
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
      NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

      doc->GetPrincipal(getter_AddRefs(mDocumentPrincipal));
    }
  } else {
    // let go of the old cached principal
    mDocumentPrincipal = nsnull;
  }

  // Always clear watchpoints, to deal with two cases:
  // 1.  The first document for this window is loading, and a miscreant has
  //     preset watchpoints on the window object in order to attack the new
  //     document's privileged information.
  // 2.  A document loaded and used watchpoints on its own window, leaving
  //     them set until the next document loads.  We must clean up window
  //     watchpoints here.
  // Watchpoints set on document and subordinate objects are all cleared
  // when those sub-window objects are finalized, after JS_ClearScope and
  // a GC run that finds them to be garbage.

  if (mContext && mJSObject)
    ::JS_ClearWatchPointsForObject((JSContext *)mContext->GetNativeContext(),
                                   mJSObject);

  // Release the navigator object so that it will be recreated for the
  // new document The plugins or mime types array may have
  // changed. See bug 150087
  if (mNavigator) {
    mNavigator->SetDocShell(nsnull);

    NS_RELEASE(mNavigator);
  }

  if (mSidebar) {
    mSidebar->SetWindow(nsnull);
    mSidebar = nsnull;
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
      nsCOMPtr<nsIDOMWindowInternal> internal;
      GetPrivateRoot(getter_AddRefs(internal));
      nsCOMPtr<nsIDOMWindowInternal> us(do_QueryInterface(NS_STATIC_CAST(nsIDOMWindow*,this)));
      if (internal == us) {
        nsresult rv;
        nsCOMPtr<nsIXBLService> xblService =
                 do_GetService("@mozilla.org/xbl;1", &rv);
        if (xblService) {
          nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(mChromeEventHandler));
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
     than going away.  See bug 61840.  */

  if (mDocShell && aDocument) {
    SetStatus(nsString());
    SetDefaultStatus(nsString());
  }

  PRBool do_clear_scope = PR_FALSE;

  if (mDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    nsCOMPtr<nsIURI> docURL;

    // If we had a document in this window the document most likely
    // made our scope "unclear"

    mIsScopeClear = PR_FALSE;

    if (doc) {
      doc->GetDocumentURL(getter_AddRefs(docURL));
    }

    if (aRemoveEventListeners && mListenerManager) {
      mListenerManager->RemoveAllListeners(PR_FALSE);
      mListenerManager = nsnull;
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

      if (!(isContentWindow && aClearScopeHint)) {
        docURL->GetSpec(url);
      }

      // The "isContentWindow && aClearScopeHint" part of the if check
      // below must be the reverse of the above if check, and vise
      // versa...

      if ((isContentWindow && aClearScopeHint) ||
          !url.Equals(NS_LITERAL_CSTRING("about:blank"))) {
        // aClearScopeHint is true, or the current document is *not*
        // about:blank, clear timeouts and clear the scope.
        ClearAllTimeouts();

        if (mContext && mJSObject) {
          do_clear_scope = PR_TRUE;
        }
      }
    }
  } else if (!aDocument) {
    // If both mDocument and aDocument are null we've just left a
    // full-page plugin page and we need to ensure that the scope is
    // cleared so that the cached window.document property (which
    // happens to be null in a full-page plugin window) is cleared.
 
    do_clear_scope = PR_TRUE;
  }

  if (do_clear_scope) {
    JSContext* cx = (JSContext *)mContext->GetNativeContext();
    ::JS_ClearScope(cx, mJSObject);
    ::JS_ClearRegExpStatics(cx);

    mIsScopeClear = PR_TRUE;
  }

  if (mContext && aDocument) {
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

      JSContext *cx = (JSContext *)mContext->GetNativeContext();

      nsWindowSH::CacheDocumentProperty(cx, mJSObject, this);
    }
  }

  // Clear our mutation bitfield.
  mMutationBits = 0;

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetDocShell(nsIDocShell* aDocShell)
{
  if (aDocShell == mDocShell)
    return NS_OK;

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
      nsCOMPtr<nsIFocusController> focusController;
      GetRootFocusController(getter_AddRefs(focusController));
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

    mContext->GC();

    mContext = nsnull;          // force release now
    mControllers = nsnull;      // force release now
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
      // get our chrome event handler from the parent.  If
      // we don't have a parent, then we need to make a new
      // window root object that will function as a chrome event
      // handler and receive all events that occur anywhere inside
      // our window.
      nsCOMPtr<nsIDOMWindow> parentWindow;
      GetParent(getter_AddRefs(parentWindow));
      if (parentWindow.get() != NS_STATIC_CAST(nsIDOMWindow*,this)) {
        nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(parentWindow));
        nsCOMPtr<nsIChromeEventHandler> handler;
        piWindow->GetChromeEventHandler(getter_AddRefs(mChromeEventHandler));
      }
      else NS_NewWindowRoot(this, getter_AddRefs(mChromeEventHandler));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetDocShell(nsIDocShell ** aDocShell)
{
  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetOpenerWindow(nsIDOMWindowInternal* aOpener)
{
  mOpener = aOpener;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner)
{
  mGlobalObjectOwner = aOwner;  // Note this is supposed to be a weak ref.
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetGlobalObjectOwner(nsIScriptGlobalObjectOwner ** aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);

  *aOwner = mGlobalObjectOwner;
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP
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

  /* mChromeEventHandler and mContext go dangling in the middle of this
     function under some circumstances (events that destroy the window)
     without this addref. */
  nsCOMPtr<nsIChromeEventHandler> kungFuDeathGrip1(mChromeEventHandler);
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip2(mContext);

  /* If this is a mouse event, use the struct to provide entropy for
   * the system.
   */
  if (gEntropyCollector &&
      (NS_EVENT_FLAG_BUBBLE != aFlags) &&
      (aEvent->message == NS_MOUSE_MOVE)) {
    //I'd like to not come in here if there is a mChromeEventHandler
    //present, but there is always one when the message is
    //NS_MOUSE_MOVE.
    //
    //Chances are this counter will overflow during the life of the
    //process, but that's OK for our case.  Means we get a little
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
  if (mFullScreen && (NS_EVENT_FLAG_CAPTURE != aFlags)) {
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
    aEvent->flags = aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);

    // Execute bindingdetached handlers before we tear ourselves
    // down.
    if (aEvent->message == NS_PAGE_UNLOAD && mDocument) {
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
      nsCOMPtr<nsIBindingManager> bindingManager;
      doc->GetBindingManager(getter_AddRefs(bindingManager));
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
  if ((NS_EVENT_FLAG_BUBBLE != aFlags) && mChromeEventHandler) {
    // Check chrome document capture here.
    // XXX The chrome can not handle this, see bug 51211
    if (aEvent->message != NS_IMAGE_LOAD) {
      mChromeEventHandler->HandleChromeEvent(aPresContext, aEvent, aDOMEvent,
                                             NS_EVENT_FLAG_CAPTURE,
                                             aEventStatus);
    }
  }

  // Local handling stage
  if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) &&
      !((NS_EVENT_FLAG_BUBBLE & aFlags) &&
        (NS_EVENT_FLAG_CANT_BUBBLE & aEvent->flags))) {
    aEvent->flags |= aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, this,
                                  aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  if (aEvent->message == NS_PAGE_LOAD)
    mIsDocumentLoaded = PR_TRUE;

  // Bubbling stage
  if ((NS_EVENT_FLAG_CAPTURE != aFlags) && mChromeEventHandler) {
    // Bubble to a chrome document if it exists
    // XXX Need a way to know if an event should really bubble or not.
    // For now filter out load and unload, since they cause problems.
    if ((aEvent->message != NS_PAGE_LOAD) &&
        (aEvent->message != NS_PAGE_UNLOAD) &&
        (aEvent->message != NS_IMAGE_LOAD) &&
        (aEvent->message != NS_FOCUS_CONTENT) &&
        (aEvent->message != NS_BLUR_CONTENT)) {
      mChromeEventHandler->HandleChromeEvent(aPresContext, aEvent,
                                             aDOMEvent, NS_EVENT_FLAG_BUBBLE,
                                             aEventStatus);
    }
  }

  if (aEvent->message == NS_PAGE_LOAD) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(mFrameElement));

    nsCOMPtr<nsIDOMWindowInternal> parent;
    GetParentInternal(getter_AddRefs(parent));

    nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(mDocShell));

    PRInt32 itemType = nsIDocShellTreeItem::typeChrome;

    if (treeItem) {
      treeItem->GetItemType(&itemType);
    }

    if (content && parent && itemType != nsIDocShellTreeItem::typeChrome) {
      // If we're not in chrome, or at a chrome boundary, fire the
      // onload event for the frame element.

      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event;

      event.eventStructType = NS_EVENT;
      event.message = NS_PAGE_LOAD;

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

  return ret;
}

JSObject *
GlobalWindowImpl::GetGlobalJSObject()
{
  return mJSObject;
}

NS_IMETHODIMP
GlobalWindowImpl::OnFinalize(JSObject *aJSObject)
{
  if (aJSObject == mJSObject) {
    mJSObject = nsnull;
  } else if (mJSObject) {
    NS_ERROR("Huh? XPConnect created more than one wrapper for this global!");
  } else {
    NS_WARNING("Weird, we're finalized with a null mJSObject?");
  }

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts)
{
  if (aEnabled && aFireTimeouts) {
    // Scripts are enabled (again?) on this context, run timeouts that
    // fired on this context while scripts were disabled.

    RunTimeout(nsnull);
  }

  return NS_OK;
}


//*****************************************************************************
// GlobalWindowImpl::nsIScriptObjectPrincipal
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::GetPrincipal(nsIPrincipal** result)
{
  NS_ENSURE_ARG_POINTER(result);

  if (mDocument) {
    // If we have a document, get the principal from the document
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

    return doc->GetPrincipal(result);
  }

  if (mDocumentPrincipal) {
    *result = mDocumentPrincipal;
    NS_ADDREF(*result);

    return NS_OK;
  }

  // If we don't have a principal and we don't have a document we
  // ask the parent window for the principal. This can happen when
  // loading a frameset that has a <frame src="javascript:xxx">, in
  // that case the global window is used in JS before we've loaded
  // a document into the window.
  nsCOMPtr<nsIDOMWindowInternal> parent;

  GetParentInternal(getter_AddRefs(parent));

  if (parent) {
    nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal(do_QueryInterface(parent));

    if (objPrincipal) {
      return objPrincipal->GetPrincipal(result);
    }
  }

  return NS_ERROR_FAILURE;
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

  *aDocument = mDocument;
  NS_IF_ADDREF(*aDocument);
  return NS_OK;
}

// like GetDocument, but merely fetches the document if it already exists
NS_IMETHODIMP GlobalWindowImpl::GetExtantDocument(nsIDOMDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = mDocument;
  NS_IF_ADDREF(*aDocument);
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
  if (!mNavigator) {
    mNavigator = new NavigatorImpl(mDocShell);
    NS_ENSURE_TRUE(mNavigator, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(mNavigator);
  }

  *aNavigator = mNavigator;
  NS_ADDREF(*aNavigator);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScreen(nsIDOMScreen** aScreen)
{
  if (!mScreen && mDocShell) {
    mScreen = new ScreenImpl(mDocShell);
    NS_ENSURE_TRUE(mScreen, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(mScreen);
  }

  *aScreen = mScreen;
  NS_ADDREF(*aScreen);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetHistory(nsIDOMHistory** aHistory)
{
  if (!mHistory && mDocShell) {
    mHistory = new HistoryImpl(mDocShell);
    NS_ENSURE_TRUE(mHistory, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(mHistory);
  }
  *aHistory = mHistory;
  NS_ADDREF(*aHistory);
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
  *aContent = domWindow;
  NS_IF_ADDREF(*aContent);

  return NS_OK;
}

// XXX for sidebar HACK, see bug 20721
NS_IMETHODIMP
GlobalWindowImpl::GetSidebar(nsISidebar** aSidebar)
{
  nsresult rv = NS_OK;

  if (!mSidebar) {
    mSidebar = do_CreateInstance(NS_SIDEBAR_CONTRACTID, &rv);

    if (mSidebar) {
      nsIDOMWindowInternal *win = NS_STATIC_CAST(nsIDOMWindowInternal *, this);
      /* no addref */
      mSidebar->SetWindow(win);
    }
  }

  *aSidebar = mSidebar;
  NS_IF_ADDREF(*aSidebar);

  return rv;

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
  if (!mMenubar) {
    mMenubar = new MenubarPropImpl();
    if (mMenubar) {
      NS_ADDREF(mMenubar);
      nsCOMPtr<nsIWebBrowserChrome> browserChrome;
      if (mDocShell &&
          NS_SUCCEEDED(GetWebBrowserChrome(getter_AddRefs(browserChrome)))) {
        mMenubar->SetWebBrowserChrome(browserChrome);
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
  if (!mToolbar) {
    mToolbar = new ToolbarPropImpl();
    if (mToolbar) {
      NS_ADDREF(mToolbar);
      nsCOMPtr<nsIWebBrowserChrome> browserChrome;
      if (mDocShell &&
          NS_SUCCEEDED(GetWebBrowserChrome(getter_AddRefs(browserChrome)))) {
        mToolbar->SetWebBrowserChrome(browserChrome);
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
  if (!mLocationbar) {
    mLocationbar = new LocationbarPropImpl();
    if (mLocationbar) {
      NS_ADDREF(mLocationbar);
      nsCOMPtr<nsIWebBrowserChrome> browserChrome;
      if (mDocShell &&
          NS_SUCCEEDED(GetWebBrowserChrome(getter_AddRefs(browserChrome)))) {
        mLocationbar->SetWebBrowserChrome(browserChrome);
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
  if (!mPersonalbar) {
    mPersonalbar = new PersonalbarPropImpl();
    if (mPersonalbar) {
      NS_ADDREF(mPersonalbar);
      nsCOMPtr<nsIWebBrowserChrome> browserChrome;
      if (mDocShell &&
          NS_SUCCEEDED(GetWebBrowserChrome(getter_AddRefs(browserChrome)))) {
        mPersonalbar->SetWebBrowserChrome(browserChrome);
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
  if (!mStatusbar) {
    mStatusbar = new StatusbarPropImpl();
    if (mStatusbar) {
      NS_ADDREF(mStatusbar);
      nsCOMPtr<nsIWebBrowserChrome> browserChrome;
      if (mDocShell &&
          NS_SUCCEEDED(GetWebBrowserChrome(getter_AddRefs(browserChrome)))) {
        mStatusbar->SetWebBrowserChrome(browserChrome);
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
  if (!mScrollbars) {
    mScrollbars = new ScrollbarsPropImpl(this);
    if (mScrollbars) {
      NS_ADDREF(mScrollbars);
      nsCOMPtr<nsIWebBrowserChrome> browserChrome;
      if (mDocShell &&
          NS_SUCCEEDED(GetWebBrowserChrome(getter_AddRefs(browserChrome)))) {
        mScrollbars->SetWebBrowserChrome(browserChrome);
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
GlobalWindowImpl::GetClosed(PRBool* aClosed)
{
  *aClosed = !mDocShell;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetFrames(nsIDOMWindowCollection** aFrames)
{
  if (!mFrames && mDocShell) {
    mFrames = new nsDOMWindowList(mDocShell);
    NS_ENSURE_TRUE(mFrames, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(mFrames);
  }

  *aFrames = NS_STATIC_CAST(nsIDOMWindowCollection *, mFrames);
  NS_IF_ADDREF(mFrames);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetCrypto(nsIDOMCrypto** aCrypto)
{
  nsresult rv;

  if (!mCrypto) {
    mCrypto = do_CreateInstance(kCryptoContractID, &rv);
  }
  *aCrypto = mCrypto;
  NS_IF_ADDREF(*aCrypto);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetPkcs11(nsIDOMPkcs11** aPkcs11)
{
  nsresult rv;

  if (!mPkcs11) {
    mPkcs11 = do_CreateInstance(kPkcs11ContractID, &rv);
  }
  *aPkcs11 = mPkcs11;
  NS_IF_ADDREF(*aPkcs11);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetControllers(nsIControllers** aResult)
{
  if (!mControllers) {
    mControllers = do_CreateInstance(kXULControllersCID);
    NS_ENSURE_TRUE(mControllers, NS_ERROR_FAILURE);
#ifdef DOM_CONTROLLER
    // Add in the default controller
    nsDOMWindowController *domController = new nsDOMWindowController(this);
    if (domController) {
      nsCOMPtr<nsIController> controller(domController);
      mControllers->AppendController(controller);
    }
#endif // DOM_CONTROLLER
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
    nsCOMPtr<nsIDocShell> openerDocShell;
    openerSGO->GetDocShell(getter_AddRefs(openerDocShell));
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(openerDocShell));
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

  if (!CanSetProperty("dom.disable_window_status_change") && !IsCallerChrome()) {
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

  if (!CanSetProperty("dom.disable_window_status_change") && !IsCallerChrome()) {
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
  FlushPendingNotifications(PR_TRUE);

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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> docShellParent;
  docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

  // It's only valid to access this from a top window.  Doesn't work from
  // sub-frames.
  if (docShellParent)
    return NS_OK; // Silent failure

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
  FlushPendingNotifications(PR_TRUE);

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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> docShellParent;
  docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

  // It's only valid to access this from a top window.  Doesn't work from
  // sub-frames.
  if (docShellParent)
    return NS_OK; // Silent failure

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

  FlushPendingNotifications(PR_TRUE);
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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
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

  FlushPendingNotifications(PR_TRUE);

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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
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
  // This one is easy.  Just ensure the variable is greater than 100;
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
  // This one is harder.  We have to get the screen size and window dimensions.

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

    FlushPendingNotifications(PR_TRUE);

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

NS_IMETHODIMP
GlobalWindowImpl::GetScrollX(PRInt32* aScrollX)
{
  NS_ENSURE_ARG_POINTER(aScrollX);
  nsresult result = NS_OK;
  nsIScrollableView *view = nsnull;      // no addref/release for views
  float p2t, t2p;

  *aScrollX = 0;

  GetScrollInfo(&view, &p2t, &t2p);
  if (view) {
    nscoord xPos, yPos;
    result = view->GetScrollPosition(xPos, yPos);
    *aScrollX = NSTwipsToIntPixels(xPos, t2p);
  }

  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScrollY(PRInt32* aScrollY)
{
  NS_ENSURE_ARG_POINTER(aScrollY);
  nsresult result = NS_OK;
  nsIScrollableView *view = nsnull;      // no addref/release for views
  float p2t, t2p;

  *aScrollY = 0;

  GetScrollInfo(&view, &p2t, &t2p);
  if (view) {
    nscoord xPos, yPos;
    result = view->GetScrollPosition(xPos, yPos);
    *aScrollY = NSTwipsToIntPixels(yPos, t2p);
  }

  return result;
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
  if (aFullScreen == mFullScreen)
    return NS_OK;

  // SetFullScreen needs to be called on the root window, so get that
  // via the DocShell tree, and if we are not already the root, 
  // call SetFullScreen on that window instead.
  nsCOMPtr<nsIDocShell> docShell;
  GetDocShell(getter_AddRefs(docShell));
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(docShell);
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

  if (aFullScreen) {
    // hide window chrome
    nsCOMPtr<nsIDOMElement> docEl;
    mDocument->GetDocumentElement(getter_AddRefs(docEl));
    if (docEl)
        docEl->SetAttribute(NS_LITERAL_STRING("hidechrome"), NS_LITERAL_STRING("true"));

    // store current position for restoration later on
    if (!mOriginalPos)
      mOriginalPos = new nsPoint();
    if (!mOriginalPos)
      return NS_ERROR_OUT_OF_MEMORY;
    
    GetScreenX(&(mOriginalPos->x));
    GetScreenY(&(mOriginalPos->y));

    // store current size for restoration later on
    if (!mOriginalSize)
      mOriginalSize = new nsSize();
    if (!mOriginalSize)
      return NS_ERROR_OUT_OF_MEMORY;

    GetOuterWidth(&(mOriginalSize->width));
    GetOuterHeight(&(mOriginalSize->height));
  
    // get screen object so we can measure screen dimensions
    nsCOMPtr<nsIDOMScreen> screen;
    GetScreen(getter_AddRefs(screen));
    PRInt32 width, height;
    screen->GetWidth(&width);
    screen->GetHeight(&height);
  
    // move to origin and size to fill entire screen
    MoveTo(0, 0);
    ResizeTo(width, height);

    // hide all OS chrome (like the windows taskbar)
    nsCOMPtr<nsIFullScreen> fullScreen =
      do_GetService("@mozilla.org/browser/fullscreen;1");
    if (fullScreen)
      fullScreen->HideAllOSChrome();
  } else {
    // show window chrome again
    nsCOMPtr<nsIDOMElement> docEl;
    mDocument->GetDocumentElement(getter_AddRefs(docEl));
    if (docEl)
     docEl->RemoveAttribute(NS_LITERAL_STRING("hidechrome"));
  
    // restore window to original size/position
    if (mOriginalPos)
      MoveTo(mOriginalPos->x, mOriginalPos->y);
    if (mOriginalSize)
      ResizeTo(mOriginalSize->width, mOriginalSize->height);
  
    // show all OS chrome again
    nsCOMPtr<nsIFullScreen> fullScreen =
      do_GetService("@mozilla.org/browser/fullscreen;1");
    if (fullScreen)
      fullScreen->ShowAllOSChrome();
  }
  
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

    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
    if (!prefs)
      return NS_OK;

    PRBool enable_dump = PR_FALSE;

    // if pref doesn't exist, disable dump output.
    nsresult rv = prefs->GetBoolPref("browser.dom.window.dump.enabled",
                                     &enable_dump);

    if (NS_FAILED(rv) || !enable_dump) {
      return NS_OK;
    }
  }
#endif

  char *cstr = ToNewUTF8String(aStr);

#ifdef XP_MAC
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
    nsCRT::free(cstr);
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
    doc->FlushPendingNotifications(PR_TRUE, PR_FALSE);
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
nsresult
GlobalWindowImpl::CheckSecurityIsChromeCaller(PRBool *aIsChrome)
{
  NS_ENSURE_ARG_POINTER(aIsChrome);

  *aIsChrome = PR_FALSE;
 
  // Check if this is a privileged system script

  NS_ENSURE_TRUE(sSecMan, NS_ERROR_FAILURE);

  PRBool isChrome = PR_FALSE;
  nsresult rv = sSecMan->SubjectPrincipalIsSystem(&isChrome);
  if (NS_SUCCEEDED(rv)) {
    *aIsChrome = isChrome;
  }

  return NS_OK;
}

// static
PRBool
GlobalWindowImpl::IsCallerChrome()
{
  PRBool is_caller_chrome = PR_FALSE;

  nsresult rv = CheckSecurityIsChromeCaller(&is_caller_chrome);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  return is_caller_chrome;
}

void
GlobalWindowImpl::MakeScriptDialogTitle(const nsAString &aInTitle, nsAString &aOutTitle)
{
  aOutTitle.Truncate(0);

  // Load the string to be prepended to titles for script
  // confirm/alert/prompt boxes.

  nsresult rv;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
     do_GetService(kCStringBundleServiceCID, &rv);

  if (NS_SUCCEEDED(rv) && stringBundleService) { 
    nsCOMPtr<nsIStringBundle> stringBundle;
    rv = stringBundleService->CreateBundle(kDOMBundleURL,
       getter_AddRefs(stringBundle));
    
    if (stringBundle) {
      nsAutoString inTitle(aInTitle);
      nsXPIDLString tempString;
      const PRUnichar *formatStrings[1];
      formatStrings[0] = inTitle.get();
      rv = stringBundle->FormatStringFromName(
        NS_LITERAL_STRING("ScriptDlgTitle").get(),
        formatStrings, 1, getter_Copies(tempString));
      if (tempString)
        aOutTitle = tempString.get();
    }
  }
  
  // Just in case
  if (aOutTitle.IsEmpty()) {
    NS_WARNING("could not get ScriptDlgTitle string from string bundle");
    aOutTitle.Assign(NS_LITERAL_STRING("[Script] "));
    aOutTitle.Append(aInTitle);
  }
}

NS_IMETHODIMP
GlobalWindowImpl::Alert(const nsAString& aString)
{
  NS_ENSURE_STATE(mDocShell);

  if (CheckForAbusePoint())
    return NS_OK;

  nsAutoString str;

  str.Assign(aString);

  // XXX: Concatenation of optional args?

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  // Test whether title needs to prefixed with [script]
  PRBool isChrome = PR_FALSE;
  nsAutoString newTitle;
  const PRUnichar *title = nsnull;
  nsresult rv = CheckSecurityIsChromeCaller(&isChrome);
  if (NS_FAILED(rv) || !isChrome) {
      MakeScriptDialogTitle(NS_LITERAL_STRING(""), newTitle);
      title = newTitle.get();
  }
  NS_WARN_IF_FALSE(!isChrome, "chrome shouldn't be calling alert(), use the prompt service");

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  return prompter->Alert(title, str.get());
}

NS_IMETHODIMP
GlobalWindowImpl::Confirm(const nsAString& aString, PRBool* aReturn)
{
  NS_ENSURE_STATE(mDocShell);

  if (CheckForAbusePoint())
    return NS_OK;

  nsAutoString str;

  *aReturn = PR_FALSE;

  str.Assign(aString);

  // XXX: Concatenation of optional args?

  // Test whether title needs to prefixed with [script]
  PRBool isChrome = PR_FALSE;
  nsAutoString newTitle;
  const PRUnichar *title = nsnull;
  nsresult rv = CheckSecurityIsChromeCaller(&isChrome);
  if (NS_FAILED(rv) || !isChrome) {
      MakeScriptDialogTitle(NS_LITERAL_STRING(""), newTitle);
      title = newTitle.get();
  }
  NS_WARN_IF_FALSE(!isChrome, "chrome shouldn't be calling confirm(), use the prompt service");
 
  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  return prompter->Confirm(title, str.get(), aReturn);
}

NS_IMETHODIMP
GlobalWindowImpl::Prompt(const nsAString& aMessage,
                         const nsAString& aInitial,
                         const nsAString& aTitle,
                         PRUint32 aSavePassword,
                         nsAString& aReturn)
{
  NS_ENSURE_STATE(mDocShell);

  if (CheckForAbusePoint())
    return NS_OK;

  aReturn.Truncate(); // XXX Null string!!!

  nsresult rv = NS_OK;

  nsCOMPtr<nsIAuthPrompt> prompter(do_GetInterface(mDocShell));

  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  PRBool b;
  nsXPIDLString uniResult;

  // Before bringing up the window, unsuppress painting and flush
  // pending reflows.
  EnsureReflowFlushAndPaint();

  // Test whether title needs to prefixed with [script]
  nsAutoString title;
  PRBool isChrome = PR_FALSE;
  rv = CheckSecurityIsChromeCaller(&isChrome);
  if (NS_FAILED(rv) || !isChrome) {
      MakeScriptDialogTitle(aTitle, title);
  }
  else
  {
      title.Assign(aTitle);
  }
  NS_WARN_IF_FALSE(!isChrome, "chrome shouldn't be calling prompt(), use the prompt service");

  rv = prompter->Prompt(title.get(),
                        PromiseFlatString(aMessage).get(), nsnull,
                        aSavePassword,
                        PromiseFlatString(aInitial).get(),
                        getter_Copies(uniResult), &b);
  NS_ENSURE_SUCCESS(rv, rv);

  if (uniResult && b) {
    aReturn.Assign(uniResult);
  }
  else {
    SetDOMStringToNull(aReturn);

    // XXX: Since DOMString's can't be null yet we'll haveto do this here...

    if (sXPConnect) {
      nsCOMPtr<nsIXPCNativeCallContext> ncc;

      sXPConnect->GetCurrentNativeCallContext(getter_AddRefs(ncc));

      if (ncc) {
        jsval *retval = nsnull;

        rv = ncc->GetRetValPtr(&retval);
        NS_ENSURE_SUCCESS(rv, rv);

        *retval = JSVAL_NULL;

        ncc->SetReturnValueWasSet(PR_TRUE);
      }
    }
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

  if (!CanSetProperty("dom.disable_window_flip") && !IsCallerChrome()) {
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
    nsCOMPtr<nsIViewManager> vm;
    presShell->GetViewManager(getter_AddRefs(vm));
    if (vm) {
      nsCOMPtr<nsIWidget> widget;
      vm->GetWidget(getter_AddRefs(widget));
      if (widget)
        // raise the window since this was a focus call on the window.
        result = widget->SetFocus(PR_TRUE);
    }
  }
  else {
    nsCOMPtr<nsIFocusController> focusController;
    GetRootFocusController(getter_AddRefs(focusController));
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

  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
  NS_ENSURE_TRUE(prefs, NS_ERROR_FAILURE);

  // if we get here, we know prefs is not null
  nsXPIDLString url;
  prefs->GetLocalizedUnicharPref(PREF_BROWSER_STARTUP_HOMEPAGE,
                                 getter_Copies(url));
  nsString homeURL;
  if (!url) {
    // if all else fails, use this
#ifdef DEBUG_seth
    printf("all else failed.  using %s as the home page\n", DEFAULT_HOME_PAGE);
#endif
    homeURL.AssignWithConversion(DEFAULT_HOME_PAGE);
  }
  else
    homeURL = url;

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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
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

  if (!CanSetProperty("dom.disable_window_move_resize") && !IsCallerChrome()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> docShellParent;
  docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

  // It's only valid to access this from a top window.  Doesn't work from
  // sub-frames.
  if (docShellParent)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(cv));
  NS_ENSURE_TRUE(markupViewer, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(markupViewer->SizeToContent(), NS_ERROR_FAILURE);

  return NS_OK;
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
  if (view)
  {
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
  if (view)
  {
    result = view->ScrollByPages(numPages);
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

#ifdef MOZ_PHOENIX
static
PRBool IsPopupWhitelisted(nsIDOMDocument* aDoc)
{
  // Phoenix whitelists popups. Subvert Mozilla's blacklist implementation and
  // use it as a whitelist instead. If/when Mozilla receives a patch to do 
  // popup whitelisting, the #ifdefs can be removed, and the right list/api can
  // be used.
  PRBool whiteListed = PR_FALSE;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDoc));
  nsCOMPtr<nsIPopupWindowManager> pm(do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID));
  if (pm) {
    nsCOMPtr<nsIURI> uri;
    if (doc)
      doc->GetDocumentURL(getter_AddRefs(uri));

    // Allow really means it wasn't in our list. So in Phoenix, we 
    // assume DENY means it's in the whitelist.  It's all backwards! Backwards, I say!
    PRUint32 permission;
    if (NS_SUCCEEDED(pm->TestPermission(uri, &permission)))
      whiteListed = (permission == nsIPopupWindowManager::DENY_POPUP);
  }

  return whiteListed;
}

static 
void FirePopupBlockedEvent(nsIDOMDocument* aDoc)
{
  if (aDoc) {
    // Fire a "DOMPopupBlocked" event so that the UI can hear about blocked popups.
    nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(aDoc));
    nsCOMPtr<nsIDOMEvent> event;
    docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
    if (event) {
      event->InitEvent(NS_LITERAL_STRING("DOMPopupBlocked"), PR_TRUE, PR_TRUE);
      PRBool noDefault;
      nsCOMPtr<nsIDOMEventTarget> targ(do_QueryInterface(aDoc));
      targ->DispatchEvent(event, &noDefault);
    }
  }
}
#endif

/*
 * Examine the current document state to see if we're in a way that is
 * typically abused by web designers.  This routine returns PR_TRUE if
 * we're running a top level script, running an onload or onunload
 * handler, or running a timeout.  The window.open code uses this
 * routine to determine wether or not to allow the new window.
 */
PRBool
GlobalWindowImpl::CheckForAbusePoint ()
{
  nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(mDocShell));

  if (item) {
    PRInt32 type = nsIDocShellTreeItem::typeChrome;
        
    item->GetItemType(&type);

    if (type != nsIDocShellTreeItem::typeContent)
      return PR_FALSE;
  }
  
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
  if (!prefs)
    return PR_FALSE;
  
  if (!mIsDocumentLoaded || mRunningTimeout) {
    PRBool blockOpenOnLoad = PR_FALSE;
    prefs->GetBoolPref("dom.disable_open_during_load", &blockOpenOnLoad);
    if (blockOpenOnLoad) {        
#ifdef DEBUG
      printf ("*** Scripts executed during (un)load or as a result of "
              "setTimeout() are potential javascript abuse points.\n");
#endif

#ifdef MOZ_PHOENIX
      // see the definition of the function for details.
      if (!IsPopupWhitelisted(mDocument))
          FirePopupBlockedEvent(mDocument);
#else
      return PR_TRUE;
#endif
    }
  } else {
    PRInt32 clickDelay = 0;
    prefs->GetIntPref("dom.disable_open_click_delay", &clickDelay);
    if (clickDelay) {
      PRTime now, ll_delta;
      PRInt32 delta;
      now = PR_Now();
      LL_SUB(ll_delta, now, mLastMouseButtonAction);
      LL_L2I(delta, ll_delta);
      delta /= 1000;
      if (delta > clickDelay)
      {
#ifdef DEBUG
        printf ("*** Scripts executed more than %ims after a mouse button "
                "action are potential javascript abuse points (%i.)\n",
                clickDelay, delta);
#endif
        
#ifdef MOZ_PHOENIX
        // see the definition of the function for details.
        if (!IsPopupWhitelisted(mDocument))
          FirePopupBlockedEvent(mDocument);
#else
        return PR_TRUE;
#endif
      }
    }
  }

  return PR_FALSE;
}

NS_IMETHODIMP
GlobalWindowImpl::Open(const nsAString& aUrl,
                       const nsAString& aName,
                       const nsAString& aOptions,
                       nsIDOMWindow **_retval)
{
  return OpenInternal(aUrl, aName, aOptions, PR_FALSE, nsnull, 0, nsnull,
                      _retval);
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

  /*
   * If we're in a commonly abused state (top level script, running a timeout,
   * or onload/onunload), and the preference is enabled, prevent window.open().
   */
  if (CheckForAbusePoint()) {
    if (name.IsEmpty()) {
      return NS_OK;
    }

    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
    // If getting a window watcher fails, we'd fail downstream anyway when trying to
    // open a new window so just bail here.
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMWindow> namedWindow;
    wwatch->GetWindowByName(name.get(), this,
                            getter_AddRefs(namedWindow));

    if (!namedWindow) {
      return NS_OK;
    }
  }

  return OpenInternal(url, name, options, PR_FALSE, nsnull, 0, nsnull,
                      _retval);
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

  FlushPendingNotifications(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Close()
{
  nsCOMPtr<nsIDOMWindow> parent;
  GetParent(getter_AddRefs(parent));

  if (parent != NS_STATIC_CAST(nsIDOMWindow *, this)) {
    // window.close() is called on a frame in a frameset, such calls
    // are ignored.

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

  // Note: the basic security check, rejecting windows not opened through JS,
  // has been removed. This was approved long ago by ...you're going to call me
  // on this, aren't you... well it was. And anyway, a better means is coming.
  // In the new world of application-level interfaces being written in JS, this
  // security check was causing problems.

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService(sJSStackContractID);

  JSContext *cx = nsnull;

  if (stack) {
    stack->Peek(&cx);
  }

  if (cx) {
    nsCOMPtr<nsIScriptContext> currentCX =
      NS_STATIC_CAST(nsIScriptContext *, ::JS_GetContextPrivate(cx));

    if (currentCX && currentCX == mContext) {
      return currentCX->SetTerminationFunction(CloseWindow,
                                               NS_STATIC_CAST(nsIDOMWindow *,
                                                              this));
    }
  }

  // If we get past the above we're closing the window right now.
  return ReallyCloseWindow();
}

NS_IMETHODIMP
GlobalWindowImpl::ReallyCloseWindow()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));

  // If there's no treeOwnerAsWin, this window must already be closed.

  if (treeOwnerAsWin) {
    treeOwnerAsWin->Destroy();
    CleanUp();
  }

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetFrameElement(nsIDOMElement** aFrameElement)
{
  *aFrameElement = nsnull;

  nsCOMPtr<nsIDocShell> docShell;
  GetDocShell(getter_AddRefs(docShell));

  nsCOMPtr<nsIDocShellTreeItem> docShellTI(do_QueryInterface(docShell));

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
GlobalWindowImpl::GetFrameElementInternal(nsIDOMElement** aFrameElement)
{
  *aFrameElement = mFrameElement;
  NS_IF_ADDREF(*aFrameElement);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetFrameElementInternal(nsIDOMElement* aFrameElement)
{
  mFrameElement = aFrameElement;

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::IsLoadingOrRunningTimeout(PRBool* aResult)
{
  *aResult = !mIsDocumentLoaded || mRunningTimeout;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::UpdateCommands(const nsAString& anAction)
{
  nsCOMPtr<nsIDOMWindowInternal> rootWindow;
  GetPrivateRoot(getter_AddRefs(rootWindow));
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

NS_IMETHODIMP
GlobalWindowImpl::Escape(const nsAString& aStr,
                         nsAString& aReturn)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIUnicodeEncoder> encoder;
  nsAutoString charset;

  nsCOMPtr<nsICharsetConverterManager>
    ccm(do_GetService(kCharsetConverterManagerCID));
  NS_ENSURE_TRUE(ccm, NS_ERROR_FAILURE);

  // Get the document character set
  charset.Assign(NS_LITERAL_STRING("UTF-8"));        // default to utf-8
  if (mDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));

    if (doc)
      result = doc->GetDocumentCharacterSet(charset);
  }
  if (NS_FAILED(result))
    return result;

  // Get an encoder for the character set
  result = ccm->GetUnicodeEncoder(&charset, getter_AddRefs(encoder));
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
  char* dest = (char *) nsMemory::Alloc(maxByteLen + 1);
  PRInt32 destLen2, destLen = maxByteLen;
  if (!dest)
    return NS_ERROR_OUT_OF_MEMORY;

  // Convert from unicode to the character set
  result = encoder->Convert(src, &srcLen, dest, &destLen);
  if (NS_FAILED(result)) {
    nsMemory::Free(dest);
    return result;
  }

  // Allow the encoder to finish the conversion
  destLen2 = maxByteLen - destLen;
  encoder->Finish(dest + destLen, &destLen2);
  dest[destLen + destLen2] = '\0';

  // Escape the string
  char *outBuf =
    nsEscape(dest, nsEscapeMask(url_XAlphas | url_XPAlphas | url_Path));
  CopyASCIItoUCS2(nsDependentCString(outBuf), aReturn);

  nsMemory::Free(outBuf);
  nsMemory::Free(dest);

  return result;
}

NS_IMETHODIMP
GlobalWindowImpl::Unescape(const nsAString& aStr,
                           nsAString& aReturn)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIUnicodeDecoder> decoder;
  nsAutoString charset;

  aReturn.Truncate();

  nsCOMPtr<nsICharsetConverterManager>
    ccm(do_GetService(kCharsetConverterManagerCID));
  NS_ENSURE_TRUE(ccm, NS_ERROR_FAILURE);

  // Get the document character set
  charset.Assign(NS_LITERAL_STRING("UTF-8"));        // default to utf-8
  if (mDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));

    if (doc)
      result = doc->GetDocumentCharacterSet(charset);
  }
  if (NS_FAILED(result))
    return result;

  // Get an decoder for the character set
  result = ccm->GetUnicodeDecoder(&charset, getter_AddRefs(decoder));
  if (NS_FAILED(result))
    return result;

  result = decoder->Reset();
  if (NS_FAILED(result))
    return result;

  // Need to copy to do the two-byte to one-byte deflation
  char *inBuf = ToNewCString(aStr);
  if (!inBuf)
    return NS_ERROR_OUT_OF_MEMORY;

  // Unescape the string
  char *src = nsUnescape(inBuf);

  PRInt32 maxLength, srcLen;
  srcLen = strlen(src);

  // Get the expected length of the result string
  result = decoder->GetMaxLength(src, srcLen, &maxLength);
  if (NS_FAILED(result) || !maxLength) {
    nsMemory::Free(src);
    return result;
  }

  // Allocate a buffer of the maximum length
  PRUnichar *dest = (PRUnichar*)nsMemory::Alloc(sizeof(PRUnichar) * maxLength);
  PRInt32 destLen = maxLength;
  if (!dest) {
    nsMemory::Free(src);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Convert from character set to unicode
  result = decoder->Convert(src, &srcLen, dest, &destLen);
  nsMemory::Free(src);
  if (NS_FAILED(result)) {
    nsMemory::Free(dest);
    return result;
  }

  aReturn.Assign(dest, destLen);
  nsMemory::Free(dest);

  return NS_OK;
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

  // GetInterface(NS_GET_IID(nsIWebBrowserFind))
  nsCOMPtr<nsIWebBrowserFind> finder(do_GetInterface(mDocShell));

  // Set the options of the search
  rv = finder->SetSearchString(PromiseFlatString(aStr).get());
  NS_ENSURE_SUCCESS(rv, rv);
  finder->SetMatchCase(caseSensitive);
  finder->SetFindBackwards(backwards);
  finder->SetWrapFind(wrapAround);
  finder->SetEntireWord(wholeWord);
  finder->SetSearchFrames(searchInFrames);

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
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(manager)))) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::RemoveEventListener(const nsAString& aType,
                                      nsIDOMEventListener* aListener,
                                      PRBool aUseCapture)
{
  if (mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::DispatchEvent(nsIDOMEvent* aEvent, PRBool* _retval)
{
  if (mDocument) {
    nsCOMPtr<nsIDocument> idoc(do_QueryInterface(mDocument));
    if (idoc) {
      // Obtain a presentation context
      PRInt32 count = idoc->GetNumberOfShells();
      if (count == 0)
        return NS_OK;

      nsCOMPtr<nsIPresShell> shell;
      idoc->GetShellAt(0, getter_AddRefs(shell));

      // Retrieve the context
      nsCOMPtr<nsIPresContext> aPresContext;
      shell->GetPresContext(getter_AddRefs(aPresContext));

      nsCOMPtr<nsIEventStateManager> esm;
      if (NS_SUCCEEDED(aPresContext->GetEventStateManager(getter_AddRefs(esm)))) {
        return esm->DispatchNewEvent(NS_STATIC_CAST(nsIScriptGlobalObject *,
                                                    this), aEvent, _retval);
      }
    }
  }
  return NS_ERROR_FAILURE;
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOMEventReceiver
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::AddEventListenerByIID(nsIDOMEventListener* aListener,
                                        const nsIID& aIID)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
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

//XXX I need another way around the circular link problem.
NS_IMETHODIMP
GlobalWindowImpl::GetNewListenerManager(nsIEventListenerManager **aResult)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GlobalWindowImpl::HandleEvent(nsIDOMEvent *aEvent)
{
  PRBool noDefault;
  return DispatchEvent(aEvent, &noDefault);
}

//*****************************************************************************
// GlobalWindowImpl::nsPIDOMWindow
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::GetPrivateParent(nsPIDOMWindow ** aParent)
{
  nsCOMPtr<nsIDOMWindow> parent;
  *aParent = nsnull;            // Set to null so we can bail out later

  GetParent(getter_AddRefs(parent));

  if (NS_STATIC_CAST(nsIDOMWindow *, this) == parent.get()) {
    nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
    if (!chromeElement)
      return NS_OK;             // This is ok, just means a null parent.

    nsCOMPtr<nsIDocument> doc;
    chromeElement->GetDocument(*getter_AddRefs(doc));
    if (!doc)
      return NS_OK;             // This is ok, just means a null parent.

    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    doc->GetScriptGlobalObject(getter_AddRefs(globalObject));
    if (!globalObject)
      return NS_OK;             // This is ok, just means a null parent.

    parent = do_QueryInterface(globalObject);
  }

  if (parent)
    CallQueryInterface(parent.get(), aParent);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetPrivateRoot(nsIDOMWindowInternal ** aParent)
{
  *aParent = nsnull;            // Set to null so we can bail out later

  nsCOMPtr<nsIDOMWindow> parent;
  GetTop(getter_AddRefs(parent));

  nsCOMPtr<nsIScriptGlobalObject> parentTop = do_QueryInterface(parent);
  nsCOMPtr<nsIDocShell> docShell;
  NS_ASSERTION(parentTop, "cannot get parentTop");
  if(parentTop == nsnull)
    return NS_ERROR_FAILURE;
  parentTop->GetDocShell(getter_AddRefs(docShell));

  // Get the chrome event handler from the doc shell, since we only
  // want to deal with XUL chrome handlers and not the new kind of
  // window root handler.
  nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;
  docShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));

  nsCOMPtr<nsIContent> chromeElement(do_QueryInterface(mChromeEventHandler));
  if (chromeElement) {
    nsCOMPtr<nsIDocument> doc;
    chromeElement->GetDocument(*getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIScriptGlobalObject> globalObject;
      doc->GetScriptGlobalObject(getter_AddRefs(globalObject));

      parent = do_QueryInterface(globalObject);
      nsCOMPtr<nsIDOMWindow> tempParent;
      parent->GetTop(getter_AddRefs(tempParent));
      CallQueryInterface(tempParent, aParent);
      return NS_OK;
    }
  }

  if (parent) {
    CallQueryInterface(parent, aParent);
  }

  return NS_OK;
}


NS_IMETHODIMP
GlobalWindowImpl::GetLocation(nsIDOMLocation ** aLocation)
{
  if (!mLocation && mDocShell) {
    mLocation = new LocationImpl(mDocShell);
    NS_IF_ADDREF(mLocation);
  }

  *aLocation = mLocation;
  NS_IF_ADDREF(mLocation);

  return NS_OK;
}

NS_IMETHODIMP
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

NS_IMETHODIMP
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

   nsCOMPtr<nsIViewManager> vm;
   presShell->GetViewManager(getter_AddRefs(vm));
   NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

   nsIView* rootView;
   vm->GetRootView(rootView);
   NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

   nsCOMPtr<nsIWidget> widget;
   rootView->GetWidget(*getter_AddRefs(widget));
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

  nsCOMPtr<nsIViewManager> vm;
  presShell->GetViewManager(getter_AddRefs(vm));
  NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

  nsIView *rootView;
  vm->GetRootView(rootView);
  NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWidget> widget;
  rootView->GetWidget(*getter_AddRefs(widget));
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  nsEventStatus status;
  nsGUIEvent guiEvent;

  guiEvent.eventStructType = NS_GUI_EVENT;
  guiEvent.point.x = 0;
  guiEvent.point.y = 0;
  guiEvent.time = PR_IntervalNow();
  guiEvent.nativeMsg = nsnull;
  guiEvent.message = NS_ACTIVATE;
  guiEvent.widget = widget;

  vm->DispatchEvent(&guiEvent, &status);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Deactivate()
{
  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIViewManager> vm;
  presShell->GetViewManager(getter_AddRefs(vm));
  NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

  nsIView *rootView;
  vm->GetRootView(rootView);
  NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWidget> widget;
  rootView->GetWidget(*getter_AddRefs(widget));
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  nsEventStatus status;
  nsGUIEvent guiEvent;

  guiEvent.eventStructType = NS_GUI_EVENT;
  guiEvent.point.x = 0;
  guiEvent.point.y = 0;
  guiEvent.time = PR_IntervalNow();
  guiEvent.nativeMsg = nsnull;
  guiEvent.message = NS_DEACTIVATE;
  guiEvent.widget = widget;

  vm->DispatchEvent(&guiEvent, &status);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetChromeEventHandler(nsIChromeEventHandler** aHandler)
{
  *aHandler = mChromeEventHandler;
  NS_IF_ADDREF(*aHandler);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetRootFocusController(nsIFocusController** aController)
{
  *aController = nsnull;

  nsCOMPtr<nsIDOMWindowInternal> rootWindow;
  GetPrivateRoot(getter_AddRefs(rootWindow));
  if (rootWindow) {
    // Obtain the chrome event handler.
    nsCOMPtr<nsIChromeEventHandler> chromeHandler;
    nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));
    piWin->GetChromeEventHandler(getter_AddRefs(chromeHandler));
    if (chromeHandler) {
      nsCOMPtr<nsPIWindowRoot> windowRoot(do_QueryInterface(chromeHandler));
      if (windowRoot) {
        windowRoot->GetFocusController(aController);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::HasMutationListeners(PRUint32 aMutationEventType,
                                       PRBool* aResult)
{
  *aResult = (mMutationBits & aMutationEventType) != 0;
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::SetMutationListeners(PRUint32 aType)
{
  mMutationBits |= aType;
  return NS_OK;
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
      *aSink = docCharset;
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebNavigation))) {
    if (mDocShell) {
      nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
      *aSink = webNav;
    }
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
    if (mDocShell) {
      nsCOMPtr<nsIContentViewer> viewer;
      mDocShell->GetContentViewer(getter_AddRefs(viewer));
      if (viewer) {
        nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
        *aSink = webBrowserPrint;
      }
    }
  }
  else {
    return QueryInterface(aIID, aSink);
  }

  NS_IF_ADDREF(((nsISupports *) * aSink));
  return NS_OK;
}

//*****************************************************************************
// GlobalWindowImpl: Window Control Functions
//*****************************************************************************

void
GlobalWindowImpl::GetParentInternal(nsIDOMWindowInternal **aParent)
{
  *aParent = nsnull;

  nsCOMPtr<nsIDOMWindow> parent;

  GetParent(getter_AddRefs(parent));

  if (parent && parent != NS_STATIC_CAST(nsIDOMWindow *, this)) {
    CallQueryInterface(parent, aParent);
    NS_ASSERTION(*aParent, "Huh, parent not an nsIDOMWindowInternal?");
  }
}

NS_IMETHODIMP
GlobalWindowImpl::OpenInternal(const nsAString& aUrl,
                               const nsAString& aName,
                               const nsAString& aOptions,
                               PRBool aDialog, jsval *argv, PRUint32 argc,
                               nsISupports *aExtraArgument,
                               nsIDOMWindow **aReturn)
{
  nsCOMPtr<nsIDOMWindow> domReturn;
  char *features = nsnull;
  char *name = nsnull;
  char *url = nsnull;
  nsresult rv = NS_OK;

  *aReturn = nsnull;

  if (!aUrl.IsEmpty()) {
    // fix bug 35076
    // if the URL contains non ASCII, escape from the first non ASCII char
    nsAutoString unescapedURL(aUrl);
    nsAutoString escapedURL;

    if (unescapedURL.IsASCII())
      escapedURL = unescapedURL;
    else {
      // const PRUnichar *pt = unescapedURL.get();
      PRUint32 len = unescapedURL.Length();
      PRUint32 nonAsciiPos = 0;

      nsReadingIterator<PRUnichar> iter, end;
      unescapedURL.BeginReading(iter);
      unescapedURL.EndReading(end);

      while (iter != end) {
        if ((0xFF80 & *iter) != 0)
          break;

        ++nonAsciiPos;

        ++iter;
      }

      nsAutoString right, escapedRight;
      unescapedURL.Left(escapedURL, nonAsciiPos);
      unescapedURL.Right(right, len - nonAsciiPos);
      if (NS_SUCCEEDED(Escape(right, escapedRight)))
        escapedURL.Append(escapedRight);
      else
        escapedURL = unescapedURL;
    }

    if (!escapedURL.IsEmpty())
      url = ToNewCString(escapedURL);

    /* Check whether the URI is allowed, but not for dialogs --
       see bug 56851. The security of this function depends on
       window.openDialog being inaccessible from web scripts */
    if (url && !aDialog)
      rv = SecurityCheckURL(url);
  }

  if (!aName.IsEmpty()) {
    name = ToNewUTF8String(aName);
  }

  if (!aOptions.IsEmpty() /* IsNullDOMString(aOptions) */) {
    features = ToNewUTF8String(aOptions);
  }

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
    if (wwatch) {
      if (argc) {
        nsCOMPtr<nsPIWindowWatcher> pwwatch(do_QueryInterface(wwatch));
        NS_ENSURE_TRUE(pwwatch, NS_ERROR_UNEXPECTED);

        PRUint32 extraArgc = argc >= 3 ? argc-3 : 0;
        rv = pwwatch->OpenWindowJS(this, url, name, features, aDialog,
                                   extraArgc, argv+3,
                                   getter_AddRefs(domReturn));
      } else {
        rv = wwatch->OpenWindow(this, url, name, features, aExtraArgument,
                                getter_AddRefs(domReturn));
      }

      if (domReturn)
        CallQueryInterface(domReturn, aReturn);
    }
  }

  if (features)
    nsMemory::Free(features);
  if (name)
    nsMemory::Free(name);
  if (url)
    nsMemory::Free(url);

  return rv;
}

void
GlobalWindowImpl::CloseWindow(nsISupports *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));

  win->ReallyCloseWindow();
}

//*****************************************************************************
// GlobalWindowImpl: Timeout Functions
//*****************************************************************************

static const char * const kSetIntervalStr = "setInterval";
static const char * const kSetTimeoutStr = "setTimeout";

nsresult
GlobalWindowImpl::SetTimeoutOrInterval(PRBool aIsInterval, PRInt32 *aReturn)
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
    if (!(expr = ::JS_ValueToString(cx, argv[0])))
      return NS_ERROR_FAILURE;
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

  // Initial ref_count to indicate that this timeout struct will
  // be held as the closure of a timer.
  timeout->ref_count = 1;
  if (aIsInterval)
    timeout->interval = (PRInt32) interval;
  if (expr) {
    if (!::JS_AddNamedRoot(cx, &timeout->expr, "timeout.expr")) {
      delete timeout;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    timeout->expr = expr;
  }
  else if (funobj) {
    int i;

    /* Leave an extra slot for a secret final argument that
       indicates to the called function how "late" the timeout is. */
    timeout->argv = (jsval *) PR_MALLOC((argc - 1) * sizeof(jsval));
    if (!timeout->argv) {
      DropTimeout(timeout);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!::JS_AddNamedRoot(cx, &timeout->funobj, "timeout.funobj")) {
      DropTimeout(timeout);
      return NS_ERROR_FAILURE;
    }
    timeout->funobj = funobj;

    timeout->argc = 0;
    for (i = 2; (PRUint32) i < argc; i++) {
      timeout->argv[i - 2] = argv[i];
      if (!::JS_AddNamedRoot(cx, &timeout->argv[i - 2], "timeout.argv[i]")) {
        DropTimeout(timeout);
        return NS_ERROR_FAILURE;
      }
      timeout->argc++;
    }
  }

  const char *filename;
  if (nsJSUtils::GetCallingLocation(cx, &filename, &timeout->lineno)) {
    timeout->filename = PL_strdup(filename);
    if (!timeout->filename) {
      DropTimeout(timeout);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  timeout->version = ::JS_VersionToString(::JS_GetVersion(cx));

  // Get principal of currently executing code, save for execution of timeout

  rv = sSecMan->GetSubjectPrincipal(getter_AddRefs(timeout->principal));

  if (NS_FAILED(rv)) {
    DropTimeout(timeout);
    return NS_ERROR_FAILURE;
  }

  LL_I2L(now, PR_IntervalNow());
  LL_D2L(delta, PR_MillisecondsToInterval((PRUint32) interval));
  LL_ADD(timeout->when, now, delta);
  nsresult err;
  timeout->timer = do_CreateInstance("@mozilla.org/timer;1", &err);
  if (NS_OK != err) {
    DropTimeout(timeout);
    return err;
  }

  err = timeout->timer->InitWithFuncCallback(TimerCallback, timeout, (PRInt32)interval,
                                             nsITimer::TYPE_ONE_SHOT);
  if (NS_OK != err) {
    DropTimeout(timeout);
    return err;
  }
  timeout->window = this;
  NS_ADDREF(timeout->window);

  InsertTimeoutIntoList(mTimeoutInsertionPoint, timeout);
  timeout->public_id = ++mTimeoutPublicIdCounter;
  *aReturn = timeout->public_id;

  return NS_OK;
}

void
GlobalWindowImpl::RunTimeout(nsTimeoutImpl *aTimeout)
{
  PRBool scripts_enabled = PR_TRUE;

  if (!mContext) {
    return;
  }

  mContext->GetScriptsEnabled(&scripts_enabled);

  if (!scripts_enabled) {
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
  nsresult rv;
  PRUint32 firingDepth = mTimeoutFiringDepth + 1;


  // Make sure that the window or the script context don't go away as
  // a result of running timeouts
  nsCOMPtr<nsIScriptGlobalObject> windowKungFuDeathGrip(this);
  nsCOMPtr<nsIScriptContext> contextKungFuDeathGrip(mContext);

  cx = (JSContext *)mContext->GetNativeContext();

  // A native timer has gone off.  See which of our timeouts need
  // servicing
  LL_I2L(now, PR_IntervalNow());

  if (aTimeout && LL_CMP(aTimeout->when, >, now)) {
    // The OS timer fired early (yikes!), and possibly out of order
    // too. Set |deadline| to be the time when the OS timer *should*
    // have fired so that any timers that *should* have fired before
    // aTimeout *will* be fired now. This happens most of the time on
    // Win2k.

    deadline = aTimeout->when;
  } else {
    deadline = now;
  }

  /* The timeout list is kept in deadline order.  Discover the
     latest timeout whose deadline has expired. On some platforms,
     native timeout events fire "early", so we need to test the
     timer as well as the deadline. */
  last_expired_timeout = nsnull;
  for (timeout = mTimeouts; timeout; timeout = timeout->next) {
    if (((timeout == aTimeout) || !LL_CMP(timeout->when, >, deadline)) &&
        (timeout->firingDepth == 0)) {
      /*
       * Mark any timeouts that are on the list to be fired with the
       * firing depth so that we can reentrantly run timeouts
       */
      timeout->firingDepth = firingDepth;
      last_expired_timeout = timeout;

      // If the timeout has a timer it means that it's a timeout
      // that's due to be fired but it's OS timer hasn't fired yet.
      // We'll go ahead and handle the timeout now so we can cancel
      // the OS timer for the timeout and relese the OS timer's
      // reference to the timeout (and set timeout->timer to nsnull to
      // indicate that the OS timer no longer owns the timeout).
      if (timeout->timer) {
        timeout->timer->Cancel();
        timeout->timer = nsnull;

        DropTimeout(timeout);
      }
    }
  }

  /* Maybe the timeout that the event was fired for has been deleted
     and there are no others timeouts with deadlines that make them
     eligible for execution yet.  Go away. */
  if (!last_expired_timeout) {
    return;
  }

  /* Insert a dummy timeout into the list of timeouts between the portion
     of the list that we are about to process now and those timeouts that
     will be processed in a future call to win_run_timeout().  This dummy
     timeout serves as the head of the list for any timeouts inserted as
     a result of running a timeout. */
  dummy_timeout.firingDepth = firingDepth;
  dummy_timeout.next = last_expired_timeout->next;
  last_expired_timeout->next = &dummy_timeout;

  /* Don't let ClearWindowTimeouts throw away our stack-allocated
     dummy timeout. */
  dummy_timeout.ref_count = 2;

  last_insertion_point = mTimeoutInsertionPoint;
  mTimeoutInsertionPoint = &dummy_timeout.next;

  prev = nsnull;
  for (timeout = mTimeouts; timeout != &dummy_timeout; timeout = next) {
    next = timeout->next;

    /*
     * Check to see if it should fire at this depth. If it shouldn't, we'll
     * ignore it
     */
    if (timeout->firingDepth == firingDepth) {
      nsTimeoutImpl *last_running_timeout = mRunningTimeout;
      mRunningTimeout = timeout;

      PRBool old_timeouts_were_cleared = mTimeoutsWereCleared;
      mTimeoutsWereCleared = PR_FALSE;

      /* Hold the timeout in case expr or funobj releases its doc. */
      HoldTimeout(timeout);

      ++mTimeoutFiringDepth;

      if (timeout->expr) {
        /* Evaluate the timeout expression. */
        const PRUnichar *script =
          NS_REINTERPRET_CAST(const PRUnichar *,
                              ::JS_GetStringChars(timeout->expr));

        nsAutoString retval;
        PRBool is_undefined;
        rv = mContext->EvaluateString(nsDependentString(script), mJSObject,
                                      timeout->principal, timeout->filename,
                                      timeout->lineno, timeout->version,
                                      retval, &is_undefined);
      } else {
        PRInt64 lateness64;
        PRInt32 lateness;

        /* Add "secret" final argument that indicates timeout
           lateness in milliseconds */
        LL_SUB(lateness64, now, timeout->when);
        LL_L2I(lateness, lateness64);
        lateness = PR_IntervalToMilliseconds(lateness);
        timeout->argv[timeout->argc] = INT_TO_JSVAL((jsint) lateness);

        PRBool bool_result;
        rv = mContext->CallEventHandler(mJSObject, timeout->funobj,
                                        timeout->argc + 1, timeout->argv,
                                        &bool_result, PR_FALSE);
      }

      --mTimeoutFiringDepth;
      mRunningTimeout = last_running_timeout;

      PRBool timeouts_were_cleared = mTimeoutsWereCleared;
      mTimeoutsWereCleared = old_timeouts_were_cleared;

      // We ignore any failures from calling EvaluateString() or
      // CallEventHandler() on the context here since we're in a loop
      // where we're likely to be running timeouts whose OS timers
      // didn't fire in time and we don't want to not fire those timers
      // now just because execution of one timer failed. We can't
      // propagate the error to anyone who cares about it from this
      // point anyway so we just drop it.

      DropTimeout(timeout, contextKungFuDeathGrip);

      // If the timeout is no longer in the list of timeouts in this
      // window it means that all timeouts were cleared (i.e. the
      // document was either destroyed or cleared). In that case we
      // simply return here since all timers were destroyed in the
      // document where timeout originated from.

      if (timeouts_were_cleared) {
        mTimeoutInsertionPoint = last_insertion_point;

        return;
      }

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
         * have happened then run the timeout immediately.
         */
        if (delay32 < 0)
          delay32 = 0;

        delay32 = PR_IntervalToMilliseconds(delay32);

        /* Reschedule timeout.  Account for possible error return in
           code below that checks for zero toid. */
        timeout->timer = do_CreateInstance("@mozilla.org/timer;1");

        // Since we're in a loop where we're likely to be running
        // multiple timeouts that were due to be handled when we
        // entered this method (but the OS timers for those timeouts
        // hadn't fired yet) we don't do an early return here just
        // because we couldn't create a timer for an interval timeout
        // since we still want to continue running the timeouts that
        // were due to be handled when we entered this method. This
        // method is never called by code that cares about success or
        // failure any way so dropping the error on the floor here is
        // no big deal.

        if (timeout->timer) {
          rv = timeout->timer->InitWithFuncCallback(TimerCallback, timeout, delay32,
                                                    nsITimer::TYPE_ONE_SHOT);

          // Likewise, don't return early even if we fail to
          // initialize the new OS timer.

          if (NS_SUCCEEDED(rv)) {
            // Increment ref_count to indicate that this OS timer is
            // holding on to the timeout struct.
            HoldTimeout(timeout);
          } else {
            NS_ERROR("Error initializing timer for DOM timeout!");

            // We failed to initialize the new OS timer, this timer
            // does us no good here so we just null out the pointer to
            // the OS timer, this will release the OS timer. As we
            // continue executing the code below we'll end up deleting
            // the timeout since it's not an interval timeout any more
            // (since timeout->timer == nsnull).

            timeout->timer = nsnull;
          }
        } else {
          NS_ERROR("Error creating timer for DOM timeout!");

          // If we weren't able to create a new OS timer for 'timeout'
          // we'll fall through here and the code below will delete
          // the timeout (since timeout->timer == nsnull).
        }
      }

      /* Running a timeout can cause another timeout to be deleted,
         so we need to reset the pointer to the following timeout. */
      next = timeout->next;

      if (!prev) {
        mTimeouts = next;
      } else {
        prev->next = next;
      }

      PRBool isInterval = (timeout->interval && timeout->timer);

      // Drop timeout struct since it's out of the list
      DropTimeout(timeout, contextKungFuDeathGrip);

      if (isInterval) {
        // Reschedule an interval timeout. Insert interval timeout
        // onto list sorted in deadline order.

        InsertTimeoutIntoList(mTimeoutInsertionPoint, timeout);
      }
    } else {
      // We skip the timeout since it's on the list to run at another
      // depth.

      prev = timeout;
    }
  }

  /* Take the dummy timeout off the head of the list */
  if (!prev) {
    mTimeouts = dummy_timeout.next;
  } else {
    prev->next = dummy_timeout.next;
  }

  mTimeoutInsertionPoint = last_insertion_point;

  return;
}

void
GlobalWindowImpl::DropTimeout(nsTimeoutImpl *aTimeout,
                              nsIScriptContext *aContext)
{
  JSRuntime *rt = nsnull;

  if (--aTimeout->ref_count > 0)
    return;

  if (!aContext)
    aContext = mContext;
  if (aContext) {
    JSContext *cx;
    cx = (JSContext *)aContext->GetNativeContext();
    rt = ::JS_GetRuntime(cx);
  } else {
    /* XXX The timeout *must* be unrooted, even if !aContext. This can be
       done without a JS context using the JSRuntime. This is safe enough,
       but it would be better to drop all a window's timeouts before its
       context is cleared. Bug 50705 describes a situation where we're not.
       In that case, at the time the context is cleared, a timeout (actually
       an Interval) is still active, but temporarily removed from the window's
       list of timers (placed instead on the timer manager's list). This makes
       the nearly handy ClearAllTimeouts routine useless, so we we settled on
       using the JSRuntime rather than relying on the window having a context.
       It would be good to remedy this workable but clumsy situation someday.
     */
    NS_WARNING("DropTimeout proceeding without context.");
    nsCOMPtr<nsIJSRuntimeService> rtsvc =
      do_GetService("@mozilla.org/js/xpc/RuntimeService;1");

    if (rtsvc)
      rtsvc->GetRuntime(&rt);
  }

  NS_ASSERTION(rt, "DropTimeout with no JSRuntime. eek!");
  if (!rt) // most unexpected. not much choice but to bail.
    return;

  if (aTimeout->expr) {
    ::JS_RemoveRootRT(rt, &aTimeout->expr);
  } else if (aTimeout->funobj) {
    ::JS_RemoveRootRT(rt, &aTimeout->funobj);

    if (aTimeout->argv) {
      int i;
      for (i = 0; i < aTimeout->argc; i++)
        ::JS_RemoveRootRT(rt, &aTimeout->argv[i]);
      PR_FREEIF(aTimeout->argv);
    }
  }

  if (aTimeout->timer) {
    aTimeout->timer->Cancel();
    aTimeout->timer = nsnull;
  }

  PR_FREEIF(aTimeout->filename);

  NS_IF_RELEASE(aTimeout->window);

  delete aTimeout;
}

void
GlobalWindowImpl::HoldTimeout(nsTimeoutImpl *aTimeout)
{
  aTimeout->ref_count++;
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
          timeout->timer = nsnull;
          DropTimeout(timeout);
        }
        DropTimeout(timeout);
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

    next = timeout->next;

    if (timeout->timer) {
      timeout->timer->Cancel();
      timeout->timer = nsnull;

      // Drop the count since the timer isn't going to hold on
      // anymore.
      DropTimeout(timeout);
    }

    // Drop the count since we're removing it from the list.
    DropTimeout(timeout);
  }

  mTimeouts = NULL;

  mTimeoutsWereCleared = PR_TRUE;
}

void
GlobalWindowImpl::InsertTimeoutIntoList(nsTimeoutImpl **aList,
                                        nsTimeoutImpl *aTimeout)
{
  nsTimeoutImpl *to;

  NS_ASSERTION(aList,
               "GlobalWindowImpl::InsertTimeoutIntoList null timeoutList");
  while ((to = *aList) != nsnull) {
    if (LL_CMP(to->when, >, aTimeout->when))
      break;
    aList = &to->next;
  }
  aTimeout->firingDepth = 0;
  aTimeout->next = to;
  *aList = aTimeout;

  // Increment the ref_count since we're in the list
  HoldTimeout(aTimeout);
}

// static
void
GlobalWindowImpl::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsTimeoutImpl *timeout = (nsTimeoutImpl *)aClosure;

  // Null out the pointer to the OS timer (aTimer) in the timeout to
  // to indicate that the timer (aTimer) no longer owns the
  // timeout. From now on 'timeout' has the reference to the timeout.
  timeout->timer = nsnull;

  timeout->window->RunTimeout(timeout);

  // Drop timeout's reference to the timeout.
  timeout->window->DropTimeout(timeout);
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
  *aBrowserChrome = browserChrome;
  NS_IF_ADDREF(*aBrowserChrome);
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
  FlushPendingNotifications(PR_TRUE);

  nsCOMPtr<nsIPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (presContext) {
    presContext->GetPixelsToTwips(aP2T);
    presContext->GetTwipsToPixels(aT2P);

    nsCOMPtr<nsIPresShell> presShell;
    presContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsIViewManager> vm;
      presShell->GetViewManager(getter_AddRefs(vm));
      if (vm)
        return vm->GetRootScrollableView(aScrollableView);
    }
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

  // get the JSContext from the call stack
  nsCOMPtr<nsIThreadJSContextStack> stack(do_GetService(sJSStackContractID));
  if (stack)
    stack->Peek(&cx);
  if (!cx)        // not bloody likely. but if there's no JS on the call stack,
    return NS_OK; // then we should pass the security check.

  /* resolve the URI, which could be relative to the calling window
     (note the algorithm to get the base URI should match the one
     used to actually kick off the load in nsWindowWatcher.cpp). */
  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIURI> uriToLoad;

  nsCOMPtr<nsIScriptContext> scriptcx =
    NS_STATIC_CAST(nsIScriptContext *, ::JS_GetContextPrivate(cx));
  if (scriptcx) {
    nsCOMPtr<nsIScriptGlobalObject> gobj;
    scriptcx->GetGlobalObject(getter_AddRefs(gobj));
    nsCOMPtr<nsIDOMWindow> caller(do_QueryInterface(gobj));
    if (caller) {
      nsCOMPtr<nsIDOMDocument> callerDOMdoc;
      caller->GetDocument(getter_AddRefs(callerDOMdoc));
      nsCOMPtr<nsIDocument> callerDoc(do_QueryInterface(callerDOMdoc));
      if (callerDoc)
        callerDoc->GetDocumentURL(getter_AddRefs(baseURI));
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
GlobalWindowImpl::FlushPendingNotifications(PRBool aFlushReflows)
{
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
  if (doc) {
    doc->FlushPendingNotifications(aFlushReflows);
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
  nsCOMPtr<nsIWidget> widget;
  nsresult rv = GetMainWidget(getter_AddRefs(widget));

  if (widget) {
    rv = widget->GetAttention();
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

  if (aCursor.Equals(NS_LITERAL_STRING("auto")))
    cursor = NS_STYLE_CURSOR_AUTO;
  else if (aCursor.Equals(NS_LITERAL_STRING("default")))
    cursor = NS_STYLE_CURSOR_DEFAULT;
  else if (aCursor.Equals(NS_LITERAL_STRING("pointer")))
    cursor = NS_STYLE_CURSOR_POINTER;
  else if (aCursor.Equals(NS_LITERAL_STRING("crosshair")))
    cursor = NS_STYLE_CURSOR_CROSSHAIR;
  else if (aCursor.Equals(NS_LITERAL_STRING("move")))
    cursor = NS_STYLE_CURSOR_MOVE;
  else if (aCursor.Equals(NS_LITERAL_STRING("text")))
    cursor = NS_STYLE_CURSOR_TEXT;
  else if (aCursor.Equals(NS_LITERAL_STRING("wait")))
    cursor = NS_STYLE_CURSOR_WAIT;
  else if (aCursor.Equals(NS_LITERAL_STRING("help")))
    cursor = NS_STYLE_CURSOR_HELP;
  else if (aCursor.Equals(NS_LITERAL_STRING("n-resize")))
    cursor = NS_STYLE_CURSOR_N_RESIZE;
  else if (aCursor.Equals(NS_LITERAL_STRING("s-resize")))
    cursor = NS_STYLE_CURSOR_S_RESIZE;
  else if (aCursor.Equals(NS_LITERAL_STRING("w-resize")))
    cursor = NS_STYLE_CURSOR_W_RESIZE;
  else if (aCursor.Equals(NS_LITERAL_STRING("e-resize")))
    cursor = NS_STYLE_CURSOR_E_RESIZE;
  else if (aCursor.Equals(NS_LITERAL_STRING("ne-resize")))
    cursor = NS_STYLE_CURSOR_NE_RESIZE;
  else if (aCursor.Equals(NS_LITERAL_STRING("nw-resize")))
    cursor = NS_STYLE_CURSOR_NW_RESIZE;
  else if (aCursor.Equals(NS_LITERAL_STRING("se-resize")))
    cursor = NS_STYLE_CURSOR_SE_RESIZE;
  else if (aCursor.Equals(NS_LITERAL_STRING("sw-resize")))
    cursor = NS_STYLE_CURSOR_SW_RESIZE;
  else if (aCursor.Equals(NS_LITERAL_STRING("copy")))
    cursor = NS_STYLE_CURSOR_COPY;      // CSS3
  else if (aCursor.Equals(NS_LITERAL_STRING("alias")))
    cursor = NS_STYLE_CURSOR_ALIAS;
  else if (aCursor.Equals(NS_LITERAL_STRING("context-menu")))
    cursor = NS_STYLE_CURSOR_CONTEXT_MENU;
  else if (aCursor.Equals(NS_LITERAL_STRING("cell")))
    cursor = NS_STYLE_CURSOR_CELL;
  else if (aCursor.Equals(NS_LITERAL_STRING("grab")))
    cursor = NS_STYLE_CURSOR_GRAB;
  else if (aCursor.Equals(NS_LITERAL_STRING("grabbing")))
    cursor = NS_STYLE_CURSOR_GRABBING;
  else if (aCursor.Equals(NS_LITERAL_STRING("spinning")))
    cursor = NS_STYLE_CURSOR_SPINNING;
  else if (aCursor.Equals(NS_LITERAL_STRING("count-up")))
    cursor = NS_STYLE_CURSOR_COUNT_UP;
  else if (aCursor.Equals(NS_LITERAL_STRING("count-down")))
    cursor = NS_STYLE_CURSOR_COUNT_DOWN;
  else if (aCursor.Equals(NS_LITERAL_STRING("count-up-down")))
    cursor = NS_STYLE_CURSOR_COUNT_UP_DOWN;
  else
    return NS_OK;

  nsCOMPtr<nsIPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (presContext) {
    nsCOMPtr<nsIEventStateManager> esm;
    if (NS_SUCCEEDED(presContext->GetEventStateManager(getter_AddRefs(esm)))) {
      // Need root widget.
      nsCOMPtr<nsIPresShell> presShell;
      mDocShell->GetPresShell(getter_AddRefs(presShell));
      NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

      nsCOMPtr<nsIViewManager> vm;
      presShell->GetViewManager(getter_AddRefs(vm));
      NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);

      nsIView *rootView;
      vm->GetRootView(rootView);
      NS_ENSURE_TRUE(rootView, NS_ERROR_FAILURE);

      nsCOMPtr<nsIWidget> widget;
      rootView->GetWidget(*getter_AddRefs(widget));
      NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

      // Call esm and set cursor.
      rv = esm->SetCursor(cursor, widget, PR_TRUE);
    }
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
  mMimeTypes(nsnull),
  mPlugins(nsnull),
  mDocShell(aDocShell)
{
  NS_INIT_ISUPPORTS();
}

NavigatorImpl::~NavigatorImpl()
{
  NS_IF_RELEASE(mMimeTypes);
  NS_IF_RELEASE(mPlugins);
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

    aAppVersion.Append(NS_LITERAL_STRING(" ("));
    res = service->GetPlatform(str);
    if (NS_FAILED(res))
      return res;

    aAppVersion += NS_ConvertASCIItoUCS2(str);

    aAppVersion.Append(NS_LITERAL_STRING("; "));
    res = service->GetLanguage(str);
    if (NS_FAILED(res))
      return res;

    aAppVersion += NS_ConvertASCIItoUCS2(str);

    aAppVersion.Append(PRUnichar(')'));
  }

  return res;
}

NS_IMETHODIMP
NavigatorImpl::GetAppName(nsAString& aAppName)
{
  aAppName.Assign(NS_LITERAL_STRING("Netscape"));
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
    aPlatform = NS_LITERAL_STRING("Win32");
#elif defined(XP_MAC) || defined(XP_MACOSX)
    // XXX not sure what to do about Mac OS X on non-PPC, but since Comm 4.x
    // doesn't know about it this will actually be backward compatible
    aPlatform = NS_LITERAL_STRING("MacPPC");
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
    NS_IF_ADDREF(mMimeTypes);
  }

  *aMimeTypes = mMimeTypes;
  NS_IF_ADDREF(mMimeTypes);

  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::GetPlugins(nsIDOMPluginArray **aPlugins)
{
  if (!mPlugins) {
    mPlugins = new PluginArrayImpl(this, mDocShell);
    if (!mPlugins)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(mPlugins);
  }

  *aPlugins = mPlugins;
  NS_ADDREF(mPlugins);

  return NS_OK;
}

NS_IMETHODIMP
NavigatorImpl::GetCookieEnabled(PRBool *aCookieEnabled)
{
  nsresult rv = NS_OK;
  *aCookieEnabled = PR_FALSE;

  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
  if (NS_FAILED(rv) || prefs == nsnull)
    return rv;

  PRInt32 cookieBehaviorPref;
  rv = prefs->GetIntPref("network.cookie.cookieBehavior", &cookieBehaviorPref);

  if (NS_FAILED(rv))
    return rv;

  const PRInt32 DONT_USE = 2;
  *aCookieEnabled = (cookieBehaviorPref != DONT_USE);
  return rv;
}

NS_IMETHODIMP
NavigatorImpl::JavaEnabled(PRBool *aReturn)
{
  nsresult rv = NS_OK;
  *aReturn = PR_FALSE;

  // determine whether user has enabled java.
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  // if pref doesn't exist, map result to false.
  if (NS_FAILED(prefs->GetBoolPref("security.enable_java", aReturn))) {
    *aReturn = PR_FALSE;
    return NS_OK;
  }

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

  nsCOMPtr<nsIPref> pref(do_GetService(kPrefServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  JSString *str = ::JS_ValueToString(cx, argv[0]);
  NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

  jsval *retval = nsnull;

  rv = ncc->GetRetValPtr(&retval);
  NS_ENSURE_SUCCESS(rv, rv);

  char *prefStr = ::JS_GetStringBytes(str);
  if (argc == 1) {
    PRInt32 prefType;

    pref->GetPrefType(prefStr, &prefType);

    switch (prefType & nsIPref::ePrefValuetypeMask) {
    case nsIPref::ePrefString:
      {
        nsXPIDLCString prefCharVal;
        rv = pref->CopyCharPref(prefStr, getter_Copies(prefCharVal));
        NS_ENSURE_SUCCESS(rv, rv);

        JSString *retStr = ::JS_NewStringCopyZ(cx, prefCharVal);
        NS_ENSURE_TRUE(retStr, NS_ERROR_OUT_OF_MEMORY);

        *retval = STRING_TO_JSVAL(retStr);

        break;
      }

    case nsIPref::ePrefInt:
      {
        PRInt32 prefIntVal;
        rv = pref->GetIntPref(prefStr, &prefIntVal);
        NS_ENSURE_SUCCESS(rv, rv);

        *retval = INT_TO_JSVAL(prefIntVal);

        break;
      }

    case nsIPref::ePrefBool:
      {
        PRBool prefBoolVal;

        rv = pref->GetBoolPref(prefStr, &prefBoolVal);
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

      rv = pref->SetCharPref(prefStr, ::JS_GetStringBytes(valueJSStr));
    } else if (JSVAL_IS_INT(argv[1])) {
      jsint valueInt = JSVAL_TO_INT(argv[1]);

      rv = pref->SetIntPref(prefStr, (PRInt32) valueInt);
    } else if (JSVAL_IS_BOOLEAN(argv[1])) {
      JSBool valueBool = JSVAL_TO_BOOLEAN(argv[1]);

      rv = pref->SetBoolPref(prefStr, (PRBool) valueBool);
    } else if (JSVAL_IS_NULL(argv[1])) {
      rv = pref->DeleteBranch(prefStr);
    }
  }

  return rv;
}


nsresult
NavigatorImpl::RefreshMIMEArray()
{
  nsresult rv = NS_OK;
  if (mMimeTypes)
    rv = mMimeTypes->Refresh();
  return rv;
}

#ifdef XP_MAC
#pragma mark -
#endif

//*****************************************************************************
//***  DOM Controller Stuff
//*****************************************************************************

#ifdef DOM_CONTROLLER
// nsDOMWindowController
const char * const sCopyString = "cmd_copy";
const char * const sCutString = "cmd_cut";
const char * const sPasteString = "cmd_paste";
const char * const sSelectAllString = "cmd_selectAll";
const char * const sSelectNoneString = "cmd_selectNone";
const char * const sCopyLinkString = "cmd_copyLink";
const char * const sCopyImageLocationString = "cmd_copyImageLocation";
const char * const sCopyImageContentsString = "cmd_copyImageContents";

const char * const sScrollTopString = "cmd_scrollTop";
const char * const sScrollBottomString = "cmd_scrollBottom";
const char * const sScrollPageUpString = "cmd_scrollPageUp";
const char * const sScrollPageDownString = "cmd_scrollPageDown";
const char * const sMovePageUpString = "cmd_movePageUp";
const char * const sMovePageDownString = "cmd_movePageDown";
const char * const sScrollLineUpString = "cmd_scrollLineUp";
const char * const sScrollLineDownString = "cmd_scrollLineDown";
const char * const sScrollLeftString = "cmd_scrollLeft";
const char * const sScrollRightString = "cmd_scrollRight";
const char * const sBrowserBackString = "cmd_browserBack";
const char * const sBrowserForwardString = "cmd_browserForward";

// These are so the browser can use editor navigation key bindings
// helps with accessibility (boolean pref accessibility.browsewithcaret)

const char * const sSelectCharPreviousString = "cmd_selectCharPrevious";
const char * const sSelectCharNextString = "cmd_selectCharNext";

const char * const sWordPreviousString = "cmd_wordPrevious";
const char * const sWordNextString = "cmd_wordNext";
const char * const sSelectWordPreviousString = "cmd_selectWordPrevious";
const char * const sSelectWordNextString = "cmd_selectWordNext";

const char * const sBeginLineString = "cmd_beginLine";
const char * const sEndLineString = "cmd_endLine";
const char * const sSelectBeginLineString = "cmd_selectBeginLine";
const char * const sSelectEndLineString = "cmd_selectEndLine";

const char * const sSelectLinePreviousString = "cmd_selectLinePrevious";
const char * const sSelectLineNextString = "cmd_selectLineNext";

const char * const sSelectPagePreviousString = "cmd_selectPagePrevious";
const char * const sSelectPageNextString = "cmd_selectPageNext";

const char * const sSelectMoveTopString = "cmd_selectMoveTop";
const char * const sSelectMoveBottomString = "cmd_selectMoveBottom";

NS_IMPL_ADDREF(nsDOMWindowController)
NS_IMPL_RELEASE(nsDOMWindowController)

NS_INTERFACE_MAP_BEGIN(nsDOMWindowController)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIController)
  NS_INTERFACE_MAP_ENTRY(nsIController)
NS_INTERFACE_MAP_END


nsDOMWindowController::nsDOMWindowController(nsIDOMWindowInternal *aWindow)
{
  NS_INIT_ISUPPORTS();
  mWindow = aWindow;

  // Set mBrowseWithCaret so we don't need to check pref every time
  mBrowseWithCaret = PR_FALSE;
  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_SUCCEEDED(GetEventStateManager(getter_AddRefs(esm))))
    esm->ResetBrowseWithCaret(&mBrowseWithCaret);
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
  if (prefs)
    prefs->RegisterCallback("accessibility.browsewithcaret", (PrefChangedFunc)nsDOMWindowController::BrowseWithCaretPrefCallback, (void*)this);
}

nsDOMWindowController::~nsDOMWindowController()
{
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
  if (prefs) 
    prefs->UnregisterCallback("accessibility.browsewithcaret", (PrefChangedFunc)nsDOMWindowController::BrowseWithCaretPrefCallback, (void*)this);
}

nsresult
nsDOMWindowController::GetEventStateManager(nsIEventStateManager **aEventStateManager)
{
  *aEventStateManager = nsnull;
  // Set browse with caret flag so we don't need to every time
  nsCOMPtr<nsIPresShell> presShell;
  GetPresShell(getter_AddRefs(presShell));

  if (presShell) {
    nsCOMPtr<nsIPresContext> presContext;
    presShell->GetPresContext(getter_AddRefs(presContext));
    if (presContext) {
      nsCOMPtr<nsIEventStateManager> esm;
      presContext->GetEventStateManager(getter_AddRefs(esm));
      *aEventStateManager = esm;
      if (esm) {
        NS_ADDREF(*aEventStateManager);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}

int PR_CALLBACK
nsDOMWindowController::BrowseWithCaretPrefCallback(const char* aPrefName, void* instance_data)
{
  nsDOMWindowController* domWindowController = (nsDOMWindowController*)instance_data;
  NS_ASSERTION(domWindowController, "bad instance data");

  nsCOMPtr<nsIEventStateManager> esm;

  if (NS_SUCCEEDED(domWindowController->GetEventStateManager(getter_AddRefs(esm))))
    esm->ResetBrowseWithCaret(&domWindowController->mBrowseWithCaret);

  return 0;  // PREF_OK
}

nsresult
nsDOMWindowController::GetEditInterface(nsIContentViewerEdit **aEditInterface)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(mWindow));
  NS_ENSURE_TRUE(sgo, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell;
  sgo->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContentViewer> viewer;
  docShell->GetContentViewer(getter_AddRefs(viewer));
  nsCOMPtr<nsIContentViewerEdit> edit(do_QueryInterface(viewer));
  NS_ENSURE_TRUE(edit, NS_ERROR_FAILURE);

  *aEditInterface = edit;
  NS_ADDREF(*aEditInterface);
  return NS_OK;
}

nsresult
nsDOMWindowController::GetPresShell(nsIPresShell **aPresShell)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(mWindow));
  NS_ENSURE_TRUE(sgo, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell;
  sgo->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(docShell->GetPresShell(aPresShell), NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult
nsDOMWindowController::GetSelectionController(nsISelectionController **aSelCon)
{
  nsCOMPtr<nsIPresShell> presShell;
  nsresult result = GetPresShell(getter_AddRefs(presShell));
  if (presShell && NS_SUCCEEDED(result)) {
    nsCOMPtr<nsISelectionController> selController =
      do_QueryInterface(presShell);
    if (selController) {
      *aSelCon = selController;
      NS_ADDREF(*aSelCon);
      return NS_OK;
    }
  }
  return NS_FAILED(result) ? result : NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsDOMWindowController::IsCommandEnabled(const char* aCommand,
                                        PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = PR_FALSE;
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIContentViewerEdit> editInterface;
  rv = GetEditInterface(getter_AddRefs(editInterface));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(editInterface, NS_ERROR_NOT_INITIALIZED);

	
  if (!nsCRT::strcmp(sCopyString,aCommand)) {
    rv = editInterface->GetCopyable(aResult);
  }
  else if (!nsCRT::strcmp(sCutString,aCommand)) {
    rv = editInterface->GetCutable(aResult);
  }
  else if (!nsCRT::strcmp(sPasteString,aCommand)) {
    rv = editInterface->GetPasteable(aResult);
  }
  else if (!nsCRT::strcmp(sSelectAllString,aCommand)) {
    *aResult = PR_TRUE;
    rv = NS_OK;
  }
  else if (!nsCRT::strcmp(sSelectNoneString,aCommand)) {
    *aResult = PR_TRUE;
    rv = NS_OK;
  }
  else if (!nsCRT::strcmp(sCopyLinkString,aCommand)) {
    rv = editInterface->GetInLink(aResult);
  }
  else if (!nsCRT::strcmp(sCopyImageLocationString,aCommand)) {
    rv = editInterface->GetInImage(aResult);
  }
  else if (!nsCRT::strcmp(sCopyImageContentsString,aCommand)) {
    rv = editInterface->GetInImage(aResult);
  }

  return rv;
}

NS_IMETHODIMP
nsDOMWindowController::SupportsCommand(const char * aCommand,
                                       PRBool *outSupported)
{
  NS_ENSURE_ARG_POINTER(outSupported);

  *outSupported = PR_FALSE;

  if (!nsCRT::strcmp(aCommand,sCopyString) ||
      !nsCRT::strcmp(aCommand,sSelectAllString) ||
      !nsCRT::strcmp(aCommand,sSelectNoneString) ||
      !nsCRT::strcmp(aCommand,sCutString) ||
      !nsCRT::strcmp(aCommand,sPasteString) ||
      !nsCRT::strcmp(aCommand,sScrollTopString) ||
      !nsCRT::strcmp(aCommand,sScrollBottomString) ||
      !nsCRT::strcmp(aCommand,sCopyLinkString) ||
      !nsCRT::strcmp(aCommand,sCopyImageLocationString) ||
      !nsCRT::strcmp(aCommand,sCopyImageContentsString) ||
      !nsCRT::strcmp(aCommand,sScrollPageUpString) ||
      !nsCRT::strcmp(aCommand,sScrollPageDownString) ||
      !nsCRT::strcmp(aCommand,sMovePageUpString) ||
      !nsCRT::strcmp(aCommand,sMovePageDownString) ||
      !nsCRT::strcmp(aCommand,sBrowserBackString) ||
      !nsCRT::strcmp(aCommand,sBrowserForwardString) ||
      !nsCRT::strcmp(aCommand,sScrollLineUpString) ||
      !nsCRT::strcmp(aCommand,sScrollLineDownString) ||
      !nsCRT::strcmp(aCommand,sScrollLeftString) ||
      !nsCRT::strcmp(aCommand,sScrollRightString) ||
      !nsCRT::strcmp(aCommand,sSelectCharPreviousString) ||
      !nsCRT::strcmp(aCommand,sSelectCharNextString) ||
      !nsCRT::strcmp(aCommand,sWordPreviousString) ||
      !nsCRT::strcmp(aCommand,sWordNextString) ||
      !nsCRT::strcmp(aCommand,sSelectWordPreviousString) ||
      !nsCRT::strcmp(aCommand,sSelectWordNextString) ||
      !nsCRT::strcmp(aCommand,sBeginLineString) ||
      !nsCRT::strcmp(aCommand,sEndLineString) ||
      !nsCRT::strcmp(aCommand,sSelectBeginLineString) ||
      !nsCRT::strcmp(aCommand,sSelectEndLineString) ||
      !nsCRT::strcmp(aCommand,sSelectLinePreviousString) ||
      !nsCRT::strcmp(aCommand,sSelectLineNextString) ||
      !nsCRT::strcmp(aCommand,sSelectPagePreviousString) ||
      !nsCRT::strcmp(aCommand,sSelectPageNextString) ||
      !nsCRT::strcmp(aCommand,sSelectMoveTopString) ||
      !nsCRT::strcmp(aCommand,sSelectMoveBottomString)
      )
    *outSupported = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowController::DoCommand(const char *aCommand)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (!nsCRT::strcmp(aCommand,sCopyString) ||
      !nsCRT::strcmp(aCommand,sSelectAllString) ||
      !nsCRT::strcmp(aCommand,sSelectNoneString) ||
      !nsCRT::strcmp(aCommand,sCutString) ||
      !nsCRT::strcmp(aCommand,sPasteString) ||
      !nsCRT::strcmp(aCommand,sCopyLinkString) ||
      !nsCRT::strcmp(aCommand,sCopyImageLocationString) ||
      !nsCRT::strcmp(aCommand,sCopyImageContentsString)) {
    rv = DoCommandWithEditInterface(aCommand);
  }
  else if (!nsCRT::strcmp(aCommand,sScrollTopString) ||
           !nsCRT::strcmp(aCommand,sScrollBottomString) ||
           !nsCRT::strcmp(aCommand,sScrollPageUpString) ||
           !nsCRT::strcmp(aCommand,sMovePageDownString) ||
           !nsCRT::strcmp(aCommand,sMovePageUpString) ||
           !nsCRT::strcmp(aCommand,sScrollPageDownString) ||
           !nsCRT::strcmp(aCommand,sScrollLineUpString) ||
           !nsCRT::strcmp(aCommand,sScrollLineDownString) ||
           !nsCRT::strcmp(aCommand,sScrollLeftString) ||
           !nsCRT::strcmp(aCommand,sScrollRightString) ||
           !nsCRT::strcmp(aCommand,sSelectCharPreviousString) ||
           !nsCRT::strcmp(aCommand,sSelectCharNextString) ||
           !nsCRT::strcmp(aCommand,sWordPreviousString) ||
           !nsCRT::strcmp(aCommand,sWordNextString) ||
           !nsCRT::strcmp(aCommand,sSelectWordPreviousString) ||
           !nsCRT::strcmp(aCommand,sSelectWordNextString) ||
           !nsCRT::strcmp(aCommand,sBeginLineString) ||
           !nsCRT::strcmp(aCommand,sEndLineString) ||
           !nsCRT::strcmp(aCommand,sSelectBeginLineString) ||
           !nsCRT::strcmp(aCommand,sSelectEndLineString) ||
           !nsCRT::strcmp(aCommand,sSelectLinePreviousString) ||
           !nsCRT::strcmp(aCommand,sSelectLineNextString) ||
           !nsCRT::strcmp(aCommand,sSelectMoveTopString) ||
           !nsCRT::strcmp(aCommand,sSelectMoveBottomString)) {
    rv = DoCommandWithSelectionController(aCommand);
    // If the user moves the caret in browse with caret mode
    // Focus whatever they move onto, if it's focusable
    if (mBrowseWithCaret) {
      nsCOMPtr<nsIEventStateManager> esm;
      if (NS_SUCCEEDED(GetEventStateManager(getter_AddRefs(esm)))) {
        PRBool isSelectionWithFocus;
        esm->MoveFocusToCaret(PR_TRUE, &isSelectionWithFocus);
      }
    }
  }
  else if (!nsCRT::strcmp(aCommand,sBrowserBackString) ||
    !nsCRT::strcmp(aCommand,sBrowserForwardString)) {
    rv = DoCommandWithWebNavigationInterface(aCommand);
  }

  return rv;
}

nsresult
nsDOMWindowController::DoCommandWithWebNavigationInterface(const char *aCommandName)
{
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(mWindow));

  nsresult rv = NS_ERROR_FAILURE;

  if (webNav) {
    if (!nsCRT::strcmp(aCommandName, sBrowserBackString)) {
      rv = webNav->GoBack();
    }
    else if (!nsCRT::strcmp(aCommandName, sBrowserForwardString)) {
      rv= webNav->GoForward();
    }
  }
  return rv;
}

nsresult
nsDOMWindowController::DoCommandWithEditInterface(const char *aCommandName)
{
  // get edit interface...
  nsCOMPtr<nsIContentViewerEdit> editInterface;
  nsresult rv = GetEditInterface(getter_AddRefs(editInterface));
  // if we can't get an edit interface, that's bad
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(editInterface, NS_ERROR_NOT_INITIALIZED);

  rv = NS_ERROR_FAILURE;

  if (!nsCRT::strcmp(aCommandName,sCopyString))
    rv = editInterface->CopySelection();
  else if (!nsCRT::strcmp(aCommandName,sSelectAllString))
    rv = editInterface->SelectAll();
  else if (!nsCRT::strcmp(aCommandName,sSelectNoneString))
    rv = editInterface->ClearSelection();
  else if (!nsCRT::strcmp(aCommandName,sCutString))
    rv = editInterface->CutSelection();
  else if (!nsCRT::strcmp(aCommandName,sPasteString))
    rv = editInterface->Paste();
  else if (!nsCRT::strcmp(aCommandName,sCopyLinkString))
    rv = editInterface->CopyLinkLocation();
  else if (!nsCRT::strcmp(aCommandName,sCopyImageLocationString))
    rv = editInterface->CopyImageLocation();
  else if (!nsCRT::strcmp(aCommandName,sCopyImageContentsString))
    rv = editInterface->CopyImageContents();

  return rv;

}

nsresult
nsDOMWindowController::DoCommandWithSelectionController(const char *aCommandName) {

  // get selection controller...
  nsCOMPtr<nsISelectionController> selCont;
  nsresult rv = GetSelectionController(getter_AddRefs(selCont));
  // if we can't get a selection controller, that's bad
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(selCont, NS_ERROR_NOT_INITIALIZED);

  rv = NS_ERROR_FAILURE;

  if (!nsCRT::strcmp(aCommandName,sScrollTopString))
    rv = (mBrowseWithCaret? selCont->CompleteMove(PR_FALSE, PR_FALSE): selCont->CompleteScroll(PR_FALSE));
  else if (!nsCRT::strcmp(aCommandName,sScrollBottomString))
    rv = (mBrowseWithCaret? selCont->CompleteMove(PR_TRUE, PR_FALSE): selCont->CompleteScroll(PR_TRUE));
  // cmd_ScrollPageUp/Down are used on Mac.  They do not move the
  // caret in caret browsing mode.
  else if (!nsCRT::strcmp(aCommandName,sScrollPageUpString))
    rv = selCont->ScrollPage(PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName,sScrollPageDownString))
    rv = selCont->ScrollPage(PR_TRUE);
  // cmd_MovePageUp/Down are used on Window/Unix.  They move the caret
  // in caret browsing mode.
  else if (!nsCRT::strcmp(aCommandName,sMovePageUpString))
    rv = (mBrowseWithCaret? selCont->PageMove(PR_FALSE, PR_FALSE): selCont->ScrollPage(PR_FALSE));
  else if (!nsCRT::strcmp(aCommandName,sMovePageDownString))
    rv = (mBrowseWithCaret? selCont->PageMove(PR_TRUE, PR_FALSE): selCont->ScrollPage(PR_TRUE));
  else if (!nsCRT::strcmp(aCommandName,sScrollLineUpString))
    rv = (mBrowseWithCaret? selCont->LineMove(PR_FALSE, PR_FALSE): selCont->ScrollLine(PR_FALSE));
  else if (!nsCRT::strcmp(aCommandName,sScrollLineDownString))
    rv = (mBrowseWithCaret? selCont->LineMove(PR_TRUE, PR_FALSE): selCont->ScrollLine(PR_TRUE));
  else if (!nsCRT::strcmp(aCommandName,sScrollLeftString))
    rv = (mBrowseWithCaret? selCont->CharacterMove(PR_FALSE, PR_FALSE): selCont->ScrollHorizontal(PR_TRUE));
  else if (!nsCRT::strcmp(aCommandName,sScrollRightString))
    rv = (mBrowseWithCaret? selCont->CharacterMove(PR_TRUE, PR_FALSE): selCont->ScrollHorizontal(PR_FALSE));
  // These commands are so the browser can use editor navigation key bindings -
  // Helps with accessibility - aaronl@chorus.net
  else if (!nsCRT::strcmp(aCommandName,sSelectCharPreviousString))
    rv = selCont->CharacterMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName,sSelectCharNextString))
    rv = selCont->CharacterMove(PR_TRUE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName,sWordPreviousString))
    rv = selCont->WordMove(PR_FALSE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName,sWordNextString))
    rv = selCont->WordMove(PR_TRUE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName,sSelectWordPreviousString))
    rv = selCont->WordMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName,sSelectWordNextString))
    rv = selCont->WordMove(PR_TRUE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName,sBeginLineString))
    rv = selCont->IntraLineMove(PR_FALSE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName,sEndLineString))
    rv = selCont->IntraLineMove(PR_TRUE, PR_FALSE);
  else if (!nsCRT::strcmp(aCommandName,sSelectBeginLineString))
    rv = selCont->IntraLineMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName,sSelectEndLineString))
    rv = selCont->IntraLineMove(PR_TRUE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName,sSelectLinePreviousString))
    rv = selCont->LineMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName,sSelectLineNextString))
    rv = selCont->LineMove(PR_TRUE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName,sSelectMoveTopString))
    rv = selCont->CompleteMove(PR_FALSE, PR_TRUE);
  else if (!nsCRT::strcmp(aCommandName,sSelectMoveBottomString))
    rv = selCont->CompleteMove(PR_TRUE, PR_TRUE);

  return rv;

}

NS_IMETHODIMP
nsDOMWindowController::OnEvent(const char * aEventName)
{
  return NS_OK;
}
#endif
