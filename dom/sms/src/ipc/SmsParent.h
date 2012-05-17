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

  NS_OVERRIDE virtual bool RecvHasSupport(bool* aHasSupport);
  NS_OVERRIDE virtual bool RecvGetNumberOfMessagesForText(const nsString& aText, PRUint16* aResult);
  NS_OVERRIDE virtual bool RecvSendMessage(const nsString& aNumber, const nsString& aMessage, const PRInt32& aRequestId, const PRUint64& aProcessId);
  NS_OVERRIDE virtual bool RecvSaveReceivedMessage(const nsString& aSender, const nsString& aBody, const PRUint64& aDate, PRInt32* aId);
  NS_OVERRIDE virtual bool RecvSaveSentMessage(const nsString& aRecipient, const nsString& aBody, const PRUint64& aDate, PRInt32* aId);
  NS_OVERRIDE virtual bool RecvGetMessage(const PRInt32& aMessageId, const PRInt32& aRequestId, const PRUint64& aProcessId);
  NS_OVERRIDE virtual bool RecvDeleteMessage(const PRInt32& aMessageId, const PRInt32& aRequestId, const PRUint64& aProcessId);
  NS_OVERRIDE virtual bool RecvCreateMessageList(const SmsFilterData& aFilter, const bool& aReverse, const PRInt32& aRequestId, const PRUint64& aProcessId);
  NS_OVERRIDE virtual bool RecvGetNextMessageInList(const PRInt32& aListId, const PRInt32& aRequestId, const PRUint64& aProcessId);
  NS_OVERRIDE virtual bool RecvClearMessageList(const PRInt32& aListId);
  NS_OVERRIDE virtual bool RecvMarkMessageRead(const PRInt32& aMessageId, const bool& aValue, const PRInt32& aRequestId, const PRUint64& aProcessId);

protected:
  virtual void ActorDestroy(ActorDestroyReason why);

private:
  static nsTArray<SmsParent*>* gSmsParents;
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsParent_h
