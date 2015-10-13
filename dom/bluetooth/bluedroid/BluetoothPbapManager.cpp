/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothPbapManager.h"

#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"
#include "BluetoothUuid.h"

#include "mozilla/dom/BluetoothPbapParametersBinding.h"
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

  // App parameters to pull phonebook
  static const AppParameterTag sPhonebookTags[] = {
    AppParameterTag::Format,
    AppParameterTag::PropertySelector,
    AppParameterTag::MaxListCount,
    AppParameterTag::ListStartOffset,
    AppParameterTag::vCardSelector
  };

  // App parameters to pull vCard listing
  static const AppParameterTag sVCardListingTags[] = {
    AppParameterTag::Order,
    AppParameterTag::SearchValue,
    AppParameterTag::SearchProperty,
    AppParameterTag::MaxListCount,
    AppParameterTag::ListStartOffset,
    AppParameterTag::vCardSelector
  };

  // App parameters to pull vCard entry
  static const AppParameterTag sVCardEntryTags[] = {
    AppParameterTag::Format,
    AppParameterTag::PropertySelector
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

BluetoothPbapManager::BluetoothPbapManager() : mPhonebookSizeRequired(false)
                                             , mConnected(false)
                                             , mRemoteMaxPacketLength(0)
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

      ObexResponseCode response = SetPhoneBookPath(pktHeaders, data[3]);
      if (response != ObexResponseCode::Success) {
        ReplyError(response);
        return;
      }

      ReplyToSetPath();
      break;
    }
    case ObexRequestCode::Get:
      /*
       * Section 6.2.2 "OBEX Headers in Multi-Packet Responses", IrOBEX 1.2
       * All OBEX request messages shall be sent as one OBEX packet containing
       * all the headers, i.e., OBEX GET with opcode 0x83 shall always be
       * used, and GET with opcode 0x03 shall never be used.
       */
      BT_LOGR("PBAP shall always use OBEX GetFinal instead of Get.");

      // no break. Treat 'Get' as 'GetFinal' for error tolerance.
    case ObexRequestCode::GetFinal: {
      /*
       * When |mVCardDataStream| requires multiple response packets to complete,
       * the client should continue to issue GET requests until the final body
       * information (i.e., End-of-Body header) arrives, along with
       * ObexResponseCode::Success
       */
      if (mVCardDataStream) {
        if (!ReplyToGet()) {
          BT_LOGR("Failed to reply to PBAP GET request.");
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

      ObexResponseCode response = NotifyPbapRequest(pktHeaders);
      if (response != ObexResponseCode::Success) {
        ReplyError(response);
        return;
      }
      // OBEX success response will be sent after gaia replies PBAP request
      break;
    }
    case ObexRequestCode::Put:
    case ObexRequestCode::PutFinal:
      ReplyError(ObexResponseCode::BadRequest);
      BT_LOGR("Unsupported ObexRequestCode %x", opCode);
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

ObexResponseCode
BluetoothPbapManager::SetPhoneBookPath(const ObexHeaderSet& aHeader,
                                       uint8_t flags)
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
      int32_t lastSlashIdx = newPath.RFindChar('/');
      if (lastSlashIdx != -1) {
        newPath = StringHead(newPath, lastSlashIdx);
      } else {
        // The parent folder is root.
        newPath.AssignLiteral("");
      }
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
      if (!newPath.IsEmpty()) {
        newPath.AppendLiteral("/");
      }
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

ObexResponseCode
BluetoothPbapManager::NotifyPbapRequest(const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Get content type and name
  nsString type, name;
  aHeader.GetContentType(type);
  aHeader.GetName(name);

  // Configure request based on content type
  nsString reqId;
  uint8_t tagCount;
  const AppParameterTag* tags;
  if (type.EqualsLiteral("x-bt/phonebook")) {
    reqId.AssignLiteral(PULL_PHONEBOOK_REQ_ID);
    tagCount = MOZ_ARRAY_LENGTH(sPhonebookTags);
    tags = sPhonebookTags;

    // Ensure the name of phonebook object is legal
    if (!IsLegalPhonebookName(name)) {
      BT_LOGR("Illegal phone book object name [%s]",
              NS_ConvertUTF16toUTF8(name).get());
      return ObexResponseCode::NotFound;
    }
  } else if (type.EqualsLiteral("x-bt/vcard-listing")) {
    reqId.AssignLiteral(PULL_VCARD_LISTING_REQ_ID);
    tagCount = MOZ_ARRAY_LENGTH(sVCardListingTags);
    tags = sVCardListingTags;

    // Section 5.3.3 "Name", PBAP 1.2:
    // ... PullvCardListing function uses relative paths. An empty name header
    // may be sent to retrieve the vCard Listing object of the current folder.
    name = name.IsEmpty() ? mCurrentPath
                          : mCurrentPath + NS_LITERAL_STRING("/") + name;
  } else if (type.EqualsLiteral("x-bt/vcard")) {
    reqId.AssignLiteral(PULL_VCARD_ENTRY_REQ_ID);
    tagCount = MOZ_ARRAY_LENGTH(sVCardEntryTags);
    tags = sVCardEntryTags;
  } else {
    BT_LOGR("Unknown PBAP request type: %s",
            NS_ConvertUTF16toUTF8(type).get());
    return ObexResponseCode::BadRequest;
  }

  // Ensure bluetooth service is available
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    BT_LOGR("Failed to get Bluetooth service");
    return ObexResponseCode::PreconditionFailed;
  }

  // Pack PBAP request
  InfallibleTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "name", name);
  for (uint8_t i = 0; i < tagCount; i++) {
    AppendNamedValueByTagId(aHeader, data, tags[i]);
  }

  bs->DistributeSignal(reqId, NS_LITERAL_STRING(KEY_ADAPTER), data);

  return ObexResponseCode::Success;
}

