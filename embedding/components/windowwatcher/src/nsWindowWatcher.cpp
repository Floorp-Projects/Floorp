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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Harshal Pradhan <keeda@hotpop.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

//#define USEWEAKREFS // (haven't quite figured that out yet)

#include "nsWindowWatcher.h"

#include "nsAutoLock.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsPrompt.h"
#include "nsWWJSUtils.h"
#include "plstr.h"

#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsIScriptContext.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIGenericFactory.h"
#include "nsIJSContextStack.h"
#include "nsIObserverService.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsArray.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIURI.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebNavigation.h"
#include "nsIWindowCreator.h"
#include "nsIWindowCreator2.h"
#include "nsIXPConnect.h"
#include "nsPIDOMWindow.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#ifdef XP_UNIX
// please see bug 78421 for the eventual "right" fix for this
#define HAVE_LAME_APPSHELL
#endif

#ifdef HAVE_LAME_APPSHELL
#include "nsIAppShell.h"
// for NS_APPSHELL_CID
#include <nsWidgetsCID.h>
#endif

#ifdef USEWEAKREFS
#include "nsIWeakReference.h"
#endif

#ifdef HAVE_LAME_APPSHELL
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
#endif

static const char *sJSStackContractID="@mozilla.org/js/xpc/ContextStack;1";

/****************************************************************
 ******************** nsWatcherWindowEntry **********************
 ****************************************************************/

class nsWindowWatcher;

struct nsWatcherWindowEntry {

  nsWatcherWindowEntry(nsIDOMWindow *inWindow, nsIWebBrowserChrome *inChrome) {
#ifdef USEWEAKREFS
    mWindow = do_GetWeakReference(inWindow);
#else
    mWindow = inWindow;
#endif
    mChrome = inChrome;
    ReferenceSelf();
  }
  ~nsWatcherWindowEntry() {}

  void InsertAfter(nsWatcherWindowEntry *inOlder);
  void Unlink();
  void ReferenceSelf();

#ifdef USEWEAKREFS
  nsCOMPtr<nsIWeakReference> mWindow;
#else // still not an owning ref
  nsIDOMWindow              *mWindow;
#endif
  nsIWebBrowserChrome       *mChrome;
  // each struct is in a circular, doubly-linked list
  nsWatcherWindowEntry      *mYounger, // next younger in sequence
                            *mOlder;
};

void nsWatcherWindowEntry::InsertAfter(nsWatcherWindowEntry *inOlder)
{
  if (inOlder) {
    mOlder = inOlder;
    mYounger = inOlder->mYounger;
    mOlder->mYounger = this;
    if (mOlder->mOlder == mOlder)
      mOlder->mOlder = this;
    mYounger->mOlder = this;
    if (mYounger->mYounger == mYounger)
      mYounger->mYounger = this;
  }
}

void nsWatcherWindowEntry::Unlink() {

  mOlder->mYounger = mYounger;
  mYounger->mOlder = mOlder;
  ReferenceSelf();
}

void nsWatcherWindowEntry::ReferenceSelf() {

  mYounger = this;
  mOlder = this;
}

/****************************************************************
 ****************** nsWatcherWindowEnumerator *******************
 ****************************************************************/

class nsWatcherWindowEnumerator : public nsISimpleEnumerator {

public:
  nsWatcherWindowEnumerator(nsWindowWatcher *inWatcher);
  virtual ~nsWatcherWindowEnumerator();
  NS_IMETHOD HasMoreElements(PRBool *retval);
  NS_IMETHOD GetNext(nsISupports **retval);

  NS_DECL_ISUPPORTS

private:
  friend class nsWindowWatcher;

  nsWatcherWindowEntry *FindNext();
  void WindowRemoved(nsWatcherWindowEntry *inInfo);

  nsWindowWatcher      *mWindowWatcher;
  nsWatcherWindowEntry *mCurrentPosition;
};

NS_IMPL_ADDREF(nsWatcherWindowEnumerator)
NS_IMPL_RELEASE(nsWatcherWindowEnumerator)
NS_IMPL_QUERY_INTERFACE1(nsWatcherWindowEnumerator, nsISimpleEnumerator)

nsWatcherWindowEnumerator::nsWatcherWindowEnumerator(nsWindowWatcher *inWatcher)
  : mWindowWatcher(inWatcher),
    mCurrentPosition(inWatcher->mOldestWindow)
{
  mWindowWatcher->AddEnumerator(this);
  mWindowWatcher->AddRef();
}

nsWatcherWindowEnumerator::~nsWatcherWindowEnumerator()
{
  mWindowWatcher->RemoveEnumerator(this);
  mWindowWatcher->Release();
}

NS_IMETHODIMP
nsWatcherWindowEnumerator::HasMoreElements(PRBool *retval)
{
  if (!retval)
    return NS_ERROR_INVALID_ARG;

  *retval = mCurrentPosition? PR_TRUE : PR_FALSE;
  return NS_OK;
}
	
NS_IMETHODIMP
nsWatcherWindowEnumerator::GetNext(nsISupports **retval)
{
  if (!retval)
    return NS_ERROR_INVALID_ARG;

  *retval = NULL;

#ifdef USEWEAKREFS
  while (mCurrentPosition) {
    CallQueryReferent(mCurrentPosition->mWindow, retval);
    if (*retval) {
      mCurrentPosition = FindNext();
      break;
    } else // window is gone!
      mWindowWatcher->RemoveWindow(mCurrentPosition);
  }
  NS_IF_ADDREF(*retval);
#else
  if (mCurrentPosition) {
    CallQueryInterface(mCurrentPosition->mWindow, retval);
    mCurrentPosition = FindNext();
  }
#endif
  return NS_OK;
}

nsWatcherWindowEntry *
nsWatcherWindowEnumerator::FindNext()
{
  nsWatcherWindowEntry *info;

  if (!mCurrentPosition)
    return 0;

  info = mCurrentPosition->mYounger;
  return info == mWindowWatcher->mOldestWindow ? 0 : info;
}

// if a window is being removed adjust the iterator's current position
void nsWatcherWindowEnumerator::WindowRemoved(nsWatcherWindowEntry *inInfo) {

  if (mCurrentPosition == inInfo)
    mCurrentPosition = mCurrentPosition != inInfo->mYounger ?
                       inInfo->mYounger : 0;
}

/****************************************************************
 ********************* EventQueueAutoPopper *********************
 ****************************************************************/

class EventQueueAutoPopper {
public:
  EventQueueAutoPopper();
  ~EventQueueAutoPopper();

  nsresult Push();

protected:
  nsCOMPtr<nsIEventQueueService> mService;
  nsCOMPtr<nsIEventQueue>        mQueue;
#ifdef HAVE_LAME_APPSHELL
  nsCOMPtr<nsIAppShell>          mAppShell;
#endif
};

EventQueueAutoPopper::EventQueueAutoPopper() : mQueue(nsnull)
{
}

EventQueueAutoPopper::~EventQueueAutoPopper()
{
#ifdef HAVE_LAME_APPSHELL
  if (mAppShell) {
    if (mQueue)
      mAppShell->ListenToEventQueue(mQueue, PR_FALSE);
    mAppShell->Spindown();
    mAppShell = nsnull;
  }
#endif

  if(mQueue)
    mService->PopThreadEventQueue(mQueue);
}

