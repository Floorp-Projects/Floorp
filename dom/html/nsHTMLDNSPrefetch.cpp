/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLDNSPrefetch.h"

#include "base/basictypes.h"
#include "mozilla/dom/Element.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoChild.h"
#include "nsURLHelper.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsIProtocolHandler.h"

#include "nsIDNSListener.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgress.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsICancelable.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/Document.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "nsIObserverService.h"
#include "mozilla/dom/Link.h"

#include "mozilla/Components.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::net;

static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static bool sInitialized = false;
static nsIDNSService* sDNSService = nullptr;
static nsHTMLDNSPrefetch::nsDeferrals* sPrefetches = nullptr;
static nsHTMLDNSPrefetch::nsListener* sDNSListener = nullptr;
bool sEsniEnabled;

nsresult nsHTMLDNSPrefetch::Initialize() {
  if (sInitialized) {
    NS_WARNING("Initialize() called twice");
    return NS_OK;
  }

  sPrefetches = new nsHTMLDNSPrefetch::nsDeferrals();
  NS_ADDREF(sPrefetches);

  sDNSListener = new nsHTMLDNSPrefetch::nsListener();
  NS_ADDREF(sDNSListener);

  sPrefetches->Activate();

  Preferences::AddBoolVarCache(&sEsniEnabled, "network.security.esni.enabled");

  sEsniEnabled = Preferences::GetBool("network.security.esni.enabled", false);

  if (IsNeckoChild()) NeckoChild::InitNeckoChild();

  sInitialized = true;
  return NS_OK;
}

