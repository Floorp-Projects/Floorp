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

void BrowsingContextWebProgress::ContextDiscarded() {
  if (!mAwaitingStop) {
    return;
  }

  // This matches what nsDocLoader::doStopDocumentLoad does, except we don't
  // bother notifying for `STATE_STOP | STATE_IS_DOCUMENT`,
  // nsBrowserStatusFilter would filter it out before it gets to the parent
  // process.
  const int32_t flags = STATE_STOP | STATE_IS_WINDOW | STATE_IS_NETWORK;
  UpdateAndNotifyListeners(((flags >> 16) & nsIWebProgress::NOTIFY_STATE_ALL),
                           [&](nsIWebProgressListener* listener) {
                             // TODO(emilio): We might want to stash the
                             // request from OnStateChange on a member or
                             // something if having no request causes trouble
                             // here. Ditto for the webprogress instance.
                             listener->OnStateChange(this,
                                                     /* aRequest = */ nullptr,
                                                     flags, NS_ERROR_ABORT);
                           });
}

////////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP
BrowsingContextWebProgress::OnStateChange(nsIWebProgress* aWebProgress,
                                          nsIRequest* aRequest,
                                          uint32_t aStateFlags,
                                          nsresult aStatus) {
  const uint32_t startDocumentFlags =
      nsIWebProgressListener::STATE_START |
      nsIWebProgressListener::STATE_IS_DOCUMENT |
      nsIWebProgressListener::STATE_IS_REQUEST |
      nsIWebProgressListener::STATE_IS_WINDOW |
      nsIWebProgressListener::STATE_IS_NETWORK;
  bool isTopLevel = false;
  nsresult rv = aWebProgress->GetIsTopLevel(&isTopLevel);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isTopLevelStartDocumentEvent =
      (aStateFlags & startDocumentFlags) == startDocumentFlags && isTopLevel;
  // If we receive a matching STATE_START for a top-level document event,
  // and we are currently not suspending this event, start suspending all
  // further matching STATE_START events after this one.
  if (isTopLevelStartDocumentEvent && !mAwaitingStop) {
    mAwaitingStop = true;
  } else if (mAwaitingStop) {
    // If we are currently suspending matching STATE_START events, check if this
    // is a corresponding STATE_STOP event.
    const uint32_t stopWindowFlags = nsIWebProgressListener::STATE_STOP |
                                     nsIWebProgressListener::STATE_IS_WINDOW;
    bool isTopLevelStopWindowEvent =
        (aStateFlags & stopWindowFlags) == stopWindowFlags && isTopLevel;
    if (isTopLevelStopWindowEvent) {
      // If this is a STATE_STOP event corresponding to the initial STATE_START
      // event, stop suspending matching STATE_START events.
      mAwaitingStop = false;
    } else if (isTopLevelStartDocumentEvent) {
      // We have received a matching STATE_START event at least twice, but
      // haven't received the corresponding STATE_STOP event for the first one.
      // Don't let this event through. This is probably a duplicate event from
      // the new BrowserParent due to a process switch.
      return NS_OK;
    }
    // Allow all other progress events that don't match top-level start document
    // flags.
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
