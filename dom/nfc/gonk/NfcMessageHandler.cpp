/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NfcMessageHandler.h"
#include <binder/Parcel.h>
#include "mozilla/dom/MozNDEFRecordBinding.h"
#include "nsDebug.h"
#include "NfcGonkMessage.h"
#include "NfcOptions.h"
#include "mozilla/unused.h"

#include <android/log.h>
#define NMH_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "NfcMessageHandler", args)

using namespace android;
using namespace mozilla;
using namespace mozilla::dom;

static const char* kChangeRFStateRequest = "changeRFState";
static const char* kReadNDEFRequest = "readNDEF";
static const char* kWriteNDEFRequest = "writeNDEF";
static const char* kMakeReadOnlyRequest = "makeReadOnly";
static const char* kFormatRequest = "format";
static const char* kTransceiveRequest = "transceive";

static const char* kChangeRFStateResponse = "ChangeRFStateResponse";
static const char* kReadNDEFResponse = "ReadNDEFResponse";
static const char* kWriteNDEFResponse = "WriteNDEFResponse";
static const char* kMakeReadOnlyResponse = "MakeReadOnlyResponse";
static const char* kFormatResponse = "FormatResponse";
static const char* kTransceiveResponse = "TransceiveResponse";

static const char* kInitializedNotification = "InitializedNotification";
static const char* kTechDiscoveredNotification = "TechDiscoveredNotification";
static const char* kTechLostNotification = "TechLostNotification";
static const char* kHCIEventTransactionNotification =
                     "HCIEventTransactionNotification";

bool
NfcMessageHandler::Marshall(Parcel& aParcel, const CommandOptions& aOptions)
{
  bool result;
  const char* type = NS_ConvertUTF16toUTF8(aOptions.mType).get();

  if (!strcmp(type, kChangeRFStateRequest)) {
    result = ChangeRFStateRequest(aParcel, aOptions);
  } else if (!strcmp(type, kReadNDEFRequest)) {
    result = ReadNDEFRequest(aParcel, aOptions);
  } else if (!strcmp(type, kWriteNDEFRequest)) {
    result = WriteNDEFRequest(aParcel, aOptions);
    mPendingReqQueue.AppendElement(NfcRequest::WriteNDEFReq);
  } else if (!strcmp(type, kMakeReadOnlyRequest)) {
    result = MakeReadOnlyRequest(aParcel, aOptions);
    mPendingReqQueue.AppendElement(NfcRequest::MakeReadOnlyReq);
  } else if (!strcmp(type, kFormatRequest)) {
    result = FormatRequest(aParcel, aOptions);
    mPendingReqQueue.AppendElement(NfcRequest::FormatReq);
  } else if (!strcmp(type, kTransceiveRequest)) {
    result = TransceiveRequest(aParcel, aOptions);
    mPendingReqQueue.AppendElement(NfcRequest::TransceiveReq);
  } else {
    result = false;
  }

  return result;
}

bool
NfcMessageHandler::Unmarshall(const Parcel& aParcel, EventOptions& aOptions)
{
  bool result;
  mozilla::unused << htonl(aParcel.readInt32());  // parcel size
  int32_t type = aParcel.readInt32();

  switch (type) {
    case NfcResponse::GeneralRsp:
      result = GeneralResponse(aParcel, aOptions);
      break;
    case NfcResponse::ChangeRFStateRsp:
      result = ChangeRFStateResponse(aParcel, aOptions);
      break;
    case NfcResponse::ReadNDEFRsp:
      result = ReadNDEFResponse(aParcel, aOptions);
      break;
    case NfcResponse::TransceiveRsp:
      result = TransceiveResponse(aParcel, aOptions);
      break;
    case NfcNotification::Initialized:
      result = InitializeNotification(aParcel, aOptions);
      break;
    case NfcNotification::TechDiscovered:
      result = TechDiscoveredNotification(aParcel, aOptions);
      break;
    case NfcNotification::TechLost:
      result = TechLostNotification(aParcel, aOptions);
      break;
    case NfcNotification::HCIEventTransaction:
      result = HCIEventTransactionNotification(aParcel, aOptions);
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
  const char* type;
  NS_ENSURE_TRUE(!mPendingReqQueue.IsEmpty(), false);
  int pendingReq = mPendingReqQueue[0];
  mPendingReqQueue.RemoveElementAt(0);

  switch (pendingReq) {
    case NfcRequest::WriteNDEFReq:
      type = kWriteNDEFResponse;
      break;
    case NfcRequest::MakeReadOnlyReq:
      type = kMakeReadOnlyResponse;
      break;
    case NfcRequest::FormatReq:
      type = kFormatResponse;
      break;
    default:
      NMH_LOG("Nfcd, unknown general response %d", pendingReq);
      return false;
  }

  aOptions.mType = NS_ConvertUTF8toUTF16(type);
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
  aParcel.writeInt32(NfcRequest::ChangeRFStateReq);
  aParcel.writeInt32(aOptions.mRfState);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::ChangeRFStateResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mType = NS_ConvertUTF8toUTF16(kChangeRFStateResponse);
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
  aParcel.writeInt32(NfcRequest::ReadNDEFReq);
  aParcel.writeInt32(aOptions.mSessionId);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::ReadNDEFResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mType = NS_ConvertUTF8toUTF16(kReadNDEFResponse);
  aOptions.mErrorCode = aParcel.readInt32();
  aOptions.mSessionId = aParcel.readInt32();

  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);

  if (aOptions.mErrorCode == NfcErrorCode::Success) {
    ReadNDEFMessage(aParcel, aOptions);
  }

  return true;
}

bool
NfcMessageHandler::TransceiveResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mType = NS_ConvertUTF8toUTF16(kTransceiveResponse);
  aOptions.mErrorCode = aParcel.readInt32();
  aOptions.mSessionId = aParcel.readInt32();

  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);

  if (aOptions.mErrorCode == NfcErrorCode::Success) {
    ReadTransceiveResponse(aParcel, aOptions);
  }

  return true;
}

bool
NfcMessageHandler::WriteNDEFRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(NfcRequest::WriteNDEFReq);
  aParcel.writeInt32(aOptions.mSessionId);
  aParcel.writeInt32(aOptions.mIsP2P);
  WriteNDEFMessage(aParcel, aOptions);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::MakeReadOnlyRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(NfcRequest::MakeReadOnlyReq);
  aParcel.writeInt32(aOptions.mSessionId);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::FormatRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(NfcRequest::FormatReq);
  aParcel.writeInt32(aOptions.mSessionId);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::TransceiveRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(NfcRequest::TransceiveReq);
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
NfcMessageHandler::InitializeNotification(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mType = NS_ConvertUTF8toUTF16(kInitializedNotification);
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
  aOptions.mType = NS_ConvertUTF8toUTF16(kTechDiscoveredNotification);
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
  aOptions.mType = NS_ConvertUTF8toUTF16(kTechLostNotification);
  aOptions.mSessionId = aParcel.readInt32();
  return true;
}

bool
NfcMessageHandler::HCIEventTransactionNotification(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mType = NS_ConvertUTF8toUTF16(kHCIEventTransactionNotification);

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
