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
 *   Travis Bogard <travis@netscape.com>
 *   Brendan Eich <brendan@mozilla.org>
 */

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

// Other Classes
#include "nsEventListenerManager.h"
#include "nsEscape.h"
#include "nsDOMPropEnums.h"
#include "nsStyleCoord.h"
#include "nsHTMLReflowState.h"
#include "nsMimeTypeArray.h"
#include "nsNetUtil.h"
#include "nsPluginArray.h"
#include "nsRDFCID.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsICharsetConverterManager.h"
#include "nsICodebasePrincipal.h"
#include "nsIContent.h"
#include "nsIContentViewerFile.h"
#include "nsIContentViewerEdit.h"
#include "nsICookieService.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocument.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMPkcs11.h"
#include "nsIEventQueueService.h"
#include "nsIEventStateManager.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsIInterfaceRequestor.h"
#include "nsIJSContextStack.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIPref.h"
#include "nsIPresShell.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIPrompt.h"
#include "nsIServiceManager.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsISelectionController.h"
#include "nsIDOMSelection.h"
#include "nsIFrameSelection.h"
#include "nsISidebar.h"         // XXX for sidebar HACK, see bug 20721
#include "nsIPrompt.h"
#include "nsIStyleContext.h"
#include "nsIWebNavigation.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebShell.h"
#include "nsIComputedDOMStyle.h"
#include "nsDOMCID.h"
#include "nsDOMError.h"

// XXX An unfortunate dependency exists here.
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"


// CIDs
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_IID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_CID(kHTTPHandlerCID, NS_IHTTPHANDLER_CID);
static NS_DEFINE_CID(kXULControllersCID, NS_XULCONTROLLERS_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


static const char *kCryptoProgID = NS_CRYPTO_PROGID;
static const char *kPkcs11ProgID = NS_PKCS11_PROGID;

//*****************************************************************************
//***    GlobalWindowImpl: Object Management
//*****************************************************************************

GlobalWindowImpl::GlobalWindowImpl() :
  mScriptObject(nsnull),
  mNavigator(nsnull), mScreen(nsnull), mHistory(nsnull), mFrames(nsnull),
  mLocation(nsnull), mMenubar(nsnull), mToolbar(nsnull), mLocationbar(nsnull),
  mPersonalbar(nsnull), mStatusbar(nsnull), mScrollbars(nsnull),
  mTimeouts(nsnull), mTimeoutInsertionPoint(nsnull), mRunningTimeout(nsnull),
  mTimeoutPublicIdCounter(1), mTimeoutFiringDepth(0),
  mFirstDocumentLoad(PR_TRUE), mGlobalObjectOwner(nsnull), mDocShell(nsnull),
  mChromeEventHandler(nsnull)
{
  NS_INIT_REFCNT();
}

GlobalWindowImpl::~GlobalWindowImpl()
{
  CleanUp();
}

void GlobalWindowImpl::CleanUp()
{
  if (mContext)
    mContext->RemoveReference(&mScriptObject, mScriptObject);
  mContext = nsnull;            // Forces Release
  mDocument = nsnull;           // Forces Release
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
}

//*****************************************************************************
// GlobalWindowImpl::nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(GlobalWindowImpl)
NS_IMPL_RELEASE(GlobalWindowImpl)

NS_INTERFACE_MAP_BEGIN(GlobalWindowImpl)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIJSScriptObject)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDOMViewCSS)
  NS_INTERFACE_MAP_ENTRY(nsIDOMAbstractView)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// GlobalWindowImpl::nsIScriptObjectOwner
