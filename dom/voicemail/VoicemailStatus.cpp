/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VoicemailStatus.h"

#include "mozilla/dom/MozVoicemailStatusBinding.h"
#include "nsIVoicemailService.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

// mProvider is owned by internal service.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(VoicemailStatus, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(VoicemailStatus)
NS_IMPL_CYCLE_COLLECTING_RELEASE(VoicemailStatus)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VoicemailStatus)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

VoicemailStatus::VoicemailStatus(nsISupports* aParent,
                                 nsIVoicemailProvider* aProvider)
  : mParent(aParent)
  , mProvider(aProvider)
{
  MOZ_ASSERT(mParent);
  MOZ_ASSERT(mProvider);
}

JSObject*
VoicemailStatus::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozVoicemailStatusBinding::Wrap(aCx, this, aGivenProto);
}

uint32_t
VoicemailStatus::ServiceId() const
{
  uint32_t result = 0;
  mProvider->GetServiceId(&result);
  return result;
}

bool
VoicemailStatus::HasMessages() const
{
  bool result = false;
  mProvider->GetHasMessages(&result);
  return result;
}

int32_t
VoicemailStatus::MessageCount() const
{
  int32_t result = 0;
  mProvider->GetMessageCount(&result);
  return result;
}

void
VoicemailStatus::GetReturnNumber(nsString& aReturnNumber) const
{
  aReturnNumber.SetIsVoid(true);
  mProvider->GetReturnNumber(aReturnNumber);
}

void
VoicemailStatus::GetReturnMessage(nsString& aReturnMessage) const
{
  aReturnMessage.SetIsVoid(true);
  mProvider->GetReturnMessage(aReturnMessage);
}

} // namespace dom
} // namespace mozilla
