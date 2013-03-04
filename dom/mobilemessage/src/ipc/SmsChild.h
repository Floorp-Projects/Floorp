/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsChild_h
#define mozilla_dom_mobilemessage_SmsChild_h

#include "mozilla/dom/mobilemessage/PSmsChild.h"
#include "mozilla/dom/mobilemessage/PSmsRequestChild.h"

class nsISmsRequest;

namespace mozilla {
namespace dom {
namespace mobilemessage {

class SmsChild : public PSmsChild
{
public:
  SmsChild();

protected:
  virtual ~SmsChild();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyReceivedMessage(const SmsMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifySendingMessage(const SmsMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifySentMessage(const SmsMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyFailedMessage(const SmsMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyDeliverySuccessMessage(const SmsMessageData& aMessage) MOZ_OVERRIDE;

  virtual bool
  RecvNotifyDeliveryErrorMessage(const SmsMessageData& aMessage) MOZ_OVERRIDE;

  virtual PSmsRequestChild*
  AllocPSmsRequest(const IPCSmsRequest& aRequest) MOZ_OVERRIDE;

  virtual bool
  DeallocPSmsRequest(PSmsRequestChild* aActor) MOZ_OVERRIDE;
};

class SmsRequestChild : public PSmsRequestChild
{
  friend class SmsChild;

  nsCOMPtr<nsISmsRequest> mReplyRequest;

public:
  SmsRequestChild(nsISmsRequest* aReplyRequest);

protected:
  virtual ~SmsRequestChild();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  Recv__delete__(const MessageReply& aReply) MOZ_OVERRIDE;
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsChild_h
