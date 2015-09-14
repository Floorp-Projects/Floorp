/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PresentationAvailabilityBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPresentationService.h"
#include "nsServiceManagerUtils.h"
#include "PresentationAvailability.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(PresentationAvailability)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PresentationAvailability, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PresentationAvailability, DOMEventTargetHelper)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(PresentationAvailability, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationAvailability, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationAvailability)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */ already_AddRefed<PresentationAvailability>
PresentationAvailability::Create(nsPIDOMWindow* aWindow)
{
  nsRefPtr<PresentationAvailability> availability = new PresentationAvailability(aWindow);
  return NS_WARN_IF(!availability->Init()) ? nullptr : availability.forget();
}

PresentationAvailability::PresentationAvailability(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mIsAvailable(false)
{
}

PresentationAvailability::~PresentationAvailability()
{
  Shutdown();
}

bool
PresentationAvailability::Init()
{
  // TODO Register listener for availability change.

  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  if (NS_WARN_IF(!deviceManager)) {
    return false;
  }
  deviceManager->GetDeviceAvailable(&mIsAvailable);

  return true;
}

void PresentationAvailability::Shutdown()
{
  // TODO Unregister listener for availability change.
}

/* virtual */ JSObject*
PresentationAvailability::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto)
{
  return PresentationAvailabilityBinding::Wrap(aCx, this, aGivenProto);
}

bool
PresentationAvailability::Value() const
{
  return mIsAvailable;
}
