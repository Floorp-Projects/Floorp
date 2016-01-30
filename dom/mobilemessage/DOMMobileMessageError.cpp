/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMMobileMessageError.h"
#include "mozilla/dom/DOMMobileMessageErrorBinding.h"
#include "MmsMessage.h"
#include "SmsMessage.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMMobileMessageError)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMMobileMessageError, DOMError)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSms)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMms)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMMobileMessageError, DOMError)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSms)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMms)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMMobileMessageError)
NS_INTERFACE_MAP_END_INHERITING(DOMError)

NS_IMPL_ADDREF_INHERITED(DOMMobileMessageError, DOMError)
NS_IMPL_RELEASE_INHERITED(DOMMobileMessageError, DOMError)

DOMMobileMessageError::DOMMobileMessageError(nsPIDOMWindowInner* aWindow,
                                             const nsAString& aName,
                                             SmsMessage* aSms)
  : DOMError(aWindow, aName)
  , mSms(aSms)
  , mMms(nullptr)
{
}

DOMMobileMessageError::DOMMobileMessageError(nsPIDOMWindowInner* aWindow,
                                             const nsAString& aName,
                                             MmsMessage* aMms)
  : DOMError(aWindow, aName)
  , mSms(nullptr)
  , mMms(aMms)
{
}

void
DOMMobileMessageError::GetData(OwningSmsMessageOrMmsMessage& aRetVal) const
{
  if (mSms) {
    aRetVal.SetAsSmsMessage() = mSms;
    return;
  }

  if (mMms) {
    aRetVal.SetAsMmsMessage() = mMms;
    return;
  }

  MOZ_CRASH("Bad object with invalid mSms and mMms.");
}

JSObject*
DOMMobileMessageError::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DOMMobileMessageErrorBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
