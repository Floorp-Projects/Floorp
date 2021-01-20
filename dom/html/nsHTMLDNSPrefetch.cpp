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
#include "mozilla/OriginAttributes.h"
#include "mozilla/StoragePrincipalHelper.h"
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
#include "mozilla/StaticPrefs_network.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::net;

class NoOpDNSListener final : public nsIDNSListener {
  // This class exists to give a safe callback no-op DNSListener
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  NoOpDNSListener() = default;

 private:
  ~NoOpDNSListener() = default;
};

NS_IMPL_ISUPPORTS(NoOpDNSListener, nsIDNSListener)

NS_IMETHODIMP
NoOpDNSListener::OnLookupComplete(nsICancelable* request, nsIDNSRecord* rec,
                                  nsresult status) {
  return NS_OK;
}

class DeferredDNSPrefetches final : public nsIWebProgressListener,
                                    public nsSupportsWeakReference,
                                    public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIOBSERVER

  DeferredDNSPrefetches();

  void Activate();
  nsresult Add(uint32_t flags, mozilla::dom::Link* aElement);

  void RemoveUnboundLinks();

 private:
  ~DeferredDNSPrefetches();
  void Flush();

  void SubmitQueue();

  uint16_t mHead;
  uint16_t mTail;
  uint32_t mActiveLoaderCount;

  nsCOMPtr<nsITimer> mTimer;
  bool mTimerArmed;
  static void Tick(nsITimer* aTimer, void* aClosure);

  static const int sMaxDeferred = 512;  // keep power of 2 for masking
  static const int sMaxDeferredMask = (sMaxDeferred - 1);

  struct deferred_entry {
    uint32_t mFlags;
    // Link implementation clears this raw pointer in its destructor.
    mozilla::dom::Link* mElement;
  } mEntries[sMaxDeferred];
};

static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static bool sInitialized = false;
static nsIDNSService* sDNSService = nullptr;
static DeferredDNSPrefetches* sPrefetches = nullptr;
static NoOpDNSListener* sDNSListener = nullptr;