//*****************************************************************************
NS_IMETHODIMP GlobalWindowImpl::GetScriptObject(nsIScriptContext* aContext,
                                                void **aScriptObject)
{
  NS_ENSURE_ARG_POINTER(aScriptObject);
  nsresult res = NS_OK;
  if (!mScriptObject) {
    res = NS_NewScriptWindow(aContext, NS_STATIC_CAST(nsIDOMWindow *, this),
                             nsnull, &mScriptObject);
    aContext->AddNamedReference(&mScriptObject, mScriptObject,
                                "window_object");
#if 0
    // This code is useful for debugging leaks of global window objects
    // that are caused by leaked GC roots.  To use it, #define
    // GC_MARK_DEBUG at the top of js/src/jsgc.c and uncomment this
    // block.  The JS engine will then show why a global window is
    // not being garbage collected.
    extern void *js_LiveThingToFind;
    static int count = 0;
    if (++count == 2) {
      js_LiveThingToFind = mScriptObject;
    }
#endif
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP GlobalWindowImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

//*****************************************************************************
// GlobalWindowImpl::nsIScriptGlobalObject
//*****************************************************************************

NS_IMETHODIMP GlobalWindowImpl::SetContext(nsIScriptContext* aContext)
{
  // if setting the context to null, then we won't get to clean up the
  // named reference, so do it now
  if (!aContext) {
    NS_WARNING("Possibly early removal of script object, see bug #41608");
    mContext->RemoveReference(&mScriptObject, mScriptObject);
  }

  mContext = aContext;
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetContext(nsIScriptContext ** aContext)
{
  *aContext = mContext;
  NS_IF_ADDREF(*aContext);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetNewDocument(nsIDOMDocument* aDocument)
{
  if (aDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
    if (doc)
      doc->GetPrincipal(getter_AddRefs(mDocumentPrincipal));
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

  ::JS_ClearWatchPointsForObject((JSContext *) mContext->GetNativeContext(),
                                 (JSObject *) mScriptObject);

  if (mFirstDocumentLoad) {
    mFirstDocumentLoad = PR_FALSE;
    mDocument = aDocument;
    return NS_OK;
  }

  if (mDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    nsCOMPtr<nsIURI> docURL;

    if (doc) {
      docURL = dont_AddRef(doc->GetDocumentURL());
      doc = nsnull;             // Forces release now
    }

    if (docURL) {
      char *str;
      docURL->GetSpec(&str);

      nsAutoString url;
      url.AssignWithConversion(str);

      //about:blank URL's do not have ClearScope called on page change.
      if (!url.EqualsWithConversion("about:blank")) {
        ClearAllTimeouts();

        if (mSidebar) {
          mSidebar->SetWindow(nsnull);
          mSidebar = nsnull;
        }

        if (mListenerManager)
          mListenerManager->RemoveAllListeners(PR_FALSE);

        if (mScriptObject && mContext) {
          ::JS_ClearScope((JSContext *) mContext->GetNativeContext(),
                          (JSObject *) mScriptObject);
        }
      }
      nsCRT::free(str);
    }

    //XXX Should this be outside the about:blank clearscope exception?
    mDocument = nsnull;         // Forces Release
  }

  if (mContext) {
    // Add an extra ref in case we release mContext during GC.
    nsCOMPtr<nsIScriptContext> kungFuDeathGrip = mContext;
    kungFuDeathGrip->GC();
  }

  mDocument = aDocument;

  if (mDocument && mContext)
    mContext->InitContext(this);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetDocShell(nsIDocShell* aDocShell)
{
  // When SetDocShell(nsnull) is called, drop our references to the 
  // script object (held via a named JS root) and the script context
  // itself.
  if (!aDocShell && mContext) {
    if (mScriptObject) {
      // Indicate that the window is now closed. Since we've
      // cleared scope, we have to explicitly set a property.
      jsval val = BOOLEAN_TO_JSVAL(JS_TRUE);
      ::JS_SetProperty((JSContext *) mContext->GetNativeContext(),
                       (JSObject *) mScriptObject, "closed", &val);
      mContext->RemoveReference(&mScriptObject, mScriptObject);
      mScriptObject = nsnull;
    }
    mContext = nsnull;          // force release now
  }
  mDocShell = aDocShell;        // Weak Reference

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
    nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;
    mDocShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
    if (chromeEventHandler)
      mChromeEventHandler = chromeEventHandler.get();   //   ref
  }
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetDocShell(nsIDocShell ** aDocShell)
{
  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetOpenerWindow(nsIDOMWindow* aOpener)
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

NS_IMETHODIMP GlobalWindowImpl::HandleDOMEvent(nsIPresContext* aPresContext,
                                               nsEvent* aEvent,
                                               nsIDOMEvent** aDOMEvent,
                                               PRUint32 aFlags,
                                               nsEventStatus* aEventStatus)
{
  nsresult ret = NS_OK;
  nsIDOMEvent *domEvent = nsnull;

  /* mChromeEventHandler and mContext go dangling in the middle of this
     function under some circumstances (events that destroy the window)
     without this addref. */
  nsCOMPtr<nsIChromeEventHandler> kungFuDeathGrip1(mChromeEventHandler);
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip2(mContext);

  if (NS_EVENT_FLAG_INIT & aFlags) {
    if (!aDOMEvent) {
      aDOMEvent = &domEvent;
    }
    aEvent->flags = aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
  }

  // Capturing stage
  if ((NS_EVENT_FLAG_BUBBLE != aFlags) && mChromeEventHandler) {
    // Check chrome document capture here.
    mChromeEventHandler->HandleChromeEvent(aPresContext, aEvent, aDOMEvent,
                                           NS_EVENT_FLAG_CAPTURE,
                                           aEventStatus);
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

  // Bubbling stage
  if ((NS_EVENT_FLAG_CAPTURE != aFlags) && mChromeEventHandler) {
    // Bubble to a chrome document if it exists
    // XXX Need a way to know if an event should really bubble or not.
    // For now filter out load and unload, since they cause problems.
    if ((aEvent->message != NS_PAGE_LOAD) &&
        (aEvent->message != NS_PAGE_UNLOAD) &&
        (aEvent->message != NS_FOCUS_CONTENT) &&
        (aEvent->message != NS_BLUR_CONTENT)) {
      mChromeEventHandler->HandleChromeEvent(aPresContext, aEvent,
                                             aDOMEvent, NS_EVENT_FLAG_BUBBLE,
                                             aEventStatus);
    }
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created an event, release here.
    if (*aDOMEvent) {
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
    }
    aDOMEvent = nsnull;
  }

  return ret;
}

//*****************************************************************************
// GlobalWindowImpl::nsIScriptObjectPrincipal
//*****************************************************************************

NS_IMETHODIMP GlobalWindowImpl::GetPrincipal(nsIPrincipal** result)
{
  if (!mDocumentPrincipal)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(result);

  *result = mDocumentPrincipal;
  NS_ADDREF(*result);

  return NS_OK;
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOMWindow
//*****************************************************************************

NS_IMETHODIMP GlobalWindowImpl::GetWindow(nsIDOMWindow** aWindow)
{
  *aWindow = NS_STATIC_CAST(nsIDOMWindow *, this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetSelf(nsIDOMWindow** aWindow)
{
  *aWindow = NS_STATIC_CAST(nsIDOMWindow *, this);
  NS_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetDocument(nsIDOMDocument** aDocument)
{
  *aDocument = mDocument;
  NS_IF_ADDREF(*aDocument);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetNavigator(nsIDOMNavigator** aNavigator)
{
  if (!mNavigator) {
    mNavigator = new NavigatorImpl();
    NS_ENSURE_TRUE(mNavigator, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(mNavigator);
  }

  *aNavigator = mNavigator;
  NS_ADDREF(*aNavigator);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetScreen(nsIDOMScreen** aScreen)
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

NS_IMETHODIMP GlobalWindowImpl::GetHistory(nsIDOMHistory** aHistory)
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

NS_IMETHODIMP GlobalWindowImpl::GetParent(nsIDOMWindow** aParent)
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
    *aParent = NS_STATIC_CAST(nsIDOMWindow *, this);
    NS_ADDREF(*aParent);
  }
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetTop(nsIDOMWindow** aTop)
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

NS_IMETHODIMP GlobalWindowImpl::Get_content(nsIDOMWindow** aContent)
{
  *aContent = nsnull;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> primaryContent;
  treeOwner->GetPrimaryContentShell(getter_AddRefs(primaryContent));

  nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(primaryContent));
  *aContent = domWindow;
  NS_IF_ADDREF(*aContent);

  return NS_OK;
}

// XXX for sidebar HACK, see bug 20721
NS_IMETHODIMP GlobalWindowImpl::GetSidebar(nsISidebar** aSidebar)
{
  nsresult rv = NS_OK;

  if (!mSidebar) {
    mSidebar = do_CreateInstance(NS_SIDEBAR_PROGID, &rv);

    if (mSidebar) {
      nsIDOMWindow *win = NS_STATIC_CAST(nsIDOMWindow *, this);
      /* no addref */
      mSidebar->SetWindow(win);
    }
  }

  *aSidebar = mSidebar;
  NS_IF_ADDREF(*aSidebar);

  return rv;

}

NS_IMETHODIMP GlobalWindowImpl::GetPrompter(nsIPrompt** aPrompt)
{
  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_NO_INTERFACE);

  NS_ADDREF(*aPrompt = prompter);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetMenubar(nsIDOMBarProp** aMenubar)
{
  if (!mMenubar) {
    mMenubar = new MenubarPropImpl();
    if (mMenubar) {
      NS_ADDREF(mMenubar);
      nsCOMPtr<nsIWebBrowserChrome> browserChrome;
      if (mDocShell &&
          NS_SUCCEEDED(GetWebBrowserChrome(getter_AddRefs(browserChrome)))) {
        mMenubar-> SetWebBrowserChrome(browserChrome);
      }
    }
  }

  *aMenubar = mMenubar;
  NS_IF_ADDREF(mMenubar);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetToolbar(nsIDOMBarProp** aToolbar)
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

NS_IMETHODIMP GlobalWindowImpl::GetLocationbar(nsIDOMBarProp** aLocationbar)
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

NS_IMETHODIMP GlobalWindowImpl::GetPersonalbar(nsIDOMBarProp** aPersonalbar)
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

NS_IMETHODIMP GlobalWindowImpl::GetStatusbar(nsIDOMBarProp** aStatusbar)
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

NS_IMETHODIMP GlobalWindowImpl::GetScrollbars(nsIDOMBarProp** aScrollbars)
{
  if (!mScrollbars) {
    mScrollbars = new ScrollbarsPropImpl();
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

NS_IMETHODIMP GlobalWindowImpl::GetDirectories(nsIDOMBarProp** aDirectories)
{
  return GetPersonalbar(aDirectories);
}

NS_IMETHODIMP GlobalWindowImpl::GetClosed(PRBool* aClosed)
{
  *aClosed = !mDocShell;
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetFrames(nsIDOMWindowCollection** aFrames)
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

NS_IMETHODIMP GlobalWindowImpl::GetCrypto(nsIDOMCrypto** aCrypto)
{
  nsresult rv;

  if (!mCrypto) {
    mCrypto = do_CreateInstance(kCryptoProgID, &rv);
  }
  *aCrypto = mCrypto;
  NS_IF_ADDREF(*aCrypto);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetPkcs11(nsIDOMPkcs11** aPkcs11)
{
  nsresult rv;

  if (!mPkcs11) {
    mPkcs11 = do_CreateInstance(kPkcs11ProgID, &rv);
  }
  *aPkcs11 = mPkcs11;
  NS_IF_ADDREF(*aPkcs11);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetControllers(nsIControllers** aResult)
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

NS_IMETHODIMP GlobalWindowImpl::GetOpener(nsIDOMWindow** aOpener)
{
  *aOpener = mOpener;
  NS_IF_ADDREF(*aOpener);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetOpener(nsIDOMWindow* aOpener)
{
  mOpener = aOpener;
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetStatus(nsAWritableString& aStatus)
{
  aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetStatus(const nsAReadableString& aStatus)
{
  mStatus = aStatus;

   nsCOMPtr<nsIWebBrowserChrome> browserChrome;
   GetWebBrowserChrome(getter_AddRefs(browserChrome));
   if(browserChrome)
      browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT, nsPromiseFlatString(aStatus));

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetDefaultStatus(nsAWritableString& aDefaultStatus)
{
  aDefaultStatus = mDefaultStatus;
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetDefaultStatus(const nsAReadableString& aDefaultStatus)
{
  mDefaultStatus = aDefaultStatus;

   nsCOMPtr<nsIWebBrowserChrome> browserChrome;
   GetWebBrowserChrome(getter_AddRefs(browserChrome));
   if(browserChrome)
      browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT_DEFAULT, nsPromiseFlatString(aDefaultStatus));

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetName(nsAWritableString& aName)
{
  nsXPIDLString name;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    docShellAsItem->GetName(getter_Copies(name));

  aName.Assign(name);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetName(const nsAReadableString& aName)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem)
    result = docShellAsItem->SetName(nsPromiseFlatString(aName));
  return result;
}

NS_IMETHODIMP GlobalWindowImpl::GetInnerWidth(PRInt32* aInnerWidth)
{
  nsCOMPtr<nsIDOMWindow> parent;

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  *aInnerWidth = 0;
  if (docShellWin)
    docShellWin->GetSize(aInnerWidth, nsnull);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetInnerWidth(PRInt32 aInnerWidth)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> docShellParent;
  docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

  // It's only valid to access this from a top window.  Doesn't work from 
  // sub-frames.
  if (docShellParent)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aInnerWidth, nsnull),
                    NS_ERROR_FAILURE);

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  PRInt32 cy = 0;
  docShellAsWin->GetSize(nsnull, &cy);
  NS_ENSURE_SUCCESS(treeOwner->SizeShellTo(docShellAsItem, aInnerWidth, cy),
                    NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetInnerHeight(PRInt32* aInnerHeight)
{
  nsCOMPtr<nsIDOMWindow> parent;

  FlushPendingNotifications();

  nsCOMPtr<nsIBaseWindow> docShellWin(do_QueryInterface(mDocShell));
  *aInnerHeight = 0;
  if (docShellWin)
    docShellWin->GetSize(nsnull, aInnerHeight);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetInnerHeight(PRInt32 aInnerHeight)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> docShellParent;
  docShellAsItem->GetSameTypeParent(getter_AddRefs(docShellParent));

  // It's only valid to access this from a top window.  Doesn't work from 
  // sub-frames.
  if (docShellParent)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(nsnull, &aInnerHeight),
                    NS_ERROR_FAILURE);

  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  PRInt32 cx = 0;
  docShellAsWin->GetSize(&cx, nsnull);
  NS_ENSURE_SUCCESS(treeOwner->
                    SizeShellTo(docShellAsItem, cx, aInnerHeight),
                    NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetOuterWidth(PRInt32* aOuterWidth)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  FlushPendingNotifications();

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(aOuterWidth, nsnull),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetOuterWidth(PRInt32 aOuterWidth)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aOuterWidth, nsnull),
                    NS_ERROR_FAILURE);

  PRInt32 cy;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(nsnull, &cy), NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(aOuterWidth, cy, PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetOuterHeight(PRInt32* aOuterHeight)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  FlushPendingNotifications();

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(nsnull, aOuterHeight),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetOuterHeight(PRInt32 aOuterHeight)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(nsnull, &aOuterHeight),
                    NS_ERROR_FAILURE);

  PRInt32 cx;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&cx, nsnull), NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(cx, aOuterHeight, PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetScreenX(PRInt32* aScreenX)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  FlushPendingNotifications();

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(aScreenX, nsnull),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetScreenX(PRInt32 aScreenX)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&aScreenX, nsnull),
                    NS_ERROR_FAILURE);

  PRInt32 y;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(nsnull, &y),
                    NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(aScreenX, y),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetScreenY(PRInt32* aScreenY)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  FlushPendingNotifications();

  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(nsnull, aScreenY),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetScreenY(PRInt32 aScreenY)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(nsnull, &aScreenY),
                    NS_ERROR_FAILURE);

  PRInt32 x;
  NS_ENSURE_SUCCESS(treeOwnerAsWin->GetPosition(&x, nsnull),
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
    nsCOMPtr<nsIScriptSecurityManager>
      securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_PROGID));
    NS_ENSURE_TRUE(securityManager, NS_ERROR_FAILURE);

    PRBool enabled;
    nsresult res =
      securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);

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
  PRInt32 screenWidth = 0, screenHeight = 0;
  PRInt32 winWidth, winHeight;

  // Check security state for use in determing window dimensions
  nsCOMPtr<nsIScriptSecurityManager>
    securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_PROGID));
  NS_ENSURE_TRUE(securityManager, NS_ERROR_FAILURE);

  PRBool enabled;
  nsresult res =
    securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);
  if (NS_FAILED(res)) {
    enabled = PR_FALSE;
  }

  if (!enabled) {
    // Get the screen dimensions
    // XXX This should use nsIScreenManager once its fully fleshed out.
    nsCOMPtr<nsIDOMScreen> screen;
    if (NS_SUCCEEDED(GetScreen(getter_AddRefs(screen)))) {
      screen->GetAvailWidth(&screenWidth);
      screen->GetAvailHeight(&screenHeight);
    }

    // Get the window size
    nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
    GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
    NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

    FlushPendingNotifications();
    NS_ENSURE_SUCCESS(treeOwnerAsWin->GetSize(&winWidth, &winHeight),
                      NS_ERROR_FAILURE);

    if (aLeft) {
      *aLeft =
        screenWidth < *aLeft + winWidth ? screenWidth - winWidth : *aLeft;
      *aLeft = *aLeft < 0 ? 0 : *aLeft;
    }
    if (aTop) {
      *aTop =
        screenHeight < *aTop + winHeight ? screenHeight - winHeight : *aTop;
      *aTop = *aTop < 0 ? 0 : *aTop;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetPageXOffset(PRInt32* aPageXOffset)
{
  return GetScrollX(aPageXOffset);
}

NS_IMETHODIMP GlobalWindowImpl::SetPageXOffset(PRInt32 aPageXOffset)
{
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetPageYOffset(PRInt32* aPageYOffset)
{
  return GetScrollY(aPageYOffset);
}

NS_IMETHODIMP GlobalWindowImpl::SetPageYOffset(PRInt32 aPageYOffset)
{
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetScrollX(PRInt32* aScrollX)
{
  NS_ENSURE_ARG_POINTER(aScrollX);
  nsresult result = NS_OK;
  nsIScrollableView *view;      // no addref/release for views
  float p2t, t2p;

  *aScrollX = 0;

  FlushPendingNotifications();

  GetScrollInfo(&view, &p2t, &t2p);
  if (view) {
    nscoord xPos, yPos;
    result = view->GetScrollPosition(xPos, yPos);
    *aScrollX = NSTwipsToIntPixels(xPos, t2p);
  }

  return result;
}

NS_IMETHODIMP GlobalWindowImpl::GetScrollY(PRInt32* aScrollY)
{
  NS_ENSURE_ARG_POINTER(aScrollY);
  nsresult result = NS_OK;
  nsIScrollableView *view;      // no addref/release for views
  float p2t, t2p;

  *aScrollY = 0;

  FlushPendingNotifications();

  GetScrollInfo(&view, &p2t, &t2p);
  if (view) {
    nscoord xPos, yPos;
    result = view->GetScrollPosition(xPos, yPos);
    *aScrollY = NSTwipsToIntPixels(yPos, t2p);
  }

  return result;
}

NS_IMETHODIMP GlobalWindowImpl::GetLength(PRUint32* aLength)
{
  nsCOMPtr<nsIDOMWindowCollection> frames;
  if (NS_SUCCEEDED(GetFrames(getter_AddRefs(frames))) && frames) {
    return frames->GetLength(aLength);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GlobalWindowImpl::Dump(const nsAReadableString& aStr)
{
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

NS_IMETHODIMP GlobalWindowImpl::Alert(JSContext* cx, jsval* argv, PRUint32 argc)
{
  NS_ENSURE_STATE(mDocShell);

  nsAutoString str;

  if (argc > 0)
    nsJSUtils::nsConvertJSValToString(str, cx, argv[0]);

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  return prompter->Alert(nsnull, str.GetUnicode());
}

NS_IMETHODIMP GlobalWindowImpl::Confirm(JSContext* cx, jsval* argv,
                                        PRUint32 argc, PRBool* aReturn)
{
  NS_ENSURE_STATE(mDocShell);

  nsAutoString str;

  *aReturn = PR_FALSE;
  if (argc > 0)
    nsJSUtils::nsConvertJSValToString(str, cx, argv[0]);

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));
  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  return prompter->Confirm(nsnull, str.GetUnicode(), aReturn);
}

NS_IMETHODIMP GlobalWindowImpl::Prompt(JSContext* cx, jsval* argv,
                                       PRUint32 argc, jsval* aReturn)
{
  NS_ENSURE_STATE(mDocShell);

  nsresult ret = NS_OK;
  nsAutoString message, initial, title;
  PRUint32 savePassword = nsIPrompt::SAVE_PASSWORD_NEVER;

  if (argc > 0) {
    nsJSUtils::nsConvertJSValToString(message, cx, argv[0]);

    if (argc > 1) {
      nsJSUtils::nsConvertJSValToString(initial, cx, argv[1]);

      if (argc > 2) {
        nsJSUtils::nsConvertJSValToString(title, cx, argv[2]);

        if (argc > 3) {
          nsJSUtils::nsConvertJSValToUint32(&savePassword, cx, argv[3]);
        }
      }
    }
  }

  nsCOMPtr<nsIPrompt> prompter(do_GetInterface(mDocShell));

  NS_ENSURE_TRUE(prompter, NS_ERROR_FAILURE);

  PRBool b;
  PRUnichar *uniResult = nsnull;
  ret = prompter->Prompt(title.GetUnicode(), message.GetUnicode(), nsnull,
                         savePassword, initial.GetUnicode(), &uniResult, &b);

  if (NS_SUCCEEDED(ret) && uniResult && b) {
    JSString *jsret =
      ::JS_NewUCStringCopyZ(cx, NS_REINTERPRET_CAST(const jschar *, uniResult));
    *aReturn = STRING_TO_JSVAL(jsret);
  }
  else {
    *aReturn = JSVAL_NULL;
  }

  if (uniResult)
    nsMemory::Free(uniResult);

  return ret;
}

NS_IMETHODIMP GlobalWindowImpl::Focus()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  if (treeOwnerAsWin)
    treeOwnerAsWin->SetVisibility(PR_TRUE);

  nsCOMPtr<nsIPresShell> presShell;
  if (mDocShell)
    mDocShell->GetPresShell(getter_AddRefs(presShell));

  nsresult result = NS_OK;
  if (presShell) {
    nsCOMPtr<nsIViewManager> vm;
    presShell->GetViewManager(getter_AddRefs(vm));
    if (vm) {
      nsCOMPtr<nsIWidget> widget;
      vm->GetWidget(getter_AddRefs(widget));
      if (widget)
        result = widget->SetFocus();
    }
  }

  return result;
}

NS_IMETHODIMP GlobalWindowImpl::Blur()
{
  nsCOMPtr<nsIDocShellTreeItem>
    docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem) {
    nsCOMPtr<nsIDocShellTreeItem> parent;
    // Parent regardless of chrome or content boundary
    docShellAsItem->GetParent(getter_AddRefs(parent));

    nsCOMPtr<nsIBaseWindow> newFocusWin;

    if (parent)
      newFocusWin = do_QueryInterface(parent);
    else {
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
      newFocusWin = do_QueryInterface(treeOwner);
    }

    if (newFocusWin)
      newFocusWin->SetFocus();
  }

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::Back()
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoBack();
}

NS_IMETHODIMP GlobalWindowImpl::Forward()
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  return webNav->GoForward();
}

NS_IMETHODIMP GlobalWindowImpl::Home()
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
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(webNav->LoadURI(homeURL.GetUnicode()), NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::Stop()
{
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
  return webNav->Stop();
}

NS_IMETHODIMP GlobalWindowImpl::Print()
{
  if (mDocShell) {
    nsCOMPtr<nsIContentViewer> viewer;
    mDocShell->GetContentViewer(getter_AddRefs(viewer));
    if (viewer) {
      nsCOMPtr<nsIContentViewerFile> viewerFile(do_QueryInterface(viewer));
      if (viewerFile)
        return viewerFile->Print(PR_FALSE, nsnull);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::MoveTo(PRInt32 aXPos, PRInt32 aYPos)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityLeftAndTop(&aXPos, &aYPos),
                    NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetPosition(aXPos, aYPos),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::MoveBy(PRInt32 aXDif, PRInt32 aYDif)
{
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

NS_IMETHODIMP GlobalWindowImpl::ResizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(CheckSecurityWidthAndHeight(&aWidth, &aHeight),
                    NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(treeOwnerAsWin->SetSize(aWidth, aHeight, PR_TRUE),
                    NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::ResizeBy(PRInt32 aWidthDif, PRInt32 aHeightDif)
{
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

NS_IMETHODIMP GlobalWindowImpl::SizeToContent()
{
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

NS_IMETHODIMP GlobalWindowImpl::GetAttention()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  nsCOMPtr<nsIWidget> widget;
  treeOwnerAsWin->GetMainWidget(getter_AddRefs(widget));
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(widget->GetAttention(), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::Scroll(PRInt32 aXScroll, PRInt32 aYScroll)
{
  return ScrollTo(aXScroll, aYScroll);
}

NS_IMETHODIMP GlobalWindowImpl::ScrollTo(PRInt32 aXScroll, PRInt32 aYScroll)
{
  nsresult result;
  nsIScrollableView *view;      // no addref/release for views
  float p2t, t2p;

  result = GetScrollInfo(&view, &p2t, &t2p);

  if (view) {
    result = view->ScrollTo(NSIntPixelsToTwips(aXScroll, p2t),
                            NSIntPixelsToTwips(aYScroll, p2t),
                            NS_VMREFRESH_IMMEDIATE);
  }

  return result;
}

NS_IMETHODIMP GlobalWindowImpl::ScrollBy(PRInt32 aXScrollDif,
                                         PRInt32 aYScrollDif)
{
  nsresult result;
  nsIScrollableView *view;      // no addref/release for views
  float p2t, t2p;

  result = GetScrollInfo(&view, &p2t, &t2p);

  if (view) {
    nscoord xPos, yPos;
    result = view->GetScrollPosition(xPos, yPos);
    if (NS_SUCCEEDED(result)) {
      result = view->ScrollTo(xPos + NSIntPixelsToTwips(aXScrollDif, p2t),
                              yPos + NSIntPixelsToTwips(aYScrollDif, p2t),
                              NS_VMREFRESH_IMMEDIATE);
    }
  }

  return result;
}

NS_IMETHODIMP GlobalWindowImpl::ClearTimeout(PRInt32 aTimerID)
{
  return ClearTimeoutOrInterval(aTimerID);
}

NS_IMETHODIMP GlobalWindowImpl::ClearInterval(PRInt32 aTimerID)
{
  return ClearTimeoutOrInterval(aTimerID);
}

NS_IMETHODIMP GlobalWindowImpl::SetTimeout(JSContext* cx, jsval* argv,
                                           PRUint32 argc, PRInt32* aReturn)
{
  return SetTimeoutOrInterval(cx, argv, argc, aReturn, PR_FALSE);
}

NS_IMETHODIMP GlobalWindowImpl::SetInterval(JSContext* cx, jsval* argv,
                                            PRUint32 argc, PRInt32* aReturn)
{
  return SetTimeoutOrInterval(cx, argv, argc, aReturn, PR_TRUE);
}

NS_IMETHODIMP GlobalWindowImpl::CaptureEvents(PRInt32 aEventFlags)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(manager)))) {
    manager->CaptureEvent(aEventFlags);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GlobalWindowImpl::ReleaseEvents(PRInt32 aEventFlags)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(manager)))) {
    manager->ReleaseEvent(aEventFlags);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GlobalWindowImpl::RouteEvent(nsIDOMEvent* aEvt)
{
  //XXX Not the best solution -joki
  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::EnableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GlobalWindowImpl::DisableExternalCapture()
{
  return NS_ERROR_FAILURE;
}

//Note: This call will lock the cursor, it will not change as it moves.
//To unlock, the cursor must be set back to CURSOR_AUTO.
NS_IMETHODIMP GlobalWindowImpl::SetCursor(const nsAReadableString& aCursor)
{
  nsresult ret = NS_OK;
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
      ret = esm->SetCursor(cursor, widget, PR_TRUE);
    }
  }

  return ret;
}

NS_IMETHODIMP GlobalWindowImpl::Open(JSContext* cx, jsval* argv, PRUint32 argc,
                                     nsIDOMWindow** aReturn)
{
  return OpenInternal(cx, argv, argc, PR_FALSE, aReturn);
}

// like Open, but attaches to the new window any extra parameters past 
// [features] as a JS property named "arguments"
NS_IMETHODIMP GlobalWindowImpl::OpenDialog(JSContext* cx,
                                           jsval* argv, PRUint32 argc,
                                           nsIDOMWindow** aReturn)
{
  return OpenInternal(cx, argv, argc, PR_TRUE, aReturn);
}

NS_IMETHODIMP GlobalWindowImpl::Close()
{
  // Note: the basic security check, rejecting windows not opened through JS,
  // has been removed. This was approved long ago by ...you're going to call me
  // on this, aren't you... well it was. And anyway, a better means is coming.
  // In the new world of application-level interfaces being written in JS, this
  // security check was causing problems.
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  treeOwnerAsWin->Destroy();
  CleanUp();

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::Close(JSContext* cx, jsval* argv, PRUint32 argc)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIScriptContext> callingContext;

  nsJSUtils::nsGetDynamicScriptContext(cx, getter_AddRefs(callingContext));
  if (callingContext) {
    nsCOMPtr<nsIScriptContext> winContext;
    result = GetContext(getter_AddRefs(winContext));
    if (NS_SUCCEEDED(result)) {
      if (winContext == callingContext) {
        result =
          callingContext->SetTerminationFunction(CloseWindow,
                                                 NS_STATIC_CAST
                                                 (nsIScriptGlobalObject *,
                                                  this));
      }
      else {
        result = Close();
      }
    }
  }
  return result;
}

NS_IMETHODIMP GlobalWindowImpl::UpdateCommands(const nsAReadableString& anAction)
{
  if (mChromeEventHandler) {
    // Just jump out to the chrome event handler.
    nsCOMPtr<nsIContent> content = do_QueryInterface(mChromeEventHandler);
    if (content) {
      // Cross the chrome/content boundary and get the nearest enclosing
      // chrome window.
      nsCOMPtr<nsIDocument> doc;
      content->GetDocument(*getter_AddRefs(doc));
      nsCOMPtr<nsIScriptGlobalObject> global;
      if (!doc)
        return NS_ERROR_NULL_POINTER;
      doc->GetScriptGlobalObject(getter_AddRefs(global));
      nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(global);
      return domWindow->UpdateCommands(anAction);
    }
    else {
      // XXX Handle the embedding case.  The chrome handler could be told
      // to poke menu items/update commands etc.  This can be used by
      // embedders if we set it up right and lets them know all sorts of
      // interesting things about Ender text fields.
    }

  }
  else {
    // See if we contain a XUL document.
    nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(mDocument);
    if (xulDoc) {
      // Retrieve the command dispatcher and call updateCommands on it.
      nsCOMPtr<nsIDOMXULCommandDispatcher> xulCommandDispatcher;
      xulDoc->GetCommandDispatcher(getter_AddRefs(xulCommandDispatcher));
      xulCommandDispatcher->UpdateCommands(anAction);
    }

    // Now call UpdateCommands on our parent window.
    nsCOMPtr<nsIDOMWindow> parent;
    GetParent(getter_AddRefs(parent));
    // GetParent returns self at the top
    if (NS_STATIC_CAST(nsIDOMWindow *, this) == parent.get())
      return NS_OK;

    return parent->UpdateCommands(anAction);
  }

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::Escape(const nsAReadableString& aStr, nsAWritableString& aReturn)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIUnicodeEncoder> encoder;
  nsAutoString charset;

  nsCOMPtr<nsICharsetConverterManager>
    ccm(do_GetService(kCharsetConverterManagerCID));
  NS_ENSURE_TRUE(ccm, NS_ERROR_FAILURE);

  // Get the document character set
  charset.AssignWithConversion("UTF-8");        // default to utf-8
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
  const PRUnichar* src = nsPromiseFlatString(aStr);

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
  CopyASCIItoUCS2(nsLiteralCString(outBuf), aReturn);

  nsMemory::Free(outBuf);
  nsMemory::Free(dest);

  return result;
}

NS_IMETHODIMP GlobalWindowImpl::Unescape(const nsAReadableString& aStr,
                                         nsAWritableString& aReturn)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIUnicodeDecoder> decoder;
  nsAutoString charset;

  nsCOMPtr<nsICharsetConverterManager>
    ccm(do_GetService(kCharsetConverterManagerCID));
  NS_ENSURE_TRUE(ccm, NS_ERROR_FAILURE);

  // Get the document character set
  charset.AssignWithConversion("UTF-8");        // default to utf-8
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
  char *inBuf = ToNewCString(aStr );
  if (!inBuf)
    return NS_ERROR_OUT_OF_MEMORY;

  // Unescape the string
  char *src = nsUnescape(inBuf);

  PRInt32 maxLength, srcLen;
  srcLen = nsCRT::strlen(src);

  // Get the expected length of the result string
  result = decoder->GetMaxLength(src, srcLen, &maxLength);
  if (NS_FAILED(result)) {
    nsMemory::Free(src);
    return result;
  }

  // Allocate a buffer of the maximum length
  PRUnichar *dest = (PRUnichar*) nsMemory::Alloc(sizeof(PRUnichar) * maxLength);
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

NS_IMETHODIMP GlobalWindowImpl::GetSelection(nsIDOMSelection** aSelection)
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


//*****************************************************************************
// GlobalWindowImpl::nsIJSScriptObject
//*****************************************************************************

PRBool GlobalWindowImpl::AddProperty(JSContext* aContext, JSObject* aObj,
                                     jsval aID, jsval* aVp)
{
  if (JSVAL_IS_STRING(aID) &&
      ::JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION) {
    JSString *str = JSVAL_TO_STRING(aID);
    if (::JS_GetStringLength(str) > 2) {
      const jschar *chars = ::JS_GetStringChars(str);

      if (chars[0] == 'o' && chars[1] == 'n') {
        nsAutoString propName;
        propName.Assign(NS_REINTERPRET_CAST(const PRUnichar *, chars));
        return CheckForEventListener(aContext, propName);
      }
    }
  }
  return PR_TRUE;
}

PRBool GlobalWindowImpl::DeleteProperty(JSContext* aContext, JSObject* aObj,
                                        jsval aID, jsval* aVp)
{
  return PR_TRUE;
}

PRBool GlobalWindowImpl::GetProperty(JSContext* aContext, JSObject* aObj,
                                     jsval aID, jsval* aVp)
{
  if (JSVAL_IS_STRING(aID)) {
    char *bytes = ::JS_GetStringBytes(JSVAL_TO_STRING(aID));
    if (PL_strcmp("location", bytes) == 0) {
      nsCOMPtr<nsIDOMLocation> location;

      if (NS_OK == GetLocation(getter_AddRefs(location))) {
        if (location) {
          nsCOMPtr<nsIScriptObjectOwner>
            owner(do_QueryInterface(location));
          if (owner) {
            JSObject *object = nsnull;
            nsCOMPtr<nsIScriptContext> scriptCX;
            nsJSUtils::nsGetDynamicScriptContext(aContext,
                                                 getter_AddRefs(scriptCX));
            if (scriptCX &&
                NS_SUCCEEDED(owner->GetScriptObject(scriptCX, (void **) &object))) {
              // set the return value
              *aVp = OBJECT_TO_JSVAL(object);
            }
          }
        }
        else
          *aVp = JSVAL_NULL;
      }
      else
        return PR_FALSE;
    }
    else if (PL_strcmp("title", bytes) == 0) {
      if (mDocShell) {
        // See if we're a chrome shell.
        PRInt32 type;
        nsCOMPtr<nsIDocShellTreeItem>
          docShellAsItem(do_QueryInterface(mDocShell));
        docShellAsItem->GetItemType(&type);
        if (type == nsIDocShellTreeItem::typeChrome) {
          nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
          if (docShellAsWin) {
            nsXPIDLString title;
            docShellAsWin->GetTitle(getter_Copies(title));

            const PRUnichar *uniTitle = NS_STATIC_CAST(const PRUnichar*, title);
            JSString *valstr =
              ::JS_NewUCStringCopyZ(aContext,
                                    NS_REINTERPRET_CAST(const jschar *,
                                                        uniTitle));
            if (!valstr)
              return PR_FALSE;

            *aVp = STRING_TO_JSVAL(valstr);
          }
        }
      }
    }
    else {
      nsIScriptSecurityManager *
        securityManager = nsJSUtils::nsGetSecurityManager(aContext, aObj);
      if (NS_FAILED(securityManager->CheckScriptAccess(aContext, aObj,
                                                       NS_DOM_PROP_WINDOW_SCRIPTGLOBALS,
                                                       PR_FALSE))) {
        return PR_FALSE;
      }
    }
  }
  return PR_TRUE;
}

PRBool GlobalWindowImpl::SetProperty(JSContext* aContext, JSObject* aObj,
                                     jsval aID, jsval* aVp)
{
  PRBool result = PR_TRUE;
  if (JSVAL_IS_STRING(aID)) {
    JSString *str = JSVAL_TO_STRING(aID);

    if (::JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION) {
      if (::JS_GetStringLength(str) > 2) {
        const jschar *chars = ::JS_GetStringChars(str);

        if (chars[0] == 'o' && chars[1] == 'n') {
          nsAutoString propName;
          propName.Assign(NS_REINTERPRET_CAST(const PRUnichar *, chars));
          result = CheckForEventListener(aContext, propName);
        }
      }
    }
    else {
      char *bytes = ::JS_GetStringBytes(str);

      if (PL_strcmp("location", bytes) == 0) {
        nsCOMPtr<nsIDOMLocation> location;

        if (NS_OK == GetLocation(getter_AddRefs(location))) {
          nsCOMPtr<nsIJSScriptObject> scriptObj = do_QueryInterface(location);
          str = ::JS_NewStringCopyZ(aContext, "href");

          if (scriptObj && str)
            result =
              scriptObj->SetProperty(aContext, aObj, STRING_TO_JSVAL(str), aVp);
        }
        else
          result = PR_FALSE;
      }
      else if (PL_strcmp("title", bytes) == 0) {
        if (mDocShell) {
          // See if we're a chrome shell.
          PRInt32 type;
          nsCOMPtr<nsIDocShellTreeItem>
            docShellAsItem(do_QueryInterface(mDocShell));
          docShellAsItem->GetItemType(&type);
          if (type == nsIDocShellTreeItem::typeChrome) {
            nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
            if (docShellAsWin) {
              JSString *valstr = ::JS_ValueToString(aContext, *aVp);
              if (!valstr) {
                result = PR_FALSE;
              }
              else {
                const PRUnichar *uniTitle =
                  NS_REINTERPRET_CAST(const PRUnichar *,
                                      ::JS_GetStringChars(valstr));
                docShellAsWin->SetTitle(uniTitle);
              }
            }
          }
        }
      }
    }
  }

  return result;
}

PRBool GlobalWindowImpl::EnumerateProperty(JSContext* aContext, JSObject* aObj)
{
  return ::JS_EnumerateStandardClasses(aContext, aObj);
}

PRBool GlobalWindowImpl::Resolve(JSContext* aContext, JSObject* aObj, jsval aID)
{
  if (JSVAL_IS_STRING(aID)) {
    JSBool resolved;
    JSString *str;
    const char *bytes;

    if (!::JS_ResolveStandardClass(aContext, aObj, aID, &resolved))
      return PR_FALSE;
    if (resolved)
      return PR_TRUE;

    str = JSVAL_TO_STRING(aID);
    bytes = ::JS_GetStringBytes(str);
    if (PL_strcmp("location", bytes) == 0) {
      ::JS_DefineProperty(aContext, (JSObject *) mScriptObject, "location",
                          JSVAL_NULL, nsnull, nsnull, 0);
    }
    else if (mDocShell) {
      nsCOMPtr<nsIDocShellTreeNode>
        docShellAsNode(do_QueryInterface(mDocShell));
      PRInt32 count;

      if (NS_SUCCEEDED(docShellAsNode->GetChildCount(&count)) && count) {
        nsCOMPtr<nsIDocShellTreeItem> child;

        const jschar *chars = ::JS_GetStringChars(str);
        const PRUnichar *unichars = NS_REINTERPRET_CAST(const PRUnichar*, chars);
        nsresult rv =
          docShellAsNode->FindChildWithName(unichars,
                                            PR_FALSE,
                                            PR_FALSE,
                                            nsnull,
                                            getter_AddRefs(child));
        if (NS_SUCCEEDED(rv)) {
          if (child) {
            JSObject *childObj;

            // We found a subframe of the right name.  The rest of this code
            // is to get its script object.
            nsCOMPtr<nsIScriptGlobalObject>
              childGlobalObject(do_GetInterface(child));
            if (childGlobalObject) {
              nsCOMPtr<nsIScriptObjectOwner>
                objOwner(do_QueryInterface(childGlobalObject));
              if (objOwner) {
                nsCOMPtr<nsIScriptContext> scriptContext;

                childGlobalObject->GetContext(getter_AddRefs(scriptContext));
                if (scriptContext) {
                  objOwner->GetScriptObject(scriptContext,
                                            (void **) &childObj);
                }
              }
            }
            // Okay, if we now have a childObj, we can define it and proceed.
            if (childObj) {
              ::JS_DefineUCProperty(aContext, (JSObject *) mScriptObject,
                                    chars, ::JS_GetStringLength(str),
                                    OBJECT_TO_JSVAL(childObj), nsnull, nsnull,
                                    0);
            }
          }
        }
      }
    }
  }

  return PR_TRUE;
}

PRBool GlobalWindowImpl::Convert(JSContext* aContext, JSObject* aObj, jsval aID)
{
  return PR_TRUE;
}

void GlobalWindowImpl::Finalize(JSContext* aContext, JSObject* aObj)
{
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOMEventTarget
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::AddEventListener(const nsAReadableString& aType, 
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
GlobalWindowImpl::RemoveEventListener(const nsAReadableString& aType,
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

NS_IMETHODIMP GlobalWindowImpl::DispatchEvent(nsIDOMEvent* aEvent)
{
  if (mDocument) {
    nsCOMPtr<nsIDocument> idoc(do_QueryInterface(mDocument));
    if (idoc) {
      // Obtain a presentation context
      PRInt32 count = idoc->GetNumberOfShells();
      if (count == 0)
        return NS_OK;

      nsCOMPtr<nsIPresShell> shell = getter_AddRefs(idoc->GetShellAt(0));

      // Retrieve the context
      nsCOMPtr<nsIPresContext> aPresContext;
      shell->GetPresContext(getter_AddRefs(aPresContext));

      nsCOMPtr<nsIEventStateManager> esm;
      if (NS_SUCCEEDED(aPresContext->GetEventStateManager(getter_AddRefs(esm)))) {
        return esm->DispatchNewEvent(NS_STATIC_CAST(nsIScriptGlobalObject *, this),
                                     aEvent);
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
GlobalWindowImpl::GetListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  if (mListenerManager)
    return CallQueryInterface(mListenerManager, aInstancePtrResult);

  // This is gonna get ugly.  Can't use NS_NewEventListenerManager because of
  // a circular link problem.
  nsCOMPtr<nsIDOMEventReceiver> doc(do_QueryInterface(mDocument));

  if (doc) {
    if (NS_OK == doc->GetNewListenerManager(aInstancePtrResult)) {
      mListenerManager = *aInstancePtrResult;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

//XXX I need another way around the circular link problem.
NS_IMETHODIMP
GlobalWindowImpl::GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GlobalWindowImpl::HandleEvent(nsIDOMEvent *aEvent)
{
  return DispatchEvent(aEvent);
}

//*****************************************************************************
// GlobalWindowImpl::nsPIDOMWindow
//*****************************************************************************

NS_IMETHODIMP GlobalWindowImpl::GetPrivateParent(nsPIDOMWindow ** aParent)
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

NS_IMETHODIMP GlobalWindowImpl::GetPrivateRoot(nsIDOMWindow ** aParent)
{
  *aParent = nsnull;            // Set to null so we can bail out later

  nsCOMPtr<nsIDOMWindow> parent;
  GetTop(getter_AddRefs(parent));

  nsCOMPtr<nsIScriptGlobalObject> parentTop = do_QueryInterface(parent);
  nsCOMPtr<nsIDocShell> docShell;
  parentTop->GetDocShell(getter_AddRefs(docShell));
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
      parent->GetTop(aParent);  // Addref done here.
      return NS_OK;
    }
  }

  if (parent) {
    *aParent = parent.get();
    NS_ADDREF(*aParent);
  }

  return NS_OK;
}


NS_IMETHODIMP GlobalWindowImpl::GetLocation(nsIDOMLocation ** aLocation)
{
  if (!mLocation && mDocShell) {
    mLocation = new LocationImpl(mDocShell);
    NS_IF_ADDREF(mLocation);
  }

  *aLocation = mLocation;
  NS_IF_ADDREF(mLocation);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::SetObjectProperty(const PRUnichar *aProperty,
                                                  nsISupports *aObject)
{
  // Get JSContext from stack.
  nsCOMPtr<nsIThreadJSContextStack>
    stack(do_GetService("nsThreadJSContextStack"));
  NS_ENSURE_TRUE(stack, NS_ERROR_FAILURE);

  JSContext *cx;
  NS_ENSURE_SUCCESS(stack->Peek(&cx), NS_ERROR_FAILURE);

  if (!cx) {
    stack->GetSafeJSContext(&cx);
    NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);
  }

  jsval propertyVal = nsnull;

  NS_IF_ADDREF(aObject);        // Convert Releases it (I know it's bad)
  nsJSUtils::nsConvertXPCObjectToJSVal(aObject, NS_GET_IID(nsISupports),
                                       cx, (JSObject *) mScriptObject,
                                       &propertyVal);

  NS_ENSURE_TRUE(::JS_SetUCProperty(cx, (JSObject *) mScriptObject,
                                    NS_REINTERPRET_CAST(const jschar *,
                                                        aProperty),
                                    nsCRT::strlen(aProperty),
                                    &propertyVal),
                   NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::GetObjectProperty(const PRUnichar *aProperty,
                                                  nsISupports ** aObject)
{
  // Get JSContext from stack.
  nsCOMPtr<nsIThreadJSContextStack>
    stack(do_GetService("nsThreadJSContextStack"));
  NS_ENSURE_TRUE(stack, NS_ERROR_FAILURE);

  JSContext *cx;
  NS_ENSURE_SUCCESS(stack->Peek(&cx), NS_ERROR_FAILURE);

  if (!cx) {
    stack->GetSafeJSContext(&cx);
    NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);
  }

  jsval propertyVal;

  if (!::JS_LookupUCProperty(cx, (JSObject *) mScriptObject,
                             NS_REINTERPRET_CAST(const jschar *, aProperty),
                             nsCRT::strlen(aProperty), &propertyVal)) {
    return NS_ERROR_FAILURE;
  }

  if (!nsJSUtils::nsConvertJSValToXPCObject(aObject, NS_GET_IID(nsISupports),
                                            cx, propertyVal)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP GlobalWindowImpl::Activate()
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
  if (treeOwnerAsWin)
    treeOwnerAsWin->SetVisibility(PR_TRUE);

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

NS_IMETHODIMP GlobalWindowImpl::Deactivate()
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  if (treeOwnerAsWin)
    treeOwnerAsWin->SetVisibility(PR_TRUE);

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
GlobalWindowImpl::GetRootCommandDispatcher(nsIDocument *aDoc,
                                           nsIDOMXULCommandDispatcher **
                                           aDispatcher)
{
  if (!aDispatcher)
    return NS_ERROR_FAILURE;

  *aDispatcher = nsnull;

  if (!aDoc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
  nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
  aDoc->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
  nsCOMPtr<nsIDOMWindow> rootWindow;
  nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(ourGlobal);
  if (ourWindow) {
    ourWindow->GetPrivateRoot(getter_AddRefs(rootWindow));
    if (rootWindow) {
      nsCOMPtr<nsIDOMDocument> rootDocument;
      rootWindow->GetDocument(getter_AddRefs(rootDocument));

      nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(rootDocument);
      if (xulDoc) {
        xulDoc->GetCommandDispatcher(aDispatcher);
      }
    }
  }
  return NS_OK;
}



NS_IMETHODIMP
GlobalWindowImpl::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
                                     PRInt32 cy, PRBool fRepaint)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  return treeOwnerAsWin->SetPositionAndSize(x, y, cx, cy, fRepaint);
}

NS_IMETHODIMP
GlobalWindowImpl::GetPositionAndSize(PRInt32 *x, PRInt32 *y, PRInt32 *cx,
                                     PRInt32 *cy)
{
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(treeOwnerAsWin));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  return treeOwnerAsWin->GetPositionAndSize(x, y, cx, cy);
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOMViewCSS
//*****************************************************************************

NS_IMETHODIMP
GlobalWindowImpl::GetComputedStyle(nsIDOMElement* aElt,
                                   const nsAReadableString& aPseudoElt,
                                   nsIDOMCSSStyleDeclaration** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_ARG_POINTER(aElt);
  *aReturn = nsnull;

  NS_ENSURE_TRUE(mDocShell, NS_OK);

  nsCOMPtr<nsIPresShell> presShell;
  mDocShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_OK);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIComputedDOMStyle> compStyle;

  compStyle =
    do_CreateInstance("component://netscape/DOM/Level2/CSS/computedStyleDeclaration", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = compStyle->Init(aElt, aPseudoElt, presShell);
  NS_ENSURE_SUCCESS(rv, rv);

  return compStyle->QueryInterface(NS_GET_IID(nsIDOMCSSStyleDeclaration),
                                   (void **) aReturn);
}

//*****************************************************************************
// GlobalWindowImpl::nsIDOMAbstractView
//*****************************************************************************

NS_IMETHODIMP GlobalWindowImpl::GetDocument(nsIDOMDocumentView ** aDocumentView)
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

//*****************************************************************************
// GlobalWindowImpl: Window Control Functions
//*****************************************************************************

NS_IMETHODIMP GlobalWindowImpl::OpenInternal(JSContext *cx,
                                             jsval *argv, PRUint32 argc,
                                             PRBool aDialog,
                                             nsIDOMWindow ** aReturn)
{
  PRUint32 chromeFlags;
  nsAutoString name;
  JSString *str;
  char *options;
  PRBool nameSpecified = PR_FALSE;
  nsCOMPtr<nsIURI> uriToLoad;

  *aReturn = nsnull;

  if (argc > 0) {
    JSString *jsStrURL = ::JS_ValueToString(cx, argv[0]);
    NS_ENSURE_TRUE(jsStrURL, NS_ERROR_FAILURE);

    nsAutoString unescapedURL;
    unescapedURL.Assign(NS_REINTERPRET_CAST(const PRUnichar *,
                                            ::JS_GetStringChars(jsStrURL)));

    // fix bug 35076
    // if the URL contains non ASCII, then escape from the first non ASCII char
    nsAutoString mURL;
    if (unescapedURL.IsASCII()) {
      mURL = unescapedURL;
    }
    else {
      const PRUnichar *pt = unescapedURL.GetUnicode();
      PRUint32 len = unescapedURL.Length();
      PRUint32 i;
      for (i = 0; i < len; i++) {
        if (0 != (0xFF80 & (*pt++)))
          break;
      }
      nsAutoString right, right2;
      unescapedURL.Left(mURL, i);
      unescapedURL.Right(right, len - i);
      if (NS_SUCCEEDED(Escape(right, right2)))
        mURL.Append(right2);
      else
        mURL = unescapedURL;
    }
    if (!mURL.IsEmpty()) {
      nsAutoString mAbsURL;
      if (mDocument) {
        // Build absolute URL relative to this document.
        nsCOMPtr<nsIURI> docURL;
        nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
        if (doc)
          docURL = dont_AddRef(doc->GetDocumentURL());

        nsCOMPtr<nsIURI> baseUri(do_QueryInterface(docURL));
        NS_ENSURE_TRUE(baseUri, NS_ERROR_FAILURE);

        NS_ENSURE_SUCCESS(NS_MakeAbsoluteURI(mAbsURL, mURL, baseUri),
                          NS_ERROR_FAILURE);
      }
      else {
        // No document.  Probably because this window's URL hasn't finished
        // loading.  All we can do is hope the URL we've been given is absolute.
        mAbsURL.Assign(NS_REINTERPRET_CAST(const PRUnichar *,
                                           ::JS_GetStringChars(jsStrURL)));
        // Make URI; if mAbsURL is relative (or otherwise bogus) this will fail.
      }
      NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uriToLoad), mAbsURL),
                        NS_ERROR_FAILURE);
    }
  }

  /* Sanity-check the optional window_name argument. */
  if (argc > 1) {
    JSString *jsStrName = ::JS_ValueToString(cx, argv[1]);
    NS_ENSURE_TRUE(jsStrName, NS_ERROR_FAILURE);

    name.Assign(NS_REINTERPRET_CAST(const PRUnichar *,
                                    ::JS_GetStringChars(jsStrName)));
    nameSpecified = PR_TRUE;

    // Check for an illegal name e.g. frame3.1
    // This prints a warning message and returns NS_ERROR_FAILURE.
    // Don't use NS_ENSURE_SUCCESS - go ahead and open up the window
    // even if the name contains an illegal character.  See bug 32898.
    CheckWindowName(cx, name);
  }

  options = nsnull;
  if (argc > 2) {
    NS_ENSURE_TRUE((str = ::JS_ValueToString(cx, argv[2])), NS_ERROR_FAILURE);
    options = ::JS_GetStringBytes(str);
  }
  chromeFlags = CalculateChromeFlags(options, aDialog);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));
  NS_ENSURE_TRUE(treeOwner, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeItem> newDocShellItem;

  // special handling for certain targets
  if (nameSpecified) {
    /* Oh good. special target names are now handled in multiple places:
       Here and within FindItemWithName, just below. I put _top here because
       here it's able to do what it should: get the topmost shell of the same
       (content/chrome) type as the docshell. treeOwner is always chrome, so
       this scheme doesn't work there, where a lot of other special case
       targets are handled. (treeOwner is, however, a good place to look
       for browser windows by name, as it does.)
     */
    if (name.EqualsIgnoreCase("_top")) {
      nsCOMPtr<nsIDocShellTreeItem> shelltree(do_QueryInterface(mDocShell));
      if (shelltree)
        shelltree->GetSameTypeRootTreeItem(getter_AddRefs(newDocShellItem));
    }
    else
      treeOwner->FindItemWithName(name.GetUnicode(), nsnull,
                                  getter_AddRefs(newDocShellItem));
  }

  nsCOMPtr<nsIEventQueue> modalEventQueue;   // This has an odd ownership model
  nsCOMPtr<nsIEventQueueService> eventQService;

  PRBool windowIsNew = PR_FALSE;
  PRBool windowIsModal = PR_FALSE;

  if (!newDocShellItem) {
    windowIsNew = PR_TRUE;
    if (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL) {
      eventQService = do_GetService(kEventQueueServiceCID);
      if (eventQService &&
          NS_SUCCEEDED(eventQService->
                       PushThreadEventQueue(getter_AddRefs(modalEventQueue))))
          windowIsModal = PR_TRUE;
    }
    treeOwner->GetNewWindow(chromeFlags, getter_AddRefs(newDocShellItem));
    NS_ENSURE_TRUE(newDocShellItem, NS_ERROR_FAILURE);
  }

  NS_ENSURE_SUCCESS(ReadyOpenedDocShellItem(newDocShellItem, aReturn),
                    NS_ERROR_FAILURE);

  if (windowIsNew) {
    PRBool present = PR_FALSE;
    PRInt32 temp;

    if (!((temp = WinHasOption(options, "outerWidth", 0, &present)) ||
          present) &&
        !((temp = WinHasOption(options, "outerHeight", 0, &present)) ||
          present))
    {
      nsCOMPtr<nsIDocShellTreeOwner> newTreeOwner;
      newDocShellItem->GetTreeOwner(getter_AddRefs(newTreeOwner));
      NS_ENSURE_TRUE(newTreeOwner, NS_ERROR_FAILURE);
      nsCOMPtr<nsIWebBrowserChrome> newChrome = do_GetInterface(newTreeOwner);
      if (newChrome)
        newChrome->SetPersistence(PR_FALSE, PR_FALSE, PR_FALSE, PR_FALSE,
                                  PR_FALSE);
    }
  }

  if (aDialog && argc > 3)
    AttachArguments(*aReturn, argv + 3, argc - 3);

  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (uriToLoad) {
    // Get security manager, check to see if URI is allowed.
    nsCOMPtr<nsIURI> newUrl;
    nsCOMPtr<nsIScriptContext> scriptCX;
    nsJSUtils::nsGetStaticScriptContext(cx, (JSObject *) mScriptObject,
                                        getter_AddRefs(scriptCX));
    if (!scriptCX ||
        NS_FAILED(scriptCX->GetSecurityManager(getter_AddRefs(secMan))) ||
        NS_FAILED(secMan->CheckLoadURIFromScript(cx, uriToLoad)))
      return NS_ERROR_FAILURE;
  }

  newDocShellItem->SetName(nameSpecified ? name.GetUnicode() : nsnull);

  nsCOMPtr<nsIDocShell> newDocShell(do_QueryInterface(newDocShellItem));
  if (uriToLoad) {
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(secMan->GetSubjectPrincipal(getter_AddRefs(principal))))
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);

    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    if (codebase) {
      newDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
      NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

      nsresult rv;
      nsCOMPtr<nsIURI> codebaseURI;
      if (NS_FAILED(rv = codebase->GetURI(getter_AddRefs(codebaseURI))))
        return rv;

      loadInfo->SetReferrer(codebaseURI);
    }
    newDocShell->LoadURI(uriToLoad, loadInfo);
  }

  if (windowIsNew)
    SizeOpenedDocShellItem(newDocShellItem, options, chromeFlags);
  if (windowIsModal) {
    nsCOMPtr<nsIDocShellTreeOwner> newTreeOwner;
    newDocShellItem->GetTreeOwner(getter_AddRefs(newTreeOwner));

    if (newTreeOwner)
      newTreeOwner->ShowModal();

    eventQService->PopThreadEventQueue(modalEventQueue);
  }

  return NS_OK;
}

// attach the given array of JS values to the given window, as a property array
// named "arguments"
NS_IMETHODIMP GlobalWindowImpl::AttachArguments(nsIDOMWindow *aWindow,
                                                jsval *argv, PRUint32 argc)
{
  if (argc == 0)
    return NS_OK;

  // copy the extra parameters into a JS Array and attach it
  nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_QueryInterface(aWindow));
  if (!scriptGlobal)
    return NS_OK;

  nsCOMPtr<nsIScriptContext> scriptContext;
  scriptGlobal->GetContext(getter_AddRefs(scriptContext));
  if (!scriptContext)
    return NS_OK;

  JSContext *jsContext;
  jsContext = (JSContext *) scriptContext->GetNativeContext();
  nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(aWindow));
  if (!owner)
    return NS_OK;

  JSObject *scriptObject;
  owner->GetScriptObject(scriptContext, (void **) &scriptObject);

  JSObject *args;
  args = ::JS_NewArrayObject(jsContext, argc, argv);
  if (!args)
    return NS_OK;

  jsval argsVal = OBJECT_TO_JSVAL(args);
  // ::JS_DefineProperty(jsContext, scriptObject, "arguments",
  // argsVal, NULL, NULL, JSPROP_PERMANENT);
  ::JS_SetProperty(jsContext, scriptObject, "arguments", &argsVal);

  return NS_OK;
}

/**
 * Calculate the chrome bitmask from a string list of features.
 * @param aFeatures a string containing a list of named chrome features
 * @param aDialog affects the assumptions made about unnamed features
 * @return the chrome bitmask
 */
PRUint32 GlobalWindowImpl::CalculateChromeFlags(char *aFeatures, PRBool aDialog)
{
   if(!aFeatures) {
      if(aDialog)
         return   nsIWebBrowserChrome::CHROME_ALL | 
                  nsIWebBrowserChrome::CHROME_OPENAS_DIALOG | 
                  nsIWebBrowserChrome::CHROME_OPENAS_CHROME;
      else
         return nsIWebBrowserChrome::CHROME_ALL;
   }

  /* This function has become complicated since browser windows and
     dialogs diverged. The difference is, browser windows assume all
     chrome not explicitly mentioned is off, if the features string
     is not null. Exceptions are some OS border chrome new with Mozilla.
     Dialogs interpret a (mostly) empty features string to mean
     "OS's choice," and also support an "all" flag explicitly disallowed
     in the standards-compliant window.(normal)open. */

  PRUint32 chromeFlags = 0;
  PRBool presenceFlag = PR_FALSE;

  chromeFlags = nsIWebBrowserChrome::CHROME_WINDOW_BORDERS;
  if (aDialog && WinHasOption(aFeatures, "all", 0, &presenceFlag))
    chromeFlags = nsIWebBrowserChrome::CHROME_ALL;

  /* Next, allow explicitly named options to override the initial settings */

  chromeFlags |= WinHasOption(aFeatures, "titlebar", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_TITLEBAR : 0;
  chromeFlags |= WinHasOption(aFeatures, "close", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_WINDOW_CLOSE : 0;
  chromeFlags |= WinHasOption(aFeatures, "toolbar", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_TOOLBAR : 0;
  chromeFlags |= WinHasOption(aFeatures, "location", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_LOCATIONBAR : 0;
  chromeFlags |= (WinHasOption(aFeatures, "directories", 0, &presenceFlag) ||
                  WinHasOption(aFeatures, "personalbar", 0, &presenceFlag))
                 ? nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR : 0;
  chromeFlags |= WinHasOption(aFeatures, "status", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_STATUSBAR : 0;
  chromeFlags |= WinHasOption(aFeatures, "menubar", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_MENUBAR : 0;
  chromeFlags |= WinHasOption(aFeatures, "scrollbars", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_SCROLLBARS : 0;
  chromeFlags |= WinHasOption(aFeatures, "resizable", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_WINDOW_RESIZE : 0;

  /* OK.
     Normal browser windows, in spite of a stated pattern of turning off
     all chrome not mentioned explicitly, will want the new OS chrome (window
     borders, titlebars, closebox) on, unless explicitly turned off.
     Dialogs, on the other hand, take the absence of any explicit settings
     to mean "OS' choice." */

  // default titlebar and closebox to "on," if not mentioned at all
  if (!PL_strcasestr(aFeatures, "titlebar"))
    chromeFlags |= nsIWebBrowserChrome::CHROME_TITLEBAR;
  if (!PL_strcasestr(aFeatures, "close"))
    chromeFlags |= nsIWebBrowserChrome::CHROME_WINDOW_CLOSE;

  if (aDialog && !presenceFlag)
    chromeFlags = nsIWebBrowserChrome::CHROME_DEFAULT;

  /* Finally, once all the above normal chrome has been divined, deal
     with the features that are more operating hints than appearance
     instructions. (Note modality implies dependence.) */

  if (WinHasOption(aFeatures, "alwaysLowered", 0, nsnull) ||
      WinHasOption(aFeatures, "z-lock", 0, nsnull))
    chromeFlags |= nsIWebBrowserChrome::CHROME_WINDOW_LOWERED;
  else if (WinHasOption(aFeatures, "alwaysRaised", 0, nsnull))
    chromeFlags |= nsIWebBrowserChrome::CHROME_WINDOW_RAISED;

  chromeFlags |= WinHasOption(aFeatures, "chrome", 0, nsnull) ?
    nsIWebBrowserChrome::CHROME_OPENAS_CHROME : 0;
  chromeFlags |= WinHasOption(aFeatures, "centerscreen", 0, nsnull) ?
    nsIWebBrowserChrome::CHROME_CENTER_SCREEN : 0;
  chromeFlags |= WinHasOption(aFeatures, "dependent", 0, nsnull) ?
    nsIWebBrowserChrome::CHROME_DEPENDENT : 0;
  chromeFlags |= WinHasOption(aFeatures, "modal", 0, nsnull) ?
    (nsIWebBrowserChrome::CHROME_MODAL | nsIWebBrowserChrome::CHROME_DEPENDENT) : 0;
  chromeFlags |= WinHasOption(aFeatures, "dialog", 0, nsnull) ?
    nsIWebBrowserChrome::CHROME_OPENAS_DIALOG : 0;

  /* and dialogs need to have the last word. assume dialogs are dialogs,
     and opened as chrome, unless explicitly told otherwise. */
  if (aDialog) {
    if (!PL_strcasestr(aFeatures, "dialog"))
      chromeFlags |= nsIWebBrowserChrome::CHROME_OPENAS_DIALOG;
    if (!PL_strcasestr(aFeatures, "chrome"))
      chromeFlags |= nsIWebBrowserChrome::CHROME_OPENAS_CHROME;
  }

  /* missing
     chromeFlags->copy_history
   */

  /* Allow disabling of commands only if there is no menubar */
  /*if(!chromeFlags & NS_CHROME_MENU_BAR_ON) {
     chromeFlags->disable_commands = !WinHasOption(aFeatures, "hotkeys");
     if(XP_STRCASESTR(aFeatures,"hotkeys")==NULL)
     chromeFlags->disable_commands = FALSE;
     }
   */

  //Check security state for use in determing window dimensions
  nsCOMPtr<nsIScriptSecurityManager>
    securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_PROGID));
  NS_ENSURE_TRUE(securityManager, NS_ERROR_FAILURE);

  PRBool enabled;
  nsresult res =
    securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);

   res = securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);
 
  if (NS_FAILED(res) || !enabled) {
    //If priv check fails, set all elements to minimum reqs., else leave them alone.
    chromeFlags |= nsIWebBrowserChrome::CHROME_TITLEBAR;
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_WINDOW_LOWERED;
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_WINDOW_RAISED;
    //XXX Temporarily removing this check to allow modal dialogs to be
    //raised from script.  A more complete security based fix is needed.
    //chromeFlags &= ~nsIWebBrowserChrome::CHROME_MODAL;
  }

  return chromeFlags;
}

NS_IMETHODIMP
GlobalWindowImpl::SizeOpenedDocShellItem(nsIDocShellTreeItem *aDocShellItem,
                                         char *aFeatures,
                                         PRUint32 aChromeFlags)
{
  NS_ENSURE_ARG(aDocShellItem);

  // Use the current window's sizes as our default
  PRInt32 chromeX = 0, chromeY = 0, chromeCX = 0, chromeCY = 0;
  PRInt32 contentCX = 0, contentCY = 0;

  nsCOMPtr<nsIBaseWindow> currentTreeOwnerAsWin;
  GetTreeOwner(getter_AddRefs(currentTreeOwnerAsWin));
  NS_ENSURE_TRUE(currentTreeOwnerAsWin, NS_ERROR_FAILURE);

  currentTreeOwnerAsWin->GetPositionAndSize(&chromeX, &chromeY, &chromeCX,
                                            &chromeCY);

  // if we are content, we may need the content sizes
  nsCOMPtr<nsIBaseWindow> currentDocShellAsWin(do_QueryInterface(mDocShell));
  currentDocShellAsWin->GetSize(&contentCX, &contentCY);

  PRBool present = PR_FALSE;
  PRBool positionSpecified = PR_FALSE;
  PRInt32 temp;

  if ((temp = WinHasOption(aFeatures, "left", 0, &present)) || present)
    chromeX = temp;
  else if ((temp = WinHasOption(aFeatures, "screenX", 0, &present)) || present)
    chromeX = temp;

  if (present)
    positionSpecified = PR_TRUE;

  present = PR_FALSE;

  if ((temp = WinHasOption(aFeatures, "top", 0, &present)) || present)
    chromeY = temp;
  else if ((temp = WinHasOption(aFeatures, "screenY", 0, &present)) || present)
    chromeY = temp;

  if (present)
    positionSpecified = PR_TRUE;

  present = PR_FALSE;

  PRBool sizeChrome = PR_FALSE;
  PRBool sizeSpecified = PR_FALSE;

  if ((temp = WinHasOption(aFeatures, "outerWidth", chromeCX, &present)) ||
      present) {
    chromeCX = temp;
    sizeChrome = PR_TRUE;
    sizeSpecified = PR_TRUE;
  }

  present = PR_FALSE;

  if ((temp = WinHasOption(aFeatures, "outerHeight", chromeCY, &present)) ||
      present) {
    chromeCY = temp;
    sizeChrome = PR_TRUE;
    sizeSpecified = PR_TRUE;
  }

  // We haven't switched to chrome sizing so we need to get the content area
  if (!sizeChrome) {
    if ((temp = WinHasOption(aFeatures, "width", chromeCX, &present)) ||
        present) {
      contentCX = temp;
      sizeSpecified = PR_TRUE;
    }
    else if ((temp = WinHasOption(aFeatures, "innerWidth", chromeCX, &present))
             || present) {
      contentCX = temp;
      sizeSpecified = PR_TRUE;
    }

    if ((temp = WinHasOption(aFeatures, "height", chromeCY, &present)) ||
        present) {
      contentCY = temp;
      sizeSpecified = PR_TRUE;
    }
    else if ((temp = WinHasOption(aFeatures, "innerHeight", chromeCY, &present))
             || present) {
      contentCY = temp;
      sizeSpecified = PR_TRUE;
    }
  }

  nsresult res;
  PRBool enabled;
  PRInt32 screenWidth = 0, screenHeight = 0;
  PRInt32 winWidth, winHeight;

  // Check security state for use in determing window dimensions
  nsCOMPtr<nsIScriptSecurityManager>
    securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_PROGID));
  NS_ENSURE_TRUE(securityManager, NS_ERROR_FAILURE);

  res = securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);
  if (NS_FAILED(res)) {
    enabled = PR_FALSE;
  }

  if (!enabled) {
    // Security check failed.  Ensure all args meet minimum reqs.
    if (sizeSpecified) {
      if (sizeChrome) {
        chromeCX = chromeCX < 100 ? 100 : chromeCX;
        chromeCY = chromeCY < 100 ? 100 : chromeCY;
      }
      else {
        contentCX = contentCX < 100 ? 100 : contentCX;
        contentCY = contentCY < 100 ? 100 : contentCY;
      }
    }

    if (positionSpecified) {
      // We'll also need the screen dimensions
      // XXX This should use nsIScreenManager once its fully fleshed out.
      nsCOMPtr<nsIDOMScreen> screen;
      if (NS_SUCCEEDED(GetScreen(getter_AddRefs(screen)))) {
        screen->GetAvailWidth(&screenWidth);
        screen->GetAvailHeight(&screenHeight);
      }

      // This isn't strictly true but close enough
      winWidth = sizeSpecified ? (sizeChrome ? chromeCX : contentCX) : 100;
      winHeight = sizeSpecified ? (sizeChrome ? chromeCY : contentCY) : 100;

      chromeX =
        screenWidth < chromeX + winWidth ? screenWidth - winWidth : chromeX;
      chromeX = chromeX < 0 ? 0 : chromeX;
      chromeY = screenHeight < chromeY + winHeight
                ? screenHeight - winHeight
                : chromeY;
      chromeY = chromeY < 0 ? 0 : chromeY;
    }
  }

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  aDocShellItem->GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_QueryInterface(treeOwner));
  NS_ENSURE_TRUE(treeOwnerAsWin, NS_ERROR_FAILURE);

  if (sizeChrome) {
    if (positionSpecified && sizeSpecified)
      treeOwnerAsWin->SetPositionAndSize(chromeX, chromeY, chromeCX,
                                         chromeCY, PR_FALSE);
    else {
      if (sizeSpecified)
        treeOwnerAsWin->SetSize(chromeCX, chromeCY, PR_FALSE);
      if (positionSpecified)
        treeOwnerAsWin->SetPosition(chromeX, chromeY);
    }
  }
  else {
    if (positionSpecified)
      treeOwnerAsWin->SetPosition(chromeX, chromeY);
    if (sizeSpecified)
      treeOwner->SizeShellTo(aDocShellItem, contentCX, contentCY);
  }
  treeOwnerAsWin->SetVisibility(PR_TRUE);

  return NS_OK;
}

// Return the nsIDOMWindow corresponding to the given nsIWebShell.
// Note this forces the creation of a script context, if one has not already
// been created.  Note it also sets the window's opener to this -- because
// it's just convenient, that's all.
NS_IMETHODIMP
GlobalWindowImpl::ReadyOpenedDocShellItem(nsIDocShellTreeItem *aDocShellItem,
                                          nsIDOMWindow **aDOMWindow)
{
  nsresult res;

  *aDOMWindow = nsnull;
  nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(aDocShellItem));
  NS_ENSURE_TRUE(globalObject, NS_ERROR_FAILURE);
  res = CallQueryInterface(globalObject.get(), aDOMWindow);
  globalObject->SetOpenerWindow(this);  // damnit
  return res;
}

// Print out a warning message and return NS_ERROR_FAILURE for illegal window names.
NS_IMETHODIMP GlobalWindowImpl::CheckWindowName(JSContext *cx, nsString& aName)
{
  PRUint32 strIndex;
  PRUnichar mChar;

  for (strIndex = 0; strIndex < aName.Length(); strIndex++) {
    mChar = aName.CharAt(strIndex);
    if (!nsCRT::IsAsciiAlpha(mChar) && !nsCRT::IsAsciiDigit(mChar) &&
        mChar != '_') {
      // Don't use js_ReportError as this will cause the application
      // to shut down (JS_ASSERT calls abort())  See bug 32898
      nsAutoString warn;
      warn.AssignWithConversion("Illegal character in window name ");
      warn.Append(aName);
      char *cp = warn.ToNewCString();
      NS_WARNING(cp);
      nsCRT::free(cp);
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

PRInt32 GlobalWindowImpl::WinHasOption(char *aOptions, const char *aName,
                                       PRInt32 aDefault, PRBool *aPresenceFlag)
{
  if (!aOptions)
    return 0;
  char *comma, *equal;
  PRInt32 found = 0;

  while (PR_TRUE) {
    while (nsCRT::IsAsciiSpace(*aOptions))
      aOptions++;

    comma = strchr(aOptions, ',');
    if (comma)
      *comma = '\0';
    equal = strchr(aOptions, '=');
    if (equal)
      *equal = '\0';
    if (nsCRT::strcasecmp(aOptions, aName) == 0) {
      if (aPresenceFlag)
        *aPresenceFlag = PR_TRUE;
      if (equal)
        if (*(equal + 1) == '*')
          found = aDefault;
        else if (nsCRT::strcasecmp(equal + 1, "yes") == 0)
          found = 1;
        else
          found = atoi(equal + 1);
      else
        found = 1;
    }
    if (equal)
      *equal = '=';
    if (comma)
      *comma = ',';
    if (found || !comma)
      break;
    aOptions = comma + 1;
  }
  return found;
}

void GlobalWindowImpl::CloseWindow(nsISupports *aWindow)
{
  nsCOMPtr<nsIDOMWindow> win(do_QueryInterface(aWindow));

  win->Close();
}

//*****************************************************************************
// GlobalWindowImpl: Timeout Functions
//*****************************************************************************

static const char *kSetIntervalStr = "setInterval";
static const char *kSetTimeoutStr = "setTimeout";

NS_IMETHODIMP GlobalWindowImpl::SetTimeoutOrInterval(JSContext *cx,
                                                     jsval *argv, PRUint32 argc,
                                                     PRInt32 *aReturn,
                                                     PRBool aIsInterval)
{
  JSString *expr = nsnull;
  JSObject *funobj = nsnull;
  nsTimeoutImpl *timeout, **insertion_point;
  jsdouble interval;
  PRInt64 now, delta;

  if (argc < 2) {
    ::JS_ReportError(cx, "Function %s requires at least 2 parameters",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_FAILURE;
  }

  if (!::JS_ValueToNumber(cx, argv[1], &interval)) {
    ::JS_ReportError(cx,
                     "Second argument to %s must be a millisecond interval",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_ILLEGAL_VALUE;
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
    return NS_ERROR_FAILURE;
  }

  timeout = PR_NEWZAP(nsTimeoutImpl);
  if (!timeout)
    return NS_ERROR_OUT_OF_MEMORY;

  // Initial ref_count to indicate that this timeout struct will
  // be held as the closure of a timer.
  timeout->ref_count = 1;
  if (aIsInterval)
    timeout->interval = (PRInt32) interval;
  if (expr) {
    if (!::JS_AddNamedRoot(cx, &timeout->expr, "timeout.expr")) {
      PR_DELETE(timeout);
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
  if (nsJSUtils::nsGetCallingLocation(cx, &filename, &timeout->lineno)) {
    timeout->filename = PL_strdup(filename);
    if (!timeout->filename) {
      DropTimeout(timeout);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  timeout->version = ::JS_VersionToString(::JS_GetVersion(cx));

  // Get principal of currently executing code, save for execution of timeout
  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager>
    securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_PROGID, &rv));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  if (NS_FAILED(securityManager->GetSubjectPrincipal(&timeout->principal)))
    return NS_ERROR_FAILURE;

  LL_I2L(now, PR_IntervalNow());
  LL_D2L(delta, PR_MillisecondsToInterval((PRUint32) interval));
  LL_ADD(timeout->when, now, delta);
  nsresult err;
  timeout->timer = do_CreateInstance("component://netscape/timer", &err);
  if (NS_OK != err) {
    DropTimeout(timeout);
    return err;
  }

  err = timeout->timer->Init(nsGlobalWindow_RunTimeout, timeout,
                             (PRInt32) interval, NS_PRIORITY_LOWEST);
  if (NS_OK != err) {
    DropTimeout(timeout);
    return err;
  }
  timeout->window = this;
  NS_ADDREF(this);
  insertion_point = (mTimeoutInsertionPoint == NULL)
                    ? &mTimeouts : mTimeoutInsertionPoint;

  InsertTimeoutIntoList(insertion_point, timeout);
  timeout->public_id = ++mTimeoutPublicIdCounter;
  *aReturn = timeout->public_id;

  return NS_OK;
}

PRBool GlobalWindowImpl::RunTimeout(nsTimeoutImpl *aTimeout)
{
  nsTimeoutImpl *next, *prev, *timeout;
  nsTimeoutImpl *last_expired_timeout, **last_insertion_point;
  nsTimeoutImpl dummy_timeout;
  JSContext *cx;
  PRInt64 now;
  nsCOMPtr<nsITimer> timer;
  nsresult rv;
  PRUint32 firingDepth = mTimeoutFiringDepth + 1;

  if (nsnull == mContext) {
    return PR_TRUE;
  }

  /* Make sure that the window or the script context don't go away as 
     a result of running timeouts */
  GlobalWindowImpl *temp = this;
  NS_ADDREF(temp);
  nsIScriptContext *tempContext = mContext;
  NS_ADDREF(tempContext);

  timer = aTimeout->timer;
  cx = (JSContext *) mContext->GetNativeContext();

  /* A native timer has gone off.  See which of our timeouts need
     servicing */
  LL_I2L(now, PR_IntervalNow());

  /* The timeout list is kept in deadline order.  Discover the
     latest timeout whose deadline has expired. On some platforms,
     native timeout events fire "early", so we need to test the
     timer as well as the deadline. */
  last_expired_timeout = nsnull;
  for (timeout = mTimeouts; timeout; timeout = timeout->next) {
    if (((timeout == aTimeout) || !LL_CMP(timeout->when, >, now)) &&
        (0 == timeout->firingDepth)) {
      /*
       * Mark any timeouts that are on the list to be fired with the
       * firing depth so that we can reentrantly run timeouts
       */
      timeout->firingDepth = firingDepth;
      last_expired_timeout = timeout;
    }
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
      nsTimeoutImpl *last_running_timeout;

      /* Hold the timeout in case expr or funobj releases its doc. */
      HoldTimeout(timeout);
      last_running_timeout = mRunningTimeout;
      mRunningTimeout = timeout;
      ++mTimeoutFiringDepth;

      if (timeout->expr) {
        /* Evaluate the timeout expression. */
        nsAutoString script(NS_REINTERPRET_CAST(const PRUnichar *,
                                                ::JS_GetStringChars(timeout->expr)));
        nsAutoString blank;
        PRBool isUndefined;
        rv = mContext->EvaluateString(script,
                                      mScriptObject,
                                      timeout->principal,
                                      timeout->filename,
                                      timeout->lineno,
                                      timeout->version, blank, &isUndefined);
      }
      else {
        PRInt64 lateness64;
        PRInt32 lateness;

        /* Add "secret" final argument that indicates timeout
           lateness in milliseconds */
        LL_SUB(lateness64, now, timeout->when);
        LL_L2I(lateness, lateness64);
        lateness = PR_IntervalToMilliseconds(lateness);
        timeout->argv[timeout->argc] = INT_TO_JSVAL((jsint) lateness);
        PRBool aBoolResult;
        rv = mContext->CallEventHandler(mScriptObject, timeout->funobj,
                                        timeout->argc + 1, timeout->argv,
                                        &aBoolResult, PR_FALSE);
      }

      --mTimeoutFiringDepth;
      mRunningTimeout = last_running_timeout;

      if (NS_FAILED(rv)) {
        mTimeoutInsertionPoint = last_insertion_point;
        NS_RELEASE(temp);
        NS_RELEASE(tempContext);
        return PR_TRUE;
      }

      /* If the temporary reference is the only one that is keeping
         the timeout around, the document was released and we should
         restart this function. */
      if (timeout->ref_count == 1) {
        mTimeoutInsertionPoint = last_insertion_point;
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
        if (delay32 < 0)
          delay32 = 0;
        delay32 = PR_IntervalToMilliseconds(delay32);

        /* Reschedule timeout.  Account for possible error return in
           code below that checks for zero toid. */
        nsresult err;
        timeout->timer =
          do_CreateInstance("component://netscape/timer", &err);
        if (NS_OK != err) {
          mTimeoutInsertionPoint = last_insertion_point;
          NS_RELEASE(temp);
          NS_RELEASE(tempContext);
          return PR_TRUE;
        }

        err = timeout->timer->Init(nsGlobalWindow_RunTimeout, timeout,
                                   delay32, NS_PRIORITY_LOWEST);
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
      if (nsnull == prev) {
        mTimeouts = next;
      }
      else {
        prev->next = next;
      }

      PRBool isInterval = (timeout->interval && timeout->timer);

      // Drop timeout struct since it's out of the list
      DropTimeout(timeout, tempContext);

      /* Free the timeout if this is not a repeating interval
       *  timeout (or if it was an interval timeout, but we were
       *  unsuccessful at rescheduling it.)
       */
      if (isInterval) {
        /* Reschedule an interval timeout */
        /* Insert interval timeout onto list sorted in deadline order. */
        InsertTimeoutIntoList(mTimeoutInsertionPoint, timeout);
      }
    }
    else {
      /*
       * We skip the timeout since it's on the list to run at another
       * depth.
       */
      prev = timeout;
    }
  }

  /* Take the dummy timeout off the head of the list */
  if (nsnull == prev) {
    mTimeouts = dummy_timeout.next;
  }
  else {
    prev->next = dummy_timeout.next;
  }

  mTimeoutInsertionPoint = last_insertion_point;

  /* Get rid of our temporary references to ourselves and the script context */
  NS_RELEASE(temp);
  NS_RELEASE(tempContext);
  return PR_TRUE;
}

void GlobalWindowImpl::DropTimeout(nsTimeoutImpl *aTimeout,
                                   nsIScriptContext *aContext)
{
  JSContext *cx;

  if (--aTimeout->ref_count > 0)
    return;

  if (!aContext)
    aContext = mContext;
  if (aContext) {
    cx = (JSContext *) aContext->GetNativeContext();

    if (aTimeout->expr)
      ::JS_RemoveRoot(cx, &aTimeout->expr);
    else if (aTimeout->funobj) {
      ::JS_RemoveRoot(cx, &aTimeout->funobj);
      if (aTimeout->argv) {
        int i;
        for (i = 0; i < aTimeout->argc; i++)
          ::JS_RemoveRoot(cx, &aTimeout->argv[i]);
        PR_FREEIF(aTimeout->argv);
      }
    }
  }
  if (aTimeout->timer) {
    aTimeout->timer->Cancel();
    aTimeout->timer = nsnull;
  }
  PR_FREEIF(aTimeout->filename);
  NS_IF_RELEASE(aTimeout->window);
  NS_IF_RELEASE(aTimeout->principal);
  PR_DELETE(aTimeout);
}

void GlobalWindowImpl::HoldTimeout(nsTimeoutImpl *aTimeout)
{
  aTimeout->ref_count++;
}

NS_IMETHODIMP GlobalWindowImpl::ClearTimeoutOrInterval(PRInt32 aTimerID)
{
  PRUint32 public_id;
  nsTimeoutImpl **top, *timeout;

  public_id = (PRUint32) aTimerID;
  if (!public_id) {             /* id of zero is reserved for internal use */
    /* return silently for compatibility (see bug 30700) */
    return NS_OK;
  }
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
          DropTimeout(timeout);
        }
        DropTimeout(timeout);
      }
      break;
    }
  }
  return NS_OK;
}

void GlobalWindowImpl::ClearAllTimeouts()
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
      // Drop the count since the timer isn't going to hold on
      // anymore.
      DropTimeout(timeout);
    }
    // Drop the count since we're removing it from the list.
    DropTimeout(timeout);
  }
  mTimeouts = NULL;
}

void GlobalWindowImpl::InsertTimeoutIntoList(nsTimeoutImpl **aList,
                                             nsTimeoutImpl *aTimeout)
{
  nsTimeoutImpl *to;

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

void nsGlobalWindow_RunTimeout(nsITimer *aTimer, void *aClosure)
{
  nsTimeoutImpl *timeout = (nsTimeoutImpl *) aClosure;

  if (timeout->window->RunTimeout(timeout)) {
    // Drop the ref_count since the timer won't be holding on to the
    // timeout struct anymore
    timeout->window->DropTimeout(timeout);
  }
}

//*****************************************************************************
// GlobalWindowImpl: Helper Functions
//*****************************************************************************

NS_IMETHODIMP GlobalWindowImpl::GetTreeOwner(nsIDocShellTreeOwner **aTreeOwner)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  return docShellAsItem->GetTreeOwner(aTreeOwner);
}

NS_IMETHODIMP GlobalWindowImpl::GetTreeOwner(nsIBaseWindow **aTreeOwner)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  if (!treeOwner) {
    *aTreeOwner = nsnull;
    return NS_OK;
  }

  return CallQueryInterface(treeOwner, aTreeOwner);
}

NS_IMETHODIMP
GlobalWindowImpl::GetWebBrowserChrome(nsIWebBrowserChrome **aBrowserChrome)
{
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  GetTreeOwner(getter_AddRefs(treeOwner));

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));
  *aBrowserChrome = browserChrome;
  NS_IF_ADDREF(*aBrowserChrome);
  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::GetScrollInfo(nsIScrollableView **aScrollableView,
                                float *aP2T, float *aT2P)
{
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

PRBool GlobalWindowImpl::CheckForEventListener(JSContext *aContext,
                                               nsString& aPropName)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  nsCOMPtr<nsIAtom> atom(getter_AddRefs(NS_NewAtom(aPropName)));

  // XXX Comparisons should really be atom based

  if (aPropName.EqualsWithConversion("onmousedown")
      || aPropName.EqualsWithConversion("onmouseup")
      || aPropName.EqualsWithConversion("onclick")
      || aPropName.EqualsWithConversion("onmouseover")
      || aPropName.EqualsWithConversion("onmouseout")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMMouseListener))))
      {
        return PR_FALSE;
      }
    }
  }
  else if (aPropName.EqualsWithConversion("onkeydown")
           || aPropName.EqualsWithConversion("onkeyup")
           || aPropName.EqualsWithConversion("onkeypress")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMKeyListener))))
      {
        return PR_FALSE;
      }
    }
  }
  else if (aPropName.EqualsWithConversion("onmousemove")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMMouseMotionListener))))
      {
        return PR_FALSE;
      }
    }
  }
  else if (aPropName.EqualsWithConversion("onfocus")
           || aPropName.EqualsWithConversion("onblur")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMFocusListener))))
      {
        return PR_FALSE;
      }
    }
  }
  else if (aPropName.EqualsWithConversion("onsubmit")
           || aPropName.EqualsWithConversion("onreset")
           || aPropName.EqualsWithConversion("onchange")
           || aPropName.EqualsWithConversion("onselect")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMFormListener))))
      {
        return PR_FALSE;
      }
    }
  }
  else if (aPropName.EqualsWithConversion("onload")
           || aPropName.EqualsWithConversion("onunload")
           || aPropName.EqualsWithConversion("onclose")
           || aPropName.EqualsWithConversion("onabort")
           || aPropName.EqualsWithConversion("onerror")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMLoadListener))))
      {
        return PR_FALSE;
      }
    }
  }
  else if (aPropName.EqualsWithConversion("onpaint")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMPaintListener))))
      {
        return PR_FALSE;
      }
    }
  }
  else if (aPropName.EqualsWithConversion("ondragdrop")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMDragListener))))
      {
        return PR_FALSE;
      }
    }
  }

  else if (aPropName.EqualsWithConversion("onresize")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMPaintListener))))
      {
        return PR_FALSE;
      }
    }
  }

  else if (aPropName.EqualsWithConversion("onscroll")) {
    if (NS_OK == GetListenerManager(getter_AddRefs(manager))) {
      nsCOMPtr<nsIScriptContext> scriptCX;
      nsJSUtils::nsGetDynamicScriptContext(aContext,
                                           getter_AddRefs(scriptCX));
      if (!scriptCX ||
          NS_FAILED(manager->RegisterScriptEventListener(scriptCX, this, atom,
                                                         NS_GET_IID
                                                         (nsIDOMPaintListener))))
      {
        return PR_FALSE;
      }
    }
  }

  return PR_TRUE;
}

