/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MmsMessage.h"

#include "MmsMessageInternal.h"
#include "mozilla/dom/MmsMessageBinding.h"

using namespace mozilla::dom::mobilemessage;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MmsMessage, mWindow, mMessage)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MmsMessage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MmsMessage)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MmsMessage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MmsMessage::MmsMessage(nsPIDOMWindowInner* aWindow, MmsMessageInternal* aMessage)
  : mWindow(aWindow)
  , mMessage(aMessage)
{
}

MmsMessage::~MmsMessage()
{
}

JSObject*
MmsMessage::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MmsMessageBinding::Wrap(aCx, this, aGivenProto);
}

void
MmsMessage::GetType(nsString& aRetVal) const
{
  mMessage->GetType(aRetVal);
}

int32_t
MmsMessage::Id() const
{
  int32_t id;
  mMessage->GetId(&id);
  return id;
}

uint64_t
MmsMessage::ThreadId() const
{
  uint64_t id;
  mMessage->GetThreadId(&id);
  return id;
}

void
MmsMessage::GetIccId(nsString& aRetVal) const
{
  mMessage->GetIccId(aRetVal);
}

void
MmsMessage::GetDelivery(nsString& aRetVal) const
{
  mMessage->GetDelivery(aRetVal);
}

void
MmsMessage::GetDeliveryInfo(nsTArray<MmsDeliveryInfo>& aRetVal) const
{
  aRetVal = mMessage->mDeliveryInfo;
}

void
MmsMessage::GetSender(nsString& aRetVal) const
{
  mMessage->GetSender(aRetVal);
}

void
MmsMessage::GetReceivers(nsTArray<nsString>& aRetVal) const
{
  aRetVal = mMessage->mReceivers;
}

uint64_t
MmsMessage::Timestamp() const
{
  uint64_t timestamp;
  mMessage->GetTimestamp(&timestamp);
  return timestamp;
}

uint64_t
MmsMessage::SentTimestamp() const
{
  uint64_t timestamp;
  mMessage->GetSentTimestamp(&timestamp);
  return timestamp;
}

bool
MmsMessage::Read() const
{
  bool read;
  mMessage->GetRead(&read);
  return read;
}

void
MmsMessage::GetSubject(nsString& aRetVal) const
{
  mMessage->GetSubject(aRetVal);
}

void
MmsMessage::GetSmil(nsString& aRetVal) const
{
  mMessage->GetSmil(aRetVal);
}

void
MmsMessage::GetAttachments(nsTArray<MmsAttachment>& aRetVal) const
{
  uint32_t length = mMessage->mAttachments.Length();

  // Duplicating the Blob with the correct parent object.
  for (uint32_t i = 0; i < length; i++) {
    MmsAttachment attachment;
    const MmsAttachment &element = mMessage->mAttachments[i];
    attachment.mId = element.mId;
    attachment.mLocation = element.mLocation;
    attachment.mContent = Blob::Create(mWindow, element.mContent->Impl());
    aRetVal.AppendElement(attachment);
  }
}

uint64_t
MmsMessage::ExpiryDate() const
{
  uint64_t date;
  mMessage->GetExpiryDate(&date);
  return date;
}

bool
MmsMessage::ReadReportRequested() const
{
  bool reportRequested;
  mMessage->GetReadReportRequested(&reportRequested);
  return reportRequested;
}

} // namespace dom
} // namespace mozilla
