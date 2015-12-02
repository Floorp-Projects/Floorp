/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothMapSmsManager.h"

#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"
#include "BluetoothUuid.h"
#include "ObexBase.h"

#include "mozilla/dom/BluetoothMapParametersBinding.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/Endian.h"
#include "mozilla/dom/File.h"

#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsIInputStream.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"

#define FILTER_NO_SMS_GSM   0x01
#define FILTER_NO_SMS_CDMA  0x02
#define FILTER_NO_EMAIL     0x04
#define FILTER_NO_MMS       0x08

USING_BLUETOOTH_NAMESPACE
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {
  // UUID of Map Mas
  static const BluetoothUuid kMapMas(MAP_MAS);

  // UUID of Map Mns
  static const BluetoothUuid kMapMns(MAP_MNS);

  // UUID used in Map OBEX MAS target header
  static const BluetoothUuid kMapMasObexTarget(0xBB, 0x58, 0x2B, 0x40,
                                               0x42, 0x0C, 0x11, 0xDB,
                                               0xB0, 0xDE, 0x08, 0x00,
                                               0x20, 0x0C, 0x9A, 0x66);

  // UUID used in Map OBEX MNS target header
  static const BluetoothUuid kMapMnsObexTarget(0xBB, 0x58, 0x2B, 0x41,
                                               0x42, 0x0C, 0x11, 0xDB,
                                               0xB0, 0xDE, 0x08, 0x00,
                                               0x20, 0x0C, 0x9A, 0x66);

  StaticRefPtr<BluetoothMapSmsManager> sMapSmsManager;
  static bool sInShutdown = false;
}

BEGIN_BLUETOOTH_NAMESPACE

NS_IMETHODIMP
BluetoothMapSmsManager::Observe(nsISupports* aSubject,
                                const char* aTopic,
                                const char16_t* aData)
{
  MOZ_ASSERT(sMapSmsManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "MapSmsManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

void
BluetoothMapSmsManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  sInShutdown = true;
  Disconnect(nullptr);
  sMapSmsManager = nullptr;
}

BluetoothMapSmsManager::BluetoothMapSmsManager()
  : mBodyRequired(false)
  , mFractionDeliverRequired(false)
  , mMasConnected(false)
  , mMnsConnected(false)
  , mNtfRequired(false)
{
  BuildDefaultFolderStructure();
}

BluetoothMapSmsManager::~BluetoothMapSmsManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  NS_WARN_IF(NS_FAILED(
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)));
}

bool
BluetoothMapSmsManager::Init()
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
BluetoothMapSmsManager*
BluetoothMapSmsManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Exit early if sMapSmsManager already exists
  if (sMapSmsManager) {
    return sMapSmsManager;
  }

  // Do not create a new instance if we're in shutdown
  if (NS_WARN_IF(sInShutdown)) {
    return nullptr;
  }

  // Create a new instance, register, and return
  BluetoothMapSmsManager *manager = new BluetoothMapSmsManager();
  if (NS_WARN_IF(!manager->Init())) {
    return nullptr;
  }

  sMapSmsManager = manager;

  return sMapSmsManager;
}

bool
BluetoothMapSmsManager::Listen()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Fail to listen if |mMasSocket| already exists
  if (NS_WARN_IF(mMasSocket)) {
    return false;
  }

  /**
   * Restart server socket since its underlying fd becomes invalid when
   * BT stops; otherwise no more read events would be received even if
   * BT restarts.
   */
  if (mMasServerSocket) {
    mMasServerSocket->Close();
    mMasServerSocket = nullptr;
  }

  mMasServerSocket = new BluetoothSocket(this);

  nsString sdpString;
#if ANDROID_VERSION >= 21
  /**
   * The way bluedroid handles MAP SDP record is very hacky.
   * In Lollipop version, SDP string format would be instanceId + msg type
   * + msg name. See add_maps_sdp in btif/src/btif_sock_sdp.c
   */
  // MAS instance id
  sdpString.AppendPrintf("%02x", SDP_SMS_MMS_INSTANCE_ID);
  // Supported message type
  sdpString.AppendPrintf("%02x", SDP_MESSAGE_TYPE_SMS_GSM |
                                 SDP_MESSAGE_TYPE_SMS_CDMA |
                                 SDP_MESSAGE_TYPE_MMS);
#endif
  /**
   * SDP service name, we don't assign RFCOMM channel directly
   * bluedroid automatically assign channel number randomly.
   */
  sdpString.AppendLiteral("SMS/MMS Message Access");
  nsresult rv = mMasServerSocket->Listen(sdpString, kMapMas,
                                         BluetoothSocketType::RFCOMM, -1, false,
                                         true);
  if (NS_FAILED(rv)) {
    mMasServerSocket = nullptr;
    return false;
  }

  return true;
}

void
BluetoothMapSmsManager::MnsDataHandler(UnixSocketBuffer* aMessage)
{
  // Ensure valid access to data[0], i.e., opCode
  int receivedLength = aMessage->GetSize();
  if (receivedLength < 1) {
    BT_LOGR("Receive empty response packet");
    return;
  }

  const uint8_t* data = aMessage->GetData();
  uint8_t opCode = data[0];
  if (opCode != ObexResponseCode::Success) {
    BT_LOGR("Unexpected OpCode: %x", opCode);
    if (mLastCommand == ObexRequestCode::Put ||
        mLastCommand == ObexRequestCode::Abort ||
        mLastCommand == ObexRequestCode::PutFinal) {
      SendMnsDisconnectRequest();
    }
  }
}