void GlobalWindowImpl::FlushPendingNotifications()
{
  if (mDocument) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mDocument));
    if (doc)
      doc->FlushPendingNotifications();
  }
}

//*****************************************************************************
// GlobalWindowImpl: Creator Function (This should go away)
//*****************************************************************************

extern
"C" NS_DOM nsresult NS_NewScriptGlobalObject(nsIScriptGlobalObject **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = nsnull;

  GlobalWindowImpl *global =
    new GlobalWindowImpl();
  NS_ENSURE_TRUE(global, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(NS_STATIC_CAST(nsIScriptGlobalObject *, global),
                            aResult);
}

//*****************************************************************************
//***    NavigatorImpl: Object Management
//*****************************************************************************

NavigatorImpl::NavigatorImpl():mScriptObject(nsnull), mMimeTypes(nsnull),
mPlugins(nsnull)
{
  NS_INIT_REFCNT();
}

NavigatorImpl::~NavigatorImpl()
{
  NS_IF_RELEASE(mMimeTypes);
  NS_IF_RELEASE(mPlugins);
}

//*****************************************************************************
//    NavigatorImpl::nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(NavigatorImpl)
NS_IMPL_RELEASE(NavigatorImpl)

NS_INTERFACE_MAP_BEGIN(NavigatorImpl)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptObjectOwner)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigator)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    NavigatorImpl::nsIScriptObjectOwner
//*****************************************************************************
NS_IMETHODIMP NavigatorImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP NavigatorImpl::GetScriptObject(nsIScriptContext *aContext,
                                             void **aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (!mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res =
      NS_NewScriptNavigator(aContext,
                            (nsISupports *) (nsIDOMNavigator *) this, global,
                            &mScriptObject);
    NS_IF_RELEASE(global);
  }

  *aScriptObject = mScriptObject;
  return res;
}