nsresult EventQueueAutoPopper::Push()
{
  if (mQueue) // only once
    return NS_ERROR_FAILURE;

  mService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
  if (!mService)
    return NS_ERROR_FAILURE;

  // push a new queue onto it
  mService->PushThreadEventQueue(getter_AddRefs(mQueue));
  if (!mQueue)
    return NS_ERROR_FAILURE;

#ifdef HAVE_LAME_APPSHELL
  // listen to the event queue
  mAppShell = do_CreateInstance(kAppShellCID);
  if (!mAppShell)
    return NS_ERROR_FAILURE;

  mAppShell->Create(0, nsnull);
  mAppShell->Spinup();

  // listen to the new queue
  mAppShell->ListenToEventQueue(mQueue, PR_TRUE);
#endif

  return NS_OK;
}

/****************************************************************
 ********************** JSContextAutoPopper *********************
 ****************************************************************/

class JSContextAutoPopper {
public:
  JSContextAutoPopper();
  ~JSContextAutoPopper();

  nsresult   Push();
  JSContext *get() { return mContext; }

protected:
  nsCOMPtr<nsIThreadJSContextStack>  mService;
  JSContext                         *mContext;
};

JSContextAutoPopper::JSContextAutoPopper() : mContext(nsnull)
{
}

JSContextAutoPopper::~JSContextAutoPopper()
{
  JSContext *cx;
  nsresult   rv;

  if(mContext) {
    rv = mService->Pop(&cx);
    NS_ASSERTION(NS_SUCCEEDED(rv) && cx == mContext, "JSContext push/pop mismatch");
  }
}

nsresult JSContextAutoPopper::Push()
{
  nsresult rv;

  if (mContext) // only once
    return NS_ERROR_FAILURE;

  mService = do_GetService(sJSStackContractID);
  if(mService) {
    rv = mService->GetSafeJSContext(&mContext);
    if (NS_SUCCEEDED(rv) && mContext) {
      rv = mService->Push(mContext);
      if (NS_FAILED(rv))
        mContext = 0;
    }
  }
  return mContext ? NS_OK : NS_ERROR_FAILURE; 
}

/****************************************************************
 ************************** AutoFree ****************************
 ****************************************************************/

class AutoFree {
public:
  AutoFree(void *aPtr) : mPtr(aPtr) {
  }
  ~AutoFree() {
    if (mPtr)
      nsMemory::Free(mPtr);
  }
  void Invalidate() {
    mPtr = 0;
  }
private:
  void *mPtr;
};

/****************************************************************
 *********************** nsWindowWatcher ************************
 ****************************************************************/

NS_IMPL_ADDREF(nsWindowWatcher)
NS_IMPL_RELEASE(nsWindowWatcher)
NS_IMPL_QUERY_INTERFACE2(nsWindowWatcher, nsIWindowWatcher, nsPIWindowWatcher)

nsWindowWatcher::nsWindowWatcher() :
        mEnumeratorList(),
        mOldestWindow(0),
        mActiveWindow(0),
        mListLock(0)
{
}

nsWindowWatcher::~nsWindowWatcher()
{
  // delete data
  while (mOldestWindow)
    RemoveWindow(mOldestWindow);

  if (mListLock)
    PR_DestroyLock(mListLock);
}