void
BluetoothMapSmsManager::MasDataHandler(UnixSocketBuffer* aMessage)
{
  /**
   * Ensure
   * - valid access to data[0], i.e., opCode
   * - received packet length smaller than max packet length
   */
  int receivedLength = aMessage->GetSize();
  if (receivedLength < 1 || receivedLength > MAX_PACKET_LENGTH) {
    SendReply(ObexResponseCode::BadRequest);
    return;
  }

  const uint8_t* data = aMessage->GetData();
  uint8_t opCode = data[0];
  ObexHeaderSet pktHeaders;
  nsString type;
  switch (opCode) {
    case ObexRequestCode::Connect:
      // Section 3.3.1 "Connect", IrOBEX 1.2
      // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
      // [Headers:var]
      if (receivedLength < 7 ||
          !ParseHeaders(&data[7], receivedLength - 7, &pktHeaders)) {
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      // "Establishing an OBEX Session"
      // The OBEX header target shall equal to MAS obex target UUID.
      if (!CompareHeaderTarget(pktHeaders)) {
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      mRemoteMaxPacketLength = BigEndian::readUint16(&data[5]);

      if (mRemoteMaxPacketLength < kObexLeastMaxSize)  {
        BT_LOGR("Remote maximum packet length %d", mRemoteMaxPacketLength);
        mRemoteMaxPacketLength = 0;
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      ReplyToConnect();
      AfterMapSmsConnected();
      break;
    case ObexRequestCode::Disconnect:
    case ObexRequestCode::Abort:
      // Section 3.3.2 "Disconnect" and Section 3.3.5 "Abort", IrOBEX 1.2
      // The format of request packet of "Disconnect" and "Abort" are the same
      // [opcode:1][length:2][Headers:var]
      if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      ReplyToDisconnectOrAbort();
      AfterMapSmsDisconnected();
      break;
    case ObexRequestCode::SetPath: {
        // Section 3.3.6 "SetPath", IrOBEX 1.2
        // [opcode:1][length:2][flags:1][contants:1][Headers:var]
        if (receivedLength < 5 ||
            !ParseHeaders(&data[5], receivedLength - 5, &pktHeaders)) {
          SendReply(ObexResponseCode::BadRequest);
          return;
        }

        uint8_t response = SetPath(data[3], pktHeaders);
        if (response != ObexResponseCode::Success) {
          SendReply(response);
          return;
        }

        ReplyToSetPath();
      }
      break;
    case ObexRequestCode::Put:
    case ObexRequestCode::PutFinal:
      // Section 3.3.3 "Put", IrOBEX 1.2
      // [opcode:1][length:2][Headers:var]
      if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      // Multi-packet PUT request (0x02) may not contain Type header
      if (!pktHeaders.Has(ObexHeaderId::Type)) {
        BT_LOGR("Missing OBEX PUT request Type header");
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      pktHeaders.GetContentType(type);
      BT_LOGR("Type: %s", NS_ConvertUTF16toUTF8(type).get());

      if (type.EqualsLiteral("x-bt/MAP-NotificationRegistration")) {
        HandleNotificationRegistration(pktHeaders);
        ReplyToPut();
      } else if (type.EqualsLiteral("x-bt/messageStatus")) {
        HandleSetMessageStatus(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/message")) {
        HandleSmsMmsPushMessage(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/MAP-messageUpdate")) {
        /* MAP 5.9, There is no concept for Sms/Mms to update inbox. If the
         * MSE does NOT allowed the polling of its mailbox it shall answer
         * with a 'Not implemented' error response.
         */
        SendReply(ObexResponseCode::NotImplemented);
      } else {
        BT_LOGR("Unknown MAP PUT request type: %s",
          NS_ConvertUTF16toUTF8(type).get());
        SendReply(ObexResponseCode::NotImplemented);
      }
      break;
    case ObexRequestCode::Get:
    case ObexRequestCode::GetFinal: {
      /* When |mDataStream| requires multiple response packets to complete,
       * the client should continue to issue GET requests until the final body
       * information (i.e., End-of-Body header) arrives, along with
       * ObexResponseCode::Success
       */
      if (mDataStream) {
        auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
        if (!ReplyToGetWithHeaderBody(Move(res), kObexRespHeaderSize)) {
          BT_LOGR("Failed to reply to MAP GET request.");
          SendReply(ObexResponseCode::InternalServerError);
        }
        return;
      }

      // [opcode:1][length:2][Headers:var]
      if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      if (!pktHeaders.Has(ObexHeaderId::Type)) {
        BT_LOGR("Missing OBEX GET request Type header");
        SendReply(ObexResponseCode::BadRequest);
        return;
      }

      pktHeaders.GetContentType(type);
      if (type.EqualsLiteral("x-obex/folder-listing")) {
        HandleSmsMmsFolderListing(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/MAP-msg-listing")) {
        HandleSmsMmsMsgListing(pktHeaders);
      } else if (type.EqualsLiteral("x-bt/message")) {
        HandleSmsMmsGetMessage(pktHeaders);
      } else {
        BT_LOGR("Unknown MAP GET request type: %s",
          NS_ConvertUTF16toUTF8(type).get());
        SendReply(ObexResponseCode::NotImplemented);
      }
      break;
    }
    default:
      SendReply(ObexResponseCode::NotImplemented);
      BT_LOGR("Unrecognized ObexRequestCode %x", opCode);
      break;
  }
}

// Virtual function of class SocketConsumer
void
BluetoothMapSmsManager::ReceiveSocketData(BluetoothSocket* aSocket,
                                          nsAutoPtr<UnixSocketBuffer>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aSocket == mMnsSocket) {
    MnsDataHandler(aMessage);
  } else {
    MasDataHandler(aMessage);
  }
}

bool
BluetoothMapSmsManager::CompareHeaderTarget(const ObexHeaderSet& aHeader)
{
  const ObexHeader* header = aHeader.GetHeader(ObexHeaderId::Target);

  if (!header) {
    BT_LOGR("No ObexHeaderId::Target in header");
    return false;
  }

  if (header->mDataLength != sizeof(BluetoothUuid)) {
    BT_LOGR("Length mismatch: %d != 16", header->mDataLength);
    return false;
  }

  for (uint8_t i = 0; i < sizeof(BluetoothUuid); i++) {
    if (header->mData[i] != kMapMasObexTarget.mUuid[i]) {
      BT_LOGR("UUID mismatch: received target[%d]=0x%x != 0x%x",
              i, header->mData[i], kMapMasObexTarget.mUuid[i]);
      return false;
    }
  }

  return true;
}

uint8_t
BluetoothMapSmsManager::SetPath(uint8_t flags,
                                const ObexHeaderSet& aHeader)
{
  // Section 5.2 "SetPath Function", MapSms 1.2
  // flags bit 1 must be 1 and bit 2~7 be 0
  if ((flags >> 1) != 1) {
    BT_LOGR("Illegal flags [0x%x]: bits 1~7 must be 0x01", flags);
    return ObexResponseCode::BadRequest;
  }

  /**
   * Three cases:
   * 1) Go up 1 level   - flags bit 0 is 1
   * 2) Go back to root - flags bit 0 is 0 AND name header is empty
   * 3) Go down 1 level - flags bit 0 is 0 AND name header is not empty,
   *                      where name header is the name of child folder
   */
  if (flags & 1) {
    // Go up 1 level
    BluetoothMapFolder* parent = mCurrentFolder->GetParentFolder();
    if (!parent) {
      mCurrentFolder = parent;
      BT_LOGR("MAS SetPath Go up 1 level");
    }
  } else {
    MOZ_ASSERT(aHeader.Has(ObexHeaderId::Name));

    nsString childFolderName;
    aHeader.GetName(childFolderName);

    if (childFolderName.IsEmpty()) {
      // Go back to root
      mCurrentFolder = mRootFolder;
      BT_LOGR("MAS SetPath Go back to root");
    } else {
      // Go down 1 level
      BluetoothMapFolder* child = mCurrentFolder->GetSubFolder(childFolderName);
      if (!child) {
        BT_LOGR("Illegal sub-folder name [%s]",
                NS_ConvertUTF16toUTF8(childFolderName).get());
        return ObexResponseCode::NotFound;
      }

      mCurrentFolder = child;
      BT_LOGR("MAS SetPath Go down to 1 level");
    }
  }

  mCurrentFolder->DumpFolderInfo();

  return ObexResponseCode::Success;
}

void
BluetoothMapSmsManager::AfterMapSmsConnected()
{
  mMasConnected = true;
}

void
BluetoothMapSmsManager::AfterMapSmsDisconnected()
{
  mMasConnected = false;
  mBodyRequired = false;
  mFractionDeliverRequired = false;

  // To ensure we close MNS connection
  DestroyMnsObexConnection();
}

bool
BluetoothMapSmsManager::IsConnected()
{
  return mMasConnected;
}

void
BluetoothMapSmsManager::GetAddress(BluetoothAddress& aDeviceAddress)
{
  return mMasSocket->GetAddress(aDeviceAddress);
}

void
BluetoothMapSmsManager::ReplyToConnect()
{
  if (mMasConnected) {
    return;
  }

  // Section 3.3.1 "Connect", IrOBEX 1.2
  // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
  // [Headers:var]
  uint8_t req[255];
  int index = 7;

  req[3] = 0x10; // version=1.0
  req[4] = 0x00; // flag=0x00
  BigEndian::writeUint16(&req[5], BluetoothMapSmsManager::MAX_PACKET_LENGTH);

  // Section 6.4 "Establishing an OBEX Session", MapSms 1.2
  // Headers: [Who:16][Connection ID]
  index += AppendHeaderWho(&req[index], 255, kMapMasObexTarget.mUuid,
                           sizeof(BluetoothUuid));
  index += AppendHeaderConnectionId(&req[index], 0x01);
  SendMasObexData(req, ObexResponseCode::Success, index);
}

void
BluetoothMapSmsManager::ReplyToDisconnectOrAbort()
{
  if (!mMasConnected) {
    return;
  }

  // Section 3.3.2 "Disconnect" and Section 3.3.5 "Abort", IrOBEX 1.2
  // The format of response packet of "Disconnect" and "Abort" are the same
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendMasObexData(req, ObexResponseCode::Success, index);
}

void
BluetoothMapSmsManager::ReplyToSetPath()
{
  if (!mMasConnected) {
    return;
  }

  // Section 3.3.6 "SetPath", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendMasObexData(req, ObexResponseCode::Success, index);
}

bool
BluetoothMapSmsManager::ReplyToGetWithHeaderBody(UniquePtr<uint8_t[]> aResponse,
                                                 unsigned int aIndex)
{
  if (!mMasConnected) {
    return false;
  }

  /**
   * This response consists of following parts:
   * - Part 1: [response code:1][length:2]
   * - Part 2a: [headerId:1][length:2][EndOfBody:0]
   *   or
   * - Part 2b: [headerId:1][length:2][Body:var]
   */
  // ---- Part 1: [response code:1][length:2] ---- //
  // [response code:1][length:2] will be set in |SendObexData|.
  // Reserve index for them here
  uint64_t bytesAvailable = 0;
  nsresult rv = mDataStream->Available(&bytesAvailable);
  if (NS_FAILED(rv)) {
    BT_LOGR("Failed to get available bytes from input stream. rv=0x%x",
            static_cast<uint32_t>(rv));
    return false;
  }

  /* In practice, some platforms can only handle zero length End-of-Body
   * header separately with Body header.
   * Thus, append End-of-Body only if the data stream had been sent out,
   * otherwise, send 'Continue' to request for next GET request.
   */
  unsigned int opcode;
  if (!bytesAvailable) {
    // ----  Part 2a: [headerId:1][length:2][EndOfBody:0] ---- //
    aIndex += AppendHeaderEndOfBody(&aResponse[aIndex]);

    // Close input stream
    mDataStream->Close();
    mDataStream = nullptr;

    opcode = ObexResponseCode::Success;
  } else {
    // ---- Part 2b: [headerId:1][length:2][Body:var] ---- //
    MOZ_ASSERT(mDataStream);

    // Compute remaining packet size to append Body, excluding Body's header
    uint32_t remainingPacketSize =
      mRemoteMaxPacketLength - kObexBodyHeaderSize - aIndex;

    // Read blob data from input stream
    uint32_t numRead = 0;
    nsAutoArrayPtr<char> buf(new char[remainingPacketSize]);
    nsresult rv = mDataStream->Read(buf, remainingPacketSize, &numRead);
    if (NS_FAILED(rv)) {
      BT_LOGR("Failed to read from input stream. rv=0x%x",
              static_cast<uint32_t>(rv));
      return false;
    }

    // |numRead| must be non-zero
    MOZ_ASSERT(numRead);

    aIndex += AppendHeaderBody(&aResponse[aIndex],
                               remainingPacketSize,
                               reinterpret_cast<uint8_t*>(buf.get()),
                               numRead);

    opcode = ObexResponseCode::Continue;
  }

  SendMasObexData(Move(aResponse), opcode, aIndex);

  return true;
}

void
BluetoothMapSmsManager::ReplyToPut()
{
  if (!mMasConnected) {
    return;
  }

  // Section 3.3.3.2 "PutResponse", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[kObexRespHeaderSize];

  SendMasObexData(req, ObexResponseCode::Success, kObexRespHeaderSize);
}

bool
BluetoothMapSmsManager::ReplyToFolderListing(long aMasId,
                                             const nsAString& aFolderlists)
{
  // TODO: Implement this for future Email support
  return false;
}

bool
BluetoothMapSmsManager::ReplyToMessagesListing(BlobParent* aActor,
                                               long aMasId,
                                               bool aNewMessage,
                                               const nsAString& aTimestamp,
                                               int aSize)
{
  RefPtr<BlobImpl> impl = aActor->GetBlobImpl();
  RefPtr<Blob> blob = Blob::Create(nullptr, impl);

  return ReplyToMessagesListing(blob.get(), aMasId, aNewMessage, aTimestamp,
                                aSize);
}

bool
BluetoothMapSmsManager::ReplyToMessagesListing(Blob* aBlob, long aMasId,
                                               bool aNewMessage,
                                               const nsAString& aTimestamp,
                                               int aSize)
{
  /* If the response code is 0x90 or 0xA0, response consists of following parts:
   * - Part 1: [response code:1][length:2]
   * - Part 2: [headerId:1][length:2][appParam:var]
   *   where [appParam:var] includes:
   *   [NewMessage:3] = [tagId:1][length:1][value:1]
   *   [MseTime:var] = [tagId:1][length:1][value:var]
   *   [MessageListingSize:4] = [tagId:1][length:1][value:2]
   * If mBodyRequired is true,
   * - Part 3: [headerId:1][length:2][Body:var]
   */
  // ---- Part 1: [response code:1][length:2] ---- //
  // [response code:1][length:2] will be set in |SendObexData|.
  // Reserve index here
  auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
  unsigned int index = kObexRespHeaderSize;

  // ---- Part 2: headerId:1][length:2][appParam:var] ---- //
  // MSETime - String with the current time basis and UTC-offset of the MSE
  nsCString timestampStr = NS_ConvertUTF16toUTF8(aTimestamp);
  const uint8_t* str = reinterpret_cast<const uint8_t*>(timestampStr.get());
  uint8_t len = timestampStr.Length();

  // Total length: [NewMessage:3] + [MseTime:var] + [MessageListingSize:4]
  nsAutoArrayPtr<uint8_t> appParameters(new uint8_t[len + 9]);
  uint8_t newMessage = aNewMessage ? 1 : 0;

  AppendAppParameter(appParameters,
                     3,
                     (uint8_t) Map::AppParametersTagId::NewMessage,
                     &newMessage,
                     sizeof(newMessage));

  AppendAppParameter(appParameters + 3,
                     len + 2,
                     (uint8_t) Map::AppParametersTagId::MSETime,
                     str,
                     len);

  uint8_t msgListingSize[2];
  BigEndian::writeUint16(&msgListingSize[0], aSize);

  AppendAppParameter(appParameters + 5 + len,
                     4,
                     (uint8_t) Map::AppParametersTagId::MessagesListingSize,
                     msgListingSize,
                     sizeof(msgListingSize));

  index += AppendHeaderAppParameters(&res[index],
                                     mRemoteMaxPacketLength,
                                     appParameters,
                                     len + 9);

  if (mBodyRequired) {
    // Open input stream only if |mBodyRequired| is true
    if (!GetInputStreamFromBlob(aBlob)) {
      SendReply(ObexResponseCode::InternalServerError);
      return false;
    }

    // ---- Part 3: [headerId:1][length:2][Body:var] ---- //
    ReplyToGetWithHeaderBody(Move(res), index);
    // Reset flag
    mBodyRequired = false;
  } else {
    SendMasObexData(Move(res), ObexResponseCode::Success, index);
  }

  return true;
}

bool
BluetoothMapSmsManager::ReplyToGetMessage(BlobParent* aActor, long aMasId)
{
  RefPtr<BlobImpl> impl = aActor->GetBlobImpl();
  RefPtr<Blob> blob = Blob::Create(nullptr, impl);

  return ReplyToGetMessage(blob.get(), aMasId);
}

bool
BluetoothMapSmsManager::ReplyToGetMessage(Blob* aBlob, long aMasId)
{
  if (!GetInputStreamFromBlob(aBlob)) {
    SendReply(ObexResponseCode::InternalServerError);
    return false;
  }

  /*
   * If the response code is 0x90 or 0xA0, response consists of following parts:
   * - Part 1: [response code:1][length:2]
   * If mFractionDeliverRequired is true,
   * - Part 2: [headerId:1][length:2][appParameters:3]
   * - Part 3: [headerId:1][length:2][Body:var]
   *   where [appParameters] includes:
   *   [FractionDeliver:3] = [tagId:1][length:1][value: 1]
   * otherwise,
   * - Part 2: [headerId:1][length:2][appParameters:3]
   */
  // ---- Part 1: [response code:1][length:2] ---- //
  // [response code:1][length:2] will be set in |SendObexData|.
  // Reserve index here
  auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
  unsigned int index = kObexRespHeaderSize;

  if (mFractionDeliverRequired) {
    // ---- Part 2: [headerId:1][length:2][appParam:3] ---- //
    uint8_t appParameters[3];
    // TODO: Support FractionDeliver, reply "1(last)" now.
    uint8_t fractionDeliver = 1;
    AppendAppParameter(appParameters,
                       sizeof(appParameters),
                       (uint8_t) Map::AppParametersTagId::FractionDeliver,
                       &fractionDeliver,
                       sizeof(fractionDeliver));

    index += AppendHeaderAppParameters(&res[index],
                                       mRemoteMaxPacketLength,
                                       appParameters,
                                       sizeof(appParameters));
  }

  // TODO: Support bMessage encoding in bug 1166652.
  // ---- Part 3: [headerId:1][length:2][Body:var] ---- //
  ReplyToGetWithHeaderBody(Move(res), index);
  mFractionDeliverRequired = false;

  return true;
}

bool
BluetoothMapSmsManager::ReplyToSendMessage(
  long aMasId, const nsAString& aHandleId, bool aStatus)
{
  if (!aStatus) {
    SendReply(ObexResponseCode::InternalServerError);
    return true;
  }

  /* Handle is mandatory if the response code is success (0x90 or 0xA0).
   * The Name header shall be used to contain the handle that was assigned by
   * the MSE device to the message that was pushed by the MCE device.
   * The handle shall be represented by a null-terminated Unicode text strings
   * with 16 hexadecimal digits.
   */
  int len = aHandleId.Length();
  nsAutoArrayPtr<uint8_t> handleId(new uint8_t[(len + 1) * 2]);

  for (int i = 0; i < len; i++) {
    BigEndian::writeUint16(&handleId[i * 2], aHandleId[i]);
  }
  BigEndian::writeUint16(&handleId[len * 2], 0);

  auto res = MakeUnique<uint8_t[]>(mRemoteMaxPacketLength);
  int index = kObexRespHeaderSize;
  index += AppendHeaderName(&res[index], mRemoteMaxPacketLength - index,
                            handleId, (len + 1) * 2);
  SendMasObexData(Move(res), ObexResponseCode::Success, index);

  return true;
}

bool
BluetoothMapSmsManager::ReplyToSetMessageStatus(long aMasId, bool aStatus)
{
  SendReply(aStatus ? ObexResponseCode::Success :
                      ObexResponseCode::InternalServerError);
  return true;
}

bool
BluetoothMapSmsManager::ReplyToMessageUpdate(long aMasId, bool aStatus)
{
  SendReply(aStatus ? ObexResponseCode::Success :
                      ObexResponseCode::InternalServerError);
  return true;
}

void
BluetoothMapSmsManager::CreateMnsObexConnection()
{
  if (mMnsSocket) {
    return;
  }

  mMnsSocket = new BluetoothSocket(this);
  // Already encrypted in previous session
  mMnsSocket->Connect(mDeviceAddress, kMapMns,
                      BluetoothSocketType::RFCOMM, -1, false, false);
}

void
BluetoothMapSmsManager::DestroyMnsObexConnection()
{
  if (!mMnsSocket) {
    return;
  }

  mMnsSocket->Close();
  mMnsSocket = nullptr;
  mNtfRequired = false;
}

void
BluetoothMapSmsManager::SendMnsConnectRequest()
{
  MOZ_ASSERT(mMnsSocket);

  // Section 3.3.1 "Connect", IrOBEX 1.2
  // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
  // [Headers:var]
  uint8_t req[255];
  int index = 7;

  req[3] = 0x10; // version=1.0
  req[4] = 0x00; // flag=0x00
  req[5] = BluetoothMapSmsManager::MAX_PACKET_LENGTH >> 8;
  req[6] = (uint8_t)BluetoothMapSmsManager::MAX_PACKET_LENGTH;

  index += AppendHeaderTarget(&req[index], 255, kMapMnsObexTarget.mUuid,
                              sizeof(BluetoothUuid));
  SendMnsObexData(req, ObexRequestCode::Connect, index);
}

void
BluetoothMapSmsManager::SendMnsDisconnectRequest()
{
  MOZ_ASSERT(mMnsSocket);

  if (!mMasConnected) {
    return;
  }

  // Section 3.3.2 "Disconnect", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendMnsObexData(req, ObexRequestCode::Disconnect, index);
}

void
BluetoothMapSmsManager::HandleSmsMmsFolderListing(const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  uint8_t buf[64];
  uint16_t maxListCount = 0;

  if (aHeader.GetAppParameter(Map::AppParametersTagId::MaxListCount,
                              buf, 64)) {
    maxListCount = BigEndian::readUint16(buf);
  }

  uint16_t startOffset = 0;
  if (aHeader.GetAppParameter(Map::AppParametersTagId::StartOffset,
                              buf, 64)) {
    startOffset = BigEndian::readUint16(buf);
  }

  // Folder listing size
  int foldersize = mCurrentFolder->GetSubFolderCount();

  uint8_t folderListingSizeValue[2];
  BigEndian::writeUint16(&folderListingSizeValue[0], foldersize);

  // Section 3.3.4 "GetResponse", IrOBEX 1.2
  // [opcode:1][length:2][FolderListingSize:4][Headers:var] where
  // Application Parameter [FolderListingSize:4] = [tagId:1][length:1][value: 2]
  uint8_t appParameter[4];
  AppendAppParameter(appParameter, sizeof(appParameter),
                     (uint8_t)Map::AppParametersTagId::FolderListingSize,
                     folderListingSizeValue, sizeof(folderListingSizeValue));

  uint8_t resp[255];
  int index = 3;
  index += AppendHeaderAppParameters(&resp[index], 255, appParameter,
                                     sizeof(appParameter));

  /*
   * MCE wants to query sub-folder size FolderListingSize AppParameter shall
   * be used in the response if the value of MaxListCount in the request is 0.
   * If MaxListCount = 0, the MSE shall ignore all other applications
   * parameters that may be presented in the request. The response shall
   * contain any Body header.
   */
  if (maxListCount) {
    nsString output;
    mCurrentFolder->GetFolderListingObjectString(output, maxListCount,
                                                 startOffset);
    index += AppendHeaderBody(&resp[index],
                              mRemoteMaxPacketLength - index,
                              reinterpret_cast<const uint8_t*>(
                                NS_ConvertUTF16toUTF8(output).get()),
                              NS_ConvertUTF16toUTF8(output).Length());

    index += AppendHeaderEndOfBody(&resp[index]);
  }

  SendMasObexData(resp, ObexResponseCode::Success, index);
}

void
BluetoothMapSmsManager::AppendBtNamedValueByTagId(
  const ObexHeaderSet& aHeader,
  InfallibleTArray<BluetoothNamedValue>& aValues,
  const Map::AppParametersTagId aTagId)
{
  uint8_t buf[64];
  if (!aHeader.GetAppParameter(aTagId, buf, 64)) {
    return;
  }

  /*
   * Follow MAP 6.3.1 Application Parameter Header
   */
  switch (aTagId) {
    case Map::AppParametersTagId::MaxListCount: {
      uint16_t maxListCount = BigEndian::readUint16(buf);
      /* MAP specification 5.4.3.1/5.5.4.1
       * If MaxListCount = 0, the response shall not contain the Body header.
       * The MSE shall ignore the request-parameters "ListStartOffset",
       * "SubjectLength" and "ParameterMask".
       */
      mBodyRequired = (maxListCount != 0);
      BT_LOGR("max list count: %d", maxListCount);
      AppendNamedValue(aValues, "maxListCount",
                       static_cast<uint32_t>(maxListCount));
      break;
    }
    case Map::AppParametersTagId::StartOffset: {
      uint16_t startOffset = BigEndian::readUint16(buf);
      BT_LOGR("start offset : %d", startOffset);
      AppendNamedValue(aValues, "startOffset",
                       static_cast<uint32_t>(startOffset));
      break;
    }
    case Map::AppParametersTagId::SubjectLength: {
      uint8_t subLength = *((uint8_t *)buf);
      BT_LOGR("msg subLength : %d", subLength);
      AppendNamedValue(aValues, "subLength", static_cast<uint32_t>(subLength));
      break;
    }
    case Map::AppParametersTagId::ParameterMask: {
      InfallibleTArray<uint32_t> parameterMask = PackParameterMask(buf, 64);
      AppendNamedValue(aValues, "parameterMask", BluetoothValue(parameterMask));
      break;
    }
    case Map::AppParametersTagId::FilterMessageType: {
      /* Follow MAP 1.2, 6.3.1
       * 0000xxx1 = "SMS_GSM"
       * 0000xx1x = "SMS_CDMA"
       * 0000x1xx = "EMAIL"
       * 00001xxx = "MMS"
       * Where
       * 0 = "no filtering, get this type"
       * 1 = "filter out this type"
       */
      uint32_t filterMessageType = *((uint8_t *)buf);

      if (filterMessageType == (FILTER_NO_EMAIL | FILTER_NO_MMS |
                                FILTER_NO_SMS_GSM) ||
          filterMessageType == (FILTER_NO_EMAIL | FILTER_NO_MMS |
                                FILTER_NO_SMS_CDMA)) {
        filterMessageType = static_cast<uint32_t>(MessageType::Sms);
      } else if (filterMessageType == (FILTER_NO_EMAIL | FILTER_NO_SMS_GSM |
                                       FILTER_NO_SMS_CDMA)) {
        filterMessageType = static_cast<uint32_t>(MessageType::Mms);
      } else if (filterMessageType == (FILTER_NO_MMS | FILTER_NO_SMS_GSM |
                                          FILTER_NO_SMS_CDMA)) {
        filterMessageType = static_cast<uint32_t>(MessageType::Email);
      } else {
        BT_LOGR("Unsupportted filter message type");
        filterMessageType = static_cast<uint32_t>(MessageType::Sms);
      }

      BT_LOGR("msg filterMessageType : %d", filterMessageType);
      AppendNamedValue(aValues, "filterMessageType",
                       static_cast<uint32_t>(filterMessageType));
      break;
    }
    case Map::AppParametersTagId::FilterPeriodBegin: {
      nsCString filterPeriodBegin((char *) buf);
      BT_LOGR("msg FilterPeriodBegin : %s", filterPeriodBegin.get());
      AppendNamedValue(aValues, "filterPeriodBegin",
                       NS_ConvertUTF8toUTF16(filterPeriodBegin));
      break;
    }
    case Map::AppParametersTagId::FilterPeriodEnd: {
      nsCString filterPeriodEnd((char*)buf);
      BT_LOGR("msg filterPeriodEnd : %s", filterPeriodEnd.get());
      AppendNamedValue(aValues, "filterPeriodEnd",
                       NS_ConvertUTF8toUTF16(filterPeriodEnd));
      break;
    }
    case Map::AppParametersTagId::FilterReadStatus: {
      using namespace mozilla::dom::ReadStatusValues;
      uint32_t filterReadStatus =
        buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0]) : 0;
      BT_LOGR("msg filter read status : %d", filterReadStatus);
      AppendNamedValue(aValues, "filterReadStatus", filterReadStatus);
      break;
    }
    case Map::AppParametersTagId::FilterRecipient: {
      // FilterRecipient encodes as UTF-8
      nsCString filterRecipient((char*) buf);
      BT_LOGR("msg filterRecipient : %s", filterRecipient.get());
      AppendNamedValue(aValues, "filterRecipient",
                       NS_ConvertUTF8toUTF16(filterRecipient));
      break;
    }
    case Map::AppParametersTagId::FilterOriginator: {
      // FilterOriginator encodes as UTF-8
      nsCString filterOriginator((char*) buf);
      BT_LOGR("msg filter Originator : %s", filterOriginator.get());
      AppendNamedValue(aValues, "filterOriginator",
                       NS_ConvertUTF8toUTF16(filterOriginator));
      break;
    }
    case Map::AppParametersTagId::FilterPriority: {
      using namespace mozilla::dom::PriorityValues;
      uint32_t filterPriority =
        buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0]) : 0;

      BT_LOGR("msg filter priority: %d", filterPriority);
      AppendNamedValue(aValues, "filterPriority", filterPriority);
      break;
    }
    case Map::AppParametersTagId::Attachment: {
      uint8_t attachment = *((uint8_t *)buf);
      BT_LOGR("msg filter attachment: %d", attachment);
      AppendNamedValue(aValues, "attachment", static_cast<uint32_t>(attachment));
      break;
    }
    case Map::AppParametersTagId::Charset: {
      using namespace mozilla::dom::FilterCharsetValues;
      uint32_t filterCharset =
        buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0]) : 0;

      BT_LOGR("msg filter charset: %d", filterCharset);
      AppendNamedValue(aValues, "charset", filterCharset);
      break;
    }
    case Map::AppParametersTagId::FractionRequest: {
      mFractionDeliverRequired = true;
      AppendNamedValue(aValues, "fractionRequest", (buf[0] != 0));
      break;
    }
    case Map::AppParametersTagId::StatusIndicator: {
      using namespace mozilla::dom::StatusIndicatorsValues;
      uint32_t filterStatusIndicator =
        buf[0] < ArrayLength(strings) ? static_cast<uint32_t>(buf[0]) : 0;

      BT_LOGR("msg filter statusIndicator: %d", filterStatusIndicator);
      AppendNamedValue(aValues, "statusIndicator", filterStatusIndicator);
      break;
    }
    case Map::AppParametersTagId::StatusValue: {
      uint8_t statusValue = *((uint8_t *)buf);
      BT_LOGR("msg filter statusvalue: %d", statusValue);
      AppendNamedValue(aValues, "statusValue",
                       static_cast<uint32_t>(statusValue));
      break;
    }
    case Map::AppParametersTagId::Transparent: {
      uint8_t transparent = *((uint8_t *)buf);
      BT_LOGR("msg filter statusvalue: %d", transparent);
      AppendNamedValue(aValues, "transparent",
                       static_cast<uint32_t>(transparent));
      break;
    }
    case Map::AppParametersTagId::Retry: {
      uint8_t retry = *((uint8_t *)buf);
      BT_LOGR("msg filter retry: %d", retry);
      AppendNamedValue(aValues, "retry", static_cast<uint32_t>(retry));
      break;
    }
    default:
      BT_LOGR("Unsupported AppParameterTag: %x", aTagId);
      break;
  }
}