//*****************************************************************************
//    NavigatorImpl::nsIDOMNavigator
//*****************************************************************************

NS_IMETHODIMP NavigatorImpl::GetUserAgent(nsAWritableString& aUserAgent)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *ua = nsnull;
    res = service->GetUserAgent(&ua);
    aUserAgent = ua;
    Recycle(ua);
  }

  return res;
}

NS_IMETHODIMP NavigatorImpl::GetAppCodeName(nsAWritableString& aAppCodeName)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *appName = nsnull;
    res = service->GetAppName(&appName);
    aAppCodeName = appName;
    Recycle(appName);
  }

  return res;
}

NS_IMETHODIMP NavigatorImpl::GetAppVersion(nsAWritableString& aAppVersion)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *str = nsnull;
    res = service->GetAppVersion(&str);
    aAppVersion = str;
    Recycle(str);

    aAppVersion.Append(NS_LITERAL_STRING(" (")); 
    res = service->GetPlatform(&str);
    if (NS_FAILED(res))
      return res;

    aAppVersion += str;
    Recycle(str);

    aAppVersion.Append(NS_LITERAL_STRING("; "));                      
    res = service->GetLanguage(&str);
    if (NS_FAILED(res))
      return res;

    aAppVersion += str;
    Recycle(str);

    aAppVersion.Append(PRUnichar(')'));
  }

  return res;
}

