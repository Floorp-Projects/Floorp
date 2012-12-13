/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_Constants_h
#define mozilla_dom_sms_Constants_h

namespace mozilla {
namespace dom {
namespace sms {

// Defined in the .cpp.
extern const char* kSmsReceivedObserverTopic;
extern const char* kSmsSendingObserverTopic;
extern const char* kSmsSentObserverTopic;
extern const char* kSmsFailedObserverTopic;
extern const char* kSmsDeliverySuccessObserverTopic;
extern const char* kSmsDeliveryErrorObserverTopic;

#define DELIVERY_RECEIVED NS_LITERAL_STRING("received")
#define DELIVERY_SENDING  NS_LITERAL_STRING("sending")
#define DELIVERY_SENT     NS_LITERAL_STRING("sent")
#define DELIVERY_ERROR    NS_LITERAL_STRING("error")

#define DELIVERY_STATUS_NOT_APPLICABLE NS_LITERAL_STRING("not-applicable")
#define DELIVERY_STATUS_SUCCESS        NS_LITERAL_STRING("success")
#define DELIVERY_STATUS_PENDING        NS_LITERAL_STRING("pending")
#define DELIVERY_STATUS_ERROR          NS_LITERAL_STRING("error")

#define MESSAGE_CLASS_NORMAL  NS_LITERAL_STRING("normal")
#define MESSAGE_CLASS_CLASS_0 NS_LITERAL_STRING("class-0")
#define MESSAGE_CLASS_CLASS_1 NS_LITERAL_STRING("class-1")
#define MESSAGE_CLASS_CLASS_2 NS_LITERAL_STRING("class-2")
#define MESSAGE_CLASS_CLASS_3 NS_LITERAL_STRING("class-3")

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_Constants_h
