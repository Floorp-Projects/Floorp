/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_Constants_h
#define mozilla_dom_sms_Constants_h

namespace mozilla {
namespace dom {
namespace sms {

extern const char* kSmsReceivedObserverTopic;  // Defined in the .cpp.
extern const char* kSmsSentObserverTopic;      // Defined in the .cpp.
extern const char* kSmsDeliveredObserverTopic; // Defined in the .cpp.

#define DELIVERY_RECEIVED NS_LITERAL_STRING("received")
#define DELIVERY_SENT     NS_LITERAL_STRING("sent")

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_Constants_h