NS_IMETHODIMP NavigatorImpl::GetAppName(nsAWritableString& aAppName)
{
  aAppName.Assign(NS_LITERAL_STRING("Netscape"));
  return NS_OK;
}

NS_IMETHODIMP NavigatorImpl::GetLanguage(nsAWritableString& aLanguage)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *lang = nsnull;
    res = service->GetLanguage(&lang);
    aLanguage = lang;
    Recycle(lang);
  }

  return res;
}

NS_IMETHODIMP NavigatorImpl::GetPlatform(nsAWritableString& aPlatform)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *plat = nsnull;
    res = service->GetPlatform(&plat);
    aPlatform = plat;
    Recycle(plat);
  }

  return res;
}

NS_IMETHODIMP NavigatorImpl::GetOscpu(nsAWritableString& aOSCPU)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *oscpu = nsnull;
    res = service->GetOscpu(&oscpu);
    aOSCPU = oscpu;
    Recycle(oscpu);
  }

  return res;
}

NS_IMETHODIMP NavigatorImpl::GetVendor(nsAWritableString& aVendor)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *vendor = nsnull;
    res = service->GetVendor(&vendor);
    aVendor = vendor;
    Recycle(vendor);
  }

  return res;
}


NS_IMETHODIMP NavigatorImpl::GetVendorSub(nsAWritableString& aVendorSub)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *vendor = nsnull;
    res = service->GetVendorSub(&vendor);
    aVendorSub = vendor;
    Recycle(vendor);
  }

  return res;
}

