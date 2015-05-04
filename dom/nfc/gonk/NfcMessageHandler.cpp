/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NfcMessageHandler.h"
#include <binder/Parcel.h>
#include "mozilla/dom/MozNDEFRecordBinding.h"
#include "nsDebug.h"
#include "NfcOptions.h"
#include "mozilla/unused.h"

#include <android/log.h>
#define NMH_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "NfcMessageHandler", args)

#define NFCD_MAJOR_VERSION 1
#define NFCD_MINOR_VERSION 22

using namespace android;
using namespace mozilla;
using namespace mozilla::dom;

bool
NfcMessageHandler::Marshall(Parcel& aParcel, const CommandOptions& aOptions)
{
  bool result;
  switch (aOptions.mType) {
    case NfcRequestType::ChangeRFState:
      result = ChangeRFStateRequest(aParcel, aOptions);
      break;
    case NfcRequestType::ReadNDEF:
      result = ReadNDEFRequest(aParcel, aOptions);
      break;
    case NfcRequestType::WriteNDEF:
      result = WriteNDEFRequest(aParcel, aOptions);
      break;
    case NfcRequestType::MakeReadOnly:
      result = MakeReadOnlyRequest(aParcel, aOptions);
      break;
    case NfcRequestType::Format:
      result = FormatRequest(aParcel, aOptions);
      break;
    case NfcRequestType::Transceive:
      result = TransceiveRequest(aParcel, aOptions);
      break;
    default:
      result = false;
      break;
  };

  return result;
}

bool
NfcMessageHandler::Unmarshall(const Parcel& aParcel, EventOptions& aOptions)
{
  mozilla::unused << htonl(aParcel.readInt32());  // parcel size
  int32_t type = aParcel.readInt32();
  bool isNtf = type >> 31;
  int32_t msgType = type & ~(1 << 31);

  return isNtf ? ProcessNotification(msgType, aParcel, aOptions) :
                 ProcessResponse(msgType, aParcel, aOptions);
}

bool
NfcMessageHandler::ProcessResponse(int32_t aType, const Parcel& aParcel, EventOptions& aOptions)
{
  bool result;
  aOptions.mRspType = static_cast<NfcResponseType>(aType);
  switch (aOptions.mRspType) {
    case NfcResponseType::ChangeRFStateRsp:
      result = ChangeRFStateResponse(aParcel, aOptions);
      break;
    case NfcResponseType::ReadNDEFRsp:
      result = ReadNDEFResponse(aParcel, aOptions);
      break;
    case NfcResponseType::WriteNDEFRsp: // Fall through.
    case NfcResponseType::MakeReadOnlyRsp:
    case NfcResponseType::FormatRsp:
      result = GeneralResponse(aParcel, aOptions);
      break;
    case NfcResponseType::TransceiveRsp:
      result = TransceiveResponse(aParcel, aOptions);
      break;
    default:
      result = false;
  }

  return result;
}

bool
NfcMessageHandler::ProcessNotification(int32_t aType, const Parcel& aParcel, EventOptions& aOptions)
{
  bool result;
  aOptions.mNtfType = static_cast<NfcNotificationType>(aType);
  switch (aOptions.mNtfType) {
    case NfcNotificationType::Initialized:
      result = InitializeNotification(aParcel, aOptions);
      break;
    case NfcNotificationType::TechDiscovered:
      result = TechDiscoveredNotification(aParcel, aOptions);
      break;
    case NfcNotificationType::TechLost:
      result = TechLostNotification(aParcel, aOptions);
      break;
    case NfcNotificationType::HciEventTransaction:
      result = HCIEventTransactionNotification(aParcel, aOptions);
      break;
    case NfcNotificationType::NdefReceived:
      result = NDEFReceivedNotification(aParcel, aOptions);
      break;
    default:
      result = false;
      break;
  }

  return result;
}

bool
NfcMessageHandler::GeneralResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mErrorCode = aParcel.readInt32();
  aOptions.mSessionId = aParcel.readInt32();

  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);
  return true;
}

bool
NfcMessageHandler::ChangeRFStateRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(static_cast<int32_t>(NfcRequestType::ChangeRFState));
  aParcel.writeInt32(aOptions.mRfState);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::ChangeRFStateResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mErrorCode = aParcel.readInt32();
  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);

  aOptions.mRfState = aParcel.readInt32();
  return true;
}

bool
NfcMessageHandler::ReadNDEFRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(static_cast<int32_t>(NfcRequestType::ReadNDEF));
  aParcel.writeInt32(aOptions.mSessionId);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::ReadNDEFResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mErrorCode = aParcel.readInt32();
  aOptions.mSessionId = aParcel.readInt32();

  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);

  if (aOptions.mErrorCode == 0) {
    ReadNDEFMessage(aParcel, aOptions);
  }

  return true;
}

bool
NfcMessageHandler::TransceiveRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(static_cast<int32_t>(NfcRequestType::Transceive));
  aParcel.writeInt32(aOptions.mSessionId);
  aParcel.writeInt32(aOptions.mTechnology);

  uint32_t length = aOptions.mCommand.Length();
  aParcel.writeInt32(length);

  void* data = aParcel.writeInplace(length);
  memcpy(data, aOptions.mCommand.Elements(), length);

  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::TransceiveResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mErrorCode = aParcel.readInt32();
  aOptions.mSessionId = aParcel.readInt32();

  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);

  if (aOptions.mErrorCode == 0) {
    ReadTransceiveResponse(aParcel, aOptions);
  }

  return true;
}

