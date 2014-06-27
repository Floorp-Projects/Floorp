/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

namespace mozilla {
namespace dom {
namespace mobilemessage {

const char* kSmsReceivedObserverTopic        = "sms-received";
const char* kSmsRetrievingObserverTopic      = "sms-retrieving";
const char* kSmsSendingObserverTopic         = "sms-sending";
const char* kSmsSentObserverTopic            = "sms-sent";
const char* kSmsFailedObserverTopic          = "sms-failed";
const char* kSmsDeliverySuccessObserverTopic = "sms-delivery-success";
const char* kSmsDeliveryErrorObserverTopic   = "sms-delivery-error";
const char* kSilentSmsReceivedObserverTopic  = "silent-sms-received";
const char* kSmsReadSuccessObserverTopic     = "sms-read-success";
const char* kSmsReadErrorObserverTopic       = "sms-read-error";
const char* kSmsDeletedObserverTopic         = "sms-deleted";

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
