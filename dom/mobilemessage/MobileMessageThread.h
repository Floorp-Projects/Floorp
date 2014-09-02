/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MobileMessageThread_h
#define mozilla_dom_mobilemessage_MobileMessageThread_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/mobilemessage/SmsTypes.h"
#include "nsIDOMMozMobileMessageThread.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class MobileMessageThread MOZ_FINAL : public nsIDOMMozMobileMessageThread
{
private:
  typedef mobilemessage::ThreadData ThreadData;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZMOBILEMESSAGETHREAD

  MobileMessageThread(uint64_t aId,
                      const nsTArray<nsString>& aParticipants,
                      uint64_t aTimestamp,
                      const nsString& aLastMessageSubject,
                      const nsString& aBody,
                      uint64_t aUnreadCount,
                      mobilemessage::MessageType aLastMessageType);

  explicit MobileMessageThread(const ThreadData& aData);

  static nsresult Create(uint64_t aId,
                         const JS::Value& aParticipants,
                         uint64_t aTimestamp,
                         const nsAString& aLastMessageSubject,
                         const nsAString& aBody,
                         uint64_t aUnreadCount,
                         const nsAString& aLastMessageType,
                         JSContext* aCx,
                         nsIDOMMozMobileMessageThread** aThread);

  const ThreadData& GetData() const { return mData; }

private:
  ~MobileMessageThread() {}

  // Don't try to use the default constructor.
  MobileMessageThread() MOZ_DELETE;

  ThreadData mData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MobileMessageThread_h
