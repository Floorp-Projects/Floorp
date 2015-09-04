/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothPbapManager.h"

#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUuid.h"
#include "ObexBase.h"

#include "mozilla/dom/File.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsIInputStream.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"

USING_BLUETOOTH_NAMESPACE
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {
  // UUID of PBAP PSE
  static const BluetoothUuid kPbapPSE = {
    {
      0x00, 0x00, 0x11, 0x2F, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
    }
  };

  // UUID used in PBAP OBEX target header
  static const BluetoothUuid kPbapObexTarget = {
    {
      0x79, 0x61, 0x35, 0xF0, 0xF0, 0xC5, 0x11, 0xD8,
      0x09, 0x66, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66
    }
  };

  StaticRefPtr<BluetoothPbapManager> sPbapManager;
  static bool sInShutdown = false;
}

BEGIN_BLUETOOTH_NAMESPACE

NS_IMETHODIMP
BluetoothPbapManager::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const char16_t* aData)
{
  MOZ_ASSERT(sPbapManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "PbapManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

void
BluetoothPbapManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  sInShutdown = true;
  Disconnect(nullptr);
  sPbapManager = nullptr;
}

BluetoothPbapManager::BluetoothPbapManager() : mConnected(false)
                                             , mRemoteMaxPacketLength(0)
                                             , mRequirePhonebookSize(false)
{
  mDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
  mCurrentPath.AssignLiteral("");
}

BluetoothPbapManager::~BluetoothPbapManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  NS_WARN_IF(NS_FAILED(
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)));
}

bool
BluetoothPbapManager::Init()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return false;
  }

  if (NS_WARN_IF(NS_FAILED(
        obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false)))) {
    return false;
  }

  /**
   * We don't start listening here as BluetoothServiceBluedroid calls Listen()
   * immediately when BT stops.
   *
   * If we start listening here, the listening fails when device boots up since
   * Listen() is called again and restarts server socket. The restart causes
   * absence of read events when device boots up.
   */

  return true;
}

//static
BluetoothPbapManager*
BluetoothPbapManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Exit early if sPbapManager already exists
  if (sPbapManager) {
    return sPbapManager;
  }

  // Do not create a new instance if we're in shutdown
  if (NS_WARN_IF(sInShutdown)) {
    return nullptr;
  }

  // Create a new instance, register, and return
  BluetoothPbapManager *manager = new BluetoothPbapManager();
  if (NS_WARN_IF(!manager->Init())) {
    return nullptr;
  }

  sPbapManager = manager;
  return sPbapManager;
}

bool
BluetoothPbapManager::Listen()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Fail to listen if |mSocket| already exists
  if (NS_WARN_IF(mSocket)) {
    return false;
  }

  /**
   * Restart server socket since its underlying fd becomes invalid when
   * BT stops; otherwise no more read events would be received even if
   * BT restarts.
   */
  if (mServerSocket) {
    mServerSocket->Close();
    mServerSocket = nullptr;
  }

  mServerSocket = new BluetoothSocket(this);

  nsresult rv = mServerSocket->Listen(
    NS_LITERAL_STRING("OBEX Phonebook Access Server"),
    kPbapPSE,
    BluetoothSocketType::RFCOMM,
    BluetoothReservedChannels::CHANNEL_PBAP_PSE, false, true);

  if (NS_FAILED(rv)) {
    mServerSocket = nullptr;
    return false;
  }

  BT_LOGR("PBAP socket is listening");
  return true;
}

