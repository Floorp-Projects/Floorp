/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//#define USEWEAKREFS // (haven't quite figured that out yet)

#include "nsWindowWatcher.h"

#include "jscntxt.h"
#include "nsAutoLock.h"
#include "nsCRT.h"
#include "nsWWJSUtils.h"
#include "nsNetUtil.h"
#include "nsPrompt.h"
#include "plstr.h"

#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMScreen.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowInternal.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIGenericFactory.h"
#include "nsIJSContextStack.h"
#include "nsIObserverService.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIURI.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebNavigation.h"
#include "nsIWindowCreator.h"
#include "nsIXPConnect.h"
#ifdef USEWEAKREFS
#include "nsIWeakReference.h"
#endif

#define NOTIFICATION_OPENED NS_LITERAL_STRING("domwindowopened")
#define NOTIFICATION_CLOSED NS_LITERAL_STRING("domwindowclosed")

static const char *sJSStackContractID="@mozilla.org/js/xpc/ContextStack;1";

/****************************************************************
 ************************* WindowInfo ***************************
 ****************************************************************/

class nsWindowWatcher;

struct WindowInfo {

  WindowInfo(nsIDOMWindow* inWindow) {
#ifdef USEWEAKREFS
    mWindow = getter_AddRefs(NS_GetWeakReference(inWindow));
#else
    mWindow = inWindow;
#endif
    ReferenceSelf();
  }
  ~WindowInfo() {}

  void InsertAfter(WindowInfo *inOlder);
  void Unlink();
  void ReferenceSelf();

#ifdef USEWEAKREFS
  nsCOMPtr<nsIWeakReference> mWindow;
#else // still not an owning ref
  nsIDOMWindow              *mWindow;
#endif
  // each struct is in a circular, doubly-linked list
  WindowInfo                *mYounger, // next younger in sequence
                            *mOlder;
};

void WindowInfo::InsertAfter(WindowInfo *inOlder)
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

void WindowInfo::Unlink() {

  mOlder->mYounger = mYounger;
  mYounger->mOlder = mOlder;
  ReferenceSelf();
}

void WindowInfo::ReferenceSelf() {

  mYounger = this;
  mOlder = this;
}

/****************************************************************
 ********************* nsWindowEnumerator ***********************
 ****************************************************************/

class nsWindowEnumerator : public nsISimpleEnumerator {

public:
  nsWindowEnumerator(nsWindowWatcher *inWatcher);
  virtual ~nsWindowEnumerator();
  NS_IMETHOD HasMoreElements(PRBool *retval);
  NS_IMETHOD GetNext(nsISupports **retval);

  NS_DECL_ISUPPORTS

private:
  friend class nsWindowWatcher;

  WindowInfo *FindNext();
  void WindowRemoved(WindowInfo *inInfo);

  nsWindowWatcher *mWindowWatcher;
  WindowInfo      *mCurrentPosition;
};

NS_IMPL_ADDREF(nsWindowEnumerator);
NS_IMPL_RELEASE(nsWindowEnumerator);
NS_IMPL_QUERY_INTERFACE1(nsWindowEnumerator, nsISimpleEnumerator);

nsWindowEnumerator::nsWindowEnumerator(nsWindowWatcher *inWatcher)
  : mWindowWatcher(inWatcher),
    mCurrentPosition(inWatcher->mOldestWindow)
{
  NS_INIT_REFCNT();
  mWindowWatcher->AddEnumerator(this);
  mWindowWatcher->AddRef();
}

nsWindowEnumerator::~nsWindowEnumerator()
{
  mWindowWatcher->RemoveEnumerator(this);
  mWindowWatcher->Release();
}

NS_IMETHODIMP
nsWindowEnumerator::HasMoreElements(PRBool *retval)
{
  if (!retval)
    return NS_ERROR_INVALID_ARG;

  *retval = mCurrentPosition? PR_TRUE : PR_FALSE;
  return NS_OK;
}
	
NS_IMETHODIMP
nsWindowEnumerator::GetNext(nsISupports **retval)
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

WindowInfo *
nsWindowEnumerator::FindNext()
{
  WindowInfo *info;

  if (!mCurrentPosition)
    return 0;

  info = mCurrentPosition->mYounger;
  return info == mWindowWatcher->mOldestWindow ? 0 : info;
}