NS_IMETHODIMP NavigatorImpl::GetProduct(nsAWritableString& aProduct)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *product = nsnull;
    res = service->GetProduct(&product);
    aProduct = product;
    Recycle(product);
  }

  return res;
}

NS_IMETHODIMP NavigatorImpl::GetProductSub(nsAWritableString& aProductSub)
{
  nsresult res;
  nsCOMPtr<nsIHTTPProtocolHandler>
    service(do_GetService(kHTTPHandlerCID, &res));
  if (NS_SUCCEEDED(res) && (nsnull != service)) {
    PRUnichar *productSub = nsnull;
    res = service->GetProductSub(&productSub);
    aProductSub = productSub;
    Recycle(productSub);
  }

  return res;
}

NS_IMETHODIMP NavigatorImpl::GetSecurityPolicy(nsAWritableString& aSecurityPolicy)
{
  return NS_OK;
}

NS_IMETHODIMP NavigatorImpl::GetMimeTypes(nsIDOMMimeTypeArray **aMimeTypes)
{
  if (!mMimeTypes) {
    mMimeTypes = new MimeTypeArrayImpl(this);
    NS_IF_ADDREF(mMimeTypes);
  }

  *aMimeTypes = mMimeTypes;
  NS_IF_ADDREF(mMimeTypes);

  return NS_OK;
}

