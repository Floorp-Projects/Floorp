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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick McManus <mcmanus@ducksong.com>
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

#ifdef MOZ_IPC
#include "base/basictypes.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoChild.h"
#include "nsURLHelper.h"
#endif

#include "nsHTMLDNSPrefetch.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsNetUtil.h"

#include "nsIDNSListener.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgress.h"
#include "nsCURILoader.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsICancelable.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "nsIObserverService.h"
#include "mozilla/dom/Link.h"

using namespace mozilla::dom;
using namespace mozilla::net;

static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
PRBool sDisablePrefetchHTTPSPref;
static PRBool sInitialized = PR_FALSE;
static nsIDNSService *sDNSService = nsnull;
static nsHTMLDNSPrefetch::nsDeferrals *sPrefetches = nsnull;
static nsHTMLDNSPrefetch::nsListener *sDNSListener = nsnull;

nsresult
nsHTMLDNSPrefetch::Initialize()
{
  if (sInitialized) {
    NS_WARNING("Initialize() called twice");
    return NS_OK;
  }
  
  sPrefetches = new nsHTMLDNSPrefetch::nsDeferrals();
  if (!sPrefetches)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(sPrefetches);

  sDNSListener = new nsHTMLDNSPrefetch::nsListener();
  if (!sDNSListener) {
    NS_IF_RELEASE(sPrefetches);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(sDNSListener);

  sPrefetches->Activate();

  nsContentUtils::AddBoolPrefVarCache("network.dns.disablePrefetchFromHTTPS", 
                                      &sDisablePrefetchHTTPSPref);
  
  // Default is false, so we need an explicit call to prime the cache.
  sDisablePrefetchHTTPSPref = 
    nsContentUtils::GetBoolPref("network.dns.disablePrefetchFromHTTPS", PR_TRUE);
  
  NS_IF_RELEASE(sDNSService);
  nsresult rv;
  rv = CallGetService(kDNSServiceCID, &sDNSService);
  if (NS_FAILED(rv)) return rv;
  
#ifdef MOZ_IPC
  if (IsNeckoChild())
    NeckoChild::InitNeckoChild();
#endif

  sInitialized = PR_TRUE;
  return NS_OK;
}

nsresult
nsHTMLDNSPrefetch::Shutdown()
{
  if (!sInitialized) {
    NS_WARNING("Not Initialized");
    return NS_OK;
  }
  sInitialized = PR_FALSE;
  NS_IF_RELEASE(sDNSService);
  NS_IF_RELEASE(sPrefetches);
  NS_IF_RELEASE(sDNSListener);
  
  return NS_OK;
}

PRBool
nsHTMLDNSPrefetch::IsAllowed (nsIDocument *aDocument)
{
  // There is no need to do prefetch on non UI scenarios such as XMLHttpRequest.
  return aDocument->IsDNSPrefetchAllowed() && aDocument->GetWindow();
}

nsresult
nsHTMLDNSPrefetch::Prefetch(Link *aElement, PRUint16 flags)
{
#ifdef MOZ_IPC
  if (IsNeckoChild()) {
    // Instead of transporting the Link object to the other process
    // we are using the hostname based function here, too. Compared to the 
    // IPC the performance hit should be negligible.
    nsAutoString hostname;
    nsresult rv = aElement->GetHostname(hostname);
    NS_ENSURE_SUCCESS(rv,rv);

    return Prefetch(hostname, flags);
  }
#endif

  if (!(sInitialized && sPrefetches && sDNSService && sDNSListener))
    return NS_ERROR_NOT_AVAILABLE;

  return sPrefetches->Add(flags, aElement);
}

nsresult
nsHTMLDNSPrefetch::PrefetchLow(Link *aElement)
{
  return Prefetch(aElement, nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult
nsHTMLDNSPrefetch::PrefetchMedium(Link *aElement)
{
  return Prefetch(aElement, nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult
nsHTMLDNSPrefetch::PrefetchHigh(Link *aElement)
{
  return Prefetch(aElement, 0);
}

nsresult
nsHTMLDNSPrefetch::Prefetch(nsAString &hostname, PRUint16 flags)
{
#ifdef MOZ_IPC
  if (IsNeckoChild()) {
    // We need to check IsEmpty() because net_IsValidHostName()
    // considers empty strings to be valid hostnames
    if (!hostname.IsEmpty() &&
        net_IsValidHostName(NS_ConvertUTF16toUTF8(hostname))) {
      gNeckoChild->SendHTMLDNSPrefetch(nsAutoString(hostname), flags);
    }
    return NS_OK;
  }
#endif

  if (!(sInitialized && sDNSService && sPrefetches && sDNSListener))
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsICancelable> tmpOutstanding;
  return sDNSService->AsyncResolve(NS_ConvertUTF16toUTF8(hostname), flags | nsIDNSService::RESOLVE_SPECULATE,
                                   sDNSListener, nsnull, getter_AddRefs(tmpOutstanding));
}

nsresult
nsHTMLDNSPrefetch::PrefetchLow(nsAString &hostname)
{
  return Prefetch(hostname, nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult
nsHTMLDNSPrefetch::PrefetchMedium(nsAString &hostname)
{
  return Prefetch(hostname, nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult
nsHTMLDNSPrefetch::PrefetchHigh(nsAString &hostname)
{
  return Prefetch(hostname, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS1(nsHTMLDNSPrefetch::nsListener,
                              nsIDNSListener)

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsListener::OnLookupComplete(nsICancelable *request,
                                              nsIDNSRecord  *rec,
                                              nsresult       status)
{
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

nsHTMLDNSPrefetch::nsDeferrals::nsDeferrals()
  : mHead(0),
    mTail(0),
    mActiveLoaderCount(0),
    mTimerArmed(PR_FALSE)
{
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
}

nsHTMLDNSPrefetch::nsDeferrals::~nsDeferrals()
{
  if (mTimerArmed) {
    mTimerArmed = PR_FALSE;
    mTimer->Cancel();
  }

  Flush();
}

NS_IMPL_ISUPPORTS3(nsHTMLDNSPrefetch::nsDeferrals,
                   nsIWebProgressListener,
                   nsISupportsWeakReference,
                   nsIObserver)

void
nsHTMLDNSPrefetch::nsDeferrals::Flush()
{
  while (mHead != mTail) {
    mEntries[mTail].mElement = nsnull;
    mTail = (mTail + 1) & sMaxDeferredMask;
  }
}

nsresult
nsHTMLDNSPrefetch::nsDeferrals::Add(PRUint16 flags, Link *aElement)
{
  // The FIFO has no lock, so it can only be accessed on main thread
  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::Add must be on main thread");

  if (((mHead + 1) & sMaxDeferredMask) == mTail)
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
    
  mEntries[mHead].mFlags = flags;
  mEntries[mHead].mElement = do_GetWeakReference(aElement);
  mHead = (mHead + 1) & sMaxDeferredMask;

  if (!mActiveLoaderCount && !mTimerArmed && mTimer) {
    mTimerArmed = PR_TRUE;
    mTimer->InitWithFuncCallback(Tick, this, 2000, nsITimer::TYPE_ONE_SHOT);
  }
  
  return NS_OK;
}

void
nsHTMLDNSPrefetch::nsDeferrals::SubmitQueue()
{
  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::SubmitQueue must be on main thread");
  nsCString hostName;
  if (!sDNSService) return;

  while (mHead != mTail) {
    nsCOMPtr<nsIContent> content = do_QueryReferent(mEntries[mTail].mElement);
    if (content && content->GetOwnerDoc()) {
      nsCOMPtr<Link> link = do_QueryInterface(content);
      nsCOMPtr<nsIURI> hrefURI(link ? link->GetURI() : nsnull);
      if (hrefURI)
        hrefURI->GetAsciiHost(hostName);

      if (!hostName.IsEmpty()) {
        nsCOMPtr<nsICancelable> tmpOutstanding;

        sDNSService->AsyncResolve(hostName, 
                                  mEntries[mTail].mFlags | nsIDNSService::RESOLVE_SPECULATE,
                                  sDNSListener, nsnull, getter_AddRefs(tmpOutstanding));
      }
    }
    
    mEntries[mTail].mElement = nsnull;
    mTail = (mTail + 1) & sMaxDeferredMask;
  }
  
  if (mTimerArmed) {
    mTimerArmed = PR_FALSE;
    mTimer->Cancel();
  }
}

void
nsHTMLDNSPrefetch::nsDeferrals::Activate()
{
  // Register as an observer for the document loader  
  nsCOMPtr<nsIWebProgress> progress = 
    do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
  if (progress)
    progress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);

  // Register as an observer for xpcom shutdown events so we can drop any element refs
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService)
    observerService->AddObserver(this, "xpcom-shutdown", PR_TRUE);
}

// nsITimer related method

void 
nsHTMLDNSPrefetch::nsDeferrals::Tick(nsITimer *aTimer, void *aClosure)
{
  nsHTMLDNSPrefetch::nsDeferrals *self = (nsHTMLDNSPrefetch::nsDeferrals *) aClosure;

  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::Tick must be on main thread");
  NS_ASSERTION(self->mTimerArmed, "Timer is not armed");
  
  self->mTimerArmed = PR_FALSE;

  // If the queue is not submitted here because there are outstanding pages being loaded,
  // there is no need to rearm the timer as the queue will be submtited when those 
  // loads complete.
  if (!self->mActiveLoaderCount) 
    self->SubmitQueue();
}

//////////// nsIWebProgressListener methods

NS_IMETHODIMP 
nsHTMLDNSPrefetch::nsDeferrals::OnStateChange(nsIWebProgress* aWebProgress, 
                                              nsIRequest *aRequest, 
                                              PRUint32 progressStateFlags, 
                                              nsresult aStatus)
{
  // The FIFO has no lock, so it can only be accessed on main thread
  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::OnStateChange must be on main thread");
  
  if (progressStateFlags & STATE_IS_DOCUMENT) {
    if (progressStateFlags & STATE_STOP) {

      // Initialization may have missed a STATE_START notification, so do
      // not go negative
      if (mActiveLoaderCount)
        mActiveLoaderCount--;

      if (!mActiveLoaderCount)
        SubmitQueue();
    }
    else if (progressStateFlags & STATE_START)
      mActiveLoaderCount++;
  }
            
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnProgressChange(nsIWebProgress *aProgress,
                                                 nsIRequest *aRequest, 
                                                 PRInt32 curSelfProgress, 
                                                 PRInt32 maxSelfProgress, 
                                                 PRInt32 curTotalProgress, 
                                                 PRInt32 maxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLDNSPrefetch::nsDeferrals::OnStatusChange(nsIWebProgress* aWebProgress,
                                               nsIRequest* aRequest,
                                               nsresult aStatus,
                                               const PRUnichar* aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLDNSPrefetch::nsDeferrals::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                                 nsIRequest *aRequest, 
                                                 PRUint32 state)
{
  return NS_OK;
}

//////////// nsIObserver method

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::Observe(nsISupports *subject,
                                        const char *topic,
                                        const PRUnichar *data)
{
  if (!strcmp(topic, "xpcom-shutdown"))
    Flush();
  
  return NS_OK;
}