// if a window is being removed adjust the iterator's current position
void nsWindowEnumerator::WindowRemoved(WindowInfo *inInfo) {

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
};

EventQueueAutoPopper::EventQueueAutoPopper() : mQueue(nsnull)
{
}

EventQueueAutoPopper::~EventQueueAutoPopper()
{
  if(mQueue)
    mService->PopThreadEventQueue(mQueue);
}

nsresult EventQueueAutoPopper::Push()
{
  if (mQueue) // only once
    return NS_ERROR_FAILURE;

  mService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
  if(mService)
    mService->PushThreadEventQueue(getter_AddRefs(mQueue));
  return mQueue ? NS_OK : NS_ERROR_FAILURE; 
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

NS_IMPL_ADDREF(nsWindowWatcher);
NS_IMPL_RELEASE(nsWindowWatcher);
NS_IMPL_QUERY_INTERFACE2(nsWindowWatcher, nsIWindowWatcher, nsPIWindowWatcher)

nsWindowWatcher::nsWindowWatcher() :
        mEnumeratorList(),
        mOldestWindow(0),
        mActiveWindow(0),
        mListLock(0)
{
  NS_INIT_REFCNT();
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
                                  windowIsModal = PR_FALSE;
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

  if (aUrl)
    rv = URIfromURL(aUrl, aParent, getter_AddRefs(uriToLoad));
  if (NS_FAILED(rv))
    return rv;

  nameSpecified = PR_FALSE;
  if (aName) {
    name.AssignWithConversion(aName);
    CheckWindowName(name);
    nameSpecified = PR_TRUE;
  }

  featuresSpecified = PR_FALSE;
  if (aFeatures) {
    features.Assign(aFeatures);
    featuresSpecified = PR_TRUE;
  }

  chromeFlags = CalculateChromeFlags(features, featuresSpecified, aDialog);

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
      if (name.EqualsIgnoreCase("_self")) {
        GetWindowTreeItem(aParent, getter_AddRefs(newDocShellItem));
      } else if (name.EqualsIgnoreCase("_top")) {
        nsCOMPtr<nsIDocShellTreeItem> shelltree;
        GetWindowTreeItem(aParent, getter_AddRefs(shelltree));
        if (shelltree)
          shelltree->GetSameTypeRootTreeItem(getter_AddRefs(newDocShellItem));
      } else
        parentTreeOwner->FindItemWithName(name.GetUnicode(), nsnull,
                                          getter_AddRefs(newDocShellItem));
    } else
      FindItemWithName(name.GetUnicode(), getter_AddRefs(newDocShellItem));
  }

  // no extant window? make a new one.
  if (!newDocShellItem) {
    windowIsNew = PR_TRUE;

    // is the parent (if any) modal? if so, we must be, too.
    PRBool weAreModal = PR_FALSE;
    if (parentTreeOwner) {
      nsCOMPtr<nsIInterfaceRequestor> parentRequestor(do_QueryInterface(parentTreeOwner));
      if (parentRequestor) {
        nsCOMPtr<nsIWebBrowserChrome> parentChrome;
        parentRequestor->GetInterface(NS_GET_IID(nsIWebBrowserChrome), getter_AddRefs(parentChrome));
        if (parentChrome)
          parentChrome->IsWindowModal(&weAreModal);
      }
    }
    if (weAreModal || (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL)) {
      rv = queueGuard.Push();
      if (NS_SUCCEEDED(rv)) {
        windowIsModal = PR_TRUE;
        // in case we added this because weAreModal
        chromeFlags |= nsIWebBrowserChrome::CHROME_MODAL | nsIWebBrowserChrome::CHROME_DEPENDENT;
      }
    }
    if (parentTreeOwner)
      parentTreeOwner->GetNewWindow(chromeFlags, getter_AddRefs(newDocShellItem));
    else if (mWindowCreator) {
      nsCOMPtr<nsIWebBrowserChrome> newChrome;
      mWindowCreator->CreateChromeWindow(0, chromeFlags, getter_AddRefs(newChrome));
      if (newChrome) {
        nsCOMPtr<nsIInterfaceRequestor> thing(do_QueryInterface(newChrome));
        if (thing) {
          /* It might be a chrome nsXULWindow, in which case it won't have
             an nsIDOMWindow (primary content shell). But in that case, it'll
             be able to hand over an nsIDocShellTreeItem directly. */
          // XXX got the order right?
          nsCOMPtr<nsIDOMWindow> newWindow;
          thing->GetInterface(NS_GET_IID(nsIDOMWindow), getter_AddRefs(newWindow));
          if (newWindow)
            GetWindowTreeItem(newWindow, getter_AddRefs(newDocShellItem));
          if (!newDocShellItem)
            thing->GetInterface(NS_GET_IID(nsIDocShellTreeItem), getter_AddRefs(newDocShellItem));
        }
      }
    }
  }

  // better have a window to use by this point
  if (!newDocShellItem)
    return NS_ERROR_FAILURE;

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

    if (PL_strcasestr(features, "width=") || PL_strcasestr(features, "height=")) {

      nsCOMPtr<nsIDocShellTreeOwner> newTreeOwner;
      newDocShellItem->GetTreeOwner(getter_AddRefs(newTreeOwner));
      if (newTreeOwner)
        newTreeOwner->SetPersistence(PR_FALSE, PR_FALSE, PR_FALSE);
    }
  }

  if (aDialog && argc > 0)
    AttachArguments(*_retval, argc, argv);

  nsCOMPtr<nsIScriptSecurityManager> secMan;

  if (uriToLoad) {
    /* Get security manager, check to see if URI is allowed.
       Don't call CheckLoadURI for dialogs - see bug 56851
       The security of this function depends on window.openDialog being 
       inaccessible from web scripts */
    JSContext                  *cx;
    nsCOMPtr<nsIScriptContext>  scriptCX;
    cx = GetExtantJSContext(aParent);
    if (!cx) {
      rv = contextGuard.Push();
      if (NS_FAILED(rv))
        return rv;
      cx = contextGuard.get();
    }
#if 0
    // better than trying so hard to find a script object? or just wrong?
    nsJSUtils::nsGetDynamicScriptContext(cx, getter_AddRefs(scriptCX));
#else
    JSObject *scriptObject = GetWindowScriptObject(aParent ? aParent : *_retval);
    nsWWJSUtils::nsGetStaticScriptContext(cx, scriptObject,
                                          getter_AddRefs(scriptCX));
#endif
    if (!scriptCX ||
        NS_FAILED(scriptCX->GetSecurityManager(getter_AddRefs(secMan))) ||
        ((!aDialog && NS_FAILED(secMan->CheckLoadURIFromScript(cx, uriToLoad)))))
      return NS_ERROR_FAILURE;
  }

  newDocShellItem->SetName(nameSpecified ? name.GetUnicode() : nsnull);

  nsCOMPtr<nsIDocShell> newDocShell(do_QueryInterface(newDocShellItem));
  if (uriToLoad) { // Get script principal and pass to docshell
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(secMan->GetSubjectPrincipal(getter_AddRefs(principal))))
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    newDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

    if (principal) {
      nsCOMPtr<nsISupports> owner(do_QueryInterface(principal));
      loadInfo->SetOwner(owner);
    }

    // Get the calling context off the JS context stack
    nsCOMPtr<nsIJSContextStack> stack = do_GetService(sJSStackContractID);

    JSContext* ccx = nsnull;

    if (stack && NS_SUCCEEDED(stack->Peek(&ccx)) && ccx) {
      JSObject *global = ::JS_GetGlobalObject(ccx);

      if (global) {
        JSClass* jsclass = ::JS_GetClass(ccx, global);

        // Check if the global object on the calling context has
        // nsISupports * private data
        if (jsclass &&
            !((~jsclass->flags) & (JSCLASS_HAS_PRIVATE |
                                   JSCLASS_PRIVATE_IS_NSISUPPORTS))) {
          nsISupports* sup = (nsISupports *)::JS_GetPrivate(ccx, global);

          nsCOMPtr<nsIDOMWindow> w(do_QueryInterface(sup));

          if (w) {
            nsCOMPtr<nsIDOMDocument> document;

            // Get the document from the window.
            w->GetDocument(getter_AddRefs(document));

            nsCOMPtr<nsIDocument> doc(do_QueryInterface(document));

            if (doc) { 
              nsCOMPtr<nsIURI> uri(dont_AddRef(doc->GetDocumentURL()));

              // Set the referrer
              loadInfo->SetReferrer(uri);
            }
          }
        }
      }
    }

    newDocShell->LoadURI(uriToLoad, loadInfo,
                         nsIWebNavigation::LOAD_FLAGS_NONE);
  }

  if (windowIsNew)
    SizeOpenedDocShellItem(newDocShellItem, aParent, features, chromeFlags);

  if (windowIsModal) {
    nsCOMPtr<nsIDocShellTreeOwner> newTreeOwner;
    nsCOMPtr<nsIInterfaceRequestor> newRequestor;
    nsCOMPtr<nsIWebBrowserChrome> newChrome;

    newDocShellItem->GetTreeOwner(getter_AddRefs(newTreeOwner));
    if (newTreeOwner)
      newRequestor = do_QueryInterface(newTreeOwner);
    if (newRequestor)
      newRequestor->GetInterface(NS_GET_IID(nsIWebBrowserChrome), getter_AddRefs(newChrome));

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
  
  nsCOMPtr<nsIObserverService> os(do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv));
  if (os) {
    rv = os->AddObserver(aObserver, NOTIFICATION_OPENED.get());
    if (NS_SUCCEEDED(rv))
      rv = os->AddObserver(aObserver, NOTIFICATION_CLOSED.get());
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
  
  nsCOMPtr<nsIObserverService> os(do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv));
  if (os) {
    os->RemoveObserver(aObserver, NOTIFICATION_OPENED.get());
    os->RemoveObserver(aObserver, NOTIFICATION_CLOSED.get());
  }
  return rv;
}