InfallibleTArray<uint32_t>
BluetoothMapSmsManager::PackParameterMask(uint8_t* aData, int aSize)
{
  InfallibleTArray<uint32_t> parameterMask;

  /* Table 6.5, MAP 6.3.1. ParameterMask is Bit 16-31 Reserved for future
   * use. The reserved bits shall be set to 0 by MCE and discarded by MSE.
   * convert big endian to little endian
   */
  uint32_t x = BigEndian::readUint32(aData);

  uint32_t count = 0;
  while (x) {
    if (x & 1) {
      parameterMask.AppendElement(count);
    }

    ++count;
    x >>= 1;
  }

  return parameterMask;
}

void
BluetoothMapSmsManager::HandleSmsMmsMsgListing(const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();

  InfallibleTArray<BluetoothNamedValue> data;

  static Map::AppParametersTagId sMsgListingParameters[] = {
    [0] = Map::AppParametersTagId::MaxListCount,
    [1] = Map::AppParametersTagId::StartOffset,
    [2] = Map::AppParametersTagId::SubjectLength,
    [3] = Map::AppParametersTagId::ParameterMask,
    [4] = Map::AppParametersTagId::FilterMessageType,
    [5] = Map::AppParametersTagId::FilterPeriodBegin,
    [6] = Map::AppParametersTagId::FilterPeriodEnd,
    [7] = Map::AppParametersTagId::FilterReadStatus,
    [8] = Map::AppParametersTagId::FilterRecipient,
    [9] = Map::AppParametersTagId::FilterOriginator,
    [10] = Map::AppParametersTagId::FilterPriority
  };

  for (uint8_t i = 0; i < MOZ_ARRAY_LENGTH(sMsgListingParameters); i++) {
    AppendBtNamedValueByTagId(aHeader, data, sMsgListingParameters[i]);
  }

  bs->DistributeSignal(NS_LITERAL_STRING(MAP_MESSAGES_LISTING_REQ_ID),
                       NS_LITERAL_STRING(KEY_ADAPTER),
                       data);
}

