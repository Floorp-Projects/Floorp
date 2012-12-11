/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_Types_h
#define mozilla_dom_sms_Types_h

#include "IPCMessageUtils.h"

namespace mozilla {
namespace dom {
namespace sms {

// For SmsMessageData.delivery.
// Please keep the following files in sync with enum below:
// embedding/android/GeckoSmsManager.java
enum DeliveryState {
  eDeliveryState_Sent = 0,
  eDeliveryState_Received,
  eDeliveryState_Sending,
  eDeliveryState_Error,
  eDeliveryState_Unknown,
  // This state should stay at the end.
  eDeliveryState_EndGuard
};

// For SmsMessageData.deliveryStatus.
enum DeliveryStatus {
  eDeliveryStatus_NotApplicable = 0,
  eDeliveryStatus_Success,
  eDeliveryStatus_Pending,
  eDeliveryStatus_Error,
  // This state should stay at the end.
  eDeliveryStatus_EndGuard
};

// For SmsFilterData.read
enum ReadState {
  eReadState_Unknown = -1,
  eReadState_Unread,
  eReadState_Read,
  // This state should stay at the end.
  eReadState_EndGuard
};

// For SmsFilterData.messageClass
enum MessageClass {
  eMessageClass_Normal = 0,
  eMessageClass_Class0,
  eMessageClass_Class1,
  eMessageClass_Class2,
  eMessageClass_Class3,
  // This state should stay at the end.
  eMessageClass_EndGuard
};

} // namespace sms
} // namespace dom
} // namespace mozilla

namespace IPC {

/**
 * Delivery state serializer.
 */
template <>
struct ParamTraits<mozilla::dom::sms::DeliveryState>
  : public EnumSerializer<mozilla::dom::sms::DeliveryState,
                          mozilla::dom::sms::eDeliveryState_Sent,
                          mozilla::dom::sms::eDeliveryState_EndGuard>
{};

/**
 * Delivery status serializer.
 */
template <>
struct ParamTraits<mozilla::dom::sms::DeliveryStatus>
  : public EnumSerializer<mozilla::dom::sms::DeliveryStatus,
                          mozilla::dom::sms::eDeliveryStatus_NotApplicable,
                          mozilla::dom::sms::eDeliveryStatus_EndGuard>
{};

/**
 * Read state serializer.
 */
template <>
struct ParamTraits<mozilla::dom::sms::ReadState>
  : public EnumSerializer<mozilla::dom::sms::ReadState,
                          mozilla::dom::sms::eReadState_Unknown,
                          mozilla::dom::sms::eReadState_EndGuard>
{};

/**
 * Message class serializer.
 */
template <>
struct ParamTraits<mozilla::dom::sms::MessageClass>
  : public EnumSerializer<mozilla::dom::sms::MessageClass,
                          mozilla::dom::sms::eMessageClass_Normal,
                          mozilla::dom::sms::eMessageClass_EndGuard>
{};

} // namespace IPC

#endif // mozilla_dom_sms_Types_h