NS_IMETHODIMP
nsWindowWatcher::GetWindowEnumerator(nsISimpleEnumerator** _retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_ARG;

  nsAutoLock lock(mListLock);
  nsWindowEnumerator *enumerator = new nsWindowEnumerator(this);
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
  mActiveWindow = aActiveWindow;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::AddWindow(nsIDOMWindow *aWindow)
{
  nsresult rv;

  if (!aWindow)
    return NS_ERROR_INVALID_ARG;
  
  // create window info struct and add to list of windows
  WindowInfo* info = new WindowInfo(aWindow);
  if (!info)
    return NS_ERROR_OUT_OF_MEMORY;

  nsAutoLock lock(mListLock);
  if (mOldestWindow)
    info->InsertAfter(mOldestWindow->mOlder);
  else
    mOldestWindow = info;

  // a window being added to us signifies a newly opened window.
  // send notifications.
  nsCOMPtr<nsIObserverService> os(do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv));
  if (os) {
    nsCOMPtr<nsISupports> domwin(do_QueryInterface(aWindow));
    rv = os->Notify(domwin, NOTIFICATION_OPENED.get(), 0);
  }

  return rv;
}

NS_IMETHODIMP
nsWindowWatcher::RemoveWindow(nsIDOMWindow *aWindow)
{
  // find the corresponding WindowInfo, remove it
  WindowInfo *info,
             *listEnd;
#ifdef USEWEAKREFS
  nsresult    rv;
  PRBool      found;
#endif

  if (!aWindow)
    return NS_ERROR_INVALID_ARG;
  
  info = mOldestWindow;
  listEnd = 0;
#ifdef USEWEAKREFS
  rv = NS_OK;
  found = PR_FALSE;
  while (info != listEnd && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMWindow> infoWindow(do_QueryReferent(info->mWindow));
    if (!infoWindow)
      rv = RemoveWindow(info);
    else if (infoWindow.get() == aWindow) {
      found = PR_TRUE;
      rv = RemoveWindow(info);
    }
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return found ? rv : NS_ERROR_INVALID_ARG;
#else
  while (info != listEnd) {
    if (info->mWindow == aWindow)
      return RemoveWindow(info);
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  NS_WARNING("requested removal of nonexistent window\n");
  return NS_ERROR_INVALID_ARG;
#endif
}

nsresult nsWindowWatcher::RemoveWindow(WindowInfo *inInfo)
{
  PRInt32  ctr,
           count = mEnumeratorList.Count();
  nsresult rv;

  {
    // notify the enumerators
    nsAutoLock lock(mListLock);
    for (ctr = 0; ctr < count; ++ctr) 
      ((nsWindowEnumerator*)mEnumeratorList[ctr])->WindowRemoved(inInfo);

    // remove the element from the list
    if (inInfo == mOldestWindow)
      mOldestWindow = inInfo->mYounger == mOldestWindow ? 0 : inInfo->mYounger;
    inInfo->Unlink();
  }

  // a window being removed from us signifies a newly closed window.
  // send notifications.
  nsCOMPtr<nsIObserverService> os(do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv));
  if (os) {
#ifdef USEWEAKREFS
    nsCOMPtr<nsISupports> domwin(do_QueryReferent(inInfo->mWindow));
    if (domwin)
      rv = os->Notify(domwin, NOTIFICATION_CLOSED.get(), 0);
    // else bummer. since the window is gone, there's nothing to notify with.
#else
    nsCOMPtr<nsISupports> domwin(do_QueryInterface(inInfo->mWindow));
    rv = os->Notify(domwin, NOTIFICATION_CLOSED.get(), 0);
#endif
  }

  delete inInfo;
  return NS_OK;
}

PRBool
nsWindowWatcher::AddEnumerator(nsWindowEnumerator* inEnumerator)
{
  // (requires a lock; assumes it's called by someone holding the lock)
  return mEnumeratorList.AppendElement(inEnumerator);
}

PRBool
nsWindowWatcher::RemoveEnumerator(nsWindowEnumerator* inEnumerator)
{
  // (requires a lock; assumes it's called by someone holding the lock)
  return mEnumeratorList.RemoveElement(inEnumerator);
}

nsresult
nsWindowWatcher::URIfromURL(const char *aURL,
                            nsIDOMWindow *aParent,
                            nsIURI **aURI)
{
  // build the URI relative to the parent window, if possible
  nsCOMPtr<nsIURI>      baseURI;
  if (aParent) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    aParent->GetDocument(getter_AddRefs(domDoc));
    if (domDoc) {
      nsCOMPtr<nsIDocument> doc;
      doc = do_QueryInterface(domDoc);
      if (doc)
        baseURI = dont_AddRef(doc->GetDocumentURL());
    }
  }

  /* make URI; if absoluteURL is relative (or otherwise bogus) this will
      fail. (don't call this function with no context window and
      a relative URL!) */
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
      warn.AssignWithConversion("Illegal character in window name ");
      warn.Append(aName);
      char *cp = warn.ToNewCString();
      NS_WARNING(cp);
      nsCRT::free(cp);
      break;
    }
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
                                               PRBool aDialog)
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
  chromeFlags |= WinHasOption(aFeatures, "minimizable", 0, &presenceFlag)
                 ? nsIWebBrowserChrome::CHROME_WINDOW_MIN : 0;

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
    securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
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

