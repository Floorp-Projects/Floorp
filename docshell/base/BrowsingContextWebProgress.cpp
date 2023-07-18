/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BrowsingContextWebProgress.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/BounceTrackingState.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "nsCOMPtr.h"
#include "nsIWebProgressListener.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsIChannel.h"
#include "xptinfo.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {

static mozilla::LazyLogModule gBCWebProgressLog("BCWebProgress");

static nsCString DescribeBrowsingContext(CanonicalBrowsingContext* aContext);
static nsCString DescribeWebProgress(nsIWebProgress* aWebProgress);
static nsCString DescribeRequest(nsIRequest* aRequest);
static nsCString DescribeWebProgressFlags(uint32_t aFlags,
                                          const nsACString& aPrefix);
static nsCString DescribeError(nsresult aError);

NS_IMPL_CYCLE_COLLECTION(BrowsingContextWebProgress, mCurrentBrowsingContext)

NS_IMPL_CYCLE_COLLECTING_ADDREF(BrowsingContextWebProgress)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BrowsingContextWebProgress)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowsingContextWebProgress)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebProgress)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgress)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END

BrowsingContextWebProgress::BrowsingContextWebProgress(
    CanonicalBrowsingContext* aBrowsingContext)
    : mCurrentBrowsingContext(aBrowsingContext) {}

BrowsingContextWebProgress::~BrowsingContextWebProgress() = default;

NS_IMETHODIMP BrowsingContextWebProgress::AddProgressListener(
    nsIWebProgressListener* aListener, uint32_t aNotifyMask) {
  nsWeakPtr listener = do_GetWeakReference(aListener);
  if (!listener) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mListenerInfoList.Contains(listener)) {
    // The listener is already registered!
    return NS_ERROR_FAILURE;
  }

  mListenerInfoList.AppendElement(ListenerInfo(listener, aNotifyMask));
  return NS_OK;
}