void
BluetoothMapSmsManager::HandleSmsMmsGetMessage(const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  InfallibleTArray<BluetoothNamedValue> data;
  nsString name;
  aHeader.GetName(name);
  AppendNamedValue(data, "handle", name);

  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::Attachment);
  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::Charset);
  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::FractionRequest);

  bs->DistributeSignal(NS_LITERAL_STRING(MAP_GET_MESSAGE_REQ_ID),
                       NS_LITERAL_STRING(KEY_ADAPTER),
                       data);
}

void
BluetoothMapSmsManager::BuildDefaultFolderStructure()
{
  /* MAP specification defines virtual folders structure
   * /
   * /telecom
   * /telecom/msg
   * /telecom/msg/inbox
   * /telecom/msg/draft
   * /telecom/msg/outbox
   * /telecom/msg/sent
   * /telecom/msg/deleted
   */
  mRootFolder = new BluetoothMapFolder(NS_LITERAL_STRING("root"), nullptr);
  BluetoothMapFolder* folder =
    mRootFolder->AddSubFolder(NS_LITERAL_STRING("telecom"));
  folder = folder->AddSubFolder(NS_LITERAL_STRING("msg"));

  // Add mandatory folders
  folder->AddSubFolder(NS_LITERAL_STRING("inbox"));
  folder->AddSubFolder(NS_LITERAL_STRING("sent"));
  folder->AddSubFolder(NS_LITERAL_STRING("deleted"));
  folder->AddSubFolder(NS_LITERAL_STRING("outbox"));
  folder->AddSubFolder(NS_LITERAL_STRING("draft"));
  mCurrentFolder = mRootFolder;
}