nsresult
nsWindowWatcher::Init()
{
  mListLock = PR_NewLock();
  if (!mListLock)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::OpenWindow(nsIDOMWindow *aParent,
                            const char *aUrl,
                            const char *aName,
                            const char *aFeatures,
                            nsISupports *aArguments,
                            nsIDOMWindow **_retval)
{
  PRUint32  argc;
  jsval    *argv = nsnull;
  nsresult  rv;

  rv = ConvertSupportsTojsvals(aParent, aArguments, &argc, &argv);
  if (NS_SUCCEEDED(rv)) {
    PRBool dialog = argc == 0 ? PR_FALSE : PR_TRUE;
    rv = OpenWindowJS(aParent, aUrl, aName, aFeatures, dialog, argc, argv,
                      _retval);
  }
  if (argv) // Free goes to libc free(). so i'm assuming a bad libc.
    nsMemory::Free(argv);
  return rv;
}

NS_IMETHODIMP
nsWindowWatcher::OpenWindowJS(nsIDOMWindow *aParent,
                              const char *aUrl,
                              const char *aName,
                              const char *aFeatures,
                              PRBool aDialog,
                              PRUint32 argc,
                              jsval *argv,
                              nsIDOMWindow **_retval)
{
  nsresult                        rv = NS_OK;
  PRBool                          nameSpecified,
                                  featuresSpecified,
                                  windowIsNew = PR_FALSE,
                                  windowIsModal = PR_FALSE,
                                  uriToLoadIsChrome = PR_FALSE;
  PRUint32                        chromeFlags;
  nsAutoString                    name;             // string version of aName
  nsCString                       features;         // string version of aFeatures
  nsCOMPtr<nsIURI>                uriToLoad;        // from aUrl, if any
  nsCOMPtr<nsIDocShellTreeOwner>  parentTreeOwner;  // from the parent window, if any
  nsCOMPtr<nsIDocShellTreeItem>   newDocShellItem;  // from the new window
  EventQueueAutoPopper            queueGuard;
  JSContextAutoPopper             contextGuard;

  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = 0;

  if (aParent)
    GetWindowTreeOwner(aParent, getter_AddRefs(parentTreeOwner));

  if (aUrl) {
    rv = URIfromURL(aUrl, aParent, getter_AddRefs(uriToLoad));
    if (NS_FAILED(rv))
      return rv;
    uriToLoad->SchemeIs("chrome", &uriToLoadIsChrome);
  }

  nameSpecified = PR_FALSE;
  if (aName) {
    CopyUTF8toUTF16(aName, name);
    CheckWindowName(name);
    nameSpecified = PR_TRUE;
  }

  featuresSpecified = PR_FALSE;
  if (aFeatures) {
    features.Assign(aFeatures);
    featuresSpecified = PR_TRUE;
    features.StripWhitespace();
  }

  nsCOMPtr<nsIDOMChromeWindow> chromeParent(do_QueryInterface(aParent));

  chromeFlags = CalculateChromeFlags(features.get(), featuresSpecified,
                                     aDialog, uriToLoadIsChrome,
                                     !aParent || chromeParent);

  // try to find an extant window with the given name
  if (nameSpecified) {
    /* Oh good. special target names are now handled in multiple places:
       Here and within FindItemWithName, just below. I put _top here because
       here it's able to do what it should: get the topmost shell of the same
       (content/chrome) type as the docshell. treeOwner is always chrome, so
       this scheme doesn't work there, where a lot of other special case
       targets are handled. (treeOwner is, however, a good place to look
       for browser windows by name, as it does.)
     */
    if (aParent) {
      if (name.LowerCaseEqualsLiteral("_self")) {
        GetWindowTreeItem(aParent, getter_AddRefs(newDocShellItem));
      } else if (name.LowerCaseEqualsLiteral("_top")) {
        nsCOMPtr<nsIDocShellTreeItem> shelltree;
        GetWindowTreeItem(aParent, getter_AddRefs(shelltree));
        if (shelltree)
          shelltree->GetSameTypeRootTreeItem(getter_AddRefs(newDocShellItem));
      } else if (name.LowerCaseEqualsLiteral("_parent")) {
        nsCOMPtr<nsIDocShellTreeItem> shelltree;
        GetWindowTreeItem(aParent, getter_AddRefs(shelltree));
        if (shelltree)
          shelltree->GetSameTypeParent(getter_AddRefs(newDocShellItem));
        // If there is no real parent then _self acts as _parent
        if (!newDocShellItem)
          newDocShellItem = shelltree;
      } else {
        /* parent is being simultaneously torn down (probably because of
           the code that keeps an old docshell alive but disconnected while
           we load a new one). not much to do but open the new window
           without a parent. */
        if (parentTreeOwner)
          parentTreeOwner->FindItemWithName(name.get(), nsnull,
                                            getter_AddRefs(newDocShellItem));
      }
    } else
      FindItemWithName(name.get(), getter_AddRefs(newDocShellItem));
  }

  // no extant window? make a new one.
  if (!newDocShellItem) {
    windowIsNew = PR_TRUE;

    nsCOMPtr<nsIWebBrowserChrome> parentChrome(do_GetInterface(parentTreeOwner));

    // is the parent (if any) modal? if so, we must be, too.
    PRBool weAreModal = (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL) != 0;
    if (!weAreModal && parentChrome)
      parentChrome->IsWindowModal(&weAreModal);

    if (weAreModal) {
      rv = queueGuard.Push();
      if (NS_SUCCEEDED(rv)) {
        windowIsModal = PR_TRUE;
        // in case we added this because weAreModal
        chromeFlags |= nsIWebBrowserChrome::CHROME_MODAL | nsIWebBrowserChrome::CHROME_DEPENDENT;
      }
    }

    NS_ASSERTION(mWindowCreator, "attempted to open a new window with no WindowCreator");
    rv = NS_ERROR_FAILURE;
    if (mWindowCreator) {
      nsCOMPtr<nsIWebBrowserChrome> newChrome;

      /* If the window creator is an nsIWindowCreator2, we can give it
         some hints. The only hint at this time is whether the opening window
         is in a situation that's likely to mean this is an unrequested
         popup window we're creating. However we're not completely honest:
         we clear that indicator if the opener is chrome, so that the
         downstream consumer can treat the indicator to mean simply
         that the new window is subject to popup control. */
      nsCOMPtr<nsIWindowCreator2> windowCreator2(do_QueryInterface(mWindowCreator));
      if (windowCreator2) {
        PRUint32 contextFlags = 0;
        PRBool popupConditions = PR_FALSE;

        // is the parent under popup conditions?
        nsCOMPtr<nsPIDOMWindow> piWindow(do_QueryInterface(aParent));
        if (piWindow)
          popupConditions = piWindow->IsLoadingOrRunningTimeout();

        // chrome is always allowed, so clear the flag if the opener is chrome
        if (popupConditions) {
          PRBool isChrome = PR_FALSE;
          nsCOMPtr<nsIScriptSecurityManager>
            sm(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
          if (sm)
            sm->SubjectPrincipalIsSystem(&isChrome);
          popupConditions = !isChrome;
        }

        if (popupConditions)
          contextFlags |= nsIWindowCreator2::PARENT_IS_LOADING_OR_RUNNING_TIMEOUT;

        PRBool cancel = PR_FALSE;
        rv = windowCreator2->CreateChromeWindow2(parentChrome, chromeFlags,
                               contextFlags, uriToLoad, &cancel,
                               getter_AddRefs(newChrome));
        if (NS_SUCCEEDED(rv) && cancel) {
          newChrome = 0; // just in case
          rv = NS_ERROR_ABORT;
        }
      }
      else
        rv = mWindowCreator->CreateChromeWindow(parentChrome, chromeFlags,
                               getter_AddRefs(newChrome));
      if (newChrome) {
        /* It might be a chrome nsXULWindow, in which case it won't have
            an nsIDOMWindow (primary content shell). But in that case, it'll
            be able to hand over an nsIDocShellTreeItem directly. */
        nsCOMPtr<nsIDOMWindow> newWindow(do_GetInterface(newChrome));
        if (newWindow)
          GetWindowTreeItem(newWindow, getter_AddRefs(newDocShellItem));
        if (!newDocShellItem)
          newDocShellItem = do_GetInterface(newChrome);
        if (!newDocShellItem)
          rv = NS_ERROR_FAILURE;
      }
    }
  }

  // better have a window to use by this point
  if (!newDocShellItem)
    return rv;

  rv = ReadyOpenedDocShellItem(newDocShellItem, aParent, _retval);
  if (NS_FAILED(rv))
    return rv;

  /* disable persistence of size/position in popups (determined by
     determining whether the features parameter specifies width or height
     in any way). We consider any overriding of the window's size or position
     in the open call as disabling persistence of those attributes.
     Popup windows (which should not persist size or position) generally set
     the size. */
  if (windowIsNew) {
    /* at the moment, the strings "height=" or "width=" never happen
       outside a size specification, so we can do this the Q&D way. */

    if (PL_strcasestr(features.get(), "width=") || PL_strcasestr(features.get(), "height=")) {

      nsCOMPtr<nsIDocShellTreeOwner> newTreeOwner;
      newDocShellItem->GetTreeOwner(getter_AddRefs(newTreeOwner));
      if (newTreeOwner)
        newTreeOwner->SetPersistence(PR_FALSE, PR_FALSE, PR_FALSE);
    }
  }

  if (aDialog && argc > 0) {
    rv = AttachArguments(*_retval, argc, argv);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  /* allow an extant window to keep its name (important for cases like
     _self where the given name is different (and invalid)). also _blank
     is not a window name. */
  if (windowIsNew)
    newDocShellItem->SetName(nameSpecified && !name.LowerCaseEqualsLiteral("_blank") ?
                             name.get() : nsnull);

  nsCOMPtr<nsIDocShell> newDocShell(do_QueryInterface(newDocShellItem));

  // When a new window is opened through JavaScript, we want it to use the
  // charset of its opener as a fallback in the event the document being loaded 
  // does not specify a charset. Failing to set this charset is not fatal, so we 
  // want to continue in the face of errors.
  nsCOMPtr<nsIScriptGlobalObject> parentSGO(do_QueryInterface(aParent));
  if (parentSGO) {
    nsIDocShell *parentDocshell = parentSGO->GetDocShell();
    // parentDocshell may be null if the parent got closed in the meantime
    if (parentDocshell) {
      nsCOMPtr<nsIContentViewer> parentContentViewer;
      parentDocshell->GetContentViewer(getter_AddRefs(parentContentViewer));
      nsCOMPtr<nsIDocumentViewer> parentDocViewer(do_QueryInterface(parentContentViewer));
      if (parentDocViewer) {
        nsCOMPtr<nsIDocument> doc;
        parentDocViewer->GetDocument(getter_AddRefs(doc));
        
        nsCOMPtr<nsIContentViewer> newContentViewer;
        newDocShell->GetContentViewer(getter_AddRefs(newContentViewer));
        nsCOMPtr<nsIMarkupDocumentViewer> newMarkupDocViewer(do_QueryInterface(newContentViewer));
        if (doc && newMarkupDocViewer) {
          newMarkupDocViewer->SetDefaultCharacterSet(doc->GetDocumentCharacterSet());
        }
      }
    }
  }

  if (uriToLoad) { // get the script principal and pass it to docshell
    JSContext *cx = GetJSContextFromCallStack();

    // get the security manager
    if (!cx)
      cx = GetJSContextFromWindow(aParent);
    if (!cx) {
      rv = contextGuard.Push();
      if (NS_FAILED(rv))
        return rv;
      cx = contextGuard.get();
    }

    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    newDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

    if (!uriToLoadIsChrome) {
      nsCOMPtr<nsIScriptSecurityManager> secMan =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);

      nsCOMPtr<nsIPrincipal> principal;
      if (NS_FAILED(secMan->GetSubjectPrincipal(getter_AddRefs(principal))))
        return NS_ERROR_FAILURE;

      if (principal) {
        nsCOMPtr<nsISupports> owner(do_QueryInterface(principal));
        loadInfo->SetOwner(owner);
      }
    }

    // Set the new window's referrer from the calling context's document:

    // get the calling context off the JS context stack
    nsCOMPtr<nsIJSContextStack> stack = do_GetService(sJSStackContractID);

    JSContext* ccx = nsnull;

    // get its document, if any
    if (stack && NS_SUCCEEDED(stack->Peek(&ccx)) && ccx) {
      nsIScriptGlobalObject *sgo =
        nsWWJSUtils::GetStaticScriptGlobal(ccx, ::JS_GetGlobalObject(ccx));

      nsCOMPtr<nsPIDOMWindow> w(do_QueryInterface(sgo));
      if (w) {
        /* use the URL from the *extant* document, if any. The usual accessor
           GetDocument will synchronously create an about:blank document if
           it has no better answer, and we only care about a real document.
           Also using GetDocument to force document creation seems to
           screw up focus in the hidden window; see bug 36016.
        */
        nsCOMPtr<nsIDocument> doc(do_QueryInterface(w->GetExtantDocument()));
        if (doc) { 
          // Set the referrer
          loadInfo->SetReferrer(doc->GetDocumentURI());
        }
      }
    }

    newDocShell->LoadURI(uriToLoad, loadInfo,
                         nsIWebNavigation::LOAD_FLAGS_NONE, PR_TRUE);
  }

  if (windowIsNew)
    SizeOpenedDocShellItem(newDocShellItem, aParent, features.get(), chromeFlags);

  if (windowIsModal) {
    nsCOMPtr<nsIDocShellTreeOwner> newTreeOwner;
    newDocShellItem->GetTreeOwner(getter_AddRefs(newTreeOwner));
    nsCOMPtr<nsIWebBrowserChrome> newChrome(do_GetInterface(newTreeOwner));
    if (newChrome)
      newChrome->ShowAsModal();
    NS_ASSERTION(newChrome, "show modal window failed: no available chrome");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::RegisterNotification(nsIObserver *aObserver)
{
  // just a convenience method; it delegates to nsIObserverService
  nsresult rv;

  if (!aObserver)
    return NS_ERROR_INVALID_ARG;
  
  nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1", &rv));
  if (os) {
    rv = os->AddObserver(aObserver, "domwindowopened", PR_FALSE);
    if (NS_SUCCEEDED(rv))
      rv = os->AddObserver(aObserver, "domwindowclosed", PR_FALSE);
  }
  return rv;
}

NS_IMETHODIMP
nsWindowWatcher::UnregisterNotification(nsIObserver *aObserver)
{
  // just a convenience method; it delegates to nsIObserverService
  nsresult rv;

  if (!aObserver)
    return NS_ERROR_INVALID_ARG;
  
  nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1", &rv));
  if (os) {
    os->RemoveObserver(aObserver, "domwindowopened");
    os->RemoveObserver(aObserver, "domwindowclosed");
  }
  return rv;
}

NS_IMETHODIMP
nsWindowWatcher::GetWindowEnumerator(nsISimpleEnumerator** _retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsWatcherWindowEnumerator *enumerator = new nsWatcherWindowEnumerator(this);
  if (enumerator)
    return CallQueryInterface(enumerator, _retval);

  return NS_ERROR_OUT_OF_MEMORY;
}
	
NS_IMETHODIMP
nsWindowWatcher::GetNewPrompter(nsIDOMWindow *aParent, nsIPrompt **_retval)
{
  return NS_NewPrompter(_retval, aParent);
}

NS_IMETHODIMP
nsWindowWatcher::GetNewAuthPrompter(nsIDOMWindow *aParent, nsIAuthPrompt **_retval)
{
  return NS_NewAuthPrompter(_retval, aParent);
}

NS_IMETHODIMP
nsWindowWatcher::SetWindowCreator(nsIWindowCreator *creator)
{
  mWindowCreator = creator; // it's an nsCOMPtr, so this is an ownership ref
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::GetActiveWindow(nsIDOMWindow **aActiveWindow)
{
  if (!aActiveWindow)
    return NS_ERROR_INVALID_ARG;

  *aActiveWindow = mActiveWindow;
  NS_IF_ADDREF(mActiveWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::SetActiveWindow(nsIDOMWindow *aActiveWindow)
{
  if (FindWindowEntry(aActiveWindow)) {
    mActiveWindow = aActiveWindow;
    return NS_OK;
  }
  NS_ERROR("invalid active window");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowWatcher::AddWindow(nsIDOMWindow *aWindow, nsIWebBrowserChrome *aChrome)
{
  nsresult rv;

  if (!aWindow)
    return NS_ERROR_INVALID_ARG;

  {
    nsWatcherWindowEntry *info;
    nsAutoLock lock(mListLock);

    // if we already have an entry for this window, adjust
    // its chrome mapping and return
    info = FindWindowEntry(aWindow);
    if (info) {
      info->mChrome = aChrome;
      return NS_OK;
    }
  
    // create a window info struct and add it to the list of windows
    info = new nsWatcherWindowEntry(aWindow, aChrome);
    if (!info)
      return NS_ERROR_OUT_OF_MEMORY;

    if (mOldestWindow)
      info->InsertAfter(mOldestWindow->mOlder);
    else
      mOldestWindow = info;
  } // leave the mListLock

  // a window being added to us signifies a newly opened window.
  // send notifications.
  nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1", &rv));
  if (os) {
    nsCOMPtr<nsISupports> domwin(do_QueryInterface(aWindow));
    rv = os->NotifyObservers(domwin, "domwindowopened", 0);
  }

  return rv;
}

NS_IMETHODIMP
nsWindowWatcher::RemoveWindow(nsIDOMWindow *aWindow)
{
  // find the corresponding nsWatcherWindowEntry, remove it

  if (!aWindow)
    return NS_ERROR_INVALID_ARG;

  nsWatcherWindowEntry *info = FindWindowEntry(aWindow);
  if (info) {
    RemoveWindow(info);
    return NS_OK;
  }
  NS_WARNING("requested removal of nonexistent window\n");
  return NS_ERROR_INVALID_ARG;
}

nsWatcherWindowEntry *
nsWindowWatcher::FindWindowEntry(nsIDOMWindow *aWindow)
{
  // find the corresponding nsWatcherWindowEntry
  nsWatcherWindowEntry *info,
                       *listEnd;
#ifdef USEWEAKREFS
  nsresult    rv;
  PRBool      found;
#endif

  info = mOldestWindow;
  listEnd = 0;
#ifdef USEWEAKREFS
  rv = NS_OK;
  found = PR_FALSE;
  while (info != listEnd && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMWindow> infoWindow(do_QueryReferent(info->mWindow));
    if (!infoWindow) // clean up dangling reference, while we're here
      rv = RemoveWindow(info);
    }
    else if (infoWindow.get() == aWindow)
      return info;

    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return 0;
#else
  while (info != listEnd) {
    if (info->mWindow == aWindow)
      return info;
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return 0;
#endif
}

nsresult nsWindowWatcher::RemoveWindow(nsWatcherWindowEntry *inInfo)
{
  PRInt32  ctr,
           count = mEnumeratorList.Count();
  nsresult rv;

  {
    // notify the enumerators
    nsAutoLock lock(mListLock);
    for (ctr = 0; ctr < count; ++ctr) 
      ((nsWatcherWindowEnumerator*)mEnumeratorList[ctr])->WindowRemoved(inInfo);

    // remove the element from the list
    if (inInfo == mOldestWindow)
      mOldestWindow = inInfo->mYounger == mOldestWindow ? 0 : inInfo->mYounger;
    inInfo->Unlink();

    // clear the active window, if they're the same
    if (mActiveWindow == inInfo->mWindow)
      mActiveWindow = 0;
  }

  // a window being removed from us signifies a newly closed window.
  // send notifications.
  nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1", &rv));
  if (os) {
#ifdef USEWEAKREFS
    nsCOMPtr<nsISupports> domwin(do_QueryReferent(inInfo->mWindow));
    if (domwin)
      rv = os->NotifyObservers(domwin, "domwindowclosed", 0);
    // else bummer. since the window is gone, there's nothing to notify with.
#else
    nsCOMPtr<nsISupports> domwin(do_QueryInterface(inInfo->mWindow));
    rv = os->NotifyObservers(domwin, "domwindowclosed", 0);
#endif
  }

  delete inInfo;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::GetChromeForWindow(nsIDOMWindow *aWindow, nsIWebBrowserChrome **_retval)
{
  if (!aWindow || !_retval)
    return NS_ERROR_INVALID_ARG;
  *_retval = 0;

  nsAutoLock lock(mListLock);
  nsWatcherWindowEntry *info = FindWindowEntry(aWindow);
  if (info) {
    *_retval = info->mChrome;
    NS_IF_ADDREF(*_retval);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::GetWindowByName(const PRUnichar *aTargetName, 
                                 nsIDOMWindow *aCurrentWindow,
                                 nsIDOMWindow **aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = nsnull;

  nsCOMPtr<nsIWebNavigation> webNav;
  nsCOMPtr<nsIDocShellTreeItem> treeItem;

  // First, check if the TargetName exists in the aCurrentWindow hierarchy
  webNav = do_GetInterface(aCurrentWindow);
  if (webNav) {
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem;

    docShellTreeItem = do_QueryInterface(webNav);
    if (docShellTreeItem) {
      docShellTreeItem->FindItemWithName(aTargetName, nsnull,
                                         getter_AddRefs(treeItem));
    }
  }

  // Next, see if the TargetName exists in any window hierarchy
  if (!treeItem) {
    FindItemWithName(aTargetName, getter_AddRefs(treeItem));
  }

  if (treeItem) {
    nsCOMPtr<nsIDOMWindow> domWindow;

    domWindow = do_GetInterface(treeItem);
    if (domWindow) {
      *aResult = domWindow;
      NS_ADDREF(*aResult);
    }
  }

  return NS_OK;
}

PRBool
nsWindowWatcher::AddEnumerator(nsWatcherWindowEnumerator* inEnumerator)
{
  // (requires a lock; assumes it's called by someone holding the lock)
  return mEnumeratorList.AppendElement(inEnumerator);
}

PRBool
nsWindowWatcher::RemoveEnumerator(nsWatcherWindowEnumerator* inEnumerator)
{
  // (requires a lock; assumes it's called by someone holding the lock)
  return mEnumeratorList.RemoveElement(inEnumerator);
}

nsresult
nsWindowWatcher::URIfromURL(const char *aURL,
                            nsIDOMWindow *aParent,
                            nsIURI **aURI)
{
  nsCOMPtr<nsIDOMWindow> baseWindow;

  /* build the URI relative to the calling JS Context, if any.
     (note this is the same context used to make the security check
     in nsGlobalWindow.cpp.) */
  JSContext *cx = GetJSContextFromCallStack();
  if (cx) {
    nsIScriptContext *scriptcx = nsWWJSUtils::GetDynamicScriptContext(cx);
    if (scriptcx) {
      baseWindow = do_QueryInterface(scriptcx->GetGlobalObject());
    }
  }

  // failing that, build it relative to the parent window, if possible
  if (!baseWindow)
    baseWindow = aParent;

  // failing that, use the given URL unmodified. It had better not be relative.

  nsIURI *baseURI = nsnull;

  // get baseWindow's document URI
  if (baseWindow) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    baseWindow->GetDocument(getter_AddRefs(domDoc));
    if (domDoc) {
      nsCOMPtr<nsIDocument> doc;
      doc = do_QueryInterface(domDoc);
      if (doc) {
        baseURI = doc->GetBaseURI();
      }
    }
  }

  // build and return the absolute URI
  return NS_NewURI(aURI, aURL, baseURI);
}

/* Check for an illegal name e.g. frame3.1
   This just prints a warning message an continues; we open the window anyway,
   (see bug 32898). */
void nsWindowWatcher::CheckWindowName(nsString& aName)
{
  nsReadingIterator<PRUnichar> scan;
  nsReadingIterator<PRUnichar> endScan;

  aName.EndReading(endScan);
  for (aName.BeginReading(scan); scan != endScan; ++scan)
    if (!nsCRT::IsAsciiAlpha(*scan) && !nsCRT::IsAsciiDigit(*scan) &&
        *scan != '_') {

      // Don't use js_ReportError as this will cause the application
      // to shut down (JS_ASSERT calls abort())  See bug 32898
      nsAutoString warn;
      warn.AssignLiteral("Illegal character in window name ");
      warn.Append(aName);
      char *cp = ToNewCString(warn);
      NS_WARNING(cp);
      nsCRT::free(cp);
      break;
    }
}

#define NS_CALCULATE_CHROME_FLAG_FOR(feature, flag)               \
    prefBranch->GetBoolPref(feature, &forceEnable);               \
    if (forceEnable && !(isChrome && aHasChromeParent)) {         \
      chromeFlags |= flag;                                        \
    } else {                                                      \
      chromeFlags |= WinHasOption(aFeatures, feature,             \
                                  0, &presenceFlag)               \
                     ? flag : 0;                                  \
    }

/**
 * Calculate the chrome bitmask from a string list of features.
 * @param aFeatures a string containing a list of named chrome features
 * @param aNullFeatures true if aFeatures was a null pointer (which fact
 *                      is lost by its conversion to a string in the caller)
 * @param aDialog affects the assumptions made about unnamed features
 * @return the chrome bitmask
 */
PRUint32 nsWindowWatcher::CalculateChromeFlags(const char *aFeatures,
                                               PRBool aFeaturesSpecified,
                                               PRBool aDialog,
                                               PRBool aChromeURL,
                                               PRBool aHasChromeParent)
{
   if(!aFeaturesSpecified || !aFeatures) {
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

  nsCOMPtr<nsIScriptSecurityManager>
    securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(securityManager, NS_ERROR_FAILURE);

  PRBool isChrome = PR_FALSE;
  securityManager->SubjectPrincipalIsSystem(&isChrome);

  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  rv = prefs->GetBranch("dom.disable_window_open_feature.", getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  PRBool forceEnable = PR_FALSE;

  NS_CALCULATE_CHROME_FLAG_FOR("titlebar",
                               nsIWebBrowserChrome::CHROME_TITLEBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("close",
                               nsIWebBrowserChrome::CHROME_WINDOW_CLOSE);
  NS_CALCULATE_CHROME_FLAG_FOR("toolbar",
                               nsIWebBrowserChrome::CHROME_TOOLBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("location",
                               nsIWebBrowserChrome::CHROME_LOCATIONBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("directories",
                               nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("personalbar",
                               nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("status",
                               nsIWebBrowserChrome::CHROME_STATUSBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("menubar",
                               nsIWebBrowserChrome::CHROME_MENUBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("scrollbars",
                               nsIWebBrowserChrome::CHROME_SCROLLBARS);
  NS_CALCULATE_CHROME_FLAG_FOR("resizable",
                               nsIWebBrowserChrome::CHROME_WINDOW_RESIZE);
  NS_CALCULATE_CHROME_FLAG_FOR("minimizable",
                               nsIWebBrowserChrome::CHROME_WINDOW_MIN);

  chromeFlags |= WinHasOption(aFeatures, "popup", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_WINDOW_POPUP : 0; 

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
  chromeFlags |= WinHasOption(aFeatures, "extrachrome", 0, nsnull) ?
    nsIWebBrowserChrome::CHROME_EXTRA : 0;
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

  // Check security state for use in determing window dimensions
  PRBool enabled;
  nsresult res =
    securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);

  if (NS_FAILED(res) || !enabled || (isChrome && !aHasChromeParent)) {
    // If priv check fails (or if we're called from chrome, but the
    // parent is not a chrome window), set all elements to minimum
    // reqs., else leave them alone.
    chromeFlags |= nsIWebBrowserChrome::CHROME_TITLEBAR;
    chromeFlags |= nsIWebBrowserChrome::CHROME_WINDOW_CLOSE;
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_WINDOW_LOWERED;
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_WINDOW_RAISED;
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_WINDOW_POPUP;
    /* Untrusted script is allowed to pose modal windows with a chrome
       scheme. This check could stand to be better. But it effectively
       prevents untrusted script from opening modal windows in general
       while still allowing alerts and the like. */
    if (!aChromeURL)
      chromeFlags &= ~(nsIWebBrowserChrome::CHROME_MODAL | nsIWebBrowserChrome::CHROME_OPENAS_CHROME);
  }

  return chromeFlags;
}

PRInt32
nsWindowWatcher::WinHasOption(const char *aOptions, const char *aName,
                              PRInt32 aDefault, PRBool *aPresenceFlag)
{
  if (!aOptions)
    return 0;

  char *comma, *equal;
  PRInt32 found = 0;

#ifdef DEBUG
    nsCAutoString options(aOptions);
    NS_ASSERTION(options.FindCharInSet(" \n\r\t") == kNotFound, 
                  "There should be no whitespace in this string!");
#endif

  while (PR_TRUE) {
    comma = PL_strchr(aOptions, ',');
    if (comma)
      *comma = '\0';
    equal = PL_strchr(aOptions, '=');
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

/* try to find an nsIDocShellTreeItem with the given name in any
   known open window. a failure to find the item will not
   necessarily return a failure method value. check aFoundItem.
*/
nsresult
nsWindowWatcher::FindItemWithName(
                    const PRUnichar* aName,
                    nsIDocShellTreeItem** aFoundItem)
{
  PRBool   more;
  nsresult rv;
  nsAutoString name(aName);

  *aFoundItem = 0;

  /* special cases */
  if(name.IsEmpty())
    return NS_OK;
  if(name.LowerCaseEqualsLiteral("_blank") || name.LowerCaseEqualsLiteral("_new"))
    return NS_OK;
  // _content will be handled by individual windows, below

  nsCOMPtr<nsISimpleEnumerator> windows;
  GetWindowEnumerator(getter_AddRefs(windows));
  if (!windows)
    return NS_ERROR_FAILURE;

  rv = NS_OK;
  do {
    windows->HasMoreElements(&more);
    if (!more)
      break;
    nsCOMPtr<nsISupports> nextSupWindow;
    windows->GetNext(getter_AddRefs(nextSupWindow));
    if (nextSupWindow) {
      nsCOMPtr<nsIDOMWindow> nextWindow(do_QueryInterface(nextSupWindow));
      if (nextWindow) {
        nsCOMPtr<nsIDocShellTreeItem> treeItem;
        GetWindowTreeItem(nextWindow, getter_AddRefs(treeItem));
        if (treeItem) {
          rv = treeItem->FindItemWithName(aName, treeItem, aFoundItem);
          if (NS_FAILED(rv) || *aFoundItem)
            break;
        }
      }
    }
  } while(1);

  return rv;
}

/* Fetch the nsIDOMWindow corresponding to the given nsIDocShellTreeItem.
   This forces the creation of a script context, if one has not already
   been created. Note it also sets the window's opener to the parent,
   if applicable -- because it's just convenient, that's all. null aParent
   is acceptable. */
nsresult
nsWindowWatcher::ReadyOpenedDocShellItem(nsIDocShellTreeItem *aOpenedItem,
                                         nsIDOMWindow        *aParent,
                                         nsIDOMWindow        **aOpenedWindow)
{
  nsresult rv = NS_ERROR_FAILURE;

  *aOpenedWindow = 0;
  nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(aOpenedItem));
  if (globalObject) {
    if (aParent) {
      nsCOMPtr<nsIDOMWindowInternal> internalParent(do_QueryInterface(aParent));
      globalObject->SetOpenerWindow(internalParent); // damnit
    }
    rv = CallQueryInterface(globalObject, aOpenedWindow);
  }
  return rv;
}

/* Size and position the new window according to aFeatures. This method
   is assumed to be called after the window has already been given
   a default position and size; thus its current position and size are
   accurate defaults. The new window is made visible at method end.
*/
void
nsWindowWatcher::SizeOpenedDocShellItem(nsIDocShellTreeItem *aDocShellItem,
                                        nsIDOMWindow *aParent,
                                        const char *aFeatures,
                                        PRUint32 aChromeFlags)
{
  // position and size of window
  PRInt32 left = 0,
          top = 0,
          width = 100,
          height = 100;
  // difference between chrome and content size
  PRInt32 chromeWidth = 0,
          chromeHeight = 0;
  // whether the window size spec refers to chrome or content
  PRBool  sizeChromeWidth = PR_TRUE,
          sizeChromeHeight = PR_TRUE;

  // get various interfaces for aDocShellItem, used throughout this method
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  aDocShellItem->GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_QueryInterface(treeOwner));
  if (!treeOwnerAsWin) // we'll need this to actually size the docshell
    return;

  /* The current position and size will be unchanged if not specified
     (and they fit entirely onscreen). Also, calculate the difference
     between chrome and content sizes on aDocShellItem's window.
     This latter point becomes important if chrome and content
     specifications are mixed in aFeatures, and when bringing the window
     back from too far off the right or bottom edges of the screen. */

  treeOwnerAsWin->GetPositionAndSize(&left, &top, &width, &height);
  { // scope shellWindow why not
    nsCOMPtr<nsIBaseWindow> shellWindow(do_QueryInterface(aDocShellItem));
    if (shellWindow) {
      PRInt32 cox, coy;
      shellWindow->GetSize(&cox, &coy);
      chromeWidth = width - cox;
      chromeHeight = height - coy;
    }
  }

  // Parse position spec, if any, from aFeatures

  PRBool  positionSpecified = PR_FALSE;
  PRBool  present;
  PRInt32 temp;

  present = PR_FALSE;
  if ((temp = WinHasOption(aFeatures, "left", 0, &present)) || present)
    left = temp;
  else if ((temp = WinHasOption(aFeatures, "screenX", 0, &present)) || present)
    left = temp;
  if (present)
    positionSpecified = PR_TRUE;

  present = PR_FALSE;
  if ((temp = WinHasOption(aFeatures, "top", 0, &present)) || present)
    top = temp;
  else if ((temp = WinHasOption(aFeatures, "screenY", 0, &present)) || present)
    top = temp;
  if (present)
    positionSpecified = PR_TRUE;

  PRBool sizeSpecified = PR_FALSE;

  // Parse size spec, if any. Chrome size overrides content size.

  if ((temp = WinHasOption(aFeatures, "outerWidth", width, nsnull))) {
    width = temp;
    sizeSpecified = PR_TRUE;
  } else if ((temp = WinHasOption(aFeatures, "width",
                                  width - chromeWidth, nsnull))) {
    width = temp;
    sizeChromeWidth = PR_FALSE;
    sizeSpecified = PR_TRUE;
  } else if ((temp = WinHasOption(aFeatures, "innerWidth",
                                  width - chromeWidth, nsnull))) {
    width = temp;
    sizeChromeWidth = PR_FALSE;
    sizeSpecified = PR_TRUE;
  }

  if ((temp = WinHasOption(aFeatures, "outerHeight", height, nsnull))) {
    height = temp;
    sizeSpecified = PR_TRUE;
  } else if ((temp = WinHasOption(aFeatures, "height",
                                  height - chromeHeight, nsnull))) {
    height = temp;
    sizeChromeHeight = PR_FALSE;
    sizeSpecified = PR_TRUE;
  } else if ((temp = WinHasOption(aFeatures, "innerHeight",
                                  height - chromeHeight, nsnull))) {
    height = temp;
    sizeChromeHeight = PR_FALSE;
    sizeSpecified = PR_TRUE;
  }

  nsresult res;
  PRBool enabled = PR_FALSE;

  // Check security state for use in determing window dimensions
  nsCOMPtr<nsIScriptSecurityManager>
    securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
  if (securityManager) {
    res = securityManager->IsCapabilityEnabled("UniversalBrowserWrite",
                                               &enabled);
    if (NS_FAILED(res))
      enabled = PR_FALSE;
    else if (enabled && aParent) {
      nsCOMPtr<nsIDOMChromeWindow> chromeWin(do_QueryInterface(aParent));

      PRBool isChrome = PR_FALSE;
      securityManager->SubjectPrincipalIsSystem(&isChrome);

      // Only enable special priveleges for chrome when chrome calls
      // open() on a chrome window
      enabled = !(isChrome && chromeWin == nsnull);
    }
  }

  if (!enabled) {

    // Security check failed.  Ensure all args meet minimum reqs.

    PRInt32 oldTop = top,
            oldLeft = left;

    // We'll also need the screen dimensions
    nsCOMPtr<nsIScreen> screen;
    nsCOMPtr<nsIScreenManager> screenMgr(do_GetService(
                                         "@mozilla.org/gfx/screenmanager;1"));
    if (screenMgr)
      screenMgr->ScreenForRect(left, top, width, height,
                               getter_AddRefs(screen));
    if (screen) {
      PRInt32 screenLeft, screenTop, screenWidth, screenHeight;
      PRInt32 winWidth = width + (sizeChromeWidth ? 0 : chromeWidth),
              winHeight = height + (sizeChromeHeight ? 0 : chromeHeight);

      screen->GetAvailRect(&screenLeft, &screenTop,
                           &screenWidth, &screenHeight);

      if (sizeSpecified) {
        /* Unlike position, force size out-of-bounds check only if
           size actually was specified. Otherwise, intrinsically sized
           windows are broken. */
        if (height < 100)
          height = 100;
        if (winHeight > screenHeight)
          height = screenHeight - (sizeChromeHeight ? 0 : chromeHeight);
        if (width < 100)
          width = 100;
        if (winWidth > screenWidth)
          width = screenWidth - (sizeChromeWidth ? 0 : chromeWidth);
      }

      if (left+winWidth > screenLeft+screenWidth)
        left = screenLeft+screenWidth - winWidth;
      if (left < screenLeft)
        left = screenLeft;
      if (top+winHeight > screenTop+screenHeight)
        top = screenTop+screenHeight - winHeight;
      if (top < screenTop)
        top = screenTop;
      if (top != oldTop || left != oldLeft)
        positionSpecified = PR_TRUE;
    }
  }

  // size and position the window

  if (positionSpecified)
    treeOwnerAsWin->SetPosition(left, top);
  if (sizeSpecified) {
    /* Prefer to trust the interfaces, which think in terms of pure
       chrome or content sizes. If we have a mix, use the chrome size
       adjusted by the chrome/content differences calculated earlier. */
    if (!sizeChromeWidth && !sizeChromeHeight)
      treeOwner->SizeShellTo(aDocShellItem, width, height);
    else {
      if (!sizeChromeWidth)
        width += chromeWidth;
      if (!sizeChromeHeight)
        height += chromeHeight;
      treeOwnerAsWin->SetSize(width, height, PR_FALSE);
    }
  }
  treeOwnerAsWin->SetVisibility(PR_TRUE);
}

// attach the given array of JS values to the given window, as a property array
// named "arguments"
nsresult
nsWindowWatcher::AttachArguments(nsIDOMWindow *aWindow,
                                 PRUint32 argc, jsval *argv)
{
  if (argc == 0)
    return NS_OK;

  // copy the extra parameters into a JS Array and attach it
  nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_QueryInterface(aWindow));
  NS_ENSURE_TRUE(scriptGlobal, NS_ERROR_UNEXPECTED);

  nsIScriptContext *scriptContext = scriptGlobal->GetContext();
  if (scriptContext) {
    JSContext *cx;
    cx = (JSContext *)scriptContext->GetNativeContext();

    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    rv = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx), aWindow,
                         NS_GET_IID(nsIDOMWindow), getter_AddRefs(wrapper));
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *window_obj;
    rv = wrapper->GetJSObject(&window_obj);
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *args;
    args = ::JS_NewArrayObject(cx, argc, argv);
    if (args) {
      jsval argsVal = OBJECT_TO_JSVAL(args);
      // ::JS_DefineProperty(cx, window_obj, "arguments",
      // argsVal, NULL, NULL, JSPROP_PERMANENT);
      ::JS_SetProperty(cx, window_obj, "arguments", &argsVal);
    }
  }

  return NS_OK;
}

nsresult
nsWindowWatcher::ConvertSupportsTojsvals(nsIDOMWindow *aWindow,
                                         nsISupports *aArgs,
                                         PRUint32 *aArgc, jsval **aArgv)
{
  nsresult rv = NS_OK;

  *aArgv = nsnull;
  *aArgc = 0;

  // copy the elements in aArgsArray into the JS array
  // window.arguments in the new window

  if (!aArgs)
    return NS_OK;

  PRUint32 argCtr, argCount;
  nsCOMPtr<nsISupportsArray> argsArray(do_QueryInterface(aArgs));

  if (argsArray) {
    argsArray->Count(&argCount);
    if (argCount == 0)
      return NS_OK;
  } else
    argCount = 1; // the nsISupports which is not an array

  jsval *argv = NS_STATIC_CAST(jsval *, nsMemory::Alloc(argCount * sizeof(jsval)));
  NS_ENSURE_TRUE(argv, NS_ERROR_OUT_OF_MEMORY);

  AutoFree             argvGuard(argv);

  JSContext           *cx;
  JSContextAutoPopper  contextGuard;

  cx = GetJSContextFromWindow(aWindow);
  if (!cx)
    cx = GetJSContextFromCallStack();
  if (!cx) {
    rv = contextGuard.Push();
    if (NS_FAILED(rv))
      return rv;
    cx = contextGuard.get();
  }

  if (argsArray)
    for (argCtr = 0; argCtr < argCount && NS_SUCCEEDED(rv); argCtr++) {
      nsCOMPtr<nsISupports> s(dont_AddRef(argsArray->ElementAt(argCtr)));
      rv = AddSupportsTojsvals(s, cx, argv + argCtr);
    }
  else
    rv = AddSupportsTojsvals(aArgs, cx, argv);

  if (NS_FAILED(rv))
    return rv;

  argvGuard.Invalidate();

  *aArgv = argv;
  *aArgc = argCount;
  return NS_OK;
}

nsresult
nsWindowWatcher::AddInterfaceTojsvals(nsISupports *aArg,
                                      JSContext *cx,
                                      jsval *aArgv)
{
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
  rv = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx), aArg,
              NS_GET_IID(nsISupports), getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *obj;
  rv = wrapper->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, rv);

  *aArgv = OBJECT_TO_JSVAL(obj);
  return NS_OK;
}

nsresult
nsWindowWatcher::AddSupportsTojsvals(nsISupports *aArg,
                                     JSContext *cx, jsval *aArgv)
{
  if (!aArg) {
    *aArgv = JSVAL_NULL;
    return NS_OK;
  }

  nsCOMPtr<nsISupportsPrimitive> argPrimitive(do_QueryInterface(aArg));
  if (!argPrimitive)
    return AddInterfaceTojsvals(aArg, cx, aArgv);

  PRUint16 type;
  argPrimitive->GetType(&type);

  switch(type) {
    case nsISupportsPrimitive::TYPE_CSTRING : {
      nsCOMPtr<nsISupportsCString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsCAutoString data;

      p->GetData(data);

      
      JSString *str = ::JS_NewStringCopyN(cx, data.get(), data.Length());
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      *aArgv = STRING_TO_JSVAL(str);

      break;
    }
    case nsISupportsPrimitive::TYPE_STRING : {
      nsCOMPtr<nsISupportsString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsAutoString data;

      p->GetData(data);

      // cast is probably safe since wchar_t and jschar are expected
      // to be equivalent; both unsigned 16-bit entities
      JSString *str =
        ::JS_NewUCStringCopyN(cx,
                              NS_REINTERPRET_CAST(const jschar *,data.get()),
                              data.Length());
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      *aArgv = STRING_TO_JSVAL(str);
      break;
    }
    case nsISupportsPrimitive::TYPE_PRBOOL : {
      nsCOMPtr<nsISupportsPRBool> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRBool data;

      p->GetData(&data);

      *aArgv = BOOLEAN_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT8 : {
      nsCOMPtr<nsISupportsPRUint8> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRUint8 data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT16 : {
      nsCOMPtr<nsISupportsPRUint16> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRUint16 data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT32 : {
      nsCOMPtr<nsISupportsPRUint32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRUint32 data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_CHAR : {
      nsCOMPtr<nsISupportsChar> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      char data;

      p->GetData(&data);

      JSString *str = ::JS_NewStringCopyN(cx, &data, 1);
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      *aArgv = STRING_TO_JSVAL(str);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRINT16 : {
      nsCOMPtr<nsISupportsPRInt16> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRInt16 data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRINT32 : {
      nsCOMPtr<nsISupportsPRInt32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRInt32 data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_FLOAT : {
      nsCOMPtr<nsISupportsFloat> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      float data;

      p->GetData(&data);

      jsdouble *d = ::JS_NewDouble(cx, data);

      *aArgv = DOUBLE_TO_JSVAL(d);

      break;
    }
    case nsISupportsPrimitive::TYPE_DOUBLE : {
      nsCOMPtr<nsISupportsDouble> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      double data;

      p->GetData(&data);

      jsdouble *d = ::JS_NewDouble(cx, data);

      *aArgv = DOUBLE_TO_JSVAL(d);

      break;
    }
    case nsISupportsPrimitive::TYPE_INTERFACE_POINTER : {
      nsCOMPtr<nsISupportsInterfacePointer> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsISupports> data;
      nsIID *iid = nsnull;

      p->GetData(getter_AddRefs(data));
      p->GetDataIID(&iid);
      NS_ENSURE_TRUE(iid, NS_ERROR_UNEXPECTED);

      AutoFree iidGuard(iid); // Free iid upon destruction.

      nsresult rv;
      nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
      rv = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx), data,
                           *iid, getter_AddRefs(wrapper));
      NS_ENSURE_SUCCESS(rv, rv);

      JSObject *obj;
      rv = wrapper->GetJSObject(&obj);
      NS_ENSURE_SUCCESS(rv, rv);

      *aArgv = OBJECT_TO_JSVAL(obj);

      break;
    }
    case nsISupportsPrimitive::TYPE_ID :
    case nsISupportsPrimitive::TYPE_PRUINT64 :
    case nsISupportsPrimitive::TYPE_PRINT64 :
    case nsISupportsPrimitive::TYPE_PRTIME :
    case nsISupportsPrimitive::TYPE_VOID : {
      NS_WARNING("Unsupported primitive type used");
      *aArgv = JSVAL_NULL;
      break;
    }
    default : {
      NS_WARNING("Unknown primitive type used");
      *aArgv = JSVAL_NULL;
      break;
    }
  }
  return NS_OK;
}

void
nsWindowWatcher::GetWindowTreeItem(nsIDOMWindow *inWindow,
                                   nsIDocShellTreeItem **outTreeItem)
{
  *outTreeItem = 0;

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(inWindow));
  if (sgo) {
    nsIDocShell *docshell = sgo->GetDocShell();
    if (docshell)
      CallQueryInterface(docshell, outTreeItem);
  }
}

void
nsWindowWatcher::GetWindowTreeOwner(nsIDOMWindow *inWindow,
                                    nsIDocShellTreeOwner **outTreeOwner)
{
  *outTreeOwner = 0;

  nsCOMPtr<nsIDocShellTreeItem> treeItem;
  GetWindowTreeItem(inWindow, getter_AddRefs(treeItem));
  if (treeItem)
    treeItem->GetTreeOwner(outTreeOwner);
}

JSContext *
nsWindowWatcher::GetJSContextFromCallStack()
{
  JSContext *cx = 0;

  nsCOMPtr<nsIThreadJSContextStack> cxStack(do_GetService(sJSStackContractID));
  if (cxStack)
    cxStack->Peek(&cx);

  return cx;
}

JSContext *
nsWindowWatcher::GetJSContextFromWindow(nsIDOMWindow *aWindow)
{
  JSContext *cx = 0;

  if (aWindow) {
    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aWindow));
    if (sgo) {
      nsIScriptContext *scx = sgo->GetContext();
      if (scx)
        cx = (JSContext *) scx->GetNativeContext();
    }
    /* (off-topic note:) the nsIScriptContext can be retrieved by
    nsCOMPtr<nsIScriptContext> scx;
    nsJSUtils::nsGetDynamicScriptContext(cx, getter_AddRefs(scx));
    */
  }

  return cx;
}

JSObject *
nsWindowWatcher::GetWindowScriptObject(nsIDOMWindow *inWindow)
{
  nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_QueryInterface(inWindow));
  return scriptGlobal ? scriptGlobal->GetGlobalJSObject() : 0;
}

