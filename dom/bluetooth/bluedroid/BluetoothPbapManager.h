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

  static BluetoothPbapManager* Get();
  bool Listen();

  /**
   * Reply vCard object to the *IPC* 'pullphonebook' request.
   *
   * @param aActor [in]          a blob actor containing the vCard objects
   * @param aPhonebookSize [in]  the number of vCard indexes in the blob
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullPhonebook(BlobParent* aActor, uint16_t aPhonebookSize);

  /**
   * Reply vCard object to the *in-process* 'pullphonebook' request.
   *
   * @param aBlob [in]           a blob contained the vCard objects
   * @param aPhonebookSize [in]  the number of vCard indexes in the blob
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullPhonebook(Blob* aBlob, uint16_t aPhonebookSize);

  /**
   * Reply vCard object to the *IPC* 'pullvcardlisting' request.
   *
   * @param aActor [in]          a blob actor containing the vCard objects
   * @param aPhonebookSize [in]  the number of vCard indexes in the blob
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullvCardListing(BlobParent* aActor, uint16_t aPhonebookSize);

  /**
   * Reply vCard object to the *in-process* 'pullvcardlisting' request.
   *
   * @param aBlob [in]           a blob contained the vCard objects
   * @param aPhonebookSize [in]  the number of vCard indexes in the blob
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullvCardListing(Blob* aBlob, uint16_t aPhonebookSize);

  /**
   * Reply vCard object to the *IPC* 'pullvcardentry' request.
   *
   * @param aActor [in]  a blob actor containing the vCard objects
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullvCardEntry(BlobParent* aActor);

  /**
   * Reply vCard object to the *in-process* 'pullvcardentry' request.
   *
   * @param aBlob [in]  a blob contained the vCard objects
   *
   * @return true if the response packet has been packed correctly and started
   *         to be sent to the remote device; false otherwise.
   */
  bool ReplyToPullvCardEntry(Blob* aBlob);

protected:
  virtual ~BluetoothPbapManager();

private:
  BluetoothPbapManager();
  bool Init();
  void HandleShutdown();

  void ReplyToConnect();
  void ReplyToDisconnectOrAbort();
  void ReplyToSetPath();
  void ReplyError(uint8_t aError);
  void SendObexData(uint8_t* aData, uint8_t aOpcode, int aSize);
  bool ReplyToGet(uint16_t aPhonebookSize = 0);

  uint8_t SetPhoneBookPath(uint8_t flags, const ObexHeaderSet& aHeader);
  uint8_t PullPhonebook(const ObexHeaderSet& aHeader);
  uint8_t PullvCardListing(const ObexHeaderSet& aHeader);
  uint8_t PullvCardEntry(const ObexHeaderSet& aHeader);
  void AppendBtNamedValueByTagId(
    const ObexHeaderSet& aHeader,
    InfallibleTArray<BluetoothNamedValue>& aValues,
    const AppParameterTag aTagId);

  InfallibleTArray<uint32_t>  PackPropertiesMask(uint8_t* aData, int aSize);
  bool CompareHeaderTarget(const ObexHeaderSet& aHeader);
  bool IsLegalPath(const nsAString& aPath);
  bool GetInputStreamFromBlob(Blob* aBlob);
  void AfterPbapConnected();
  void AfterPbapDisconnected();

  /**
   * Current phonebook path
   */
  nsString mCurrentPath;

  /**
   * OBEX session status. Set when OBEX session is established.
   */
  bool mConnected;
  nsString mDeviceAddress;

  /**
   * Maximum packet length that remote device can receive
   */
  unsigned int mRemoteMaxPacketLength;

  // If a connection has been established, mSocket will be the socket
  // communicating with the remote socket. We maintain the invariant that if
  // mSocket is non-null, mServerSocket must be null (and vice versa).
  nsRefPtr<BluetoothSocket> mSocket;

  // Server socket. Once an inbound connection is established, it will hand
  // over the ownership to mSocket, and get a new server socket while Listen()
  // is called.
  nsRefPtr<BluetoothSocket> mServerSocket;

  /**
   * The data stream of vCards which is used in current processing response.
   */
  nsCOMPtr<nsIInputStream> mVCardDataStream;

  /**
   * A flag to indicate whether 'PhonebookSize' is mandatory for next OBEX
   * response
   */
  bool mRequirePhonebookSize;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothPbapManager_h
