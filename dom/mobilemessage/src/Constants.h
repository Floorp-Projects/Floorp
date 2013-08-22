/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_Constants_h
#define mozilla_dom_mobilemessage_Constants_h

namespace mozilla {
namespace dom {
namespace mobilemessage {

// Defined in the .cpp.
extern const char* kSmsReceivedObserverTopic;
extern const char* kSmsRetrievingObserverTopic;
extern const char* kSmsSendingObserverTopic;
extern const char* kSmsSentObserverTopic;
extern const char* kSmsFailedObserverTopic;
extern const char* kSmsDeliverySuccessObserverTopic;
extern const char* kSmsDeliveryErrorObserverTopic;
extern const char* kSilentSmsReceivedObserverTopic;

#define DELIVERY_RECEIVED       NS_LITERAL_STRING("received")
#define DELIVERY_SENDING        NS_LITERAL_STRING("sending")
#define DELIVERY_SENT           NS_LITERAL_STRING("sent")
#define DELIVERY_ERROR          NS_LITERAL_STRING("error")
#define DELIVERY_NOT_DOWNLOADED NS_LITERAL_STRING("not-downloaded")

#define DELIVERY_STATUS_NOT_APPLICABLE NS_LITERAL_STRING("not-applicable")
#define DELIVERY_STATUS_SUCCESS        NS_LITERAL_STRING("success")
#define DELIVERY_STATUS_PENDING        NS_LITERAL_STRING("pending")
#define DELIVERY_STATUS_ERROR          NS_LITERAL_STRING("error")
#define DELIVERY_STATUS_REJECTED       NS_LITERAL_STRING("rejected")
#define DELIVERY_STATUS_MANUAL         NS_LITERAL_STRING("manual")

#define MESSAGE_CLASS_NORMAL  NS_LITERAL_STRING("normal")
#define MESSAGE_CLASS_CLASS_0 NS_LITERAL_STRING("class-0")
#define MESSAGE_CLASS_CLASS_1 NS_LITERAL_STRING("class-1")
#define MESSAGE_CLASS_CLASS_2 NS_LITERAL_STRING("class-2")
#define MESSAGE_CLASS_CLASS_3 NS_LITERAL_STRING("class-3")

#define MESSAGE_TYPE_SMS NS_LITERAL_STRING("sms")
#define MESSAGE_TYPE_MMS NS_LITERAL_STRING("mms")

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_Constants_h