void
BluetoothMapSmsManager::HandleNotificationRegistration(
  const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  uint8_t buf[64];
  if (!aHeader.GetAppParameter(Map::AppParametersTagId::NotificationStatus,
                               buf, 64)) {
    return;
  }

  bool ntfRequired = static_cast<bool>(buf[0]);
  if (mNtfRequired == ntfRequired) {
    // Ignore request
    return;
  }

  mNtfRequired = ntfRequired;
  /*
   * Initialization sequence for a MAP session that uses both the Messsage
   * Access service and the Message Notification service. The MNS connection
   * shall be established by the first SetNotificationRegistration set to ON
   * during MAP session. Only one MNS connection per device pair.
   * Section 6.4.2, MAP
   * If the Message Access connection is disconnected after Message Notification
   * connection establishment, this will automatically indicate a MAS
   * Notification-Deregistration for this MAS instance.
   */
  if (mNtfRequired) {
    CreateMnsObexConnection();
  } else {
    /*
     * TODO: we shall check multiple MAS instances unregister notification to
     * drop MNS connection, but now we only support SMS/MMS, so drop connection
     * directly.
     */
    DestroyMnsObexConnection();
  }
}

void
BluetoothMapSmsManager::HandleSetMessageStatus(const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  InfallibleTArray<BluetoothNamedValue> data;
  nsString name;
  aHeader.GetName(name);
  /* The Name header shall contain the handle of the message the status of which
   * shall be modified. The handle shall be represented by a null-terminated
   * Unicode text string with 16 hexadecimal digits.
   */
  AppendNamedValue(data, "handle", name);

  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::StatusIndicator);
  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::StatusValue);

  bs->DistributeSignal(NS_LITERAL_STRING(MAP_SET_MESSAGE_STATUS_REQ_ID),
                       NS_LITERAL_STRING(KEY_ADAPTER), data);
}