NS_IMETHODIMP NavigatorImpl::GetPlugins(nsIDOMPluginArray **aPlugins)
{
  if (!mPlugins) {
    mPlugins = new PluginArrayImpl(this);
    NS_IF_ADDREF(mPlugins);
  }

  *aPlugins = mPlugins;
  NS_IF_ADDREF(mPlugins);

  return NS_OK;
}

NS_IMETHODIMP NavigatorImpl::GetCookieEnabled(PRBool *aCookieEnabled)
{
  nsresult rv = NS_OK;
  *aCookieEnabled = PR_FALSE;

  NS_WITH_SERVICE(nsICookieService, service, kCookieServiceCID, &rv);
  if (NS_FAILED(rv) || service == nsnull)
    return rv;

  return service->CookieEnabled(aCookieEnabled);
}


NS_IMETHODIMP NavigatorImpl::JavaEnabled(PRBool *aReturn)
{
  nsresult rv = NS_OK;
  *aReturn = PR_FALSE;

  // determine whether user has enabled java.
  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
  if (NS_FAILED(rv) || prefs == nsnull)
    return rv;

  // if pref doesn't exist, map result to false.
  if (prefs->GetBoolPref("security.enable_java", aReturn) != NS_OK)
    *aReturn = PR_FALSE;

  return rv;
}

