/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsChild_h
#define mozilla_dom_mobilemessage_SmsChild_h

#include "mozilla/dom/mobilemessage/PSmsChild.h"
#include "mozilla/dom/mobilemessage/PSmsRequestChild.h"
#include "mozilla/dom/mobilemessage/PMobileMessageCursorChild.h"
#include "nsIDOMDOMCursor.h"
#include "nsIMobileMessageCallback.h"
#include "nsIMobileMessageCursorCallback.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

class SmsChild : public PSmsChild
{
public:
  SmsChild()
  {
    MOZ_COUNT_CTOR(SmsChild);
  }

protected:
  virtual ~SmsChild()
  {
    MOZ_COUNT_DTOR(SmsChild);
  }

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyReceivedMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyRetrievingMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifySendingMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifySentMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyFailedMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyDeliverySuccessMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyDeliveryErrorMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyReceivedSilentMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyReadSuccessMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyReadErrorMessage(const MobileMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyDeletedMessageInfo(const DeletedMessageInfoData& aDeletedInfo) MOZ_OVERRIDE;

  virtual PSmsRequestChild*
  AllocPSmsRequestChild(const IPCSmsRequest& aRequest) MOZ_OVERRIDE;

  virtual bool
  DeallocPSmsRequestChild(PSmsRequestChild* aActor) MOZ_OVERRIDE;

  virtual PMobileMessageCursorChild*
  AllocPMobileMessageCursorChild(const IPCMobileMessageCursor& aCursor) MOZ_OVERRIDE;

  virtual bool
  DeallocPMobileMessageCursorChild(PMobileMessageCursorChild* aActor) MOZ_OVERRIDE;
};

class SmsRequestChild : public PSmsRequestChild
{
  friend class SmsChild;

  nsCOMPtr<nsIMobileMessageCallback> mReplyRequest;

public:
  explicit SmsRequestChild(nsIMobileMessageCallback* aReplyRequest);

protected:
  virtual ~SmsRequestChild()
  {
    MOZ_COUNT_DTOR(SmsRequestChild);
  }

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  Recv__delete__(const MessageReply& aReply) MOZ_OVERRIDE;
};

class MobileMessageCursorChild : public PMobileMessageCursorChild
                               , public nsICursorContinueCallback
{
  friend class SmsChild;

  nsCOMPtr<nsIMobileMessageCursorCallback> mCursorCallback;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICURSORCONTINUECALLBACK

  explicit MobileMessageCursorChild(nsIMobileMessageCursorCallback* aCallback);

protected:
  virtual ~MobileMessageCursorChild()
  {
    MOZ_COUNT_DTOR(MobileMessageCursorChild);
  }

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyResult(const MobileMessageCursorData& aData) MOZ_OVERRIDE;

  virtual bool
  Recv__delete__(const int32_t& aError) MOZ_OVERRIDE;

private:
  void
  DoNotifyResult(const nsTArray<MobileMessageData>& aData);

  void
  DoNotifyResult(const nsTArray<ThreadData>& aData);
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsChild_h