void
BluetoothMapSmsManager::HandleSmsMmsPushMessage(const ObexHeaderSet& aHeader)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (!aHeader.Has(ObexHeaderId::Body) &&
      !aHeader.Has(ObexHeaderId::EndOfBody)) {
    BT_LOGR("Error! Fail to find Body/EndOfBody. Ignore this push request");
    return;
  }

  InfallibleTArray<BluetoothNamedValue> data;
  nsString name;
  aHeader.GetName(name);
  AppendNamedValue(data, "folderName", name);

  AppendBtNamedValueByTagId(aHeader, data,
                            Map::AppParametersTagId::Transparent);
  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Retry);

  /* TODO: Support native format charset (mandatory format).
   *
   * Charset indicates Gaia application how to deal with encoding.
   * - Native: If the message object shall be delivered without trans-coding.
   * - UTF-8:  If the message text shall be trans-coded to UTF-8.
   *
   * We only support UTF-8 charset due to current SMS API limitation.
   */
  AppendBtNamedValueByTagId(aHeader, data, Map::AppParametersTagId::Charset);

  // Get Body
  uint8_t* bodyPtr = nullptr;
  aHeader.GetBody(&bodyPtr, &mBodySegmentLength);
  mBodySegment = bodyPtr;

  RefPtr<BluetoothMapBMessage> bmsg =
    new BluetoothMapBMessage(bodyPtr, mBodySegmentLength);

  /* If FolderName is outbox:
   *   1. Parse body to get SMS
   *   2. Get receipent subject
   *   3. Send it to Gaia
   * Otherwise reply HTTP_NOT_ACCEPTABLE
   */

  nsCString subject;
  bmsg->GetBody(subject);
  // It's possible that subject is empty, send it anyway
  AppendNamedValue(data, "messageBody", subject);

  nsTArray<RefPtr<VCard>> recipients;
  bmsg->GetRecipients(recipients);

  // Get the topmost level, only one recipient for SMS case
  if (!recipients.IsEmpty()) {
    nsCString recipient;
    recipients[0]->GetTelephone(recipient);
    AppendNamedValue(data, "recipient", recipient);
  }

  bs->DistributeSignal(NS_LITERAL_STRING(MAP_SEND_MESSAGE_REQ_ID),
                       NS_LITERAL_STRING(KEY_ADAPTER), data);
}