nsresult nsHTMLDNSPrefetch::Initialize() {
  if (sInitialized) {
    NS_WARNING("Initialize() called twice");
    return NS_OK;
  }

  sPrefetches = new DeferredDNSPrefetches();
  NS_ADDREF(sPrefetches);

  sDNSListener = new NoOpDNSListener();
  NS_ADDREF(sDNSListener);

  sPrefetches->Activate();

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

uint32_t nsHTMLDNSPrefetch::PriorityToDNSServiceFlags(Priority aPriority) {
  switch (aPriority) {
    case Priority::Low:
      return uint32_t(nsIDNSService::RESOLVE_PRIORITY_LOW);
    case Priority::Medium:
      return uint32_t(nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
    case Priority::High:
      return 0u;
  }
  MOZ_ASSERT_UNREACHABLE("Unknown priority");
  return 0u;
}

nsresult nsHTMLDNSPrefetch::Prefetch(Link* aElement, Priority aPriority) {
  if (!(sInitialized && sPrefetches && sDNSListener) || !EnsureDNSService()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return sPrefetches->Add(
      GetDNSFlagsFromLink(aElement) | PriorityToDNSServiceFlags(aPriority),
      aElement);
}

nsresult nsHTMLDNSPrefetch::Prefetch(
    const nsAString& hostname, bool isHttps,
    const OriginAttributes& aPartitionedPrincipalOriginAttributes,
    uint32_t flags) {
  if (IsNeckoChild()) {
    // We need to check IsEmpty() because net_IsValidHostName()
    // considers empty strings to be valid hostnames
    if (!hostname.IsEmpty() &&
        net_IsValidHostName(NS_ConvertUTF16toUTF8(hostname))) {
      // during shutdown gNeckoChild might be null
      if (gNeckoChild) {
        gNeckoChild->SendHTMLDNSPrefetch(nsString(hostname), isHttps,
                                         aPartitionedPrincipalOriginAttributes,
                                         flags);
      }
    }
    return NS_OK;
  }

  if (!(sInitialized && sPrefetches && sDNSListener) || !EnsureDNSService())
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsICancelable> tmpOutstanding;
  nsresult rv = sDNSService->AsyncResolveNative(
      NS_ConvertUTF16toUTF8(hostname), nsIDNSService::RESOLVE_TYPE_DEFAULT,
      flags | nsIDNSService::RESOLVE_SPECULATE, nullptr, sDNSListener, nullptr,
      aPartitionedPrincipalOriginAttributes, getter_AddRefs(tmpOutstanding));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (StaticPrefs::network_dns_upgrade_with_https_rr() ||
      StaticPrefs::network_dns_use_https_rr_as_altsvc()) {
    Unused << sDNSService->AsyncResolveNative(
        NS_ConvertUTF16toUTF8(hostname), nsIDNSService::RESOLVE_TYPE_HTTPSSVC,
        flags | nsIDNSService::RESOLVE_SPECULATE, nullptr, sDNSListener,
        nullptr, aPartitionedPrincipalOriginAttributes,
        getter_AddRefs(tmpOutstanding));
  }

  return NS_OK;
}

nsresult nsHTMLDNSPrefetch::Prefetch(
    const nsAString& hostname, bool isHttps,
    const OriginAttributes& aPartitionedPrincipalOriginAttributes,
    nsIRequest::TRRMode aMode, Priority aPriority) {
  return Prefetch(hostname, isHttps, aPartitionedPrincipalOriginAttributes,
                  nsIDNSService::GetFlagsFromTRRMode(aMode) |
                      PriorityToDNSServiceFlags(aPriority));
}

nsresult nsHTMLDNSPrefetch::CancelPrefetch(Link* aElement, Priority aPriority,
                                           nsresult aReason) {
  if (!(sInitialized && sPrefetches && sDNSListener) || !EnsureDNSService()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint32_t flags =
      GetDNSFlagsFromLink(aElement) | PriorityToDNSServiceFlags(aPriority);

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

  OriginAttributes oa;
  StoragePrincipalHelper::GetOriginAttributesForNetworkState(
      element->OwnerDoc(), oa);

  return CancelPrefetch(hostname, isHttps, oa, flags, aReason);
}

nsresult nsHTMLDNSPrefetch::CancelPrefetch(
    const nsAString& hostname, bool isHttps,
    const OriginAttributes& aPartitionedPrincipalOriginAttributes,
    uint32_t flags, nsresult aReason) {
  // Forward this request to Necko Parent if we're a child process
  if (IsNeckoChild()) {
    // We need to check IsEmpty() because net_IsValidHostName()
    // considers empty strings to be valid hostnames
    if (!hostname.IsEmpty() &&
        net_IsValidHostName(NS_ConvertUTF16toUTF8(hostname))) {
      // during shutdown gNeckoChild might be null
      if (gNeckoChild) {
        gNeckoChild->SendCancelHTMLDNSPrefetch(
            nsString(hostname), isHttps, aPartitionedPrincipalOriginAttributes,
            flags, aReason);
      }
    }
    return NS_OK;
  }

  if (!(sInitialized && sPrefetches && sDNSListener) || !EnsureDNSService())
    return NS_ERROR_NOT_AVAILABLE;

  // Forward cancellation to DNS service
  nsresult rv = sDNSService->CancelAsyncResolveNative(
      NS_ConvertUTF16toUTF8(hostname), nsIDNSService::RESOLVE_TYPE_DEFAULT,
      flags | nsIDNSService::RESOLVE_SPECULATE,
      nullptr,  // resolverInfo
      sDNSListener, aReason, aPartitionedPrincipalOriginAttributes);

  if (StaticPrefs::network_dns_upgrade_with_https_rr() ||
      StaticPrefs::network_dns_use_https_rr_as_altsvc()) {
    Unused << sDNSService->CancelAsyncResolveNative(
        NS_ConvertUTF16toUTF8(hostname), nsIDNSService::RESOLVE_TYPE_HTTPSSVC,
        flags | nsIDNSService::RESOLVE_SPECULATE,
        nullptr,  // resolverInfo
        sDNSListener, aReason, aPartitionedPrincipalOriginAttributes);
  }
  return rv;
}

nsresult nsHTMLDNSPrefetch::CancelPrefetch(
    const nsAString& hostname, bool isHttps,
    const OriginAttributes& aPartitionedPrincipalOriginAttributes,
    nsIRequest::TRRMode aTRRMode, Priority aPriority, nsresult aReason) {
  return CancelPrefetch(hostname, isHttps,
                        aPartitionedPrincipalOriginAttributes,
                        nsIDNSService::GetFlagsFromTRRMode(aTRRMode) |
                            PriorityToDNSServiceFlags(aPriority),
                        aReason);
}

void nsHTMLDNSPrefetch::LinkDestroyed(Link* aLink) {
  MOZ_ASSERT(aLink->IsInDNSPrefetch());
  if (sPrefetches) {
    // Clean up all the possible links at once.
    sPrefetches->RemoveUnboundLinks();
  }
}

DeferredDNSPrefetches::DeferredDNSPrefetches()
    : mHead(0), mTail(0), mActiveLoaderCount(0), mTimerArmed(false) {
  mTimer = NS_NewTimer();
}

DeferredDNSPrefetches::~DeferredDNSPrefetches() {
  if (mTimerArmed) {
    mTimerArmed = false;
    mTimer->Cancel();
  }

  Flush();
}

NS_IMPL_ISUPPORTS(DeferredDNSPrefetches, nsIWebProgressListener,
                  nsISupportsWeakReference, nsIObserver)

void DeferredDNSPrefetches::Flush() {
  while (mHead != mTail) {
    if (mEntries[mTail].mElement) {
      mEntries[mTail].mElement->ClearIsInDNSPrefetch();
    }
    mEntries[mTail].mElement = nullptr;
    mTail = (mTail + 1) & sMaxDeferredMask;
  }
}

nsresult DeferredDNSPrefetches::Add(uint32_t flags, Link* aElement) {
  // The FIFO has no lock, so it can only be accessed on main thread
  NS_ASSERTION(NS_IsMainThread(),
               "DeferredDNSPrefetches::Add must be on main thread");

  aElement->OnDNSPrefetchDeferred();

  if (((mHead + 1) & sMaxDeferredMask) == mTail)
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;

  aElement->SetIsInDNSPrefetch();
  mEntries[mHead].mFlags = flags;
  mEntries[mHead].mElement = aElement;
  mHead = (mHead + 1) & sMaxDeferredMask;

  if (!mActiveLoaderCount && !mTimerArmed && mTimer) {
    mTimerArmed = true;
    mTimer->InitWithNamedFuncCallback(
        Tick, this, 2000, nsITimer::TYPE_ONE_SHOT,
        "nsHTMLDNSPrefetch::DeferredDNSPrefetches::Tick");
  }

  return NS_OK;
}

void DeferredDNSPrefetches::SubmitQueue() {
  NS_ASSERTION(NS_IsMainThread(),
               "DeferredDNSPrefetches::SubmitQueue must be on main thread");
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

        OriginAttributes oa;
        StoragePrincipalHelper::GetOriginAttributesForNetworkState(
            element->OwnerDoc(), oa);

        if (!hostName.IsEmpty() && NS_SUCCEEDED(rv) && !isLocalResource &&
            element) {
          if (IsNeckoChild()) {
            // during shutdown gNeckoChild might be null
            if (gNeckoChild) {
              gNeckoChild->SendHTMLDNSPrefetch(NS_ConvertUTF8toUTF16(hostName),
                                               isHttps, oa,
                                               mEntries[mTail].mFlags);
            }
          } else {
            nsCOMPtr<nsICancelable> tmpOutstanding;

            rv = sDNSService->AsyncResolveNative(
                hostName, nsIDNSService::RESOLVE_TYPE_DEFAULT,
                mEntries[mTail].mFlags | nsIDNSService::RESOLVE_SPECULATE,
                nullptr, sDNSListener, nullptr, oa,
                getter_AddRefs(tmpOutstanding));
            // Fetch HTTPS RR if needed.
            if (NS_SUCCEEDED(rv) &&
                (StaticPrefs::network_dns_upgrade_with_https_rr() ||
                 StaticPrefs::network_dns_use_https_rr_as_altsvc())) {
              sDNSService->AsyncResolveNative(
                  hostName, nsIDNSService::RESOLVE_TYPE_HTTPSSVC,
                  mEntries[mTail].mFlags | nsIDNSService::RESOLVE_SPECULATE,
                  nullptr, sDNSListener, nullptr, oa,
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

void DeferredDNSPrefetches::Activate() {
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

void DeferredDNSPrefetches::RemoveUnboundLinks() {
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

void DeferredDNSPrefetches::Tick(nsITimer* aTimer, void* aClosure) {
  auto* self = static_cast<DeferredDNSPrefetches*>(aClosure);

  NS_ASSERTION(NS_IsMainThread(),
               "DeferredDNSPrefetches::Tick must be on main thread");
  NS_ASSERTION(self->mTimerArmed, "Timer is not armed");

  self->mTimerArmed = false;

  // If the queue is not submitted here because there are outstanding pages
  // being loaded, there is no need to rearm the timer as the queue will be
  // submtited when those loads complete.
  if (!self->mActiveLoaderCount) self->SubmitQueue();
}

//////////// nsIWebProgressListener methods

NS_IMETHODIMP
DeferredDNSPrefetches::OnStateChange(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest,
                                     uint32_t progressStateFlags,
                                     nsresult aStatus) {
  // The FIFO has no lock, so it can only be accessed on main thread
  NS_ASSERTION(NS_IsMainThread(),
               "DeferredDNSPrefetches::OnStateChange must be on main thread");

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
DeferredDNSPrefetches::OnProgressChange(nsIWebProgress* aProgress,
                                        nsIRequest* aRequest,
                                        int32_t curSelfProgress,
                                        int32_t maxSelfProgress,
                                        int32_t curTotalProgress,
                                        int32_t maxTotalProgress) {
  return NS_OK;
}

NS_IMETHODIMP
DeferredDNSPrefetches::OnLocationChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest, nsIURI* location,
                                        uint32_t aFlags) {
  return NS_OK;
}

NS_IMETHODIMP
DeferredDNSPrefetches::OnStatusChange(nsIWebProgress* aWebProgress,
                                      nsIRequest* aRequest, nsresult aStatus,
                                      const char16_t* aMessage) {
  return NS_OK;
}

NS_IMETHODIMP
DeferredDNSPrefetches::OnSecurityChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest, uint32_t aState) {
  return NS_OK;
}

NS_IMETHODIMP
DeferredDNSPrefetches::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                              nsIRequest* aRequest,
                                              uint32_t aEvent) {
  return NS_OK;
}

//////////// nsIObserver method

NS_IMETHODIMP
DeferredDNSPrefetches::Observe(nsISupports* subject, const char* topic,
                               const char16_t* data) {
  if (!strcmp(topic, "xpcom-shutdown")) Flush();

  return NS_OK;
}
