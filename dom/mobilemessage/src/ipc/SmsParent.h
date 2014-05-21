/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsParent_h
#define mozilla_dom_mobilemessage_SmsParent_h

#include "mozilla/dom/mobilemessage/PSmsParent.h"
#include "mozilla/dom/mobilemessage/PSmsRequestParent.h"
#include "mozilla/dom/mobilemessage/PMobileMessageCursorParent.h"
#include "nsIDOMDOMCursor.h"
#include "nsIMobileMessageCallback.h"
#include "nsIMobileMessageCursorCallback.h"
#include "nsIObserver.h"

namespace mozilla {
namespace dom {

class ContentParent;

namespace mobilemessage {

class SmsParent : public PSmsParent
                , public nsIObserver
{
  friend class mozilla::dom::ContentParent;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

protected:
  virtual bool
  RecvAddSilentNumber(const nsString& aNumber) MOZ_OVERRIDE;

  virtual bool
  RecvRemoveSilentNumber(const nsString& aNumber) MOZ_OVERRIDE;

  SmsParent();
  virtual ~SmsParent()
  {
    MOZ_COUNT_DTOR(SmsParent);
  }

  virtual void
  ActorDestroy(ActorDestroyReason why);

  virtual bool
  RecvPSmsRequestConstructor(PSmsRequestParent* aActor,
                             const IPCSmsRequest& aRequest) MOZ_OVERRIDE;

  virtual PSmsRequestParent*
  AllocPSmsRequestParent(const IPCSmsRequest& aRequest) MOZ_OVERRIDE;

  virtual bool
  DeallocPSmsRequestParent(PSmsRequestParent* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvPMobileMessageCursorConstructor(PMobileMessageCursorParent* aActor,
                                      const IPCMobileMessageCursor& aCursor) MOZ_OVERRIDE;

  virtual PMobileMessageCursorParent*
  AllocPMobileMessageCursorParent(const IPCMobileMessageCursor& aCursor) MOZ_OVERRIDE;

  virtual bool
  DeallocPMobileMessageCursorParent(PMobileMessageCursorParent* aActor) MOZ_OVERRIDE;

  bool
  GetMobileMessageDataFromMessage(nsISupports* aMsg, MobileMessageData& aData);

private:
  nsTArray<nsString> mSilentNumbers;
};

class SmsRequestParent : public PSmsRequestParent
                       , public nsIMobileMessageCallback
{
  friend class SmsParent;

  bool mActorDestroyed;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGECALLBACK

protected:
  SmsRequestParent()
    : mActorDestroyed(false)
  {
    MOZ_COUNT_CTOR(SmsRequestParent);
  }

  virtual ~SmsRequestParent()
  {
    MOZ_COUNT_DTOR(SmsRequestParent);
  }

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  bool
  DoRequest(const SendMessageRequest& aRequest);

  bool
  DoRequest(const RetrieveMessageRequest& aRequest);

  bool
  DoRequest(const GetMessageRequest& aRequest);

  bool
  DoRequest(const DeleteMessageRequest& aRequest);

  bool
  DoRequest(const MarkMessageReadRequest& aRequest);

  bool
  DoRequest(const GetSegmentInfoForTextRequest& aRequest);

  bool
  DoRequest(const GetSmscAddressRequest& aRequest);

  nsresult
  SendReply(const MessageReply& aReply);
};

class MobileMessageCursorParent : public PMobileMessageCursorParent
                                , public nsIMobileMessageCursorCallback
{
  friend class SmsParent;

  nsCOMPtr<nsICursorContinueCallback> mContinueCallback;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGECURSORCALLBACK

protected:
  MobileMessageCursorParent()
  {
    MOZ_COUNT_CTOR(MobileMessageCursorParent);
  }

  virtual ~MobileMessageCursorParent()
  {
    MOZ_COUNT_DTOR(MobileMessageCursorParent);
  }

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvContinue() MOZ_OVERRIDE;

  bool
  DoRequest(const CreateMessageCursorRequest& aRequest);

  bool
  DoRequest(const CreateThreadCursorRequest& aRequest);
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsParent_h