bool
BluetoothMapSmsManager::GetInputStreamFromBlob(Blob* aBlob)
{
  if (mDataStream) {
    mDataStream->Close();
    mDataStream = nullptr;
  }

  ErrorResult rv;
  aBlob->GetInternalStream(getter_AddRefs(mDataStream), rv);
  if (rv.Failed()) {
    BT_LOGR("Failed to get internal stream from blob. rv=0x%x",
            rv.ErrorCodeAsInt());
    return false;
  }

  return true;
}

void
BluetoothMapSmsManager::SendReply(uint8_t aResponseCode)
{
  BT_LOGR("[0x%x]", aResponseCode);

  // Section 3.2 "Response Format", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[kObexRespHeaderSize];

  SendMasObexData(req, aResponseCode, kObexRespHeaderSize);
}

void
BluetoothMapSmsManager::SendMasObexData(uint8_t* aData, uint8_t aOpcode,
                                        int aSize)
{
  SetObexPacketInfo(aData, aOpcode, aSize);
  mMasSocket->SendSocketData(new UnixSocketRawData(aData, aSize));
}

void
BluetoothMapSmsManager::SendMasObexData(UniquePtr<uint8_t[]> aData,
                                        uint8_t aOpcode, int aSize)
{
  SetObexPacketInfo(aData.get(), aOpcode, aSize);
  mMasSocket->SendSocketData(new UnixSocketRawData(Move(aData), aSize));
}

