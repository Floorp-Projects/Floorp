/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundVideoDecodingPermissionObserver.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/StaticPrefs_media.h"
#include "MediaDecoder.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"

namespace mozilla {

BackgroundVideoDecodingPermissionObserver::
    BackgroundVideoDecodingPermissionObserver(MediaDecoder* aDecoder)
    : mDecoder(aDecoder), mIsRegisteredForEvent(false) {
  MOZ_ASSERT(mDecoder);
}

NS_IMETHODIMP
BackgroundVideoDecodingPermissionObserver::Observe(nsISupports* aSubject,
                                                   const char* aTopic,
                                                   const char16_t* aData) {
  if (!StaticPrefs::media_resume_background_video_on_tabhover()) {
    return NS_OK;
  }

  if (!IsValidEventSender(aSubject)) {
    return NS_OK;
  }

  if (strcmp(aTopic, "unselected-tab-hover") == 0) {
    bool allowed = !NS_strcmp(aData, u"true");
    mDecoder->SetIsBackgroundVideoDecodingAllowed(allowed);
  }
  return NS_OK;
}

void BackgroundVideoDecodingPermissionObserver::RegisterEvent() {
  MOZ_ASSERT(!mIsRegisteredForEvent);
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "unselected-tab-hover", false);
    mIsRegisteredForEvent = true;
    if (nsContentUtils::IsInStableOrMetaStableState()) {
      // Events shall not be fired synchronously to prevent anything visible
      // from the scripts while we are in stable state.
      if (nsCOMPtr<dom::Document> doc = GetOwnerDoc()) {
        doc->Dispatch(NewRunnableMethod(
            "BackgroundVideoDecodingPermissionObserver::EnableEvent", this,
            &BackgroundVideoDecodingPermissionObserver::EnableEvent));
      }
    } else {
      EnableEvent();
    }
  }
}

void BackgroundVideoDecodingPermissionObserver::UnregisterEvent() {
  MOZ_ASSERT(mIsRegisteredForEvent);
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "unselected-tab-hover");
    mIsRegisteredForEvent = false;
    mDecoder->SetIsBackgroundVideoDecodingAllowed(false);
    if (nsContentUtils::IsInStableOrMetaStableState()) {
      // Events shall not be fired synchronously to prevent anything visible
      // from the scripts while we are in stable state.
      if (nsCOMPtr<dom::Document> doc = GetOwnerDoc()) {
        doc->Dispatch(NewRunnableMethod(
            "BackgroundVideoDecodingPermissionObserver::DisableEvent", this,
            &BackgroundVideoDecodingPermissionObserver::DisableEvent));
      }
    } else {
      DisableEvent();
    }
  }
}

BackgroundVideoDecodingPermissionObserver::
    ~BackgroundVideoDecodingPermissionObserver() {
  MOZ_ASSERT(!mIsRegisteredForEvent);
}

void BackgroundVideoDecodingPermissionObserver::EnableEvent() const {
  // If we can't get document or outer window, then you can't reach the chrome
  // <browser> either, so we don't need want to dispatch the event.
  dom::Document* doc = GetOwnerDoc();
  if (!doc || !doc->GetWindow()) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(doc, u"UnselectedTabHover:Enable"_ns,
                               CanBubble::eYes, ChromeOnlyDispatch::eYes);
  asyncDispatcher->PostDOMEvent();
}

void BackgroundVideoDecodingPermissionObserver::DisableEvent() const {
  // If we can't get document or outer window, then you can't reach the chrome
  // <browser> either, so we don't need want to dispatch the event.
  dom::Document* doc = GetOwnerDoc();
  if (!doc || !doc->GetWindow()) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(doc, u"UnselectedTabHover:Disable"_ns,
                               CanBubble::eYes, ChromeOnlyDispatch::eYes);
  asyncDispatcher->PostDOMEvent();
}

dom::BrowsingContext* BackgroundVideoDecodingPermissionObserver::GetOwnerBC()
    const {
  dom::Document* doc = GetOwnerDoc();
  return doc ? doc->GetBrowsingContext() : nullptr;
}

dom::Document* BackgroundVideoDecodingPermissionObserver::GetOwnerDoc() const {
  if (!mDecoder->GetOwner()) {
    return nullptr;
  }

  return mDecoder->GetOwner()->GetDocument();
}

bool BackgroundVideoDecodingPermissionObserver::IsValidEventSender(
    nsISupports* aSubject) const {
  nsCOMPtr<nsPIDOMWindowInner> senderInner(do_QueryInterface(aSubject));
  if (!senderInner) {
    return false;
  }

  RefPtr<dom::BrowsingContext> senderBC = senderInner->GetBrowsingContext();
  if (!senderBC) {
    return false;
  }
  // Valid sender should be in the same browsing context tree as where owner is.
  return GetOwnerBC() ? GetOwnerBC()->Top() == senderBC->Top() : false;
}

NS_IMPL_ISUPPORTS(BackgroundVideoDecodingPermissionObserver, nsIObserver)

}  // namespace mozilla