// Virtual function of class SocketConsumer
void
BluetoothPbapManager::ReceiveSocketData(BluetoothSocket* aSocket,
                                        nsAutoPtr<UnixSocketBuffer>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * Ensure
   * - valid access to data[0], i.e., opCode
   * - received packet length smaller than max packet length
   */
  int receivedLength = aMessage->GetSize();
  if (receivedLength < 1 || receivedLength > MAX_PACKET_LENGTH) {
    ReplyError(ObexResponseCode::BadRequest);
    return;
  }

  const uint8_t* data = aMessage->GetData();
  uint8_t opCode = data[0];
  ObexHeaderSet pktHeaders;
  switch (opCode) {
    case ObexRequestCode::Connect:
      // Section 3.3.1 "Connect", IrOBEX 1.2
      // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
      // [Headers:var]
      if (receivedLength < 7 ||
          !ParseHeaders(&data[7], receivedLength - 7, &pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      // Section 6.4 "Establishing an OBEX Session", PBAP 1.2
      // The OBEX header target shall equal to kPbapObexTarget.
      if (!CompareHeaderTarget(pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      // Save the max packet length from remote information
      mRemoteMaxPacketLength = ((static_cast<int>(data[5]) << 8) | data[6]);

      if (mRemoteMaxPacketLength < kObexLeastMaxSize) {
        BT_LOGR("Remote maximum packet length %d is smaller than %d bytes",
          mRemoteMaxPacketLength, kObexLeastMaxSize);
        mRemoteMaxPacketLength = 0;
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      ReplyToConnect();
      AfterPbapConnected();
      break;
    case ObexRequestCode::Disconnect:
    case ObexRequestCode::Abort:
      // Section 3.3.2 "Disconnect" and Section 3.3.5 "Abort", IrOBEX 1.2
      // The format of request packet of "Disconnect" and "Abort" are the same
      // [opcode:1][length:2][Headers:var]
      if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      ReplyToDisconnectOrAbort();
      AfterPbapDisconnected();
      break;
    case ObexRequestCode::SetPath: {
      // Section 3.3.6 "SetPath", IrOBEX 1.2
      // [opcode:1][length:2][flags:1][contants:1][Headers:var]
      if (receivedLength < 5 ||
          !ParseHeaders(&data[5], receivedLength - 5, &pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      uint8_t response = SetPhoneBookPath(data[3], pktHeaders);
      if (response != ObexResponseCode::Success) {
        ReplyError(response);
        return;
      }

      ReplyToSetPath();
      break;
    }
    case ObexRequestCode::Get:
      // Section 6.2.2 "OBEX Headers in Multi-Packet Responses", IrOBEX 1.2
      // All OBEX request messages shall be sent as one OBEX packet containing
      // all of the headers. I.e. OBEX GET with opcode 0x83 shall always be
      // used. OBEX GET with opcode 0x03 shall never be used.
      BT_WARNING("PBAP shall always uses OBEX GetFinal instead of Get.");

      // no break. Treat 'Get' as 'GetFinal' for error tolerance.
    case ObexRequestCode::GetFinal: {
      // As long as 'mVCardDataStream' requires multiple response packets to
      // complete, the client should continue to issue GET requests until the
      // final body information (in an End-of-Body header) arrives, along with
      // the response code 0xA0 Success.
      if (mVCardDataStream) {
        if (!ReplyToGet()) {
          BT_WARNING("Failed to reply to PBAP GET request.");
          ReplyError(ObexResponseCode::InternalServerError);
        }
        return;
      }

      // Section 3.1 "Request format", IrOBEX 1.2
      // The format of an OBEX request is
      // [opcode:1][length:2][Headers:var]
      if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      nsString type;
      pktHeaders.GetContentType(type);

      uint8_t response;
      if (type.EqualsLiteral("x-bt/vcard-listing")) {
        response = PullvCardListing(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/vcard")) {
        response = PullvCardEntry(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/phonebook")) {
        response = PullPhonebook(pktHeaders);
      } else {
        response = ObexResponseCode::BadRequest;
        BT_LOGR("Unknown PBAP request type: %s",
                NS_ConvertUTF16toUTF8(type).get());
      }

      // The OBEX success response will be sent after Gaia replies the PBAP
      // request.
      if (response != ObexResponseCode::Success) {
        ReplyError(response);
        return;
      }
      break;
    }
    case ObexRequestCode::Put:
    case ObexRequestCode::PutFinal:
      ReplyError(ObexResponseCode::BadRequest);
      break;
    default:
      ReplyError(ObexResponseCode::NotImplemented);
      BT_LOGR("Unrecognized ObexRequestCode %x", opCode);
      break;
  }
}

bool
BluetoothPbapManager::CompareHeaderTarget(const ObexHeaderSet& aHeader)
{
  if (!aHeader.Has(ObexHeaderId::Target)) {
    BT_LOGR("No ObexHeaderId::Target in header");
    return false;
  }

  uint8_t* targetPtr;
  int targetLength;
  aHeader.GetTarget(&targetPtr, &targetLength);

  if (targetLength != sizeof(BluetoothUuid)) {
    BT_LOGR("Length mismatch: %d != 16", targetLength);
    return false;
  }

  for (uint8_t i = 0; i < sizeof(BluetoothUuid); i++) {
    if (targetPtr[i] != kPbapObexTarget.mUuid[i]) {
      BT_LOGR("UUID mismatch: received target[%d]=0x%x != 0x%x",
              i, targetPtr[i], kPbapObexTarget.mUuid[i]);
      return false;
    }
  }

  return true;
}

uint8_t
BluetoothPbapManager::SetPhoneBookPath(uint8_t flags,
                                       const ObexHeaderSet& aHeader)
{
  // Section 5.2 "SetPhoneBook Function", PBAP 1.2
  // flags bit 1 must be 1 and bit 2~7 be 0
  if ((flags >> 1) != 1) {
    BT_LOGR("Illegal flags [0x%x]: bits 1~7 must be 0x01", flags);
    return ObexResponseCode::BadRequest;
  }

  nsString newPath = mCurrentPath;

  /**
   * Three cases:
   * 1) Go up 1 level   - flags bit 0 is 1
   * 2) Go back to root - flags bit 0 is 0 AND name header is empty
   * 3) Go down 1 level - flags bit 0 is 0 AND name header is not empty,
   *                      where name header is the name of child folder
   */
  if (flags & 1) {
    // Go up 1 level
    if (!newPath.IsEmpty()) {
      newPath = StringHead(newPath, newPath.RFindChar('/'));
    }
  } else {
    MOZ_ASSERT(aHeader.Has(ObexHeaderId::Name));

    nsString childFolderName;
    aHeader.GetName(childFolderName);
    if (childFolderName.IsEmpty()) {
      // Go back to root
      newPath.AssignLiteral("");
    } else {
      // Go down 1 level
      newPath.AppendLiteral("/");
      newPath.Append(childFolderName);
    }
  }

  // Ensure the new path is legal
  if (!IsLegalPath(newPath)) {
    BT_LOGR("Illegal phone book path [%s]",
            NS_ConvertUTF16toUTF8(newPath).get());
    return ObexResponseCode::NotFound;
  }

  mCurrentPath = newPath;
  BT_LOGR("current path [%s]", NS_ConvertUTF16toUTF8(mCurrentPath).get());

  return ObexResponseCode::Success;
}

uint8_t
BluetoothPbapManager::PullPhonebook(const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    return ObexResponseCode::PreconditionFailed;
  }

  InfallibleTArray<BluetoothNamedValue> data;

  nsString name;
  aHeader.GetName(name);
  BT_APPEND_NAMED_VALUE(data, "name", name);

  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::Format);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::PropertySelector);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::MaxListCount);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::ListStartOffset);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::vCardSelector);

  #ifdef MOZ_B2G_BT_API_V1
    bs->DistributeSignal(
      BluetoothSignal(NS_LITERAL_STRING(PULL_PHONEBOOK_REQ_ID),
                      NS_LITERAL_STRING(KEY_ADAPTER),
                      data));
  #else
    bs->DistributeSignal(NS_LITERAL_STRING(PULL_PHONEBOOK_REQ_ID),
                         NS_LITERAL_STRING(KEY_ADAPTER),
                         data);
  #endif

  return ObexResponseCode::Success;
}

uint8_t
BluetoothPbapManager::PullvCardListing(const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    return ObexResponseCode::PreconditionFailed;
  }

  InfallibleTArray<BluetoothNamedValue> data;

  nsString name;
  aHeader.GetName(name);
  BT_APPEND_NAMED_VALUE(data, "name", name);

  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::Order);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::SearchValue);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::SearchProperty);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::MaxListCount);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::ListStartOffset);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::vCardSelector);

  #ifdef MOZ_B2G_BT_API_V1
    bs->DistributeSignal(
      BluetoothSignal(NS_LITERAL_STRING(PULL_VCARD_LISTING_REQ_ID),
                      NS_LITERAL_STRING(KEY_ADAPTER),
                      data));
  #else
    bs->DistributeSignal(NS_LITERAL_STRING(PULL_VCARD_LISTING_REQ_ID),
                         NS_LITERAL_STRING(KEY_ADAPTER),
                         data);
  #endif

  return ObexResponseCode::Success;
}

