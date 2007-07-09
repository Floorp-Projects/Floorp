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
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
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

#include "nsDOMOfflineLoadStatusList.h"
#include "nsDOMClassInfo.h"
#include "nsIMutableArray.h"
#include "nsCPrefetchService.h"
#include "nsIObserverService.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsIContent.h"
#include "nsNetCID.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsContentUtils.h"
#include "nsDOMError.h"
#include "nsNetUtil.h"
#include "nsAutoPtr.h"

#define LOADREQUESTED_STR "loadrequested"
#define LOADCOMPLETED_STR "loadcompleted"

//
// nsDOMOfflineLoadStatus
//

// XXX
//
// nsDOMOfflineLoadStatusList has an array wrapper in its classinfo struct
// (LoadStatusList) that exposes nsDOMOfflineLoadStatusList::Item() as
// array items.
//
// For scripts to recognize these as nsIDOMLoadStatus objects, I needed
// to add a LoadStatus classinfo.
//
// Because the prefetch service is outside the dom module, it can't
// actually use the LoadStatus classinfo.
//
// nsDOMOfflineLoadStatus is just a simple wrapper around the
// nsIDOMLoadStatus objects returned by the prefetch service that adds the
// LoadStatus classinfo implementation.
//
// Is there a better way around this?

class nsDOMOfflineLoadStatus : public nsIDOMLoadStatus
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMLOADSTATUS

  nsDOMOfflineLoadStatus(nsIDOMLoadStatus *status);
  virtual ~nsDOMOfflineLoadStatus();

  const nsIDOMLoadStatus *Implementation() { return mStatus; }
private:
  nsCOMPtr<nsIDOMLoadStatus> mStatus;
};

NS_INTERFACE_MAP_BEGIN(nsDOMOfflineLoadStatus)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMLoadStatus)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadStatus)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(LoadStatus)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMOfflineLoadStatus)
NS_IMPL_RELEASE(nsDOMOfflineLoadStatus)

nsDOMOfflineLoadStatus::nsDOMOfflineLoadStatus(nsIDOMLoadStatus *aStatus)
  : mStatus(aStatus)
{
}

nsDOMOfflineLoadStatus::~nsDOMOfflineLoadStatus()
{
}

NS_IMETHODIMP
nsDOMOfflineLoadStatus::GetSource(nsIDOMNode **aSource)
{
  return mStatus->GetSource(aSource);
}

NS_IMETHODIMP
nsDOMOfflineLoadStatus::GetUri(nsAString &aURI)
{
  return mStatus->GetUri(aURI);
}

NS_IMETHODIMP
nsDOMOfflineLoadStatus::GetTotalSize(PRInt32 *aTotalSize)
{
  return mStatus->GetTotalSize(aTotalSize);
}

NS_IMETHODIMP
nsDOMOfflineLoadStatus::GetLoadedSize(PRInt32 *aLoadedSize)
{
  return mStatus->GetLoadedSize(aLoadedSize);
}

NS_IMETHODIMP
nsDOMOfflineLoadStatus::GetReadyState(PRUint16 *aReadyState)
{
  return mStatus->GetReadyState(aReadyState);
}

NS_IMETHODIMP
nsDOMOfflineLoadStatus::GetStatus(PRUint16 *aStatus)
{
  return mStatus->GetStatus(aStatus);
}

//
// nsDOMOfflineLoadStatusList
//

NS_INTERFACE_MAP_BEGIN(nsDOMOfflineLoadStatusList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMLoadStatusList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadStatusList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(LoadStatusList)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMOfflineLoadStatusList)
NS_IMPL_RELEASE(nsDOMOfflineLoadStatusList)

nsDOMOfflineLoadStatusList::nsDOMOfflineLoadStatusList(nsIURI *aURI)
  : mInitialized(PR_FALSE)
  , mURI(aURI)
{
}

nsDOMOfflineLoadStatusList::~nsDOMOfflineLoadStatusList()
{
  mLoadRequestedEventListeners.Clear();
  mLoadCompletedEventListeners.Clear();
}