void
BluetoothPbapManager::AppendNamedValueByTagId(
  const ObexHeaderSet& aHeader,
  InfallibleTArray<BluetoothNamedValue>& aValues,
  const AppParameterTag aTagId)
{
  uint8_t buf[64];
  if (!aHeader.GetAppParameter(aTagId, buf, 64)) {
    return;
  }

  switch (aTagId) {
    case AppParameterTag::Order: {
      using namespace mozilla::dom::vCardOrderTypeValues;
      uint32_t order =
        buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0])
                                      : 0; // default: indexed
      AppendNamedValue(aValues, "order", order);
      break;
    }
    case AppParameterTag::SearchProperty: {
      using namespace mozilla::dom::vCardSearchKeyTypeValues;
      uint32_t searchKey =
        buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0])
                                      : 0; // default: name
      AppendNamedValue(aValues, "searchKey", searchKey);
      break;
    }
    case AppParameterTag::SearchValue:
      // Section 5.3.4.3 "SearchValue {<text string>}", PBAP 1.2
      // The UTF-8 character set shall be used for <text string>.

      // Store UTF-8 string with nsCString to follow MDN:Internal_strings
      AppendNamedValue(aValues, "searchText", nsCString((char *) buf));
      break;
    case AppParameterTag::MaxListCount: {
      uint16_t maxListCount = ReadLittleEndianUInt16(buf);
      AppendNamedValue(aValues, "maxListCount",
                       static_cast<uint32_t>(maxListCount));

      // Section 5 "Phone Book Access Profile Functions", PBAP 1.2
      // Replying 'PhonebookSize' is mandatory if 'MaxListCount' parameter
      // is present in the request with a value of 0, else it is excluded.
      mPhonebookSizeRequired = !maxListCount;
      break;
    }
    case AppParameterTag::ListStartOffset:
      AppendNamedValue(aValues, "listStartOffset",
                       static_cast<uint32_t>(ReadLittleEndianUInt16(buf)));
      break;
    case AppParameterTag::PropertySelector:
      AppendNamedValue(aValues, "propSelector", PackPropertiesMask(buf, 64));
      break;
    case AppParameterTag::Format:
      AppendNamedValue(aValues, "format", static_cast<bool>(buf[0]));
      break;
    case AppParameterTag::vCardSelector: {
      bool hasSelectorOperator = aHeader.GetAppParameter(
        AppParameterTag::vCardSelectorOperator, buf, 64);
      AppendNamedValue(aValues,
                       hasSelectorOperator && buf[0] ? "vCardSelector_AND"
                                                     : "vCardSelector_OR",
                       PackPropertiesMask(buf, 64));
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
    "telecom",
    "telecom/pb",
    "telecom/ich",
    "telecom/och",
    "telecom/mch",
    "telecom/cch",
    "SIM1",
    "SIM1/telecom",
    "SIM1/telecom/pb",
    "SIM1/telecom/ich",
    "SIM1/telecom/och",
    "SIM1/telecom/mch",
    "SIM1/telecom/cch"
  };

  NS_ConvertUTF16toUTF8 path(aPath);
  for (uint8_t i = 0; i < MOZ_ARRAY_LENGTH(sLegalPaths); i++) {
    if (!strcmp(path.get(), sLegalPaths[i])) {
      return true;
    }
  }

  return false;
}