uint8_t
BluetoothPbapManager::PullvCardEntry(const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    return ObexResponseCode::PreconditionFailed;
  }

  InfallibleTArray<BluetoothNamedValue> data;

  nsString name;
  aHeader.GetName(name);
  BT_APPEND_NAMED_VALUE(data, "name", name);

  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::Format);
  AppendBtNamedValueByTagId(aHeader, data, AppParameterTag::PropertySelector);

  #ifdef MOZ_B2G_BT_API_V1
    bs->DistributeSignal(
      BluetoothSignal(NS_LITERAL_STRING(PULL_VCARD_ENTRY_REQ_ID),
                      NS_LITERAL_STRING(KEY_ADAPTER),
                      data));
  #else
    bs->DistributeSignal(NS_LITERAL_STRING(PULL_VCARD_ENTRY_REQ_ID),
                         NS_LITERAL_STRING(KEY_ADAPTER),
                         data);
  #endif

  return ObexResponseCode::Success;
}

void
BluetoothPbapManager::AppendBtNamedValueByTagId(
  const ObexHeaderSet& aHeader,
  InfallibleTArray<BluetoothNamedValue>& aValues,
  const AppParameterTag aTagId)
{
  uint8_t buf[64];

  switch (aTagId) {
    case AppParameterTag::Order: {
      if (!aHeader.GetAppParameter(AppParameterTag::Order, buf, 64)) {
        break;
      }

      static const nsString sOrderStr[] = {NS_LITERAL_STRING("alphanumeric"),
                                           NS_LITERAL_STRING("indexed"),
                                           NS_LITERAL_STRING("phonetical")};
      uint8_t order = buf[0];
      if (order < MOZ_ARRAY_LENGTH(sOrderStr)) {
        BT_APPEND_NAMED_VALUE(aValues, "order", sOrderStr[order]);
      } else {
        BT_WARNING("%s: Unexpected value '%d' of 'Order'", __FUNCTION__, order);
      }
      break;
    }
    case AppParameterTag::SearchValue: {
      if (!aHeader.GetAppParameter(AppParameterTag::SearchValue, buf, 64)) {
        break;
      }

      // Section 5.3.4.3 "SearchValue {<text string>}", PBAP 1.2
      // The UTF-8 character set shall be used for <text string>.

      // Use nsCString to store UTF-8 string here to follow the suggestion of
      // 'MDN:Internal_strings'.
      nsCString text((char *) buf);

      BT_APPEND_NAMED_VALUE(aValues, "searchText", text);
      break;
    }
    case AppParameterTag::SearchProperty: {
      if (!aHeader.GetAppParameter(AppParameterTag::SearchProperty, buf, 64)) {
        break;
      }

      static const nsString sSearchKeyStr[] = {NS_LITERAL_STRING("name"),
                                               NS_LITERAL_STRING("number"),
                                               NS_LITERAL_STRING("sound")};
      uint8_t searchKey = buf[0];
      if (searchKey < MOZ_ARRAY_LENGTH(sSearchKeyStr)) {
        BT_APPEND_NAMED_VALUE(aValues, "searchKey", sSearchKeyStr[searchKey]);
      } else {
        BT_WARNING("%s: Unexpected value '%d' of 'SearchProperty'",
                   __FUNCTION__, searchKey);
      }
      break;
    }
    case AppParameterTag::MaxListCount: {
      if (!aHeader.GetAppParameter(AppParameterTag::MaxListCount, buf, 64)) {
        break;
      }

      uint16_t maxListCount = *((uint16_t *)buf);

      // convert big endian to little endian
      maxListCount = (maxListCount >> 8) | (maxListCount << 8);

      // Section 5 "Phone Book Access Profile Functions", PBAP 1.2
      // Replying 'PhonebookSize' is mandatory if 'MaxListCount' parameter is
      // present in the request with a value of 0, else it is excluded.
      mRequirePhonebookSize = !maxListCount;

      BT_APPEND_NAMED_VALUE(aValues, "maxListCount", (uint32_t) maxListCount);
      break;
    }
    case AppParameterTag::ListStartOffset: {
      if (!aHeader.GetAppParameter(AppParameterTag::ListStartOffset, buf, 64)) {
        break;
      }

      uint16_t listStartOffset = *((uint16_t *)buf);

      // convert big endian to little endian
      listStartOffset = (listStartOffset >> 8) | (listStartOffset << 8);

      BT_APPEND_NAMED_VALUE(aValues, "listStartOffset",
                           (uint32_t) listStartOffset);
      break;
    }
    case AppParameterTag::PropertySelector: {
      if (!aHeader.GetAppParameter(
          AppParameterTag::PropertySelector, buf, 64)) {
        break;
      }

      InfallibleTArray<uint32_t> props = PackPropertiesMask(buf, 64);

      BT_APPEND_NAMED_VALUE(aValues, "propSelector", props);
      break;
    }
    case AppParameterTag::Format: {
      if (!aHeader.GetAppParameter(AppParameterTag::Format, buf, 64)) {
        break;
      }

      bool usevCard3 = buf[0];
      BT_APPEND_NAMED_VALUE(aValues, "format", usevCard3);
      break;
    }
    case AppParameterTag::vCardSelector: {
      if (!aHeader.GetAppParameter(AppParameterTag::vCardSelector, buf, 64)) {
        break;
      }

      InfallibleTArray<uint32_t> props = PackPropertiesMask(buf, 64);

      bool hasVCardSelectorOperator = aHeader.GetAppParameter(
        AppParameterTag::vCardSelectorOperator, buf, 64);

      if (hasVCardSelectorOperator && buf[0]) {
        BT_APPEND_NAMED_VALUE(aValues, "vCardSelector_AND",
                              BluetoothValue(props));
      } else {
        BT_APPEND_NAMED_VALUE(aValues, "vCardSelector_OR",
                              BluetoothValue(props));
      }
      break;
    }
    default:
      BT_LOGR("Unsupported AppParameterTag: %x", aTagId);
      break;
  }
}

