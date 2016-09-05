/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationAvailability.h"

#include "mozilla/dom/PresentationAvailabilityBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPresentationService.h"
#include "nsServiceManagerUtils.h"
#include "PresentationLog.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationAvailability)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PresentationAvailability, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromises)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PresentationAvailability, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromises);
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(PresentationAvailability, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationAvailability, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationAvailability)
  NS_INTERFACE_MAP_ENTRY(nsIPresentationAvailabilityListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */ already_AddRefed<PresentationAvailability>
PresentationAvailability::Create(nsPIDOMWindowInner* aWindow,
                                 const nsTArray<nsString>& aUrls,
                                 RefPtr<Promise>& aPromise)
{
  RefPtr<PresentationAvailability> availability =
    new PresentationAvailability(aWindow, aUrls);
  return NS_WARN_IF(!availability->Init(aPromise)) ? nullptr
                                                   : availability.forget();
}

PresentationAvailability::PresentationAvailability(nsPIDOMWindowInner* aWindow,
                                                   const nsTArray<nsString>& aUrls)
  : DOMEventTargetHelper(aWindow)
  , mIsAvailable(false)
  , mUrls(aUrls)
{
}

PresentationAvailability::~PresentationAvailability()
{
  Shutdown();
}

bool
PresentationAvailability::Init(RefPtr<Promise>& aPromise)
{
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return false;
  }

  nsresult rv = service->RegisterAvailabilityListener(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // If the user agent is unable to monitor available device,
    // Resolve promise with |value| set to false.
    mIsAvailable = false;
    aPromise->MaybeResolve(this);
    return true;
  }

  EnqueuePromise(aPromise);

  AvailabilityCollection* collection = AvailabilityCollection::GetSingleton();
  if (collection) {
    collection->Add(this);
  }

  return true;
}

void PresentationAvailability::Shutdown()
{
  AvailabilityCollection* collection = AvailabilityCollection::GetSingleton();
  if (collection ) {
    collection->Remove(this);
  }

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return;
  }

  nsresult rv = service->UnregisterAvailabilityListener(this);
  NS_WARN_IF(NS_FAILED(rv));
}

/* virtual */ void
PresentationAvailability::DisconnectFromOwner()
{
  Shutdown();
  DOMEventTargetHelper::DisconnectFromOwner();
}

/* virtual */ JSObject*
PresentationAvailability::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto)
{
  return PresentationAvailabilityBinding::Wrap(aCx, this, aGivenProto);
}

bool
PresentationAvailability::Equals(const uint64_t aWindowID,
                                 const nsTArray<nsString>& aUrls) const
{
  if (GetOwner() && GetOwner()->WindowID() == aWindowID &&
      mUrls.Length() == aUrls.Length()) {
    for (const auto& url : aUrls) {
      if (!mUrls.Contains(url)) {
        return false;
      }
    }
    return true;
  }

  return false;
}

bool
PresentationAvailability::IsCachedValueReady()
{
  // All pending promises will be solved when cached value is ready and
  // no promise should be enqueued afterward.
  return mPromises.IsEmpty();
}

void
PresentationAvailability::EnqueuePromise(RefPtr<Promise>& aPromise)
{
  mPromises.AppendElement(aPromise);
}

bool
PresentationAvailability::Value() const
{
  return mIsAvailable;
}

NS_IMETHODIMP
PresentationAvailability::NotifyAvailableChange(bool aIsAvailable)
{
  return NS_DispatchToCurrentThread(NewRunnableMethod
                                    <bool>(this,
                                           &PresentationAvailability::UpdateAvailabilityAndDispatchEvent,
                                           aIsAvailable));
}

void
PresentationAvailability::UpdateAvailabilityAndDispatchEvent(bool aIsAvailable)
{
  PRES_DEBUG("%s\n", __func__);
  bool isChanged = (aIsAvailable != mIsAvailable);

  mIsAvailable = aIsAvailable;

  if (!mPromises.IsEmpty()) {
    // Use the first availability change notification to resolve promise.
    do {
      nsTArray<RefPtr<Promise>> promises = Move(mPromises);
      for (auto& promise : promises) {
        promise->MaybeResolve(this);
      }
      // more promises may have been added to mPromises, at least in theory
    } while (!mPromises.IsEmpty());

    return;
  }

  if (isChanged) {
    NS_WARN_IF(NS_FAILED(DispatchTrustedEvent(NS_LITERAL_STRING("change"))));
  }
}
