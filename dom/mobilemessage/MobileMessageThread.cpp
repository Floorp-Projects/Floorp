/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageThread.h"

#include "MobileMessageThreadInternal.h"
#include "mozilla/dom/MobileMessageThreadBinding.h"

using namespace mozilla::dom::mobilemessage;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MobileMessageThread, mWindow, mThread)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileMessageThread)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileMessageThread)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileMessageThread)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MobileMessageThread::MobileMessageThread(nsPIDOMWindowInner* aWindow,
                                         MobileMessageThreadInternal* aThread)
  : mWindow(aWindow)
  , mThread(aThread)
{
}

MobileMessageThread::~MobileMessageThread()
{
}

JSObject*
MobileMessageThread::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto)
{
  return MobileMessageThreadBinding::Wrap(aCx, this, aGivenProto);
}

uint64_t
MobileMessageThread::Id() const
{
  uint64_t id;
  mThread->GetId(&id);
  return id;
}

void
MobileMessageThread::GetLastMessageSubject(nsString& aRetVal) const
{
  mThread->GetLastMessageSubject(aRetVal);
}

void
MobileMessageThread::GetBody(nsString& aRetVal) const
{
  mThread->GetBody(aRetVal);
}

uint64_t
MobileMessageThread::UnreadCount() const
{
  uint64_t count;
  mThread->GetUnreadCount(&count);
  return count;
}

void
MobileMessageThread::GetParticipants(nsTArray<nsString>& aRetVal) const
{
  aRetVal = mThread->mData.participants();
}

DOMTimeStamp
MobileMessageThread::Timestamp() const
{
  DOMTimeStamp timestamp;
  mThread->GetTimestamp(&timestamp);
  return timestamp;
}

void
MobileMessageThread::GetLastMessageType(nsString& aRetVal) const
{
  mThread->GetLastMessageType(aRetVal);
}

} // namespace dom
} // namespace mozilla
