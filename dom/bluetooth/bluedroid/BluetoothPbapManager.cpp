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
      }
      break;
    case ObexRequestCode::Put:
    case ObexRequestCode::PutFinal:
    case ObexRequestCode::Get:
    case ObexRequestCode::GetFinal:
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
  uint8_t req[255];
  int index = 7;

  req[3] = 0x10; // version=1.0
  req[4] = 0x00; // flag=0x00
  req[5] = BluetoothPbapManager::MAX_PACKET_LENGTH >> 8;
  req[6] = (uint8_t)BluetoothPbapManager::MAX_PACKET_LENGTH;

  // Section 6.4 "Establishing an OBEX Session", PBAP 1.2
  // Headers: [Who:16][Connection ID]
  index += AppendHeaderWho(&req[index], 255, kPbapObexTarget.mUuid,
                           sizeof(BluetoothUuid));
  index += AppendHeaderConnectionId(&req[index], 0x01);

  SendObexData(req, ObexResponseCode::Success, index);
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
  uint8_t req[255];
  int index = 3;

  SendObexData(req, ObexResponseCode::Success, index);
}

void
BluetoothPbapManager::ReplyToSetPath()
{
  if (!mConnected) {
    return;
  }

  // Section 3.3.6 "SetPath", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendObexData(req, ObexResponseCode::Success, index);
}

void
BluetoothPbapManager::ReplyError(uint8_t aError)
{
  BT_LOGR("[0x%x]", aError);

  // Section 3.2 "Response Format", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendObexData(req, aError, index);
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