bool
BluetoothPbapManager::IsLegalPath(const nsAString& aPath)
{
  static const char* sLegalPaths[] = {
    "", // root
    "/telecom",
    "/telecom/pb",
    "/telecom/ich",
    "/telecom/och",
    "/telecom/mch",
    "/telecom/cch",
    "/SIM1",
    "/SIM1/telecom",
    "/SIM1/telecom/pb",
    "/SIM1/telecom/ich",
    "/SIM1/telecom/och",
    "/SIM1/telecom/mch",
    "/SIM1/telecom/cch"
  };

  NS_ConvertUTF16toUTF8 path(aPath);
  for (uint8_t i = 0; i < MOZ_ARRAY_LENGTH(sLegalPaths); i++) {
    if (!strcmp(path.get(), sLegalPaths[i])) {
      return true;
    }
  }

  return false;
}

void
BluetoothPbapManager::AfterPbapConnected()
{
  mCurrentPath.AssignLiteral("");
  mConnected = true;
}

void
BluetoothPbapManager::AfterPbapDisconnected()
{
  mConnected = false;

  mRemoteMaxPacketLength = 0;
  mRequirePhonebookSize = false;

  if (mVCardDataStream) {
    mVCardDataStream->Close();
    mVCardDataStream = nullptr;
  }
}