nsresult
nsDOMOfflineLoadStatusList::Init()
{
  if (mInitialized) {
    return NS_OK;
  }

  mInitialized = PR_TRUE;

  nsresult rv = mURI->GetHostPort(mHostPort);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheService> serv =
    do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheSession> session;
  rv = serv->CreateSession("HTTP-offline",
                           nsICache::STORE_OFFLINE,
                           nsICache::STREAM_BASED,
                           getter_AddRefs(session));
  NS_ENSURE_SUCCESS(rv, rv);

  mCacheSession = do_QueryInterface(session, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the current list of loads from the prefetch queue
  nsCOMPtr<nsIPrefetchService> prefetchService =
    do_GetService(NS_PREFETCHSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = prefetchService->EnumerateQueue(PR_FALSE, PR_TRUE, getter_AddRefs(e));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool more;
  while (NS_SUCCEEDED(rv = e->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsIDOMLoadStatus> status;
    rv = e->GetNext(getter_AddRefs(status));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool shouldInclude;
    rv = ShouldInclude(status, &shouldInclude);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!shouldInclude) {
      continue;
    }

    nsDOMOfflineLoadStatus *wrapper = new nsDOMOfflineLoadStatus(status);
    if (!wrapper) return NS_ERROR_OUT_OF_MEMORY;

    mItems.AppendObject(wrapper);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // watch for changes in the prefetch queue
  nsCOMPtr<nsIObserverService> observerServ =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = observerServ->AddObserver(this, "offline-load-requested", PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = observerServ->AddObserver(this, "offline-load-completed", PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMOfflineLoadStatusList::ShouldInclude(nsIDOMLoadStatus *aStatus,
                                          PRBool *aShouldInclude)
{
  *aShouldInclude = PR_FALSE;

  nsAutoString uriStr;
  nsresult rv = aStatus->GetUri(uriStr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), uriStr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString hostport;
  rv = uri->GetHostPort(hostport);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hostport != mHostPort)
    return NS_OK;

  // Check that the URL is owned by this domain
  nsCAutoString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> source;
  rv = aStatus->GetSource(getter_AddRefs(source));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString ownerURI;
  if (source) {
    // Came from a <link> element, check that it's owned by this URI
    rv = mURI->GetSpec(ownerURI);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Didn't come from a <link> element, check that it's owned by
    // resource list (no owner URI)
    ownerURI.Truncate();
  }

  PRBool owned;
  rv = mCacheSession->KeyIsOwned(mHostPort, ownerURI, spec, &owned);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!owned) {
    return NS_OK;
  }

  *aShouldInclude = PR_TRUE;

  return NS_OK;
}

nsIDOMLoadStatus *
nsDOMOfflineLoadStatusList::FindWrapper(nsIDOMLoadStatus *aStatus,
                                        PRUint32 *index)
{
  for (int i = 0; i < mItems.Count(); i++) {
    nsDOMOfflineLoadStatus *item = NS_STATIC_CAST(nsDOMOfflineLoadStatus*,
                                                  mItems[i]);
    if (item->Implementation() == aStatus) {
      *index = i;
      return mItems[i];
    }
  }

  return nsnull;
}

//
// nsDOMOfflineLoadStatusList::nsIDOMLoadStatusList
//

NS_IMETHODIMP
nsDOMOfflineLoadStatusList::GetLength(PRUint32 *aLength)
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  *aLength = mItems.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineLoadStatusList::Item(PRUint32 aItem, nsIDOMLoadStatus **aStatus)
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  if ((PRInt32)aItem > mItems.Count()) return NS_ERROR_DOM_INDEX_SIZE_ERR;

  NS_ADDREF(*aStatus = mItems[aItem]);

  return NS_OK;
}

//
// nsDOMOfflineLoadStatusList::nsIDOMEventTarget
//

static nsIScriptContext *
GetCurrentContext()
{
  // Get JSContext from stack.
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");

  if (!stack) {
    return nsnull;
  }

  JSContext *cx;

  if (NS_FAILED(stack->Peek(&cx)) || !cx) {
    return nsnull;
  }

  return GetScriptContextFromJSContext(cx);
}

NS_IMETHODIMP
nsDOMOfflineLoadStatusList::AddEventListener(const nsAString& aType,
                                             nsIDOMEventListener *aListener,
                                             PRBool aUseCapture)
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_ARG(aListener);

  nsCOMArray<nsIDOMEventListener> *array;

#define IMPL_ADD_LISTENER(_type, _member)    \
  if (aType.EqualsLiteral(_type)) {           \
    array = &(_member);                      \
  } else

  IMPL_ADD_LISTENER(LOADREQUESTED_STR, mLoadRequestedEventListeners)
  IMPL_ADD_LISTENER(LOADCOMPLETED_STR, mLoadCompletedEventListeners)
  {
    return NS_ERROR_INVALID_ARG;
  }

  array->AppendObject(aListener);
  mScriptContext = GetCurrentContext();
#undef IMPL_ADD_LISTENER

  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineLoadStatusList::RemoveEventListener(const nsAString &aType,
                                                nsIDOMEventListener *aListener,
                                                PRBool aUseCapture)
{
  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_ARG(aListener);

  nsCOMArray<nsIDOMEventListener> *array;

#define IMPL_REMOVE_LISTENER(_type, _member)  \
  if (aType.EqualsLiteral(_type)) {            \
    array = &(_member);                       \
  } else

  IMPL_REMOVE_LISTENER(LOADREQUESTED_STR, mLoadRequestedEventListeners)
  IMPL_REMOVE_LISTENER(LOADCOMPLETED_STR, mLoadCompletedEventListeners)
  {
    return NS_ERROR_INVALID_ARG;
  }

  // Allow a caller to remove O(N^2) behavior by removing end-to-start.
  for (PRUint32 i = array->Count() - 1; i != PRUint32(-1); --i) {
    if (array->ObjectAt(i) == aListener) {
      array->RemoveObjectAt(i);
      break;
    }
  }

#undef IMPL_ADD_LISTENER

  return NS_OK;
}

NS_IMETHODIMP
nsDOMOfflineLoadStatusList::DispatchEvent(nsIDOMEvent *evt, PRBool *_retval)
{
  // Ignored

  return NS_OK;
}

void
nsDOMOfflineLoadStatusList::NotifyEventListeners(const nsCOMArray<nsIDOMEventListener>& aListeners,
                                                 nsIDOMEvent* aEvent)
{
  // XXXbz wouldn't it be easier to just have an actual nsEventListenerManager
  // to work with or something?  I feel like we're duplicating code here...
  //
  // (and this was duplicated from XMLHttpRequest)
  if (!aEvent)
    return;

  nsCOMPtr<nsIJSContextStack> stack;
  JSContext *cx = nsnull;

  if (mScriptContext) {
    stack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");

    if (stack) {
      cx = (JSContext *)mScriptContext->GetNativeContext();

      if (cx) {
        stack->Push(cx);
      }
    }
  }

  PRInt32 count = aListeners.Count();
  for (PRInt32 index = 0; index < count; ++index) {
    nsIDOMEventListener* listener = aListeners[index];

    if (listener) {
      listener->HandleEvent(aEvent);
    }
  }

  if (cx) {
    stack->Pop(&cx);
  }
}

nsresult
nsDOMOfflineLoadStatusList::SendLoadEvent(const nsAString &aEventName,
                                          const nsCOMArray<nsIDOMEventListener> &aListeners,
                                          nsIDOMLoadStatus *aStatus)
{
  NS_ENSURE_ARG(aStatus);

  if (aListeners.Count() == 0) return NS_OK;

  nsRefPtr<nsDOMLoadStatusEvent> event = new nsDOMLoadStatusEvent(aEventName,
                                                                  aStatus);
  if (!event) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = event->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  NotifyEventListeners(aListeners,
                       NS_STATIC_CAST(nsIDOMLoadStatusEvent*, event));

  return NS_OK;
}

//
// nsDOMLoadStatusList::nsIObserver
//
NS_IMETHODIMP
nsDOMOfflineLoadStatusList::Observe(nsISupports *aSubject,
                                    const char *aTopic,
                                    const PRUnichar *aData)
{
  nsresult rv;
  if (!strcmp(aTopic, "offline-load-requested")) {
    nsCOMPtr<nsIDOMLoadStatus> status = do_QueryInterface(aSubject);
    if (status) {
      PRBool shouldInclude;
      rv = ShouldInclude(status, &shouldInclude);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!shouldInclude) return NS_OK;

      nsDOMOfflineLoadStatus *wrapper = new nsDOMOfflineLoadStatus(status);
      if (!wrapper) return NS_ERROR_OUT_OF_MEMORY;

      mItems.AppendObject(wrapper);

      rv = SendLoadEvent(NS_LITERAL_STRING(LOADREQUESTED_STR),
                         mLoadRequestedEventListeners,
                         wrapper);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else if (!strcmp(aTopic, "offline-load-completed")) {
    nsCOMPtr<nsIDOMLoadStatus> status = do_QueryInterface(aSubject);
    if (status) {
      PRUint32 index;
      nsCOMPtr<nsIDOMLoadStatus> wrapper = FindWrapper(status, &index);
      if (wrapper) {
        mItems.RemoveObjectAt(index);

        rv = SendLoadEvent(NS_LITERAL_STRING(LOADCOMPLETED_STR),
                           mLoadCompletedEventListeners,
                           wrapper);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

//
// nsDOMLoadStatusEvent
//

NS_INTERFACE_MAP_BEGIN(nsDOMLoadStatusEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadStatusEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(LoadStatusEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMLoadStatusEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMLoadStatusEvent, nsDOMEvent)

nsresult
nsDOMLoadStatusEvent::Init()
{
  nsresult rv = InitEvent(mEventName, PR_TRUE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // This init method is only called by native code, so set the
  // trusted flag there.
  SetTrusted(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMLoadStatusEvent::InitLoadStatusEvent(const nsAString& aTypeArg,
                                          PRBool aCanBubbleArg,
                                          PRBool aCancelableArg,
                                          nsIDOMLoadStatus* aStatusArg)
{
  nsresult rv = InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mStatus = aStatusArg;

  return NS_OK;
}


NS_IMETHODIMP
nsDOMLoadStatusEvent::InitLoadStatusEventNS(const nsAString& aNamespaceURIArg,
                                            const nsAString& aTypeArg,
                                            PRBool aCanBubbleArg,
                                            PRBool aCancelableArg,
                                            nsIDOMLoadStatus* aStatusArg)
{
  // XXX: Figure out what to do with aNamespaceURIArg here!
  nsresult rv = InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mStatus = aStatusArg;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMLoadStatusEvent::GetStatus(nsIDOMLoadStatus **aStatus)
{
  NS_ADDREF(*aStatus = mStatus);

  return NS_OK;
}
