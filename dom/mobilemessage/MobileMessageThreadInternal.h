/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MobileMessageThreadInternal_h
#define mozilla_dom_mobilemessage_MobileMessageThreadInternal_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/mobilemessage/SmsTypes.h"
#include "nsIMobileMessageThread.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class MobileMessageThread;

namespace mobilemessage {

class ThreadData;

class MobileMessageThreadInternal final : public nsIMobileMessageThread
{
  // This allows the MobileMessageThread class to access data members, i.e. participants
  // without JS API.
  friend class mozilla::dom::MobileMessageThread;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGETHREAD

  MobileMessageThreadInternal(uint64_t aId,
                              const nsTArray<nsString>& aParticipants,
                              uint64_t aTimestamp,
                              const nsString& aLastMessageSubject,
                              const nsString& aBody,
                              uint64_t aUnreadCount,
                              mobilemessage::MessageType aLastMessageType);

  explicit MobileMessageThreadInternal(const ThreadData& aData);

  static nsresult Create(uint64_t aId,
                         const JS::Value& aParticipants,
                         uint64_t aTimestamp,
                         const nsAString& aLastMessageSubject,
                         const nsAString& aBody,
                         uint64_t aUnreadCount,
                         const nsAString& aLastMessageType,
                         JSContext* aCx,
                         nsIMobileMessageThread** aThread);

  const ThreadData& GetData() const { return mData; }

private:
  ~MobileMessageThreadInternal() {}

  // Don't try to use the default constructor.
  MobileMessageThreadInternal() = delete;

  ThreadData mData;
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MobileMessageThreadInternal_h