bool
BluetoothPbapManager::IsConnected()
{
  return mConnected;
}

void
BluetoothPbapManager::GetAddress(nsAString& aDeviceAddress)
{
  return mSocket->GetAddress(aDeviceAddress);
}

void
BluetoothPbapManager::ReplyToConnect()
{
  if (mConnected) {
    return;
  }

  // Section 3.3.1 "Connect", IrOBEX 1.2
  // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
  // [Headers:var]
  uint8_t res[kObexLeastMaxSize];
  int index = 7;

  res[3] = 0x10; // version=1.0
  res[4] = 0x00; // flag=0x00
  res[5] = BluetoothPbapManager::MAX_PACKET_LENGTH >> 8;
  res[6] = (uint8_t)BluetoothPbapManager::MAX_PACKET_LENGTH;

  // Section 6.4 "Establishing an OBEX Session", PBAP 1.2
  // Headers: [Who:16][Connection ID]
  index += AppendHeaderWho(&res[index], kObexLeastMaxSize,
                           kPbapObexTarget.mUuid, sizeof(BluetoothUuid));
  index += AppendHeaderConnectionId(&res[index], 0x01);

  SendObexData(res, ObexResponseCode::Success, index);
}

void
BluetoothPbapManager::ReplyToDisconnectOrAbort()
{
  if (!mConnected) {
    return;
  }

  // Section 3.3.2 "Disconnect" and Section 3.3.5 "Abort", IrOBEX 1.2
  // The format of response packet of "Disconnect" and "Abort" are the same
  // [opcode:1][length:2][Headers:var]
  uint8_t res[kObexLeastMaxSize];
  int index = kObexRespHeaderSize;

  SendObexData(res, ObexResponseCode::Success, index);
}

