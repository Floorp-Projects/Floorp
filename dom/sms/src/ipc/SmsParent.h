/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsParent_h
#define mozilla_dom_sms_SmsParent_h

#include "mozilla/dom/sms/PSmsParent.h"
#include "nsIObserver.h"

namespace mozilla {
namespace dom {
namespace sms {

class SmsParent : public PSmsParent
                , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void GetAll(nsTArray<SmsParent*>& aArray);

  SmsParent();

  virtual bool RecvHasSupport(bool* aHasSupport) MOZ_OVERRIDE;
  virtual bool RecvGetNumberOfMessagesForText(const nsString& aText, uint16_t* aResult) MOZ_OVERRIDE;
  virtual bool RecvSendMessage(const nsString& aNumber, const nsString& aMessage, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvSaveReceivedMessage(const nsString& aSender, const nsString& aBody, const uint64_t& aDate, int32_t* aId) MOZ_OVERRIDE;
  virtual bool RecvSaveSentMessage(const nsString& aRecipient, const nsString& aBody, const uint64_t& aDate, int32_t* aId) MOZ_OVERRIDE;
  virtual bool RecvGetMessage(const int32_t& aMessageId, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvDeleteMessage(const int32_t& aMessageId, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvCreateMessageList(const SmsFilterData& aFilter, const bool& aReverse, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvGetNextMessageInList(const int32_t& aListId, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvClearMessageList(const int32_t& aListId) MOZ_OVERRIDE;
  virtual bool RecvMarkMessageRead(const int32_t& aMessageId, const bool& aValue, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;

protected:
  virtual void ActorDestroy(ActorDestroyReason why);

private:
  static nsTArray<SmsParent*>* gSmsParents;
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsParent_h
