/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoChild.h"
#include "nsURLHelper.h"

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
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "nsIObserverService.h"
#include "mozilla/dom/Link.h"

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::net;

static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
bool sDisablePrefetchHTTPSPref;
static bool sInitialized = false;
static nsIDNSService *sDNSService = nullptr;
static nsHTMLDNSPrefetch::nsDeferrals *sPrefetches = nullptr;
static nsHTMLDNSPrefetch::nsListener *sDNSListener = nullptr;

nsresult
nsHTMLDNSPrefetch::Initialize()
{
  if (sInitialized) {
    NS_WARNING("Initialize() called twice");
    return NS_OK;
  }
  
  sPrefetches = new nsHTMLDNSPrefetch::nsDeferrals();
  NS_ADDREF(sPrefetches);

  sDNSListener = new nsHTMLDNSPrefetch::nsListener();
  NS_ADDREF(sDNSListener);

  sPrefetches->Activate();

  Preferences::AddBoolVarCache(&sDisablePrefetchHTTPSPref,
                               "network.dns.disablePrefetchFromHTTPS");
  
  // Default is false, so we need an explicit call to prime the cache.
  sDisablePrefetchHTTPSPref = 
    Preferences::GetBool("network.dns.disablePrefetchFromHTTPS", true);
  
  NS_IF_RELEASE(sDNSService);
  nsresult rv;
  rv = CallGetService(kDNSServiceCID, &sDNSService);
  if (NS_FAILED(rv)) return rv;
  
  if (IsNeckoChild())
    NeckoChild::InitNeckoChild();

  sInitialized = true;
  return NS_OK;
}

nsresult
nsHTMLDNSPrefetch::Shutdown()
{
  if (!sInitialized) {
    NS_WARNING("Not Initialized");
    return NS_OK;
  }
  sInitialized = false;
  NS_IF_RELEASE(sDNSService);
  NS_IF_RELEASE(sPrefetches);
  NS_IF_RELEASE(sDNSListener);
  
  return NS_OK;
}

bool
nsHTMLDNSPrefetch::IsAllowed (nsIDocument *aDocument)
{
  if (NS_IsAppOffline(aDocument->NodePrincipal())) {
    return false;
  }

  // There is no need to do prefetch on non UI scenarios such as XMLHttpRequest.
  return aDocument->IsDNSPrefetchAllowed() && aDocument->GetWindow();
}