PRInt32
nsWindowWatcher::WinHasOption(const char *aOptions, const char *aName,
                              PRInt32 aDefault, PRBool *aPresenceFlag)
{
  if (!aOptions)
    return 0;

  char *comma, *equal;
  PRInt32 found = 0;

  while (PR_TRUE) {
    while (nsCRT::IsAsciiSpace(*aOptions))
      aOptions++;

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

  *aFoundItem = 0;

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

void
nsWindowWatcher::SizeOpenedDocShellItem(nsIDocShellTreeItem *aDocShellItem,
                                        nsIDOMWindow *aParent,
                                        const char *aFeatures,
                                        PRUint32 aChromeFlags)
{
  PRInt32 chromeX = 0, chromeY = 0, chromeCX = 100, chromeCY = 100;
  PRInt32 contentCX = 100, contentCY = 100;

  // Use sizes from the parent window, if any, as our default
  if (aParent) {
    nsCOMPtr<nsIDocShellTreeItem> item;

    GetWindowTreeItem(aParent, getter_AddRefs(item));
    if (item) {
      // if we are content, we may need the content sizes
      nsCOMPtr<nsIBaseWindow> win(do_QueryInterface(item));
      win->GetSize(&contentCX, &contentCY);

      // now the main window
      nsCOMPtr<nsIDocShellTreeOwner> owner;
      item->GetTreeOwner(getter_AddRefs(owner));
      if (owner) {
        nsCOMPtr<nsIBaseWindow> basewin(do_QueryInterface(owner));
        if (basewin)
          basewin->GetPositionAndSize(&chromeX, &chromeY,
                                      &chromeCX, &chromeCY);
      }
    }
  }

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
  PRBool enabled = PR_FALSE;
  PRInt32 screenWidth = 0, screenHeight = 0;
  PRInt32 winWidth, winHeight;

  // Check security state for use in determing window dimensions
  nsCOMPtr<nsIScriptSecurityManager>
    securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));
  if (securityManager) {
    res = securityManager->IsCapabilityEnabled("UniversalBrowserWrite", &enabled);
    if (NS_FAILED(res))
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
      // XXX This should use nsIScreenManager once it's fully fleshed out.
      nsCOMPtr<nsIDOMScreen> screen;
      if (aParent) {
        nsCOMPtr<nsIDOMWindowInternal> intparent(do_QueryInterface(aParent));
        if (intparent)
          intparent->GetScreen(getter_AddRefs(screen));
      } else {
        // XXX hmmph. try the new window.
      }
      if (screen) {
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
  if (treeOwnerAsWin) {
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
  }
}

// attach the given array of JS values to the given window, as a property array
// named "arguments"
void
nsWindowWatcher::AttachArguments(nsIDOMWindow *aWindow,
                                 PRUint32 argc, jsval *argv)
{
  if (argc == 0)
    return;

  // copy the extra parameters into a JS Array and attach it
  nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_QueryInterface(aWindow));
  if (scriptGlobal) {
    nsCOMPtr<nsIScriptContext> scriptContext;
    scriptGlobal->GetContext(getter_AddRefs(scriptContext));
    if (scriptContext) {
      JSContext *cx;
      cx = (JSContext *) scriptContext->GetNativeContext();
      nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(aWindow));
      if (owner) {
        JSObject *scriptObject;
        owner->GetScriptObject(scriptContext, (void **) &scriptObject);
        JSObject *args;
        args = ::JS_NewArrayObject(cx, argc, argv);
        if (args) {
          jsval argsVal = OBJECT_TO_JSVAL(args);
          // ::JS_DefineProperty(cx, scriptObject, "arguments",
          // argsVal, NULL, NULL, JSPROP_PERMANENT);
          ::JS_SetProperty(cx, scriptObject, "arguments", &argsVal);
        }
      }
    }
  }
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

  JSContext           *cx = GetExtantJSContext(aWindow);
  JSContextAutoPopper  contextGuard;

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
    case nsISupportsPrimitive::TYPE_STRING : {
      nsCOMPtr<nsISupportsString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      char *data;

      p->GetData(&data);

      JSString *str = ::JS_NewString(cx, data, nsCRT::strlen(data));
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      *aArgv = STRING_TO_JSVAL(str);

      break;
    }
    case nsISupportsPrimitive::TYPE_WSTRING : {
      nsCOMPtr<nsISupportsWString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRUnichar *data;

      p->GetData(&data);

      // cast is probably safe since wchar_t and jschar are expected
      // to be equivalent; both unsigned 16-bit entities
      JSString *str = ::JS_NewUCString(cx, NS_REINTERPRET_CAST(jschar *, data),
                          nsCRT::strlen(data));
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
    nsCOMPtr<nsIDocShell> docshell;
    sgo->GetDocShell(getter_AddRefs(docshell));
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
nsWindowWatcher::GetExtantJSContext(nsIDOMWindow *aWindow)
{
  JSContext *cx;

  // given a window, we'll use its
  cx = 0;
  if (aWindow) {
    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aWindow));
    if (sgo) {
      nsCOMPtr<nsIScriptContext> scx;
      sgo->GetContext(getter_AddRefs(scx));
      if (scx)
        cx = (JSContext *) scx->GetNativeContext();
    }
    /* (off-topic note:) the nsIScriptContext can be retrieved by
    nsCOMPtr<nsIScriptContext> scx;
    nsJSUtils::nsGetDynamicScriptContext(cx, getter_AddRefs(scx));
    */
  }

  // still no JSContext? try pulling one from the stack.
  if (!cx) {
    nsCOMPtr<nsIThreadJSContextStack> cxStack(do_GetService(sJSStackContractID));
    if (cxStack)
      cxStack->Peek(&cx);
    /* We explicitly do not use GetSafeJSContext to force one if Peek
       finds nothing. That's done, if necessary, by a helper class which
       knows how to clean up after itself.
    */
  }

  return cx;
}

JSObject *
nsWindowWatcher::GetWindowScriptObject(nsIDOMWindow *inWindow)
{
  JSObject *object = 0;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_QueryInterface(inWindow));
  if (scriptGlobal) {
    nsCOMPtr<nsIScriptContext> scriptContext;
    scriptGlobal->GetContext(getter_AddRefs(scriptContext));

    nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(inWindow));
    if (owner)
      owner->GetScriptObject(scriptContext, (void **) &object);
  }
  return object;
}