void
BluetoothPbapManager::ReplyToSetPath()
{
  if (!mConnected) {
    return;
  }

  // Section 3.3.6 "SetPath", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t res[kObexLeastMaxSize];
  int index = kObexRespHeaderSize;

  SendObexData(res, ObexResponseCode::Success, index);
}

InfallibleTArray<uint32_t>
BluetoothPbapManager::PackPropertiesMask(uint8_t* aData, int aSize)
{
  InfallibleTArray<uint32_t> propSelector;

  // Table 5.1 "Property Mask", PBAP 1.2
  // PropertyMask is a 64-bit mask that indicates the properties contained in
  // the requested vCard objects. We only support bit 0~31 since the rest are
  // reserved for future use or vendor specific properties.

  // convert big endian to little endian
  uint32_t x = (aData[7] << 0)  | (aData[6] << 8) |
               (aData[5] << 16) | (aData[4] << 24);

  uint32_t count = 0;
  while (!x) {
    if (x & 1) {
      propSelector.AppendElement(count);
    }

    ++count;
    x >>= 1;
  }

  return propSelector;
}

bool
BluetoothPbapManager::ReplyToPullPhonebook(BlobParent* aActor,
                                           uint16_t aPhonebookSize)
{
  nsRefPtr<BlobImpl> impl = aActor->GetBlobImpl();
  nsRefPtr<Blob> blob = Blob::Create(nullptr, impl);

  return ReplyToPullPhonebook(blob.get(), aPhonebookSize);
}

bool
BluetoothPbapManager::ReplyToPullPhonebook(Blob* aBlob, uint16_t aPhonebookSize)
{
  if (!mConnected) {
    return false;
  }

  if (!GetInputStreamFromBlob(aBlob)) {
    ReplyError(ObexResponseCode::InternalServerError);
    return false;
  }

  return ReplyToGet(aPhonebookSize);
}