NS_IMETHODIMP BrowsingContextWebProgress::RemoveProgressListener(
    nsIWebProgressListener* aListener) {
  nsWeakPtr listener = do_GetWeakReference(aListener);
  if (!listener) {
    return NS_ERROR_INVALID_ARG;
  }

  return mListenerInfoList.RemoveElement(listener) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetBrowsingContextXPCOM(
    BrowsingContext** aBrowsingContext) {
  NS_IF_ADDREF(*aBrowsingContext = mCurrentBrowsingContext);
  return NS_OK;
}

BrowsingContext* BrowsingContextWebProgress::GetBrowsingContext() {
  return mCurrentBrowsingContext;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetDOMWindow(
    mozIDOMWindowProxy** aDOMWindow) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetIsTopLevel(bool* aIsTopLevel) {
  *aIsTopLevel = mCurrentBrowsingContext->IsTop();
  return NS_OK;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetIsLoadingDocument(
    bool* aIsLoadingDocument) {
  *aIsLoadingDocument = mIsLoadingDocument;
  return NS_OK;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetLoadType(uint32_t* aLoadType) {
  *aLoadType = mLoadType;
  return NS_OK;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetTarget(nsIEventTarget** aTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP BrowsingContextWebProgress::SetTarget(nsIEventTarget* aTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void BrowsingContextWebProgress::UpdateAndNotifyListeners(
    uint32_t aFlag,
    const std::function<void(nsIWebProgressListener*)>& aCallback) {
  RefPtr<BrowsingContextWebProgress> kungFuDeathGrip = this;

  ListenerArray::ForwardIterator iter(mListenerInfoList);
  while (iter.HasMore()) {
    ListenerInfo& info = iter.GetNext();
    if (!(info.mNotifyMask & aFlag)) {
      continue;
    }

    nsCOMPtr<nsIWebProgressListener> listener =
        do_QueryReferent(info.mWeakListener);
    if (!listener) {
      mListenerInfoList.RemoveElement(info);
      continue;
    }

    aCallback(listener);
  }

  mListenerInfoList.Compact();

  // Notify the parent BrowsingContextWebProgress of the event to continue
  // propagating.
  auto* parent = mCurrentBrowsingContext->GetParent();
  if (parent && parent->GetWebProgress()) {
    aCallback(parent->GetWebProgress());
  }
}

void BrowsingContextWebProgress::ContextDiscarded() {
  if (!mIsLoadingDocument) {
    return;
  }

  // If our BrowsingContext is being discarded while still loading a document,
  // fire a synthetic `STATE_STOP` to end the ongoing load.
  MOZ_LOG(gBCWebProgressLog, LogLevel::Info,
          ("Discarded while loading %s",
           DescribeBrowsingContext(mCurrentBrowsingContext).get()));

  // This matches what nsDocLoader::doStopDocumentLoad does, except we don't
  // bother notifying for `STATE_STOP | STATE_IS_DOCUMENT`,
  // nsBrowserStatusFilter would filter it out before it gets to the parent
  // process.
  nsCOMPtr<nsIRequest> request = mLoadingDocumentRequest;
  OnStateChange(this, request, STATE_STOP | STATE_IS_WINDOW | STATE_IS_NETWORK,
                NS_ERROR_ABORT);
}

void BrowsingContextWebProgress::ContextReplaced(
    CanonicalBrowsingContext* aNewContext) {
  mCurrentBrowsingContext = aNewContext;
}

already_AddRefed<BounceTrackingState>
BrowsingContextWebProgress::GetBounceTrackingState() {
  if (!mBounceTrackingState) {
    mBounceTrackingState = BounceTrackingState::GetOrCreate(this);
  }
  return do_AddRef(mBounceTrackingState);
}

////////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
BrowsingContextWebProgress::OnStateChange(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          uint32_t aStateFlags,
                                          nsresult aStatus) {
  MOZ_LOG(
      gBCWebProgressLog, LogLevel::Info,
      ("OnStateChange(%s, %s, %s, %s) on %s",
       DescribeWebProgress(aWebProgress).get(), DescribeRequest(aRequest).get(),
       DescribeWebProgressFlags(aStateFlags, "STATE_"_ns).get(),
       DescribeError(aStatus).get(),
       DescribeBrowsingContext(mCurrentBrowsingContext).get()));

  bool targetIsThis = aWebProgress == this;

  // We may receive a request from an in-process nsDocShell which doesn't have
  // `aWebProgress == this` which we should still consider as targeting
  // ourselves.
  if (nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(aWebProgress);
      docShell && docShell->GetBrowsingContext() == mCurrentBrowsingContext) {
    targetIsThis = true;
    aWebProgress->GetLoadType(&mLoadType);
  }

  // Track `mIsLoadingDocument` based on the notifications we've received so far
  // if the nsIWebProgress being targeted is this one.
  if (targetIsThis) {
    constexpr uint32_t startFlags = nsIWebProgressListener::STATE_START |
                                    nsIWebProgressListener::STATE_IS_DOCUMENT |
                                    nsIWebProgressListener::STATE_IS_REQUEST |
                                    nsIWebProgressListener::STATE_IS_WINDOW |
                                    nsIWebProgressListener::STATE_IS_NETWORK;
    constexpr uint32_t stopFlags = nsIWebProgressListener::STATE_STOP |
                                   nsIWebProgressListener::STATE_IS_WINDOW;
    constexpr uint32_t redirectFlags =
        nsIWebProgressListener::STATE_IS_REDIRECTED_DOCUMENT;
    if ((aStateFlags & startFlags) == startFlags) {
      if (mIsLoadingDocument) {
        // We received a duplicate `STATE_START` notification, silence this
        // notification until we receive the matching `STATE_STOP` to not fire
        // duplicate `STATE_START` notifications into frontend on process
        // switches.
        return NS_OK;
      }
      mIsLoadingDocument = true;

      // Record the request we started the load with, so we can emit a synthetic
      // `STATE_STOP` notification if the BrowsingContext is discarded before
      // the notification arrives.
      mLoadingDocumentRequest = aRequest;
    } else if ((aStateFlags & stopFlags) == stopFlags) {
      // We've received a `STATE_STOP` notification targeting this web progress,
      // clear our loading document flag.
      mIsLoadingDocument = false;
      mLoadingDocumentRequest = nullptr;
    } else if (mIsLoadingDocument &&
               (aStateFlags & redirectFlags) == redirectFlags) {
      // If we see a redirected document load, update the loading request which
      // we'll emit the synthetic STATE_STOP notification with.
      mLoadingDocumentRequest = aRequest;
    }
  }

  UpdateAndNotifyListeners(
      ((aStateFlags >> 16) & nsIWebProgress::NOTIFY_STATE_ALL),
      [&](nsIWebProgressListener* listener) {
        listener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
      });
  return NS_OK;
}

NS_IMETHODIMP
BrowsingContextWebProgress::OnProgressChange(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             int32_t aCurSelfProgress,
                                             int32_t aMaxSelfProgress,
                                             int32_t aCurTotalProgress,
                                             int32_t aMaxTotalProgress) {
  MOZ_LOG(
      gBCWebProgressLog, LogLevel::Info,
      ("OnProgressChange(%s, %s, %d, %d, %d, %d) on %s",
       DescribeWebProgress(aWebProgress).get(), DescribeRequest(aRequest).get(),
       aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress,
       DescribeBrowsingContext(mCurrentBrowsingContext).get()));
  UpdateAndNotifyListeners(
      nsIWebProgress::NOTIFY_PROGRESS, [&](nsIWebProgressListener* listener) {
        listener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress,
                                   aMaxSelfProgress, aCurTotalProgress,
                                   aMaxTotalProgress);
      });
  return NS_OK;
}

NS_IMETHODIMP
BrowsingContextWebProgress::OnLocationChange(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             nsIURI* aLocation,
                                             uint32_t aFlags) {
  MOZ_LOG(
      gBCWebProgressLog, LogLevel::Info,
      ("OnLocationChange(%s, %s, %s, %s) on %s",
       DescribeWebProgress(aWebProgress).get(), DescribeRequest(aRequest).get(),
       aLocation ? aLocation->GetSpecOrDefault().get() : "<null>",
       DescribeWebProgressFlags(aFlags, "LOCATION_CHANGE_"_ns).get(),
       DescribeBrowsingContext(mCurrentBrowsingContext).get()));
  UpdateAndNotifyListeners(
      nsIWebProgress::NOTIFY_LOCATION, [&](nsIWebProgressListener* listener) {
        listener->OnLocationChange(aWebProgress, aRequest, aLocation, aFlags);
      });
  return NS_OK;
}

NS_IMETHODIMP
BrowsingContextWebProgress::OnStatusChange(nsIWebProgress* aWebProgress,
                                           nsIRequest* aRequest,
                                           nsresult aStatus,
                                           const char16_t* aMessage) {
  MOZ_LOG(
      gBCWebProgressLog, LogLevel::Info,
      ("OnStatusChange(%s, %s, %s, \"%s\") on %s",
       DescribeWebProgress(aWebProgress).get(), DescribeRequest(aRequest).get(),
       DescribeError(aStatus).get(), NS_ConvertUTF16toUTF8(aMessage).get(),
       DescribeBrowsingContext(mCurrentBrowsingContext).get()));
  UpdateAndNotifyListeners(
      nsIWebProgress::NOTIFY_STATUS, [&](nsIWebProgressListener* listener) {
        listener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
      });
  return NS_OK;
}

NS_IMETHODIMP
BrowsingContextWebProgress::OnSecurityChange(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             uint32_t aState) {
  MOZ_LOG(
      gBCWebProgressLog, LogLevel::Info,
      ("OnSecurityChange(%s, %s, %x) on %s",
       DescribeWebProgress(aWebProgress).get(), DescribeRequest(aRequest).get(),
       aState, DescribeBrowsingContext(mCurrentBrowsingContext).get()));
  UpdateAndNotifyListeners(
      nsIWebProgress::NOTIFY_SECURITY, [&](nsIWebProgressListener* listener) {
        listener->OnSecurityChange(aWebProgress, aRequest, aState);
      });
  return NS_OK;
}

NS_IMETHODIMP
BrowsingContextWebProgress::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                                   nsIRequest* aRequest,
                                                   uint32_t aEvent) {
  MOZ_LOG(
      gBCWebProgressLog, LogLevel::Info,
      ("OnContentBlockingEvent(%s, %s, %x) on %s",
       DescribeWebProgress(aWebProgress).get(), DescribeRequest(aRequest).get(),
       aEvent, DescribeBrowsingContext(mCurrentBrowsingContext).get()));
  UpdateAndNotifyListeners(nsIWebProgress::NOTIFY_CONTENT_BLOCKING,
                           [&](nsIWebProgressListener* listener) {
                             listener->OnContentBlockingEvent(aWebProgress,
                                                              aRequest, aEvent);
                           });
  return NS_OK;
}

NS_IMETHODIMP
BrowsingContextWebProgress::GetDocumentRequest(nsIRequest** aRequest) {
  NS_IF_ADDREF(*aRequest = mLoadingDocumentRequest);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Helper methods for notification logging

static nsCString DescribeBrowsingContext(CanonicalBrowsingContext* aContext) {
  if (!aContext) {
    return "<null>"_ns;
  }

  nsCOMPtr<nsIURI> currentURI = aContext->GetCurrentURI();
  return nsPrintfCString(
      "{top:%d, id:%" PRIx64 ", url:%s}", aContext->IsTop(), aContext->Id(),
      currentURI ? currentURI->GetSpecOrDefault().get() : "<null>");
}

static nsCString DescribeWebProgress(nsIWebProgress* aWebProgress) {
  if (!aWebProgress) {
    return "<null>"_ns;
  }

  bool isTopLevel = false;
  aWebProgress->GetIsTopLevel(&isTopLevel);
  bool isLoadingDocument = false;
  aWebProgress->GetIsLoadingDocument(&isLoadingDocument);
  return nsPrintfCString("{isTopLevel:%d, isLoadingDocument:%d}", isTopLevel,
                         isLoadingDocument);
}

static nsCString DescribeRequest(nsIRequest* aRequest) {
  if (!aRequest) {
    return "<null>"_ns;
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (!channel) {
    return "<non-channel>"_ns;
  }

  nsCOMPtr<nsIURI> originalURI;
  channel->GetOriginalURI(getter_AddRefs(originalURI));
  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));

  return nsPrintfCString(
      "{URI:%s, originalURI:%s}",
      uri ? uri->GetSpecOrDefault().get() : "<null>",
      originalURI ? originalURI->GetSpecOrDefault().get() : "<null>");
}

static nsCString DescribeWebProgressFlags(uint32_t aFlags,
                                          const nsACString& aPrefix) {
  nsCString flags;
  uint32_t remaining = aFlags;

  // Hackily fetch the names of each constant from the XPT information used for
  // reflecting it into JS. This doesn't need to be reliable and just exists as
  // a logging aid.
  //
  // If a change to xpt in the future breaks this code, just delete it and
  // replace it with a normal hex log.
  if (const auto* ifaceInfo =
          nsXPTInterfaceInfo::ByIID(NS_GET_IID(nsIWebProgressListener))) {
    for (uint16_t i = 0; i < ifaceInfo->ConstantCount(); ++i) {
      const auto& constInfo = ifaceInfo->Constant(i);
      nsDependentCString name(constInfo.Name());
      if (!StringBeginsWith(name, aPrefix)) {
        continue;
      }

      if (remaining & constInfo.mValue) {
        remaining &= ~constInfo.mValue;
        if (!flags.IsEmpty()) {
          flags.AppendLiteral("|");
        }
        flags.Append(name);
      }
    }
  }
  if (remaining != 0 || flags.IsEmpty()) {
    if (!flags.IsEmpty()) {
      flags.AppendLiteral("|");
    }
    flags.AppendInt(remaining, 16);
  }
  return flags;
}

static nsCString DescribeError(nsresult aError) {
  nsCString name;
  GetErrorName(aError, name);
  return name;
}

}  // namespace dom
}  // namespace mozilla