nsresult nsHTMLDNSPrefetch::Shutdown() {
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

static bool EnsureDNSService() {
  if (sDNSService) {
    return true;
  }

  NS_IF_RELEASE(sDNSService);
  nsresult rv;
  rv = CallGetService(kDNSServiceCID, &sDNSService);
  if (NS_FAILED(rv)) {
    return false;
  }

  return !!sDNSService;
}

bool nsHTMLDNSPrefetch::IsAllowed(Document* aDocument) {
  // There is no need to do prefetch on non UI scenarios such as XMLHttpRequest.
  return aDocument->IsDNSPrefetchAllowed() && aDocument->GetWindow();
}

static uint32_t GetDNSFlagsFromLink(Link* aElement) {
  if (!aElement || !aElement->GetElement() ||
      !aElement->GetElement()->OwnerDoc()->GetChannel()) {
    return 0;
  }
  nsIRequest::TRRMode mode =
      aElement->GetElement()->OwnerDoc()->GetChannel()->GetTRRMode();
  return nsIDNSService::GetFlagsFromTRRMode(mode);
}

nsresult nsHTMLDNSPrefetch::Prefetch(Link* aElement, uint32_t flags) {
  if (!(sInitialized && sPrefetches && sDNSListener) || !EnsureDNSService())
    return NS_ERROR_NOT_AVAILABLE;

  flags |= GetDNSFlagsFromLink(aElement);
  return sPrefetches->Add(flags, aElement);
}

nsresult nsHTMLDNSPrefetch::PrefetchLow(Link* aElement) {
  return Prefetch(aElement, nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult nsHTMLDNSPrefetch::PrefetchMedium(Link* aElement) {
  return Prefetch(aElement, nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult nsHTMLDNSPrefetch::PrefetchHigh(Link* aElement) {
  return Prefetch(aElement, 0);
}

nsresult nsHTMLDNSPrefetch::Prefetch(const nsAString& hostname, bool isHttps,
                                     const OriginAttributes& aOriginAttributes,
                                     uint32_t flags) {
  if (IsNeckoChild()) {
    // We need to check IsEmpty() because net_IsValidHostName()
    // considers empty strings to be valid hostnames
    if (!hostname.IsEmpty() &&
        net_IsValidHostName(NS_ConvertUTF16toUTF8(hostname))) {
      // during shutdown gNeckoChild might be null
      if (gNeckoChild) {
        gNeckoChild->SendHTMLDNSPrefetch(nsString(hostname), isHttps,
                                         aOriginAttributes, flags);
      }
    }
    return NS_OK;
  }

  if (!(sInitialized && sPrefetches && sDNSListener) || !EnsureDNSService())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsICancelable> tmpOutstanding;
  nsresult rv = sDNSService->AsyncResolveNative(
      NS_ConvertUTF16toUTF8(hostname), flags | nsIDNSService::RESOLVE_SPECULATE,
      sDNSListener, nullptr, aOriginAttributes, getter_AddRefs(tmpOutstanding));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Fetch ESNI keys if needed.
  if (isHttps && sEsniEnabled) {
    nsAutoCString esniHost;
    esniHost.Append("_esni.");
    esniHost.Append(NS_ConvertUTF16toUTF8(hostname));
    Unused << sDNSService->AsyncResolveByTypeNative(
        esniHost, nsIDNSService::RESOLVE_TYPE_TXT,
        flags | nsIDNSService::RESOLVE_SPECULATE, sDNSListener, nullptr,
        aOriginAttributes, getter_AddRefs(tmpOutstanding));
  }

  return NS_OK;
}

nsresult nsHTMLDNSPrefetch::PrefetchLow(
    const nsAString& hostname, bool isHttps,
    const OriginAttributes& aOriginAttributes, nsIRequest::TRRMode aMode) {
  return Prefetch(hostname, isHttps, aOriginAttributes,
                  nsIDNSService::GetFlagsFromTRRMode(aMode) |
                      nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult nsHTMLDNSPrefetch::PrefetchMedium(
    const nsAString& hostname, bool isHttps,
    const OriginAttributes& aOriginAttributes, nsIRequest::TRRMode aMode) {
  return Prefetch(hostname, isHttps, aOriginAttributes,
                  nsIDNSService::GetFlagsFromTRRMode(aMode) |
                      nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult nsHTMLDNSPrefetch::PrefetchHigh(
    const nsAString& hostname, bool isHttps,
    const OriginAttributes& aOriginAttributes, nsIRequest::TRRMode aMode) {
  return Prefetch(hostname, isHttps, aOriginAttributes,
                  nsIDNSService::GetFlagsFromTRRMode(aMode));
}

nsresult nsHTMLDNSPrefetch::CancelPrefetch(Link* aElement, uint32_t flags,
                                           nsresult aReason) {
  if (!(sInitialized && sPrefetches && sDNSListener) || !EnsureDNSService())
    return NS_ERROR_NOT_AVAILABLE;

  nsAutoString hostname;
  aElement->GetHostname(hostname);

  Element* element = aElement->GetElement();
  NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);

  nsAutoString protocol;
  aElement->GetProtocol(protocol);
  bool isHttps = false;
  if (protocol.EqualsLiteral("https:")) {
    isHttps = true;
  }
  return CancelPrefetch(hostname, isHttps,
                        element->NodePrincipal()->OriginAttributesRef(), flags,
                        aReason);
}

nsresult nsHTMLDNSPrefetch::CancelPrefetch(
    const nsAString& hostname, bool isHttps,
    const OriginAttributes& aOriginAttributes, uint32_t flags,
    nsresult aReason) {
  // Forward this request to Necko Parent if we're a child process
  if (IsNeckoChild()) {
    // We need to check IsEmpty() because net_IsValidHostName()
    // considers empty strings to be valid hostnames
    if (!hostname.IsEmpty() &&
        net_IsValidHostName(NS_ConvertUTF16toUTF8(hostname))) {
      // during shutdown gNeckoChild might be null
      if (gNeckoChild) {
        gNeckoChild->SendCancelHTMLDNSPrefetch(
            nsString(hostname), isHttps, aOriginAttributes, flags, aReason);
      }
    }
    return NS_OK;
  }

  if (!(sInitialized && sPrefetches && sDNSListener) || !EnsureDNSService())
    return NS_ERROR_NOT_AVAILABLE;

  // Forward cancellation to DNS service
  nsresult rv = sDNSService->CancelAsyncResolveNative(
      NS_ConvertUTF16toUTF8(hostname), flags | nsIDNSService::RESOLVE_SPECULATE,
      sDNSListener, aReason, aOriginAttributes);
  // Cancel fetching ESNI keys if needed.
  if (sEsniEnabled && isHttps) {
    nsAutoCString esniHost;
    esniHost.Append("_esni.");
    esniHost.Append(NS_ConvertUTF16toUTF8(hostname));
    sDNSService->CancelAsyncResolveByTypeNative(
        esniHost, nsIDNSService::RESOLVE_TYPE_TXT,
        flags | nsIDNSService::RESOLVE_SPECULATE, sDNSListener, aReason,
        aOriginAttributes);
  }
  return rv;
}

nsresult nsHTMLDNSPrefetch::CancelPrefetchLow(Link* aElement,
                                              nsresult aReason) {
  return CancelPrefetch(
      aElement,
      GetDNSFlagsFromLink(aElement) | nsIDNSService::RESOLVE_PRIORITY_LOW,
      aReason);
}

nsresult nsHTMLDNSPrefetch::CancelPrefetchLow(
    const nsAString& hostname, bool isHttps,
    const OriginAttributes& aOriginAttributes, nsIRequest::TRRMode aTRRMode,
    nsresult aReason) {
  return CancelPrefetch(hostname, isHttps, aOriginAttributes,
                        nsIDNSService::GetFlagsFromTRRMode(aTRRMode) |
                            nsIDNSService::RESOLVE_PRIORITY_LOW,
                        aReason);
}

void nsHTMLDNSPrefetch::LinkDestroyed(Link* aLink) {
  MOZ_ASSERT(aLink->IsInDNSPrefetch());
  if (sPrefetches) {
    // Clean up all the possible links at once.
    sPrefetches->RemoveUnboundLinks();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsHTMLDNSPrefetch::nsListener, nsIDNSListener)

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsListener::OnLookupComplete(nsICancelable* request,
                                                nsIDNSRecord* rec,
                                                nsresult status) {
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

nsHTMLDNSPrefetch::nsDeferrals::nsDeferrals()
    : mHead(0), mTail(0), mActiveLoaderCount(0), mTimerArmed(false) {
  mTimer = NS_NewTimer();
  ;
}

nsHTMLDNSPrefetch::nsDeferrals::~nsDeferrals() {
  if (mTimerArmed) {
    mTimerArmed = false;
    mTimer->Cancel();
  }

  Flush();
}

NS_IMPL_ISUPPORTS(nsHTMLDNSPrefetch::nsDeferrals, nsIWebProgressListener,
                  nsISupportsWeakReference, nsIObserver)

void nsHTMLDNSPrefetch::nsDeferrals::Flush() {
  while (mHead != mTail) {
    if (mEntries[mTail].mElement) {
      mEntries[mTail].mElement->ClearIsInDNSPrefetch();
    }
    mEntries[mTail].mElement = nullptr;
    mTail = (mTail + 1) & sMaxDeferredMask;
  }
}

nsresult nsHTMLDNSPrefetch::nsDeferrals::Add(uint32_t flags, Link* aElement) {
  // The FIFO has no lock, so it can only be accessed on main thread
  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::Add must be on main thread");

  aElement->OnDNSPrefetchDeferred();

  if (((mHead + 1) & sMaxDeferredMask) == mTail)
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;

  aElement->SetIsInDNSPrefetch();
  mEntries[mHead].mFlags = flags;
  mEntries[mHead].mElement = aElement;
  mHead = (mHead + 1) & sMaxDeferredMask;

  if (!mActiveLoaderCount && !mTimerArmed && mTimer) {
    mTimerArmed = true;
    mTimer->InitWithNamedFuncCallback(Tick, this, 2000, nsITimer::TYPE_ONE_SHOT,
                                      "nsHTMLDNSPrefetch::nsDeferrals::Tick");
  }

  return NS_OK;
}

void nsHTMLDNSPrefetch::nsDeferrals::SubmitQueue() {
  NS_ASSERTION(NS_IsMainThread(),
               "nsDeferrals::SubmitQueue must be on main thread");
  nsCString hostName;
  if (!EnsureDNSService()) {
    return;
  }

  while (mHead != mTail) {
    nsCOMPtr<Link> link = mEntries[mTail].mElement;
    if (link) {
      link->ClearIsInDNSPrefetch();
      // Only prefetch here if request was deferred and deferral not cancelled
      if (link && link->HasDeferredDNSPrefetchRequest()) {
        nsCOMPtr<nsIURI> hrefURI(link ? link->GetURI() : nullptr);
        bool isLocalResource = false;
        nsresult rv = NS_OK;
        Element* element = link->GetElement();

        hostName.Truncate();
        bool isHttps = false;
        if (hrefURI) {
          hrefURI->GetAsciiHost(hostName);
          rv = NS_URIChainHasFlags(hrefURI,
                                   nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                                   &isLocalResource);
          isHttps = hrefURI->SchemeIs("https");
        }

        if (!hostName.IsEmpty() && NS_SUCCEEDED(rv) && !isLocalResource &&
            element) {
          if (IsNeckoChild()) {
            // during shutdown gNeckoChild might be null
            if (gNeckoChild) {
              gNeckoChild->SendHTMLDNSPrefetch(
                  NS_ConvertUTF8toUTF16(hostName), isHttps,
                  element->NodePrincipal()->OriginAttributesRef(),
                  mEntries[mTail].mFlags);
            }
          } else {
            nsCOMPtr<nsICancelable> tmpOutstanding;

            rv = sDNSService->AsyncResolveNative(
                hostName,
                mEntries[mTail].mFlags | nsIDNSService::RESOLVE_SPECULATE,
                sDNSListener, nullptr,
                element->NodePrincipal()->OriginAttributesRef(),
                getter_AddRefs(tmpOutstanding));
            // Fetch ESNI keys if needed.
            if (NS_SUCCEEDED(rv) && sEsniEnabled && isHttps) {
              nsAutoCString esniHost;
              esniHost.Append("_esni.");
              esniHost.Append(hostName);
              sDNSService->AsyncResolveByTypeNative(
                  esniHost, nsIDNSService::RESOLVE_TYPE_TXT,
                  mEntries[mTail].mFlags | nsIDNSService::RESOLVE_SPECULATE,
                  sDNSListener, nullptr,
                  element->NodePrincipal()->OriginAttributesRef(),
                  getter_AddRefs(tmpOutstanding));
            }
            // Tell link that deferred prefetch was requested
            if (NS_SUCCEEDED(rv)) link->OnDNSPrefetchRequested();
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

void nsHTMLDNSPrefetch::nsDeferrals::Activate() {
  // Register as an observer for the document loader
  nsCOMPtr<nsIWebProgress> progress = components::DocLoader::Service();
  if (progress)
    progress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);

  // Register as an observer for xpcom shutdown events so we can drop any
  // element refs
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService)
    observerService->AddObserver(this, "xpcom-shutdown", true);
}

void nsHTMLDNSPrefetch::nsDeferrals::RemoveUnboundLinks() {
  uint16_t tail = mTail;
  while (mHead != tail) {
    if (mEntries[tail].mElement &&
        !mEntries[tail].mElement->GetElement()->IsInComposedDoc()) {
      mEntries[tail].mElement->ClearIsInDNSPrefetch();
      mEntries[tail].mElement = nullptr;
    }
    tail = (tail + 1) & sMaxDeferredMask;
  }
}

// nsITimer related method

void nsHTMLDNSPrefetch::nsDeferrals::Tick(nsITimer* aTimer, void* aClosure) {
  nsHTMLDNSPrefetch::nsDeferrals* self =
      (nsHTMLDNSPrefetch::nsDeferrals*)aClosure;

  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::Tick must be on main thread");
  NS_ASSERTION(self->mTimerArmed, "Timer is not armed");

  self->mTimerArmed = false;

  // If the queue is not submitted here because there are outstanding pages
  // being loaded, there is no need to rearm the timer as the queue will be
  // submtited when those loads complete.
  if (!self->mActiveLoaderCount) self->SubmitQueue();
}

//////////// nsIWebProgressListener methods

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnStateChange(nsIWebProgress* aWebProgress,
                                              nsIRequest* aRequest,
                                              uint32_t progressStateFlags,
                                              nsresult aStatus) {
  // The FIFO has no lock, so it can only be accessed on main thread
  NS_ASSERTION(NS_IsMainThread(),
               "nsDeferrals::OnStateChange must be on main thread");

  if (progressStateFlags & STATE_IS_DOCUMENT) {
    if (progressStateFlags & STATE_STOP) {
      // Initialization may have missed a STATE_START notification, so do
      // not go negative
      if (mActiveLoaderCount) mActiveLoaderCount--;

      if (!mActiveLoaderCount) SubmitQueue();
    } else if (progressStateFlags & STATE_START)
      mActiveLoaderCount++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnProgressChange(nsIWebProgress* aProgress,
                                                 nsIRequest* aRequest,
                                                 int32_t curSelfProgress,
                                                 int32_t maxSelfProgress,
                                                 int32_t curTotalProgress,
                                                 int32_t maxTotalProgress) {
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI* location,
                                                 uint32_t aFlags) {
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnStatusChange(nsIWebProgress* aWebProgress,
                                               nsIRequest* aRequest,
                                               nsresult aStatus,
                                               const char16_t* aMessage) {
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnSecurityChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 uint32_t aState) {
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnContentBlockingEvent(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest, uint32_t aEvent) {
  return NS_OK;
}

//////////// nsIObserver method

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::Observe(nsISupports* subject, const char* topic,
                                        const char16_t* data) {
  if (!strcmp(topic, "xpcom-shutdown")) Flush();

  return NS_OK;
}
