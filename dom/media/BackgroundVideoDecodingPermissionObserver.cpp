/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundVideoDecodingPermissionObserver.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/StaticPrefs.h"
#include "MediaDecoder.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"

namespace mozilla {

BackgroundVideoDecodingPermissionObserver::
  BackgroundVideoDecodingPermissionObserver(MediaDecoder* aDecoder)
  : mDecoder(aDecoder)
  , mIsRegisteredForEvent(false)
{
  MOZ_ASSERT(mDecoder);
}

NS_IMETHODIMP
BackgroundVideoDecodingPermissionObserver::Observe(nsISupports* aSubject,
                                                   const char* aTopic,
                                                   const char16_t* aData)
{
  if (!StaticPrefs::MediaResumeBkgndVideoOnTabhover()) {
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

void
BackgroundVideoDecodingPermissionObserver::RegisterEvent()
{
  MOZ_ASSERT(!mIsRegisteredForEvent);
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "unselected-tab-hover", false);
    mIsRegisteredForEvent = true;
    if (nsContentUtils::IsInStableOrMetaStableState()) {
      // Events shall not be fired synchronously to prevent anything visible
      // from the scripts while we are in stable state.
      if (nsCOMPtr<nsIDocument> doc = GetOwnerDoc()) {
        doc->Dispatch(
          TaskCategory::Other,
          NewRunnableMethod(
            "BackgroundVideoDecodingPermissionObserver::"
            "EnableEvent",
            this,
            &BackgroundVideoDecodingPermissionObserver::
              EnableEvent));
      }
    } else {
      EnableEvent();
    }
  }
}

void
BackgroundVideoDecodingPermissionObserver::UnregisterEvent()
{
  MOZ_ASSERT(mIsRegisteredForEvent);
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "unselected-tab-hover");
    mIsRegisteredForEvent = false;
    mDecoder->SetIsBackgroundVideoDecodingAllowed(false);
    if (nsContentUtils::IsInStableOrMetaStableState()) {
      // Events shall not be fired synchronously to prevent anything visible
      // from the scripts while we are in stable state.
      if (nsCOMPtr<nsIDocument> doc = GetOwnerDoc()) {
        doc->Dispatch(
          TaskCategory::Other,
          NewRunnableMethod(
            "BackgroundVideoDecodingPermissionObserver::"
            "DisableEvent",
            this,
            &BackgroundVideoDecodingPermissionObserver::
              DisableEvent));
      }
    } else {
      DisableEvent();
    }
  }
}

BackgroundVideoDecodingPermissionObserver::
  ~BackgroundVideoDecodingPermissionObserver()
{
  MOZ_ASSERT(!mIsRegisteredForEvent);
}

void
BackgroundVideoDecodingPermissionObserver::EnableEvent() const
{
  nsIDocument* doc = GetOwnerDoc();
  if (!doc) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(doc,
                             NS_LITERAL_STRING("UnselectedTabHover:Enable"),
                             /* Bubbles */ true,
                             /* OnlyChromeDispatch */ true);
  asyncDispatcher->PostDOMEvent();
}

void
BackgroundVideoDecodingPermissionObserver::DisableEvent() const
{
  nsIDocument* doc = GetOwnerDoc();
  if (!doc) {
    return;
  }

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(doc,
                             NS_LITERAL_STRING("UnselectedTabHover:Disable"),
                             /* Bubbles */ true,
                             /* OnlyChromeDispatch */ true);
  asyncDispatcher->PostDOMEvent();
}

already_AddRefed<nsPIDOMWindowOuter>
BackgroundVideoDecodingPermissionObserver::GetOwnerWindow() const
{
  nsIDocument* doc = GetOwnerDoc();
  if (!doc) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> innerWin = doc->GetInnerWindow();
  if (!innerWin) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> outerWin = innerWin->GetOuterWindow();
  if (!outerWin) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> topWin = outerWin->GetTop();
  return topWin.forget();
}

nsIDocument*
BackgroundVideoDecodingPermissionObserver::GetOwnerDoc() const
{
  if (!mDecoder->GetOwner()) {
    return nullptr;
  }

  return mDecoder->GetOwner()->GetDocument();
}

bool
BackgroundVideoDecodingPermissionObserver::IsValidEventSender(
  nsISupports* aSubject) const
{
  nsCOMPtr<nsPIDOMWindowInner> senderInner(do_QueryInterface(aSubject));
  if (!senderInner) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> senderOuter = senderInner->GetOuterWindow();
  if (!senderOuter) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> senderTop = senderOuter->GetTop();
  if (!senderTop) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> ownerTop = GetOwnerWindow();
  if (!ownerTop) {
    return false;
  }

  return ownerTop == senderTop;
}

NS_IMPL_ISUPPORTS(BackgroundVideoDecodingPermissionObserver, nsIObserver)

} // namespace mozilla
