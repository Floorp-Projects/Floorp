/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothPbapManager_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothPbapManager_h

#include "BluetoothCommon.h"
#include "BluetoothProfileManagerBase.h"
#include "BluetoothSocketObserver.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/ipc/SocketBase.h"
#include "mozilla/UniquePtr.h"
#include "nsICryptoHash.h"
#include "ObexBase.h"

class nsICryptoHash;
class nsIInputStream;
namespace mozilla {
  namespace dom {
    class Blob;
    class BlobParent;
  }
}

BEGIN_BLUETOOTH_NAMESPACE

/*
 * Defined in section 6.2.1 "Application Parameters Header", PBAP ver 1.2
 */
enum AppParameterTag {
  Order                   = 0x01,
  SearchValue             = 0x02,
  SearchProperty          = 0x03,
  MaxListCount            = 0x04,
  ListStartOffset         = 0x05,
  PropertySelector        = 0x06,
  Format                  = 0x07,
  PhonebookSize           = 0x08,
  NewMissedCalls          = 0x09,
  // ----- enumerators below are supported since PBAP 1.2 ----- //
  PrimaryVersionCounter   = 0x0A,
  SecondaryVersionCounter = 0x0B,
  vCardSelector           = 0x0C,
  DatabaseIdentifier      = 0x0D,
  vCardSelectorOperator   = 0x0E,
  ResetNewMissedCalls     = 0x0F,
  PbapSupportedFeatures   = 0x10
};

class BluetoothSocket;
class ObexHeaderSet;

class BluetoothPbapManager : public BluetoothSocketObserver
                           , public BluetoothProfileManagerBase
{
public:
  BT_DECL_PROFILE_MGR_BASE
  BT_DECL_SOCKET_OBSERVER
  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("PBAP");
  }

  static const int MAX_PACKET_LENGTH = 0xFFFE;
  static const int DIGEST_LENGTH = 16;

  static void InitPbapInterface(BluetoothProfileResultHandler* aRes);
  static void DeinitPbapInterface(BluetoothProfileResultHandler* aRes);
  static BluetoothPbapManager* Get();

  bool Listen();

  /**
   * Reply to OBEX authenticate challenge with password.
   *
   * @param aPassword [in]  the password known by only client and server.
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  void ReplyToAuthChallenge(const nsAString& aPassword);

  /**
   * Reply vCard objects to IPC 'pullphonebook' request.
   *
   * @param aActor         [in]  blob actor of the vCard objects
   * @param aPhonebookSize [in]  number of vCard indexes in the blob
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullPhonebook(BlobParent* aActor, uint16_t aPhonebookSize);
  /**
   * Reply vCard objects to in-process 'pullphonebook' request.
   *
   * @param aBlob          [in]  blob of the vCard objects
   * @param aPhonebookSize [in]  number of vCard indexes in the blob
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullPhonebook(Blob* aBlob, uint16_t aPhonebookSize);

  /**
   * Reply vCard objects to IPC 'pullvcardlisting' request.
   *
   * @param aActor         [in]  blob actor of the vCard objects
   * @param aPhonebookSize [in]  number of vCard indexes in the blob
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullvCardListing(BlobParent* aActor, uint16_t aPhonebookSize);
  /**
   * Reply vCard objects to in-process 'pullvcardlisting' request.
   *
   * @param aBlob          [in]  blob of the vCard objects
   * @param aPhonebookSize [in]  number of vCard indexes in the blob
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullvCardListing(Blob* aBlob, uint16_t aPhonebookSize);

  /**
   * Reply vCard object to IPC 'pullvcardentry' request.
   *
   * @param aActor [in]  blob actor of the vCard object
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullvCardEntry(BlobParent* aActor);
  /**
   * Reply vCard object to in-process 'pullvcardentry' request.
   *
   * @param aBlob  [in]  blob of the vCard object
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullvCardEntry(Blob* aBlob);

protected:
  virtual ~BluetoothPbapManager();

private:
  BluetoothPbapManager();

  nsresult Init();
  void Uninit();
  void HandleShutdown();

  void ReplyToConnect(const nsAString& aPassword = EmptyString());
  void ReplyToDisconnectOrAbort();
  void ReplyToSetPath();
  bool ReplyToGet(uint16_t aPhonebookSize = 0);
  void ReplyError(uint8_t aError);
  void SendObexData(uint8_t* aData, uint8_t aOpcode, int aSize);
  void SendObexData(UniquePtr<uint8_t[]> aData, uint8_t aOpcode, int aSize);

  ObexResponseCode SetPhoneBookPath(const ObexHeaderSet& aHeader,
                                    uint8_t flags);
  ObexResponseCode NotifyPbapRequest(const ObexHeaderSet& aHeader);
  ObexResponseCode NotifyPasswordRequest(const ObexHeaderSet& aHeader);
  void AppendNamedValueByTagId(const ObexHeaderSet& aHeader,
                               InfallibleTArray<BluetoothNamedValue>& aValues,
                               const AppParameterTag aTagId);

  InfallibleTArray<uint32_t> PackPropertiesMask(uint8_t* aData, int aSize);
  bool CompareHeaderTarget(const ObexHeaderSet& aHeader);
  bool IsLegalPath(const nsAString& aPath);
  bool IsLegalPhonebookName(const nsAString& aName);
  bool GetInputStreamFromBlob(Blob* aBlob);
  void AfterPbapConnected();
  void AfterPbapDisconnected();
  nsresult MD5Hash(char *buf, uint32_t len); // mHashRes stores the result

  /**
   * The nonce for OBEX authentication procedure.
   * Its value shall differ each time remote OBEX client sends it
   */
  uint8_t mRemoteNonce[DIGEST_LENGTH];
  uint8_t mHashRes[DIGEST_LENGTH];
  nsCOMPtr<nsICryptoHash> mVerifier;

  /**
   * Whether phonebook size is required for OBEX response
   */
  bool mPhonebookSizeRequired;

  /**
   * OBEX session status. Set when OBEX session is established
   */
  bool mConnected;
  BluetoothAddress mDeviceAddress;

  /**
   * Current phonebook path
   */
  nsString mCurrentPath;

  /**
   * Maximum packet length that remote device can receive
   */
  unsigned int mRemoteMaxPacketLength;

  // If a connection has been established, mSocket will be the socket
  // communicating with the remote socket. We maintain the invariant that if
  // mSocket is non-null, mServerSocket must be null (and vice versa).
  RefPtr<BluetoothSocket> mSocket;

  // Server socket. Once an inbound connection is established, it will hand
  // over the ownership to mSocket, and get a new server socket while Listen()
  // is called.
  RefPtr<BluetoothSocket> mServerSocket;

  /**
   * The vCard data stream for current processing response
   */
  nsCOMPtr<nsIInputStream> mVCardDataStream;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothPbapManager_h
