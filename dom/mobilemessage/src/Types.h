/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_Types_h
#define mozilla_dom_mobilemessage_Types_h

#include "IPCMessageUtils.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

// For MmsMessageData.state and SmsMessageData.deliveryState
// Please keep the following files in sync with enum below:
// mobile/android/base/GeckoSmsManager.java
enum DeliveryState {
  eDeliveryState_Sent = 0,
  eDeliveryState_Received,
  eDeliveryState_Sending,
  eDeliveryState_Error,
  eDeliveryState_Unknown,
  eDeliveryState_NotDownloaded,
  // This state should stay at the end.
  eDeliveryState_EndGuard
};

// For {Mms,Sms}MessageData.deliveryStatus.
enum DeliveryStatus {
  eDeliveryStatus_NotApplicable = 0,
  eDeliveryStatus_Success,
  eDeliveryStatus_Pending,
  eDeliveryStatus_Error,
  // This state should stay at the end.
  eDeliveryStatus_EndGuard
};

// For {Mms,Sms}FilterData.read.
enum ReadState {
  eReadState_Unknown = -1,
  eReadState_Unread,
  eReadState_Read,
  // This state should stay at the end.
  eReadState_EndGuard
};

// For {Mms,Sms}FilterData.messageClass.
enum MessageClass {
  eMessageClass_Normal = 0,
  eMessageClass_Class0,
  eMessageClass_Class1,
  eMessageClass_Class2,
  eMessageClass_Class3,
  // This state should stay at the end.
  eMessageClass_EndGuard
};

// For ThreadData.
enum MessageType {
  eMessageType_SMS = 0,
  eMessageType_MMS,
  // This state should stay at the end.
  eMessageType_EndGuard
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

namespace IPC {

/**
 * Delivery state serializer.
 */
template <>
struct ParamTraits<mozilla::dom::mobilemessage::DeliveryState>
  : public EnumSerializer<mozilla::dom::mobilemessage::DeliveryState,
                          mozilla::dom::mobilemessage::eDeliveryState_Sent,
                          mozilla::dom::mobilemessage::eDeliveryState_EndGuard>
{};

/**
 * Delivery status serializer.
 */
template <>
struct ParamTraits<mozilla::dom::mobilemessage::DeliveryStatus>
  : public EnumSerializer<mozilla::dom::mobilemessage::DeliveryStatus,
                          mozilla::dom::mobilemessage::eDeliveryStatus_NotApplicable,
                          mozilla::dom::mobilemessage::eDeliveryStatus_EndGuard>
{};

/**
 * Read state serializer.
 */
template <>
struct ParamTraits<mozilla::dom::mobilemessage::ReadState>
  : public EnumSerializer<mozilla::dom::mobilemessage::ReadState,
                          mozilla::dom::mobilemessage::eReadState_Unknown,
                          mozilla::dom::mobilemessage::eReadState_EndGuard>
{};

/**
 * Message class serializer.
 */
template <>
struct ParamTraits<mozilla::dom::mobilemessage::MessageClass>
  : public EnumSerializer<mozilla::dom::mobilemessage::MessageClass,
                          mozilla::dom::mobilemessage::eMessageClass_Normal,
                          mozilla::dom::mobilemessage::eMessageClass_EndGuard>
{};

/**
 * MessageType class serializer.
 */
template <>
struct ParamTraits<mozilla::dom::mobilemessage::MessageType>
  : public EnumSerializer<mozilla::dom::mobilemessage::MessageType,
                          mozilla::dom::mobilemessage::eMessageType_SMS,
                          mozilla::dom::mobilemessage::eMessageType_EndGuard>
{};

} // namespace IPC

#endif // mozilla_dom_mobilemessage_Types_h
