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

#include "nsAutoLock.h"
#include "nsWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsIGenericFactory.h"
#include "nsIObserverService.h"
#ifdef USEWEAKREFS
#include "nsIWeakReference.h"
#endif

#define NOTIFICATION_OPENED NS_LITERAL_STRING("domwindowopened")
#define NOTIFICATION_CLOSED NS_LITERAL_STRING("domwindowclosed")

/****************************************************************
 ************************* winfowInfo ***************************
 ****************************************************************/

class nsWindowWatcher;

struct WindowInfo {

#ifdef USEWEAKREFS
  WindowInfo(nsIDOMWindow* inWindow) {
    mWindow = getter_AddRefs(NS_GetWeakReference(inWindow));
#else
  WindowInfo(nsIDOMWindow* inWindow) :
    mWindow(inWindow) {
#endif
    ReferenceSelf();
  }
  ~WindowInfo() {}

  void InsertAfter(WindowInfo *inOlder);
  void Unlink();
  void ReferenceSelf();

#ifdef USEWEAKREFS
  nsCOMPtr<nsIWeakReference> mWindow;
#else
  nsCOMPtr<nsIDOMWindow>     mWindow;
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
  ~nsWindowEnumerator();
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
                            nsIDOMWindow **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
    rv = os->AddObserver(aObserver, NOTIFICATION_OPENED);
    if (NS_SUCCEEDED(rv))
      rv = os->AddObserver(aObserver, NOTIFICATION_CLOSED);
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
    os->RemoveObserver(aObserver, NOTIFICATION_OPENED);
    os->RemoveObserver(aObserver, NOTIFICATION_CLOSED);
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
nsWindowWatcher::GetActiveWindow(nsIDOMWindow * *aActiveWindow)
{
  if (!aActiveWindow)
    return NS_ERROR_INVALID_ARG;

  *aActiveWindow = mActiveWindow;
  NS_IF_ADDREF(mActiveWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::SetActiveWindow(nsIDOMWindow * aActiveWindow)
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
    rv = os->Notify(domwin, NOTIFICATION_OPENED, 0);
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
    if (info->mWindow.get() == aWindow)
      return RemoveWindow(info);
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return NS_ERROR_INVALID_ARG;
#endif
}

NS_IMETHODIMP nsWindowWatcher::RemoveWindow(WindowInfo *inInfo)
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
      rv = os->Notify(domwin, NOTIFICATION_CLOSED, 0);
    // else bummer. since the window is gone, there's nothing to notify with.
#else
    nsCOMPtr<nsISupports> domwin(do_QueryInterface(inInfo->mWindow));
    rv = os->Notify(domwin, NOTIFICATION_CLOSED, 0);
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

