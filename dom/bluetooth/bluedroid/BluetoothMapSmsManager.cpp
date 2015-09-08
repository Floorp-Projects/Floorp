/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothMapSmsManager.h"

#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUuid.h"
#include "ObexBase.h"

#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"

USING_BLUETOOTH_NAMESPACE
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {
  // UUID of Map Mas
  static const BluetoothUuid kMapMas = {
    {
      0x00, 0x00, 0x11, 0x32, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
    }
  };
  // UUID of Map Mns
  static const BluetoothUuid kMapMns = {
    {
      0x00, 0x00, 0x11, 0x33, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
    }
  };
  // UUID used in Map OBEX MAS target header
  static const BluetoothUuid kMapMasObexTarget = {
    {
      0xBB, 0x58, 0x2B, 0x40, 0x42, 0x0C, 0x11, 0xDB,
      0xB0, 0xDE, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66
    }
  };

  // UUID used in Map OBEX MNS target header
  static const BluetoothUuid kMapMnsObexTarget = {
    {
      0xBB, 0x58, 0x2B, 0x41, 0x42, 0x0C, 0x11, 0xDB,
      0xB0, 0xDE, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66
    }
  };

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

BluetoothMapSmsManager::BluetoothMapSmsManager() : mMasConnected(false),
                                                   mMnsConnected(false),
                                                   mNtfRequired(false)
{
  mDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
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
    ReplyError(ObexResponseCode::BadRequest);
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
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      // "Establishing an OBEX Session"
      // The OBEX header target shall equal to MAS obex target UUID.
      if (!CompareHeaderTarget(pktHeaders)) {
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      mRemoteMaxPacketLength = ((static_cast<int>(data[5]) << 8) | data[6]);

      if (mRemoteMaxPacketLength < 255) {
        BT_LOGR("Remote maximum packet length %d", mRemoteMaxPacketLength);
        mRemoteMaxPacketLength = 0;
        ReplyError(ObexResponseCode::BadRequest);
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
        ReplyError(ObexResponseCode::BadRequest);
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
          ReplyError(ObexResponseCode::BadRequest);
          return;
        }

        uint8_t response = SetPath(data[3], pktHeaders);
        if (response != ObexResponseCode::Success) {
          ReplyError(response);
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
        ReplyError(ObexResponseCode::BadRequest);
        return;
      }

      if (pktHeaders.Has(ObexHeaderId::Type)) {
        pktHeaders.GetContentType(type);
        BT_LOGR("Type: %s", NS_ConvertUTF16toUTF8(type).get());
        ReplyToPut();

        if (type.EqualsLiteral("x-bt/MAP-NotificationRegistration")) {
          HandleNotificationRegistration(pktHeaders);
        } else if (type.EqualsLiteral("x-bt/MAP-event-report")) {
          HandleEventReport(pktHeaders);
        } else if (type.EqualsLiteral("x-bt/messageStatus")) {
          HandleMessageStatus(pktHeaders);
        }
      }
      break;
    case ObexRequestCode::Get:
    case ObexRequestCode::GetFinal: {
        // [opcode:1][length:2][Headers:var]
        if (receivedLength < 3 ||
          !ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
          ReplyError(ObexResponseCode::BadRequest);
          return;
        }
        pktHeaders.GetContentType(type);
        if (type.EqualsLiteral("x-obex/folder-listing")) {
          HandleSmsMmsFolderListing(pktHeaders);
        } else if (type.EqualsLiteral("x-bt/MAP-msg-listing")) {
          // TODO: Implement this feature in Bug 1166675
        } else if (type.EqualsLiteral("x-bt/message"))  {
          // TODO: Implement this feature in Bug 1166679
        } else {
          BT_LOGR("Unknown MAP request type: %s",
            NS_ConvertUTF16toUTF8(type).get());
        }
      }
      break;
    default:
      ReplyError(ObexResponseCode::NotImplemented);
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
    if (targetPtr[i] != kMapMasObexTarget.mUuid[i]) {
      BT_LOGR("UUID mismatch: received target[%d]=0x%x != 0x%x",
              i, targetPtr[i], kMapMasObexTarget.mUuid[i]);
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
  // To ensure we close MNS connection
  DestroyMnsObexConnection();
}

bool
BluetoothMapSmsManager::IsConnected()
{
  return mMasConnected;
}

void
BluetoothMapSmsManager::GetAddress(nsAString& aDeviceAddress)
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
  req[5] = BluetoothMapSmsManager::MAX_PACKET_LENGTH >> 8;
  req[6] = (uint8_t)BluetoothMapSmsManager::MAX_PACKET_LENGTH;

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

void
BluetoothMapSmsManager::ReplyToPut()
{
  if (!mMasConnected) {
    return;
  }

  // Section 3.3.3.2 "PutResponse", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendMasObexData(req, ObexResponseCode::Success, index);
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
    maxListCount = *((uint16_t *)buf);
    // convert big endian to little endian
    maxListCount = (maxListCount >> 8) | (maxListCount << 8);
  }

  uint16_t startOffset = 0;
  if (aHeader.GetAppParameter(Map::AppParametersTagId::StartOffset,
                              buf, 64)) {
    startOffset = *((uint16_t *)buf);
    // convert big endian to little endian
    startOffset = (startOffset >> 8) | (startOffset << 8);
  }

  // Folder listing size
  int foldersize = mCurrentFolder->GetSubFolderCount();

  // Convert little endian to big endian
  uint8_t folderListingSizeValue[2];
  folderListingSizeValue[0] = (foldersize & 0xFF00) >> 8;
  folderListingSizeValue[1] = (foldersize & 0x00FF);

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
BluetoothMapSmsManager::HandleEventReport(const ObexHeaderSet& aHeader)
{
  // TODO: Handle event report in Bug 1166666
}

void
BluetoothMapSmsManager::HandleMessageStatus(const ObexHeaderSet& aHeader)
{
  // TODO: Handle MessageStatus update in Bug 1186836
}

void
BluetoothMapSmsManager::ReplyError(uint8_t aError)
{
  BT_LOGR("[0x%x]", aError);

  // Section 3.2 "Response Format", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendMasObexData(req, aError, index);
}

void
BluetoothMapSmsManager::SendMasObexData(uint8_t* aData, uint8_t aOpcode,
                                        int aSize)
{
  SetObexPacketInfo(aData, aOpcode, aSize);
  mMasSocket->SendSocketData(new UnixSocketRawData(aData, aSize));
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
  mDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
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
BluetoothMapSmsManager::Connect(const nsAString& aDeviceAddress,
                                BluetoothProfileController* aController)
{
  MOZ_ASSERT(false);
}

void
BluetoothMapSmsManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                            const nsAString& aServiceUuid,
                                            int aChannel)
{
  MOZ_ASSERT(false);
}

void
BluetoothMapSmsManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
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
