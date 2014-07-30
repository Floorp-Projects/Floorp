/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NfcMessageHandler.h"
#include <binder/Parcel.h>
#include "nsDebug.h"
#include "NfcGonkMessage.h"
#include "NfcOptions.h"

#include <android/log.h>
#define CHROMIUM_LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "NfcMessageHandler", args)

using namespace android;
using namespace mozilla;

static const char* kConfigRequest = "config";
static const char* kGetDetailsNDEF = "getDetailsNDEF";
static const char* kReadNDEFRequest = "readNDEF";
static const char* kWriteNDEFRequest = "writeNDEF";
static const char* kMakeReadOnlyNDEFRequest = "makeReadOnlyNDEF";
static const char* kConnectRequest = "connect";
static const char* kCloseRequest = "close";

static const char* kConfigResponse = "ConfigResponse";
static const char* kGetDetailsNDEFResponse = "GetDetailsNDEFResponse";
static const char* kReadNDEFResponse = "ReadNDEFResponse";
static const char* kWriteNDEFResponse = "WriteNDEFResponse";
static const char* kMakeReadOnlyNDEFResponse = "MakeReadOnlyNDEFResponse";
static const char* kConnectResponse = "ConnectResponse";
static const char* kCloseResponse = "CloseResponse";

static const char* kInitializedNotification = "InitializedNotification";
static const char* kTechDiscoveredNotification = "TechDiscoveredNotification";
static const char* kTechLostNotification = "TechLostNotification";

bool
NfcMessageHandler::Marshall(Parcel& aParcel, const CommandOptions& aOptions)
{
  bool result;
  const char* type = NS_ConvertUTF16toUTF8(aOptions.mType).get();

  if (!strcmp(type, kConfigRequest)) {
    result = ConfigRequest(aParcel, aOptions);
  } else if (!strcmp(type, kGetDetailsNDEF)) {
    result = GetDetailsNDEFRequest(aParcel, aOptions);
  } else if (!strcmp(type, kReadNDEFRequest)) {
    result = ReadNDEFRequest(aParcel, aOptions);
  } else if (!strcmp(type, kWriteNDEFRequest)) {
    result = WriteNDEFRequest(aParcel, aOptions);
    mPendingReqQueue.AppendElement(eNfcRequest_WriteNDEF);
  } else if (!strcmp(type, kMakeReadOnlyNDEFRequest)) {
    result = MakeReadOnlyNDEFRequest(aParcel, aOptions);
    mPendingReqQueue.AppendElement(eNfcRequest_MakeReadOnlyNDEF);
  } else if (!strcmp(type, kConnectRequest)) {
    result = ConnectRequest(aParcel, aOptions);
    mPendingReqQueue.AppendElement(eNfcRequest_Connect);
  } else if (!strcmp(type, kCloseRequest)) {
    result = CloseRequest(aParcel, aOptions);
    mPendingReqQueue.AppendElement(eNfcRequest_Close);
  } else {
    result = false;
  }

  return result;
}

bool
NfcMessageHandler::Unmarshall(const Parcel& aParcel, EventOptions& aOptions)
{
  bool result;
  uint32_t parcelSize = htonl(aParcel.readInt32());
  int32_t type = aParcel.readInt32();

  switch (type) {
    case eNfcResponse_General:
      result = GeneralResponse(aParcel, aOptions);
      break;
    case eNfcResponse_Config:
      result = ConfigResponse(aParcel, aOptions);
      break;
    case eNfcResponse_GetDetailsNDEF:
      result = GetDetailsNDEFResponse(aParcel, aOptions);
      break;
    case eNfcResponse_ReadNDEF:
      result = ReadNDEFResponse(aParcel, aOptions);
      break;
    case eNfcNotification_Initialized:
      result = InitializeNotification(aParcel, aOptions);
      break;
    case eNfcNotification_TechDiscovered:
      result = TechDiscoveredNotification(aParcel, aOptions);
      break;
    case eNfcNotification_TechLost:
      result = TechLostNotification(aParcel, aOptions);
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
    case eNfcRequest_WriteNDEF:
      type = kWriteNDEFResponse;
      break;
    case eNfcRequest_MakeReadOnlyNDEF:
      type = kMakeReadOnlyNDEFResponse;
      break;
    case eNfcRequest_Connect:
      type = kConnectResponse;
      break;
    case eNfcRequest_Close:
      type = kCloseResponse;
      break;
  }

  aOptions.mType = NS_ConvertUTF8toUTF16(type);
  aOptions.mStatus = aParcel.readInt32();
  aOptions.mSessionId = aParcel.readInt32();

  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);
  return true;
}