bool
BluetoothPbapManager::ReplyToPullvCardListing(BlobParent* aActor,
                                              uint16_t aPhonebookSize)
{
  nsRefPtr<BlobImpl> impl = aActor->GetBlobImpl();
  nsRefPtr<Blob> blob = Blob::Create(nullptr, impl);

  return ReplyToPullvCardListing(blob.get(), aPhonebookSize);
}

bool
BluetoothPbapManager::ReplyToPullvCardListing(Blob* aBlob,
                                              uint16_t aPhonebookSize)
{
  if (!mConnected) {
    return false;
  }

  if (!GetInputStreamFromBlob(aBlob)) {
    ReplyError(ObexResponseCode::InternalServerError);
    return false;
  }

  return ReplyToGet(aPhonebookSize);
}

bool
BluetoothPbapManager::ReplyToPullvCardEntry(BlobParent* aActor)
{
  nsRefPtr<BlobImpl> impl = aActor->GetBlobImpl();
  nsRefPtr<Blob> blob = Blob::Create(nullptr, impl);

  return ReplyToPullvCardEntry(blob.get());
}

bool
BluetoothPbapManager::ReplyToPullvCardEntry(Blob* aBlob)
{
  if (!mConnected) {
    return false;
  }

  if (!GetInputStreamFromBlob(aBlob)) {
    ReplyError(ObexResponseCode::InternalServerError);
    return false;
  }

  return ReplyToGet();
}

bool
BluetoothPbapManager::ReplyToGet(uint16_t aPhonebookSize)
{
  MOZ_ASSERT(mVCardDataStream);
  MOZ_ASSERT(mRemoteMaxPacketLength >= kObexLeastMaxSize);

  // This response will be composed by these four parts.
  // Part 1: [response code:1][length:2]
  // Part 2: [headerId:1][length:2][PhonebookSize:4]  (optional)
  // Part 3: [headerId:1][length:2][Body:var]
  // Part 4: [headerId:1][length:2][EndOfBody:0]      (optional)

  uint8_t* res = new uint8_t[mRemoteMaxPacketLength];

  // ---- Part 1, move index for [response code:1][length:2] ---- //
  // res[0~2] will be set in SendObexData()
  unsigned int index = kObexRespHeaderSize;

  // ---- Part 2, add [response code:1][length:2] to response ---- //
  if (mRequirePhonebookSize) {
    // convert little endian to big endian
    uint8_t phonebookSize[2];
    phonebookSize[0] = (aPhonebookSize & 0xFF00) >> 8;
    phonebookSize[1] = aPhonebookSize & 0x00FF;

    // Section 6.2.1 "Application Parameters Header", PBAP 1.2
    // appParameters: [headerId:1][length:2][PhonebookSize:4], where
    //                [PhonebookSize:4] = [tagId:1][length:1][value:2]
    uint8_t appParameters[4];
    AppendAppParameter(appParameters,
                       sizeof(appParameters),
                       (uint8_t) AppParameterTag::PhonebookSize,
                       phonebookSize,
                       sizeof(phonebookSize));

    index += AppendHeaderAppParameters(&res[index],
                                       mRemoteMaxPacketLength,
                                       appParameters,
                                       sizeof(appParameters));
    mRequirePhonebookSize = false;
  }

  // ---- Part 3, add [headerId:1][length:2][Body:var] to response ---- //
  // Remaining packet size to append Body, excluding Body's header
  uint32_t remainingPacketSize = mRemoteMaxPacketLength - kObexBodyHeaderSize
                                                        - index;

  // Read vCard data from input stream
  uint32_t numRead = 0;
  nsAutoArrayPtr<char> buffer(new char[remainingPacketSize]);
  nsresult rv = mVCardDataStream->Read(buffer, remainingPacketSize, &numRead);
  if (NS_FAILED(rv)) {
    BT_WARNING("Failed to read from input stream.");
    return false;
  }

  if (numRead) {
    index += AppendHeaderBody(&res[index],
                              remainingPacketSize,
                              (uint8_t*) buffer.forget(),
                              numRead);
  }

  // More GET requests are required if remaining packet size isn't
  // enough for 1) number of bytes read and 2) one EndOfBody's header
  uint8_t opcode;
  if (numRead + kObexBodyHeaderSize > remainingPacketSize) {
    opcode = ObexResponseCode::Continue;
  } else {
    // ---- Part 4, add [headerId:1][length:2][EndOfBody:var] to response --- //
    opcode = ObexResponseCode::Success;
    index += AppendHeaderEndOfBody(&res[index]);

    mVCardDataStream->Close();
    mVCardDataStream = nullptr;
  }

  SendObexData(res, opcode, index);
  delete [] res;

  return true;
}