NS_IMETHODIMP NavigatorImpl::TaintEnabled(PRBool *aReturn)
{
  *aReturn = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP NavigatorImpl::Preference(JSContext *cx, jsval *argv,
                                        PRUint32 argc, jsval *aReturn)
{
  nsresult result = NS_OK;

  *aReturn = JSVAL_NULL;

  JSObject *self = (JSObject *) mScriptObject;
  if (!self)
    return NS_ERROR_FAILURE;

  NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                  NS_SCRIPTSECURITYMANAGER_PROGID, &result);
  if (NS_FAILED(result))
    return result;

  result =
    secMan->CheckScriptAccess(cx, self, NS_DOM_PROP_NAVIGATOR_PREFERENCE,
                              argc != 1);
  if (NS_FAILED(result)) {
    // Need to throw error here
    return NS_ERROR_FAILURE;
  }

  NS_WITH_SERVICE(nsIPref, pref, kPrefServiceCID, &result);
  if (NS_FAILED(result))
    return result;

  if (argc > 0) {
    JSString *str = ::JS_ValueToString(cx, argv[0]);
    if (str) {
      char *prefStr = ::JS_GetStringBytes(str);
      if (argc == 1) {
        PRInt32 prefType;
        pref->GetPrefType(prefStr, &prefType);
        switch (prefType & nsIPref::ePrefValuetypeMask) {
        case nsIPref::ePrefString:
          {
            char *prefCharVal;
            result = pref->CopyCharPref(prefStr, &prefCharVal);
            if (NS_SUCCEEDED(result)) {
              JSString *retStr = ::JS_NewStringCopyZ(cx, prefCharVal);
              if (retStr)
                *aReturn = STRING_TO_JSVAL(retStr);
              PL_strfree(prefCharVal);
            }
            break;
          }

        case nsIPref::ePrefInt:
          {
            PRInt32 prefIntVal;
            result = pref->GetIntPref(prefStr, &prefIntVal);
            if (NS_SUCCEEDED(result))
              *aReturn = INT_TO_JSVAL(prefIntVal);
            break;
          }

        case nsIPref::ePrefBool:
          {
            PRBool prefBoolVal;
            result = pref->GetBoolPref(prefStr, &prefBoolVal);
            if (NS_SUCCEEDED(result))
              *aReturn = BOOLEAN_TO_JSVAL(prefBoolVal);
            break;
          }
        }
      }
      else {
        if (JSVAL_IS_STRING(argv[1])) {
          JSString *valueJSStr = ::JS_ValueToString(cx, argv[1]);
          if (valueJSStr)
            result = pref->SetCharPref(prefStr, ::JS_GetStringBytes(valueJSStr));
        }
        else if (JSVAL_IS_INT(argv[1])) {
          jsint valueInt = JSVAL_TO_INT(argv[1]);
          result = pref->SetIntPref(prefStr, (PRInt32) valueInt);
        }
        else if (JSVAL_IS_BOOLEAN(argv[1])) {
          JSBool valueBool = JSVAL_TO_BOOLEAN(argv[1]);
          result = pref->SetBoolPref(prefStr, (PRBool) valueBool);
        }
        else if (JSVAL_IS_NULL(argv[1])) {
          result = pref->DeleteBranch(prefStr);
        }
      }
    }
  }

  return result;
}

#ifdef XP_MAC
#pragma mark -
#endif

//*****************************************************************************
//***  DOM Controller Stuff
//*****************************************************************************

#ifdef DOM_CONTROLLER
// nsDOMWindowController
const char *sCopyString = "cmd_copy";
const char *sCutString = "cmd_cut";
const char *sPasteString = "cmd_paste";
const char *sSelectAllString = "cmd_selectAll";

const char *sBeginLineString = "cmd_beginLine";
const char *sEndLineString = "cmd_endLine";
const char *sSelectBeginLineString = "cmd_selectBeginLine";
const char *sSelectEndLineString = "cmd_selectEndLine";

const char *sScrollTopString = "cmd_scrollTop";
const char *sScrollBottomString = "cmd_scrollBottom";

const char *sMoveTopString = "cmd_moveTop";
const char *sMoveBottomString = "cmd_moveBottom";
const char *sSelectMoveTopString = "cmd_selectTop";
const char *sSelectMoveBottomString = "cmd_selectBottom";

const char *sDownString = "cmd_linedown";
const char *sUpString = "cmd_lineup";
const char *sSelectDownString = "cmd_selectLineDown";
const char *sSelectUpString = "cmd_selectLineUp";

const char *sLeftString = "cmd_charPrevious";
const char *sRightString = "cmd_charNext";
const char *sSelectLeftString = "cmd_selectCharPrevious";
const char *sSelectRightString = "cmd_selectCharNext";


const char *sWordLeftString = "cmd_wordPrevious";
const char *sWordRightString = "cmd_wordNext";
const char *sSelectWordLeftString = "cmd_selectWordPrevious";
const char *sSelectWordRightString = "cmd_selectWordNext";

const char *sScrollPageUp = "cmd_scrollPageUp";
const char *sScrollPageDown = "cmd_scrollPageDown";

const char *sScrollLineUp = "cmd_scrollLineUp";
const char *sScrollLineDown = "cmd_scrollLineDown";

const char *sMovePageUp = "cmd_scrollPageUp";
const char *sMovePageDown = "cmd_scrollPageDown";
const char *sSelectMovePageUp = "cmd_selectPageUp";
const char *sSelectMovePageDown = "cmd_selectPageDown";

const char *sScrollLeft = "cmd_scrollLeft";
const char *sScrollRight = "cmd_scrollRight";

NS_IMPL_ADDREF(nsDOMWindowController)
NS_IMPL_RELEASE(nsDOMWindowController)

NS_INTERFACE_MAP_BEGIN(nsDOMWindowController)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIController)
  NS_INTERFACE_MAP_ENTRY(nsIController)
NS_INTERFACE_MAP_END
//NS_IMPL_QUERY_INTERFACE1(nsDOMWindowController, nsIController)

nsDOMWindowController::nsDOMWindowController(nsIDOMWindow *aWindow)
{
  NS_INIT_REFCNT();
  mWindow = aWindow;
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



NS_IMETHODIMP nsDOMWindowController::IsCommandEnabled(const PRUnichar *aCommand,
                                                      PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = PR_FALSE;
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIContentViewerEdit> editInterface;
  rv = GetEditInterface(getter_AddRefs(editInterface));
  if (NS_FAILED(rv))
    return rv;

  if (nsCAutoString(sCopyString).EqualsWithConversion(aCommand)) {
    rv = editInterface->GetCopyable(aResult);
  }
  else if (nsCAutoString(sCutString).EqualsWithConversion(aCommand)) {
    rv = editInterface->GetCutable(aResult);
  }
  else if (nsCAutoString(sPasteString).EqualsWithConversion(aCommand)) {
    rv = editInterface->GetPasteable(aResult);
  }
  else if (nsCAutoString(sSelectAllString).EqualsWithConversion(aCommand)) {
    *aResult = PR_TRUE;
    rv = NS_OK;
  }
  return rv;

}

NS_IMETHODIMP nsDOMWindowController::SupportsCommand(const PRUnichar *aCommand,
                                                     PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = PR_FALSE;
  if (nsCAutoString(sCopyString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectAllString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sCutString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sPasteString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sBeginLineString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sEndLineString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectBeginLineString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectEndLineString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sScrollTopString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sScrollBottomString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sMoveTopString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sMoveBottomString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectMoveTopString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectMoveBottomString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sDownString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sUpString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sLeftString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sRightString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectDownString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectUpString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectLeftString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectRightString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sWordLeftString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sWordRightString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectWordLeftString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectWordRightString).EqualsWithConversion(aCommand) ||
      nsCAutoString(sScrollPageUp).EqualsWithConversion(aCommand) ||
      nsCAutoString(sScrollPageDown).EqualsWithConversion(aCommand) ||
      nsCAutoString(sScrollLineUp).EqualsWithConversion(aCommand) ||
      nsCAutoString(sScrollLineDown).EqualsWithConversion(aCommand) ||
      nsCAutoString(sMovePageUp).EqualsWithConversion(aCommand) ||
      nsCAutoString(sMovePageDown).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectMovePageUp).EqualsWithConversion(aCommand) ||
      nsCAutoString(sSelectMovePageDown).EqualsWithConversion(aCommand) ||
      nsCAutoString(sScrollLeft).EqualsWithConversion(aCommand) ||
      nsCAutoString(sScrollRight).EqualsWithConversion(aCommand)) {
    *aResult = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsDOMWindowController::DoCommand(const PRUnichar *aCommand)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIContentViewerEdit> editInterface;
  nsCOMPtr<nsISelectionController> selCont;
  rv = GetEditInterface(getter_AddRefs(editInterface));
  nsCOMPtr<nsIPresShell> presShell;
  if (NS_FAILED(rv))
    return rv;

  if (nsCAutoString(sCopyString).EqualsWithConversion(aCommand)) {
    rv = editInterface->CopySelection();
  }
  else if (nsCAutoString(sCutString).EqualsWithConversion(aCommand)) {
    rv = editInterface->CutSelection();
  }
  else if (nsCAutoString(sPasteString).EqualsWithConversion(aCommand)) {
    rv = editInterface->Paste();
  }
  else if (nsCAutoString(sSelectAllString).EqualsWithConversion(aCommand)) {
    rv = editInterface->SelectAll();
  }
  else if (nsCAutoString(sScrollPageUp).EqualsWithConversion(aCommand)) {
    NS_ENSURE_SUCCESS(GetSelectionController(getter_AddRefs(selCont)),
                      NS_ERROR_FAILURE);
    return selCont->ScrollPage(PR_FALSE);
  }
  else if (nsCAutoString(sScrollPageDown).EqualsWithConversion(aCommand)) {
    NS_ENSURE_SUCCESS(GetSelectionController(getter_AddRefs(selCont)),
                      NS_ERROR_FAILURE);
    return selCont->ScrollPage(PR_TRUE);
  }
  else if (nsCAutoString(sScrollLineUp).EqualsWithConversion(aCommand)) {
    NS_ENSURE_SUCCESS(GetSelectionController(getter_AddRefs(selCont)),
                      NS_ERROR_FAILURE);
    return selCont->ScrollLine(PR_FALSE);
  }
  else if (nsCAutoString(sScrollLineDown).EqualsWithConversion(aCommand)) {
    NS_ENSURE_SUCCESS(GetSelectionController(getter_AddRefs(selCont)),
                      NS_ERROR_FAILURE);
    return selCont->ScrollLine(PR_TRUE);
  }
  else if (nsCAutoString(sScrollLeft).EqualsWithConversion(aCommand)) {
    NS_ENSURE_SUCCESS(GetSelectionController(getter_AddRefs(selCont)),
                      NS_ERROR_FAILURE);
    return selCont->ScrollHorizontal(PR_TRUE);
  }
  else if (nsCAutoString(sScrollRight).EqualsWithConversion(aCommand)) {
    NS_ENSURE_SUCCESS(GetSelectionController(getter_AddRefs(selCont)),
                      NS_ERROR_FAILURE);
    return selCont->ScrollHorizontal(PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDOMWindowController::OnEvent(const PRUnichar *aEventName)
{
  return NS_OK;
}
#endif