void
BluetoothMapSmsManager::SendMnsObexData(uint8_t* aData, uint8_t aOpcode,
                                        int aSize)
{
  mLastCommand = aOpcode;
  SetObexPacketInfo(aData, aOpcode, aSize);
  mMnsSocket->SendSocketData(new UnixSocketRawData(aData, aSize));
}

void
BluetoothMapSmsManager::OnSocketConnectSuccess(BluetoothSocket* aSocket)
{
  MOZ_ASSERT(aSocket);

  // MNS socket is connected
  if (aSocket == mMnsSocket) {
    mMnsConnected = true;
    SendMnsConnectRequest();
    return;
  }
  // MAS socket is connected
  // Close server socket as only one session is allowed at a time
  mMasServerSocket.swap(mMasSocket);

  // Cache device address since we can't get socket address when a remote
  // device disconnect with us.
  mMasSocket->GetAddress(mDeviceAddress);
}

void
BluetoothMapSmsManager::OnSocketConnectError(BluetoothSocket* aSocket)
{
  // MNS socket connection error
  if (aSocket == mMnsSocket) {
    mMnsConnected = false;
    mMnsSocket = nullptr;
    return;
  }

  // MAS socket connection error
  mMasServerSocket = nullptr;
  mMasSocket = nullptr;
}

void
BluetoothMapSmsManager::OnSocketDisconnect(BluetoothSocket* aSocket)
{
  MOZ_ASSERT(aSocket);

  if (mDataStream) {
    mDataStream->Close();
    mDataStream = nullptr;
  }

  // MNS socket is disconnected
  if (aSocket == mMnsSocket) {
    mMnsConnected = false;
    mMnsSocket = nullptr;
    BT_LOGR("MNS socket disconnected");
    return;
  }

  // MAS server socket is closed
  if (aSocket != mMasSocket) {
    // Do nothing when a listening server socket is closed.
    return;
  }

  // MAS socket is disconnected
  AfterMapSmsDisconnected();
  mDeviceAddress.Clear();
  mMasSocket = nullptr;

  Listen();
}

void
BluetoothMapSmsManager::Disconnect(BluetoothProfileController* aController)
{
  if (!mMasSocket) {
    BT_WARNING("%s: No ongoing connection to disconnect", __FUNCTION__);
    return;
  }

  mMasSocket->Close();
}

NS_IMPL_ISUPPORTS(BluetoothMapSmsManager, nsIObserver)

void
BluetoothMapSmsManager::Connect(const BluetoothAddress& aDeviceAddress,
                                BluetoothProfileController* aController)
{
  MOZ_ASSERT(false);
}

void
BluetoothMapSmsManager::OnGetServiceChannel(
  const BluetoothAddress& aDeviceAddress,
  const BluetoothUuid& aServiceUuid,
  int aChannel)
{
  MOZ_ASSERT(false);
}

void
BluetoothMapSmsManager::OnUpdateSdpRecords(const BluetoothAddress& aDeviceAddress)
{
  MOZ_ASSERT(false);
}

void
BluetoothMapSmsManager::OnConnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(false);
}

void
BluetoothMapSmsManager::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(false);
}

void
BluetoothMapSmsManager::Reset()
{
  MOZ_ASSERT(false);
}

END_BLUETOOTH_NAMESPACE
