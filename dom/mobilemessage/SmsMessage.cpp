/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsMessage.h"

#include "SmsMessageInternal.h"
#include "mozilla/dom/SmsMessageBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SmsMessage, mWindow, mMessage)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SmsMessage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SmsMessage)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SmsMessage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SmsMessage::SmsMessage(nsPIDOMWindowInner* aWindow, SmsMessageInternal* aMessage)
  : mWindow(aWindow)
  , mMessage(aMessage)
{
}

SmsMessage::~SmsMessage()
{
}

JSObject*
SmsMessage::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SmsMessageBinding::Wrap(aCx, this, aGivenProto);
}

void
SmsMessage::GetType(nsString& aRetVal) const
{
  mMessage->GetType(aRetVal);
}

int32_t
SmsMessage::Id() const
{
  int32_t id;
  mMessage->GetId(&id);
  return id;
}

uint64_t
SmsMessage::ThreadId() const
{
  uint64_t id;
  mMessage->GetThreadId(&id);
  return id;
}

void
SmsMessage::GetIccId(nsString& aRetVal) const
{
  mMessage->GetIccId(aRetVal);
}

void
SmsMessage::GetDelivery(nsString& aRetVal) const
{
  mMessage->GetDelivery(aRetVal);
}

void
SmsMessage::GetDeliveryStatus(nsString& aRetVal) const
{
  mMessage->GetDeliveryStatus(aRetVal);
}

void
SmsMessage::GetSender(nsString& aRetVal) const
{
  mMessage->GetSender(aRetVal);
}

void
SmsMessage::GetReceiver(nsString& aRetVal) const
{
  mMessage->GetReceiver(aRetVal);
}

void
SmsMessage::GetBody(nsString& aRetVal) const
{
  mMessage->GetBody(aRetVal);
}

void
SmsMessage::GetMessageClass(nsString& aRetVal) const
{
  mMessage->GetMessageClass(aRetVal);
}

uint64_t
SmsMessage::Timestamp() const
{
  uint64_t timestamp;
  mMessage->GetTimestamp(&timestamp);
  return timestamp;
}

uint64_t
SmsMessage::SentTimestamp() const
{
  uint64_t timestamp;
  mMessage->GetSentTimestamp(&timestamp);
  return timestamp;
}

uint64_t
SmsMessage::DeliveryTimestamp() const
{
  uint64_t timestamp;
  mMessage->GetDeliveryTimestamp(&timestamp);
  return timestamp;
}

bool
SmsMessage::Read() const
{
  bool read;
  mMessage->GetRead(&read);
  return read;
}

} // namespace dom
} // namespace mozilla
