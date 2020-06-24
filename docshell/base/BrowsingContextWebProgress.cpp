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

NS_IMETHODIMP BrowsingContextWebProgress::GetIsTopLevel(bool* aIsTopLevel) {
  *aIsTopLevel = true;
  return NS_OK;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetIsLoadingDocument(
    bool* aIsLoadingDocument) {
  *aIsLoadingDocument = false;
  return NS_OK;
}

NS_IMETHODIMP BrowsingContextWebProgress::GetLoadType(uint32_t* aLoadType) {
  *aLoadType = 0;
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
}

////////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
BrowsingContextWebProgress::OnStateChange(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          uint32_t aStateFlags,
                                          nsresult aStatus) {
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
  UpdateAndNotifyListeners(nsIWebProgress::NOTIFY_CONTENT_BLOCKING,
                           [&](nsIWebProgressListener* listener) {
                             listener->OnContentBlockingEvent(aWebProgress,
                                                              aRequest, aEvent);
                           });
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