bool
BluetoothPbapManager::GetInputStreamFromBlob(Blob* aBlob)
{
  // PBAP can only handle one OBEX BODY transfer at the same time.
  if (mVCardDataStream) {
    BT_WARNING("Shouldn't handle multiple PBAP responses at the same time");
    mVCardDataStream->Close();
    mVCardDataStream = nullptr;
  }

  ErrorResult rv;
  aBlob->GetInternalStream(getter_AddRefs(mVCardDataStream), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return false;
  }

  return true;
}

void
BluetoothPbapManager::ReplyError(uint8_t aError)
{
  BT_LOGR("[0x%x]", aError);

  // Section 3.2 "Response Format", IrOBEX 1.2
  // [response code:1][length:2][data:var]
  uint8_t res[kObexLeastMaxSize];
  SendObexData(res, aError, kObexBodyHeaderSize);
}

void
BluetoothPbapManager::SendObexData(uint8_t* aData, uint8_t aOpcode, int aSize)
{
  SetObexPacketInfo(aData, aOpcode, aSize);
  mSocket->SendSocketData(new UnixSocketRawData(aData, aSize));
}

void
BluetoothPbapManager::OnSocketConnectSuccess(BluetoothSocket* aSocket)
{
  MOZ_ASSERT(aSocket);
  MOZ_ASSERT(aSocket == mServerSocket);
  MOZ_ASSERT(!mSocket);

  BT_LOGR("PBAP socket is connected");

  // Close server socket as only one session is allowed at a time
  mServerSocket.swap(mSocket);

  // Cache device address since we can't get socket address when a remote
  // device disconnect with us.
  mSocket->GetAddress(mDeviceAddress);
}

void
BluetoothPbapManager::OnSocketConnectError(BluetoothSocket* aSocket)
{
  mServerSocket = nullptr;
  mSocket = nullptr;
}

void
BluetoothPbapManager::OnSocketDisconnect(BluetoothSocket* aSocket)
{
  MOZ_ASSERT(aSocket);

  if (aSocket != mSocket) {
    // Do nothing when a listening server socket is closed.
    return;
  }

  AfterPbapDisconnected();
  mDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
  mSocket = nullptr;

  Listen();
}

void
BluetoothPbapManager::Disconnect(BluetoothProfileController* aController)
{
  if (mSocket) {
    mSocket->Close();
  } else {
    BT_WARNING("%s: No ongoing connection to disconnect", __FUNCTION__);
  }
}

NS_IMPL_ISUPPORTS(BluetoothPbapManager, nsIObserver)

void
BluetoothPbapManager::Connect(const nsAString& aDeviceAddress,
                              BluetoothProfileController* aController)
{
  MOZ_ASSERT(false);
}

void
BluetoothPbapManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                          const nsAString& aServiceUuid,
                                          int aChannel)
{
  MOZ_ASSERT(false);
}

void
BluetoothPbapManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(false);
}

void
BluetoothPbapManager::OnConnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(false);
}

void
BluetoothPbapManager::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(false);
}

void
BluetoothPbapManager::Reset()
{
  MOZ_ASSERT(false);
}

END_BLUETOOTH_NAMESPACE