bool
BluetoothPbapManager::IsLegalPhonebookName(const nsAString& aName)
{
  static const char* sLegalNames[] = {
    "telecom/pb.vcf",
    "telecom/ich.vcf",
    "telecom/och.vcf",
    "telecom/mch.vcf",
    "telecom/cch.vcf",
    "SIM1/telecom/pb.vcf",
    "SIM1/telecom/ich.vcf",
    "SIM1/telecom/och.vcf",
    "SIM1/telecom/mch.vcf",
    "SIM1/telecom/cch.vcf"
  };

  NS_ConvertUTF16toUTF8 name(aName);
  for (uint8_t i = 0; i < MOZ_ARRAY_LENGTH(sLegalNames); i++) {
    if (!strcmp(name.get(), sLegalNames[i])) {
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
  mPhonebookSizeRequired = false;

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
  while (x) {
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

  /**
   * Open vCard input stream only if |mPhonebookSizeRequired| is false,
   * according to section 2.1.4.3 "MaxListCount", PBAP 1.2:
   * "When MaxListCount = 0, ... The response shall not contain any
   *  Body header."
   */
  if (!mPhonebookSizeRequired && !GetInputStreamFromBlob(aBlob)) {
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

  /**
   * Open vCard input stream only if |mPhonebookSizeRequired| is false,
   * according to section 5.3.4.4 "MaxListCount", PBAP 1.2:
   * "When MaxListCount = 0, ... The response shall not contain a Body header."
   */
  if (!mPhonebookSizeRequired && !GetInputStreamFromBlob(aBlob)) {
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
  MOZ_ASSERT(mRemoteMaxPacketLength >= kObexLeastMaxSize);

  /**
   * This response consists of following parts:
   * - Part 1: [response code:1][length:2]
   *
   * If |mPhonebookSizeRequired| is true,
   * - Part 2: [headerId:1][length:2][PhonebookSize:4]
   * - Part 3: [headerId:1][length:2][EndOfBody:0]
   * Otherwise,
   * - Part 2a: [headerId:1][length:2][EndOfBody:0]
   *   or
   * - Part 2b: [headerId:1][length:2][Body:var]
   */
  uint8_t* res = new uint8_t[mRemoteMaxPacketLength];
  uint8_t opcode;

  // ---- Part 1: [response code:1][length:2] ---- //
  // [response code:1][length:2] will be set in |SendObexData|.
  // Reserve index for them here
  unsigned int index = kObexRespHeaderSize;

  if (mPhonebookSizeRequired) {
    // ---- Part 2: [headerId:1][length:2][PhonebookSize:4] ---- //
    uint16_t pbSizeBigEndian = ConvertEndiannessUInt16(aPhonebookSize);

    // Section 6.2.1 "Application Parameters Header", PBAP 1.2
    // appParameters: [headerId:1][length:2][PhonebookSize:4], where
    //                [PhonebookSize:4] = [tagId:1][length:1][value:2]
    uint8_t appParameters[4];
    AppendAppParameter(appParameters,
                       sizeof(appParameters),
                       static_cast<uint8_t>(AppParameterTag::PhonebookSize),
                       (uint8_t*) &pbSizeBigEndian,
                       sizeof(pbSizeBigEndian));

    index += AppendHeaderAppParameters(&res[index],
                                       mRemoteMaxPacketLength,
                                       appParameters,
                                       sizeof(appParameters));

    // ---- Part 3: [headerId:1][length:2][EndOfBody:0] ---- //
    opcode = ObexResponseCode::Success;
    index += AppendHeaderEndOfBody(&res[index]);

    mPhonebookSizeRequired = false;
  } else {
    MOZ_ASSERT(mVCardDataStream);

    uint64_t bytesAvailable = 0;
    nsresult rv = mVCardDataStream->Available(&bytesAvailable);
    if (NS_FAILED(rv)) {
      BT_LOGR("Failed to get available bytes from input stream. rv=0x%x",
              static_cast<uint32_t>(rv));
      return false;
    }

    /*
     * In practice, some platforms can only handle zero length End-of-Body
     * header separately with Body header.
     * Thus, append End-of-Body only if the data stream had been sent out,
     * otherwise, send 'Continue' to request for next GET request.
     */
    if (!bytesAvailable) {
      // ----  Part 2a: [headerId:1][length:2][EndOfBody:0] ---- //
      index += AppendHeaderEndOfBody(&res[index]);

      // Close input stream
      mVCardDataStream->Close();
      mVCardDataStream = nullptr;

      opcode = ObexResponseCode::Success;
    } else {
      // Compute remaining packet size to append Body, excluding Body's header
      uint32_t remainingPacketSize =
        mRemoteMaxPacketLength - kObexBodyHeaderSize - index;

      // Read vCard data from input stream
      uint32_t numRead = 0;
      nsAutoArrayPtr<char> buf(new char[remainingPacketSize]);
      rv = mVCardDataStream->Read(buf, remainingPacketSize, &numRead);
      if (NS_FAILED(rv)) {
        BT_LOGR("Failed to read from input stream. rv=0x%x",
                static_cast<uint32_t>(rv));
        return false;
      }

      // |numRead| must be non-zero as |bytesAvailable| is non-zero
      MOZ_ASSERT(numRead);

      // ----  Part 2b: [headerId:1][length:2][Body:var] ---- //
      index += AppendHeaderBody(&res[index],
                                remainingPacketSize,
                                (uint8_t*) buf.forget(),
                                numRead);

      opcode = ObexResponseCode::Continue;
    }
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
    BT_LOGR("Shouldn't handle multiple PBAP responses simultaneously");
    mVCardDataStream->Close();
    mVCardDataStream = nullptr;
  }

  ErrorResult rv;
  aBlob->GetInternalStream(getter_AddRefs(mVCardDataStream), rv);
  if (rv.Failed()) {
    BT_LOGR("Failed to get internal stream from blob. rv=0x%x",
            rv.ErrorCodeAsInt());
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
  if (!mSocket) {
    BT_LOGR("No ongoing connection to disconnect");
    return;
  }

  mSocket->Close();
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