nsresult
nsHTMLDNSPrefetch::Prefetch(Link *aElement, uint16_t flags)
{
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
nsHTMLDNSPrefetch::Prefetch(const nsAString &hostname, uint16_t flags)
{
  if (IsNeckoChild()) {
    // We need to check IsEmpty() because net_IsValidHostName()
    // considers empty strings to be valid hostnames
    if (!hostname.IsEmpty() &&
        net_IsValidHostName(NS_ConvertUTF16toUTF8(hostname))) {
      gNeckoChild->SendHTMLDNSPrefetch(nsAutoString(hostname), flags);
    }
    return NS_OK;
  }

  if (!(sInitialized && sDNSService && sPrefetches && sDNSListener))
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsICancelable> tmpOutstanding;
  return sDNSService->AsyncResolve(NS_ConvertUTF16toUTF8(hostname),
                                   flags | nsIDNSService::RESOLVE_SPECULATE,
                                   sDNSListener, nullptr, 
                                   getter_AddRefs(tmpOutstanding));
}

nsresult
nsHTMLDNSPrefetch::PrefetchLow(const nsAString &hostname)
{
  return Prefetch(hostname, nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult
nsHTMLDNSPrefetch::PrefetchMedium(const nsAString &hostname)
{
  return Prefetch(hostname, nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult
nsHTMLDNSPrefetch::PrefetchHigh(const nsAString &hostname)
{
  return Prefetch(hostname, 0);
}

nsresult
nsHTMLDNSPrefetch::CancelPrefetch(Link *aElement,
                                  uint16_t flags,
                                  nsresult aReason)
{
  if (!(sInitialized && sPrefetches && sDNSService && sDNSListener))
    return NS_ERROR_NOT_AVAILABLE;

  nsAutoString hostname;
  ErrorResult rv;
  aElement->GetHostname(hostname, rv);
  return CancelPrefetch(hostname, flags, aReason);
}

nsresult
nsHTMLDNSPrefetch::CancelPrefetch(const nsAString &hostname,
                                  uint16_t flags,
                                  nsresult aReason)
{
  // Forward this request to Necko Parent if we're a child process
  if (IsNeckoChild()) {
    // We need to check IsEmpty() because net_IsValidHostName()
    // considers empty strings to be valid hostnames
    if (!hostname.IsEmpty() &&
        net_IsValidHostName(NS_ConvertUTF16toUTF8(hostname))) {
      gNeckoChild->SendCancelHTMLDNSPrefetch(nsString(hostname), flags,
                                             aReason);
    }
    return NS_OK;
  }

  if (!(sInitialized && sDNSService && sPrefetches && sDNSListener))
    return NS_ERROR_NOT_AVAILABLE;

  // Forward cancellation to DNS service
  return sDNSService->CancelAsyncResolve(NS_ConvertUTF16toUTF8(hostname),
                                         flags
                                         | nsIDNSService::RESOLVE_SPECULATE,
                                         sDNSListener, aReason);
}

nsresult
nsHTMLDNSPrefetch::CancelPrefetchLow(Link *aElement, nsresult aReason)
{
  return CancelPrefetch(aElement, nsIDNSService::RESOLVE_PRIORITY_LOW,
                        aReason);
}

nsresult
nsHTMLDNSPrefetch::CancelPrefetchLow(const nsAString &hostname, nsresult aReason)
{
  return CancelPrefetch(hostname, nsIDNSService::RESOLVE_PRIORITY_LOW,
                        aReason);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsHTMLDNSPrefetch::nsListener,
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
    mTimerArmed(false)
{
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
}

nsHTMLDNSPrefetch::nsDeferrals::~nsDeferrals()
{
  if (mTimerArmed) {
    mTimerArmed = false;
    mTimer->Cancel();
  }

  Flush();
}

NS_IMPL_ISUPPORTS(nsHTMLDNSPrefetch::nsDeferrals,
                  nsIWebProgressListener,
                  nsISupportsWeakReference,
                  nsIObserver)

void
nsHTMLDNSPrefetch::nsDeferrals::Flush()
{
  while (mHead != mTail) {
    mEntries[mTail].mElement = nullptr;
    mTail = (mTail + 1) & sMaxDeferredMask;
  }
}

nsresult
nsHTMLDNSPrefetch::nsDeferrals::Add(uint16_t flags, Link *aElement)
{
  // The FIFO has no lock, so it can only be accessed on main thread
  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::Add must be on main thread");

  aElement->OnDNSPrefetchDeferred();

  if (((mHead + 1) & sMaxDeferredMask) == mTail)
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
    
  mEntries[mHead].mFlags = flags;
  mEntries[mHead].mElement = do_GetWeakReference(aElement);
  mHead = (mHead + 1) & sMaxDeferredMask;

  if (!mActiveLoaderCount && !mTimerArmed && mTimer) {
    mTimerArmed = true;
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
    if (content) {
      nsCOMPtr<Link> link = do_QueryInterface(content);
      // Only prefetch here if request was deferred and deferral not cancelled
      if (link && link->HasDeferredDNSPrefetchRequest()) {
        nsCOMPtr<nsIURI> hrefURI(link ? link->GetURI() : nullptr);
        if (hrefURI)
          hrefURI->GetAsciiHost(hostName);

        if (!hostName.IsEmpty()) {
          if (IsNeckoChild()) {
            gNeckoChild->SendHTMLDNSPrefetch(NS_ConvertUTF8toUTF16(hostName),
                                           mEntries[mTail].mFlags);
          } else {
            nsCOMPtr<nsICancelable> tmpOutstanding;

            nsresult rv = sDNSService->AsyncResolve(hostName, 
                                    mEntries[mTail].mFlags
                                    | nsIDNSService::RESOLVE_SPECULATE,
                                    sDNSListener, nullptr,
                                    getter_AddRefs(tmpOutstanding));
            // Tell link that deferred prefetch was requested
            if (NS_SUCCEEDED(rv))
              link->OnDNSPrefetchRequested();
          }
        }
      }
    }
    
    mEntries[mTail].mElement = nullptr;
    mTail = (mTail + 1) & sMaxDeferredMask;
  }
  
  if (mTimerArmed) {
    mTimerArmed = false;
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
    observerService->AddObserver(this, "xpcom-shutdown", true);
}

// nsITimer related method

void 
nsHTMLDNSPrefetch::nsDeferrals::Tick(nsITimer *aTimer, void *aClosure)
{
  nsHTMLDNSPrefetch::nsDeferrals *self = (nsHTMLDNSPrefetch::nsDeferrals *) aClosure;

  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::Tick must be on main thread");
  NS_ASSERTION(self->mTimerArmed, "Timer is not armed");
  
  self->mTimerArmed = false;

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
                                              uint32_t progressStateFlags, 
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
                                                 int32_t curSelfProgress, 
                                                 int32_t maxSelfProgress, 
                                                 int32_t curTotalProgress, 
                                                 int32_t maxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location,
                                                 uint32_t aFlags)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLDNSPrefetch::nsDeferrals::OnStatusChange(nsIWebProgress* aWebProgress,
                                               nsIRequest* aRequest,
                                               nsresult aStatus,
                                               const char16_t* aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLDNSPrefetch::nsDeferrals::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                                 nsIRequest *aRequest, 
                                                 uint32_t state)
{
  return NS_OK;
}

//////////// nsIObserver method

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::Observe(nsISupports *subject,
                                        const char *topic,
                                        const char16_t *data)
{
  if (!strcmp(topic, "xpcom-shutdown"))
    Flush();
  
  return NS_OK;
}
