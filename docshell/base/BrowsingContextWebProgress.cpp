/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BrowsingContextWebProgress.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF(BrowsingContextWebProgress)
NS_IMPL_RELEASE(BrowsingContextWebProgress)

NS_INTERFACE_MAP_BEGIN(BrowsingContextWebProgress)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebProgress)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgress)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END

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

NS_IMETHODIMP BrowsingContextWebProgress::GetDOMWindow(
    mozIDOMWindowProxy** aDOMWindow) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetDOMWindowID(
    uint64_t* aDOMWindowID) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetInnerDOMWindowID(
    uint64_t* aInnerDOMWindowID) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetIsTopLevel(bool* aIsTopLevel) {
  *aIsTopLevel = true;
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
    nsIWebProgress* aWebProgress, uint32_t aFlag,
    const std::function<void(nsIWebProgressListener*)>& aCallback) {
  RefPtr<BrowsingContextWebProgress> kungFuDeathGrip = this;
  bool isTop = true;
  if (aWebProgress) {
    aWebProgress->GetIsTopLevel(&isTop);
  }
  if (!isTop) {
    return;
  }
  if (aWebProgress) {
    aWebProgress->GetIsLoadingDocument(&mIsLoadingDocument);
    aWebProgress->GetLoadType(&mLoadType);
  }

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
}

////////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
BrowsingContextWebProgress::OnStateChange(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          uint32_t aStateFlags,
                                          nsresult aStatus) {
  UpdateAndNotifyListeners(
      aWebProgress, ((aStateFlags >> 16) & nsIWebProgress::NOTIFY_STATE_ALL),
      [&](nsIWebProgressListener* listener) {
        listener->OnStateChange(this, aRequest, aStateFlags, aStatus);
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
  UpdateAndNotifyListeners(aWebProgress, nsIWebProgress::NOTIFY_PROGRESS,
                           [&](nsIWebProgressListener* listener) {
                             listener->OnProgressChange(
                                 this, aRequest, aCurSelfProgress,
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
  UpdateAndNotifyListeners(aWebProgress, nsIWebProgress::NOTIFY_LOCATION,
                           [&](nsIWebProgressListener* listener) {
                             listener->OnLocationChange(this, aRequest,
                                                        aLocation, aFlags);
                           });
  return NS_OK;
}

NS_IMETHODIMP
BrowsingContextWebProgress::OnStatusChange(nsIWebProgress* aWebProgress,
                                           nsIRequest* aRequest,
                                           nsresult aStatus,
                                           const char16_t* aMessage) {
  UpdateAndNotifyListeners(aWebProgress, nsIWebProgress::NOTIFY_STATUS,
                           [&](nsIWebProgressListener* listener) {
                             listener->OnStatusChange(this, aRequest, aStatus,
                                                      aMessage);
                           });
  return NS_OK;
}

NS_IMETHODIMP
BrowsingContextWebProgress::OnSecurityChange(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             uint32_t aState) {
  UpdateAndNotifyListeners(aWebProgress, nsIWebProgress::NOTIFY_SECURITY,
                           [&](nsIWebProgressListener* listener) {
                             listener->OnSecurityChange(this, aRequest, aState);
                           });
  return NS_OK;
}

NS_IMETHODIMP
BrowsingContextWebProgress::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                                   nsIRequest* aRequest,
                                                   uint32_t aEvent) {
  UpdateAndNotifyListeners(
      aWebProgress, nsIWebProgress::NOTIFY_CONTENT_BLOCKING,
      [&](nsIWebProgressListener* listener) {
        listener->OnContentBlockingEvent(this, aRequest, aEvent);
      });
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
