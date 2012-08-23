/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsChild_h
#define mozilla_dom_sms_SmsChild_h

#include "mozilla/dom/sms/PSmsChild.h"

namespace mozilla {
namespace dom {
namespace sms {

class SmsChild : public PSmsChild
{
public:
  virtual bool RecvNotifyReceivedMessage(const SmsMessageData& aMessage) MOZ_OVERRIDE;
  virtual bool RecvNotifySentMessage(const SmsMessageData& aMessage) MOZ_OVERRIDE;
  virtual bool RecvNotifyDeliveredMessage(const SmsMessageData& aMessage) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestSmsSent(const SmsMessageData& aMessage, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestSmsSendFailed(const int32_t& aError, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestGotSms(const SmsMessageData& aMessage, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestGetSmsFailed(const int32_t& aError, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestSmsDeleted(const bool& aDeleted, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestSmsDeleteFailed(const int32_t& aError, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestNoMessageInList(const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestCreateMessageList(const int32_t& aListId, const SmsMessageData& aMessage, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestGotNextMessage(const SmsMessageData& aMessage, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestReadListFailed(const int32_t& aError, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestMarkedMessageRead(const bool& aRead, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
  virtual bool RecvNotifyRequestMarkMessageReadFailed(const int32_t& aError, const int32_t& aRequestId, const uint64_t& aProcessId) MOZ_OVERRIDE;
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsChild_h
