/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothoppmanager_h__
#define mozilla_dom_bluetooth_bluetoothoppmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothProfileManagerBase.h"
#include "BluetoothSocketObserver.h"
#include "DeviceStorage.h"
#include "mozilla/dom/ipc/Blob.h"
#include "mozilla/ipc/UnixSocket.h"
#include "nsCOMArray.h"

class nsIOutputStream;
class nsIInputStream;
class nsIVolumeMountLock;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;
class ObexHeaderSet;
class SendFileBatch;

class BluetoothOppManager : public BluetoothSocketObserver
                          , public BluetoothProfileManagerBase
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  BT_DECL_PROFILE_MGR_BASE
  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("OPP");
  }

  static const int MAX_PACKET_LENGTH = 0xFFFE;

  virtual ~BluetoothOppManager();
  static BluetoothOppManager* Get();
  void ClientDataHandler(mozilla::ipc::UnixSocketRawData* aMessage);
  void ServerDataHandler(mozilla::ipc::UnixSocketRawData* aMessage);

  bool Listen();

  bool SendFile(const nsAString& aDeviceAddress, BlobParent* aActor);
  bool StopSendingFile();
  bool ConfirmReceivingFile(bool aConfirm);

  void SendConnectRequest();
  void SendPutHeaderRequest(const nsAString& aFileName, int aFileSize);
  void SendPutRequest(uint8_t* aFileBody, int aFileBodyLength);
  void SendPutFinalRequest();
  void SendDisconnectRequest();

  void ExtractPacketHeaders(const ObexHeaderSet& aHeader);
  bool ExtractBlobHeaders();
  void CheckPutFinal(uint32_t aNumRead);

  // The following functions are inherited from BluetoothSocketObserver
  void ReceiveSocketData(
    BluetoothSocket* aSocket,
    nsAutoPtr<mozilla::ipc::UnixSocketRawData>& aMessage) MOZ_OVERRIDE;
  virtual void OnSocketConnectSuccess(BluetoothSocket* aSocket) MOZ_OVERRIDE;
  virtual void OnSocketConnectError(BluetoothSocket* aSocket) MOZ_OVERRIDE;
  virtual void OnSocketDisconnect(BluetoothSocket* aSocket) MOZ_OVERRIDE;

private:
  BluetoothOppManager();
  bool Init();
  void HandleShutdown();

  void StartFileTransfer();
  void StartSendingNextFile();
  void FileTransferComplete();
  void UpdateProgress();
  void ReceivingFileConfirmation();
  bool CreateFile();
  bool WriteToFile(const uint8_t* aData, int aDataLength);
  void DeleteReceivedFile();
  void ReplyToConnect();
  void ReplyToDisconnectOrAbort();
  void ReplyToPut(bool aFinal, bool aContinue);
  void ReplyError(uint8_t aError);
  void AfterOppConnected();
  void AfterFirstPut();
  void AfterOppDisconnected();
  void ValidateFileName();
  bool IsReservedChar(char16_t c);
  void ClearQueue();
  void RetrieveSentFileName();
  void NotifyAboutFileChange();
  bool AcquireSdcardMountLock();
  void SendObexData(uint8_t* aData, uint8_t aOpcode, int aSize);
  void AppendBlobToSend(const nsAString& aDeviceAddress, BlobParent* aActor);
  void DiscardBlobsToSend();
  bool ProcessNextBatch();
  void ConnectInternal(const nsAString& aDeviceAddress);

  /**
   * Usually we won't get a full PUT packet in one operation, which means that
   * a packet may be devided into several parts and BluetoothOppManager should
   * be in charge of assembling.
   *
   * @return true if a packet has been fully received.
   *         false if the received length exceeds/not reaches the expected
   *         length.
   */
  bool ComposePacket(uint8_t aOpCode,
                     mozilla::ipc::UnixSocketRawData* aMessage);

  /**
   * OBEX session status.
   * Set when OBEX session is established.
   */
  bool mConnected;
  nsString mDeviceAddress;

  /**
   * Remote information
   */
  uint8_t mRemoteObexVersion;
  uint8_t mRemoteConnectionFlags;
  int mRemoteMaxPacketLength;

  /**
   * For sending files, we decide our next action based on current command and
   * previous one.
   * For receiving files, we don't need previous command and it is set to 0
   * as a default value.
   */
  int mLastCommand;

  int mPacketLength;
  int mPutPacketReceivedLength;
  int mBodySegmentLength;
  int mUpdateProgressCounter;

  /**
   * When it is true and the target service on target device couldn't be found,
   * refreshing SDP records is necessary.
   */
  bool mNeedsUpdatingSdpRecords;

  /**
   * Set when StopSendingFile() is called.
   */
  bool mAbortFlag;

  /**
   * Set when receiving the first PUT packet of a new file
   */
  bool mNewFileFlag;

  /**
   * Set when receiving a PutFinal packet
   */
  bool mPutFinalFlag;

  /**
   * Set when FileTransferComplete() is called
   */
  bool mSendTransferCompleteFlag;

  /**
   * Set when a transfer is successfully completed.
   */
  bool mSuccessFlag;

  /**
   * True: Receive file (Server)
   * False: Send file (Client)
   */
  bool mIsServer;

  /**
   * Set when receiving the first PUT packet and wait for
   * ConfirmReceivingFile() to be called.
   */
  bool mWaitingForConfirmationFlag;

  nsString mFileName;
  nsString mContentType;
  uint32_t mFileLength;
  uint32_t mSentFileLength;
  bool mWaitingToSendPutFinal;

  nsAutoArrayPtr<uint8_t> mBodySegment;
  nsAutoArrayPtr<uint8_t> mReceivedDataBuffer;

  int mCurrentBlobIndex;
  nsCOMPtr<nsIDOMBlob> mBlob;
  nsTArray<SendFileBatch> mBatches;

  /**
   * A seperate member thread is required because our read calls can block
   * execution, which is not allowed to happen on the IOThread.
   */
  nsCOMPtr<nsIThread> mReadFileThread;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  nsCOMPtr<nsIInputStream> mInputStream;
  nsCOMPtr<nsIVolumeMountLock> mMountLock;
  nsRefPtr<DeviceStorageFile> mDsFile;

  // If a connection has been established, mSocket will be the socket
  // communicating with the remote socket. We maintain the invariant that if
  // mSocket is non-null, mServerSocket must be null (and vice versa).
  nsRefPtr<BluetoothSocket> mSocket;

  // Server sockets. Once an inbound connection is established, it will hand
  // over the ownership to mSocket, and get a new server socket while Listen()
  // is called.
  nsRefPtr<BluetoothSocket> mServerSocket;
};

END_BLUETOOTH_NAMESPACE

#endif
