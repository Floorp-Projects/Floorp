/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothOppManager_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothOppManager_h

#include "BluetoothCommon.h"
#include "BluetoothProfileManagerBase.h"
#include "BluetoothSocketObserver.h"
#include "DeviceStorage.h"
#include "mozilla/ipc/SocketBase.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMArray.h"

class nsIOutputStream;
class nsIInputStream;
class nsIVolumeMountLock;

namespace mozilla {
namespace dom {
class Blob;
class BlobParent;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSocket;
class ObexHeaderSet;

class BluetoothOppManager : public BluetoothSocketObserver
                          , public BluetoothProfileManagerBase
{
  class CloseSocketTask;
  class ReadFileTask;
  class SendFileBatch;
  class SendSocketDataTask;

public:

  BT_DECL_PROFILE_MGR_BASE
  BT_DECL_SOCKET_OBSERVER
  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("OPP");
  }

  static const int MAX_PACKET_LENGTH = 0xFFFE;

  static void InitOppInterface(BluetoothProfileResultHandler* aRes);
  static void DeinitOppInterface(BluetoothProfileResultHandler* aRes);
  static BluetoothOppManager* Get();

  void ClientDataHandler(mozilla::ipc::UnixSocketBuffer* aMessage);
  void ServerDataHandler(mozilla::ipc::UnixSocketBuffer* aMessage);

  bool Listen();

  bool SendFile(const BluetoothAddress& aDeviceAddress, BlobParent* aActor);
  bool SendFile(const BluetoothAddress& aDeviceAddress, Blob* aBlob);
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

protected:
  virtual ~BluetoothOppManager();

private:
  BluetoothOppManager();
  nsresult Init();
  void Uninit();
  void HandleShutdown();
  void HandleVolumeStateChanged(nsISupports* aSubject);

  void StartFileTransfer();
  void StartSendingNextFile();
  void FileTransferComplete();
  void UpdateProgress();
  void ReceivingFileConfirmation();
  bool CreateFile();
  bool WriteToFile(const uint8_t* aData, int aDataLength);
  void RestoreReceivedFileAndNotify();
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
  void SendObexData(UniquePtr<uint8_t[]> aData, uint8_t aOpcode, int aSize);
  void AppendBlobToSend(const BluetoothAddress& aDeviceAddress, Blob* aBlob);
  void DiscardBlobsToSend();
  bool ProcessNextBatch();
  void ConnectInternal(const BluetoothAddress& aDeviceAddress);

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
                     mozilla::ipc::UnixSocketBuffer* aMessage);

  /**
   * OBEX session status.
   * Set when OBEX session is established.
   */
  bool mConnected;
  BluetoothAddress mDeviceAddress;

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
   * This holds the time when OPP manager fail to get service channel and
   * prepare to refresh SDP records.
   */
  mozilla::TimeStamp mLastServiceChannelCheck;

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

  UniquePtr<uint8_t[]> mBodySegment;
  UniquePtr<uint8_t[]> mReceivedDataBuffer;

  int mCurrentBlobIndex;
  RefPtr<Blob> mBlob;
  nsTArray<SendFileBatch> mBatches;

  /**
   * A seperate member thread is required because our read calls can block
   * execution, which is not allowed to happen on the IOThread.
   */
  nsCOMPtr<nsIThread> mReadFileThread;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  nsCOMPtr<nsIInputStream> mInputStream;
  nsCOMPtr<nsIVolumeMountLock> mMountLock;
  RefPtr<DeviceStorageFile> mDsFile;
  RefPtr<DeviceStorageFile> mDummyDsFile;

  // If a connection has been established, mSocket will be the socket
  // communicating with the remote socket. We maintain the invariant that if
  // mSocket is non-null, mServerSocket must be null (and vice versa).
  RefPtr<BluetoothSocket> mSocket;

  // Server sockets. Once an inbound connection is established, it will hand
  // over the ownership to mSocket, and get a new server socket while Listen()
  // is called.
  RefPtr<BluetoothSocket> mServerSocket;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothOppManager_h