bool
NfcMessageHandler::WriteNDEFRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(static_cast<int32_t>(NfcRequestType::WriteNDEF));
  aParcel.writeInt32(aOptions.mSessionId);
  aParcel.writeInt32(aOptions.mIsP2P);
  WriteNDEFMessage(aParcel, aOptions);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::MakeReadOnlyRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(static_cast<int32_t>(NfcRequestType::MakeReadOnly));
  aParcel.writeInt32(aOptions.mSessionId);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::FormatRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(static_cast<int32_t>(NfcRequestType::Format));
  aParcel.writeInt32(aOptions.mSessionId);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::InitializeNotification(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mStatus = aParcel.readInt32();
  aOptions.mMajorVersion = aParcel.readInt32();
  aOptions.mMinorVersion = aParcel.readInt32();

  if (aOptions.mMajorVersion != NFCD_MAJOR_VERSION ||
      aOptions.mMinorVersion != NFCD_MINOR_VERSION) {
    NMH_LOG("NFCD version mismatched. majorVersion: %d, minorVersion: %d",
            aOptions.mMajorVersion, aOptions.mMinorVersion);
  }

  return true;
}

bool
NfcMessageHandler::TechDiscoveredNotification(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mSessionId = aParcel.readInt32();
  aOptions.mIsP2P = aParcel.readInt32();

  int32_t techCount = aParcel.readInt32();
  aOptions.mTechList.AppendElements(
    static_cast<const uint8_t*>(aParcel.readInplace(techCount)), techCount);

  int32_t idCount = aParcel.readInt32();
  aOptions.mTagId.AppendElements(
    static_cast<const uint8_t*>(aParcel.readInplace(idCount)), idCount);

  int32_t ndefMsgCount = aParcel.readInt32();
  if (ndefMsgCount != 0) {
    ReadNDEFMessage(aParcel, aOptions);
  }

  int32_t ndefInfo = aParcel.readInt32();
  if (ndefInfo) {
    aOptions.mTagType = aParcel.readInt32();
    aOptions.mMaxNDEFSize = aParcel.readInt32();
    aOptions.mIsReadOnly = aParcel.readInt32();
    aOptions.mIsFormatable = aParcel.readInt32();
  }

  return true;
}

bool
NfcMessageHandler::TechLostNotification(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mSessionId = aParcel.readInt32();
  return true;
}

bool
NfcMessageHandler::HCIEventTransactionNotification(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mOriginType = aParcel.readInt32();
  aOptions.mOriginIndex = aParcel.readInt32();

  int32_t aidLength = aParcel.readInt32();
  aOptions.mAid.AppendElements(
    static_cast<const uint8_t*>(aParcel.readInplace(aidLength)), aidLength);

  int32_t payloadLength = aParcel.readInt32();
  aOptions.mPayload.AppendElements(
    static_cast<const uint8_t*>(aParcel.readInplace(payloadLength)), payloadLength);

  return true;
}

bool
NfcMessageHandler::NDEFReceivedNotification(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mSessionId = aParcel.readInt32();
  int32_t ndefMsgCount = aParcel.readInt32();
  if (ndefMsgCount != 0) {
    ReadNDEFMessage(aParcel, aOptions);
  }

  return true;
}

bool
NfcMessageHandler::ReadNDEFMessage(const Parcel& aParcel, EventOptions& aOptions)
{
  int32_t recordCount = aParcel.readInt32();
  aOptions.mRecords.SetCapacity(recordCount);

  for (int i = 0; i < recordCount; i++) {
    int32_t tnf = aParcel.readInt32();
    NDEFRecordStruct record;
    record.mTnf = static_cast<TNF>(tnf);

    int32_t typeLength = aParcel.readInt32();
    record.mType.AppendElements(
      static_cast<const uint8_t*>(aParcel.readInplace(typeLength)), typeLength);

    int32_t idLength = aParcel.readInt32();
    record.mId.AppendElements(
      static_cast<const uint8_t*>(aParcel.readInplace(idLength)), idLength);

    int32_t payloadLength = aParcel.readInt32();
    record.mPayload.AppendElements(
      static_cast<const uint8_t*>(aParcel.readInplace(payloadLength)), payloadLength);

    aOptions.mRecords.AppendElement(record);
  }

  return true;
}

bool
NfcMessageHandler::WriteNDEFMessage(Parcel& aParcel, const CommandOptions& aOptions)
{
  int recordCount = aOptions.mRecords.Length();
  aParcel.writeInt32(recordCount);
  for (int i = 0; i < recordCount; i++) {
    const NDEFRecordStruct& record = aOptions.mRecords[i];
    aParcel.writeInt32(static_cast<int32_t>(record.mTnf));

    void* data;

    aParcel.writeInt32(record.mType.Length());
    data = aParcel.writeInplace(record.mType.Length());
    memcpy(data, record.mType.Elements(), record.mType.Length());

    aParcel.writeInt32(record.mId.Length());
    data = aParcel.writeInplace(record.mId.Length());
    memcpy(data, record.mId.Elements(), record.mId.Length());

    aParcel.writeInt32(record.mPayload.Length());
    data = aParcel.writeInplace(record.mPayload.Length());
    memcpy(data, record.mPayload.Elements(), record.mPayload.Length());
  }

  return true;
}

bool
NfcMessageHandler::ReadTransceiveResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  uint32_t length = aParcel.readInt32();

  aOptions.mResponse.AppendElements(
    static_cast<const uint8_t*>(aParcel.readInplace(length)), length);

  return true;
}