bool
NfcMessageHandler::ConfigRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(eNfcRequest_Config);
  aParcel.writeInt32(aOptions.mPowerLevel);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  mPowerLevelQueue.AppendElement(aOptions.mPowerLevel);
  return true;
}

bool
NfcMessageHandler::ConfigResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mType = NS_ConvertUTF8toUTF16(kConfigResponse);
  aOptions.mStatus = aParcel.readInt32();

  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);

  NS_ENSURE_TRUE(!mPowerLevelQueue.IsEmpty(), false);
  aOptions.mPowerLevel = mPowerLevelQueue[0];
  mPowerLevelQueue.RemoveElementAt(0);
  return true;
}

bool
NfcMessageHandler::GetDetailsNDEFRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(eNfcRequest_GetDetailsNDEF);
  aParcel.writeInt32(aOptions.mSessionId);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::GetDetailsNDEFResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mType = NS_ConvertUTF8toUTF16(kGetDetailsNDEFResponse);
  aOptions.mStatus = aParcel.readInt32();
  aOptions.mSessionId = aParcel.readInt32();
  int readOnly = aParcel.readInt32();
  aOptions.mIsReadOnly = readOnly & 0xff;
  aOptions.mCanBeMadeReadOnly = (readOnly >> 8) & 0xff;
  aOptions.mMaxSupportedLength = aParcel.readInt32();

  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);
  return true;
}

bool
NfcMessageHandler::ReadNDEFRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(eNfcRequest_ReadNDEF);
  aParcel.writeInt32(aOptions.mSessionId);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::ReadNDEFResponse(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mType = NS_ConvertUTF8toUTF16(kReadNDEFResponse);
  aOptions.mStatus = aParcel.readInt32();
  aOptions.mSessionId = aParcel.readInt32();

  NS_ENSURE_TRUE(!mRequestIdQueue.IsEmpty(), false);
  aOptions.mRequestId = mRequestIdQueue[0];
  mRequestIdQueue.RemoveElementAt(0);
  ReadNDEFMessage(aParcel, aOptions);
  return true;
}

bool
NfcMessageHandler::WriteNDEFRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(eNfcRequest_WriteNDEF);
  aParcel.writeInt32(aOptions.mSessionId);
  WriteNDEFMessage(aParcel, aOptions);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::MakeReadOnlyNDEFRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(eNfcRequest_MakeReadOnlyNDEF);
  aParcel.writeInt32(aOptions.mSessionId);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::ConnectRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(eNfcRequest_Connect);
  aParcel.writeInt32(aOptions.mSessionId);
  aParcel.writeInt32(aOptions.mTechType);
  mRequestIdQueue.AppendElement(aOptions.mRequestId);
  return true;
}

bool
NfcMessageHandler::CloseRequest(Parcel& aParcel, const CommandOptions& aOptions)
{
  aParcel.writeInt32(eNfcRequest_Close);
  aParcel.writeInt32(aOptions.mSessionId);
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

  if (aOptions.mMajorVersion != NFCD_MAJOR_VERSION &&
      aOptions.mMinorVersion != NFCD_MAJOR_VERSION) {
     CHROMIUM_LOG("NFCD version mismatched. majorVersion: %d, minorVersion: %d",
                  aOptions.mMajorVersion, aOptions.mMinorVersion);
  }

  return true;
}

bool
NfcMessageHandler::TechDiscoveredNotification(const Parcel& aParcel, EventOptions& aOptions)
{
  aOptions.mType = NS_ConvertUTF8toUTF16(kTechDiscoveredNotification);
  aOptions.mSessionId = aParcel.readInt32();

  int32_t techCount = aParcel.readInt32();
  aOptions.mTechList.AppendElements(
      static_cast<const uint8_t*>(aParcel.readInplace(techCount)), techCount);

  int32_t ndefMsgCount = aParcel.readInt32();
  if (ndefMsgCount != 0) {
    ReadNDEFMessage(aParcel, aOptions);
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
NfcMessageHandler::ReadNDEFMessage(const Parcel& aParcel, EventOptions& aOptions)
{
  int32_t recordCount = aParcel.readInt32();
  aOptions.mRecords.SetCapacity(recordCount);

  for (int i = 0; i < recordCount; i++) {
    int32_t tnf = aParcel.readInt32();
    NDEFRecordStruct record;
    record.mTnf = tnf;

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
    aParcel.writeInt32(record.mTnf);

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
