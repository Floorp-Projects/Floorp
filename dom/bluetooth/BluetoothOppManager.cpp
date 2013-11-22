/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothOppManager.h"

#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"
#include "BluetoothUuid.h"
#include "ObexBase.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsCExternalHandlerService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIDOMFile.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIMIMEService.h"
#include "nsIOutputStream.h"
#include "nsIVolumeService.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"

#define TARGET_SUBDIR "Download/Bluetooth/"

USING_BLUETOOTH_NAMESPACE
using namespace mozilla;
using namespace mozilla::ipc;

namespace {
// Sending system message "bluetooth-opp-update-progress" every 50kb
static const uint32_t kUpdateProgressBase = 50 * 1024;

/*
 * The format of the header of an PUT request is
 * [opcode:1][packet length:2][headerId:1][header length:2]
 */
static const uint32_t kPutRequestHeaderSize = 6;

StaticRefPtr<BluetoothOppManager> sBluetoothOppManager;
static bool sInShutdown = false;
}

class mozilla::dom::bluetooth::SendFileBatch {
public:
  SendFileBatch(const nsAString& aDeviceAddress, BlobParent* aActor)
    : mDeviceAddress(aDeviceAddress)
  {
    mBlobs.AppendElement(aActor->GetBlob().get());
  }

  nsString mDeviceAddress;
  nsCOMArray<nsIDOMBlob> mBlobs;
};

NS_IMETHODIMP
BluetoothOppManager::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const PRUnichar* aData)
{
  MOZ_ASSERT(sBluetoothOppManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothOppManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

class SendSocketDataTask : public nsRunnable
{
public:
  SendSocketDataTask(uint8_t* aStream, uint32_t aSize)
    : mStream(aStream)
    , mSize(aSize)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    sBluetoothOppManager->SendPutRequest(mStream, mSize);

    return NS_OK;
  }

private:
  nsAutoArrayPtr<uint8_t> mStream;
  uint32_t mSize;
};

class ReadFileTask : public nsRunnable
{
public:
  ReadFileTask(nsIInputStream* aInputStream,
               uint32_t aRemoteMaxPacketSize) : mInputStream(aInputStream)
  {
    MOZ_ASSERT(NS_IsMainThread());

    mAvailablePacketSize = aRemoteMaxPacketSize - kPutRequestHeaderSize;
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread());

    uint32_t numRead;
    nsAutoArrayPtr<char> buf(new char[mAvailablePacketSize]);

    // function inputstream->Read() only works on non-main thread
    nsresult rv = mInputStream->Read(buf, mAvailablePacketSize, &numRead);
    if (NS_FAILED(rv)) {
      // Needs error handling here
      BT_WARNING("Failed to read from input stream");
      return NS_ERROR_FAILURE;
    }

    if (numRead > 0) {
      sBluetoothOppManager->CheckPutFinal(numRead);

      nsRefPtr<SendSocketDataTask> task =
        new SendSocketDataTask((uint8_t*)buf.forget(), numRead);
      if (NS_FAILED(NS_DispatchToMainThread(task))) {
        BT_WARNING("Failed to dispatch to main thread!");
        return NS_ERROR_FAILURE;
      }
    }

    return NS_OK;
  };

private:
  nsCOMPtr<nsIInputStream> mInputStream;
  uint32_t mAvailablePacketSize;
};

class CloseSocketTask : public Task
{
public:
  CloseSocketTask(BluetoothSocket* aSocket) : mSocket(aSocket)
  {
    MOZ_ASSERT(aSocket);
  }

  void Run() MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mSocket->GetConnectionStatus() ==
        SocketConnectionStatus::SOCKET_CONNECTED) {
      mSocket->Disconnect();
    }
  }

private:
  nsRefPtr<BluetoothSocket> mSocket;
};

BluetoothOppManager::BluetoothOppManager() : mConnected(false)
                                           , mRemoteObexVersion(0)
                                           , mRemoteConnectionFlags(0)
                                           , mRemoteMaxPacketLength(0)
                                           , mLastCommand(0)
                                           , mPacketLength(0)
                                           , mPacketReceivedLength(0)
                                           , mBodySegmentLength(0)
                                           , mAbortFlag(false)
                                           , mNewFileFlag(false)
                                           , mPutFinalFlag(false)
                                           , mSendTransferCompleteFlag(false)
                                           , mSuccessFlag(false)
                                           , mIsServer(true)
                                           , mWaitingForConfirmationFlag(false)
                                           , mFileLength(0)
                                           , mSentFileLength(0)
                                           , mWaitingToSendPutFinal(false)
                                           , mCurrentBlobIndex(-1)
{
  mConnectedDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
}

BluetoothOppManager::~BluetoothOppManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }
}

bool
BluetoothOppManager::Init()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);
  if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
    BT_WARNING("Failed to add shutdown observer!");
    return false;
  }

  Listen();

  return true;
}

//static
BluetoothOppManager*
BluetoothOppManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothOppManager already exists, exit early
  if (sBluetoothOppManager) {
    return sBluetoothOppManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(sInShutdown, nullptr);

  // Create a new instance, register, and return
  BluetoothOppManager *manager = new BluetoothOppManager();
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  sBluetoothOppManager = manager;
  return sBluetoothOppManager;
}

void
BluetoothOppManager::ConnectInternal(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Stop listening because currently we only support one connection at a time.
  if (mRfcommSocket) {
    mRfcommSocket->Disconnect();
    mRfcommSocket = nullptr;
  }

  if (mL2capSocket) {
    mL2capSocket->Disconnect();
    mL2capSocket = nullptr;
  }

  mIsServer = false;

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || sInShutdown || mSocket) {
    OnSocketConnectError(mSocket);
    return;
  }

  mNeedsUpdatingSdpRecords = true;

  nsString uuid;
  BluetoothUuidHelper::GetString(BluetoothServiceClass::OBJECT_PUSH, uuid);

  if (NS_FAILED(bs->GetServiceChannel(aDeviceAddress, uuid, this))) {
    OnSocketConnectError(mSocket);
    return;
  }

  mSocket =
    new BluetoothSocket(this, BluetoothSocketType::RFCOMM, true, true);
}

void
BluetoothOppManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;

  if (mSocket) {
    mSocket->Disconnect();
    mSocket = nullptr;
  }
  if (mRfcommSocket) {
    mRfcommSocket->Disconnect();
    mRfcommSocket = nullptr;
  }
  if (mL2capSocket) {
    mL2capSocket->Disconnect();
    mL2capSocket = nullptr;
  }
  sBluetoothOppManager = nullptr;
}

bool
BluetoothOppManager::Listen()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mSocket) {
    BT_WARNING("mSocket exists. Failed to listen.");
    return false;
  }

  if (!mRfcommSocket) {
    mRfcommSocket =
      new BluetoothSocket(this, BluetoothSocketType::RFCOMM, true, true);

    if (!mRfcommSocket->Listen(BluetoothReservedChannels::CHANNEL_OPUSH)) {
      BT_WARNING("[OPP] Can't listen on RFCOMM socket!");
      mRfcommSocket = nullptr;
      return false;
    }
  }

  if (!mL2capSocket) {
    mL2capSocket =
      new BluetoothSocket(this, BluetoothSocketType::EL2CAP, true, true);

    if (!mL2capSocket->Listen(BluetoothReservedChannels::CHANNEL_OPUSH_L2CAP)) {
      BT_WARNING("[OPP] Can't listen on L2CAP socket!");
      mRfcommSocket->Disconnect();
      mRfcommSocket = nullptr;
      mL2capSocket = nullptr;
      return false;
    }
  }

  mIsServer = true;

  return true;
}

void
BluetoothOppManager::StartSendingNextFile()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!IsConnected());
  MOZ_ASSERT(!mBatches.IsEmpty());
  MOZ_ASSERT(mBatches[0].mBlobs.Length() > mCurrentBlobIndex + 1);

  mBlob = mBatches[0].mBlobs[++mCurrentBlobIndex];

  // Before sending content, we have to send a header including
  // information such as file name, file length and content type.
  ExtractBlobHeaders();
  StartFileTransfer();

  if (mCurrentBlobIndex == 0) {
    // We may have more than one file waiting for transferring, but only one
    // CONNECT request would be sent. Therefore check if this is the very first
    // file at the head of queue.
    SendConnectRequest();
  } else {
    SendPutHeaderRequest(mFileName, mFileLength);
    AfterFirstPut();
  }
}

bool
BluetoothOppManager::SendFile(const nsAString& aDeviceAddress,
                              BlobParent* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());

  AppendBlobToSend(aDeviceAddress, aActor);
  if (!mSocket) {
    ProcessNextBatch();
  }

  return true;
}

void
BluetoothOppManager::AppendBlobToSend(const nsAString& aDeviceAddress,
                                      BlobParent* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());

  int indexTail = mBatches.Length() - 1;

  /**
   * Create a new batch if
   * - mBatches is empty, or
   * - aDeviceAddress differs from mDeviceAddress of the last batch
   */
  if (mBatches.IsEmpty() ||
      aDeviceAddress != mBatches[indexTail].mDeviceAddress) {
    SendFileBatch batch(aDeviceAddress, aActor);
    mBatches.AppendElement(batch);
  } else {
    mBatches[indexTail].mBlobs.AppendElement(aActor->GetBlob().get());
  }
}

void
BluetoothOppManager::DiscardBlobsToSend()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!mBatches.IsEmpty());
  MOZ_ASSERT(!mIsServer);

  int length = (int) mBatches[0].mBlobs.Length();
  while (length > mCurrentBlobIndex + 1) {
    mBlob = mBatches[0].mBlobs[++mCurrentBlobIndex];

    BT_LOGR("%s: idx %d", __FUNCTION__, mCurrentBlobIndex);
    ExtractBlobHeaders();
    StartFileTransfer();
    FileTransferComplete();
  }
}

bool
BluetoothOppManager::ProcessNextBatch()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Remove the processed batch.
  // A batch is processed if we've incremented mCurrentBlobIndex for it.
  if (mCurrentBlobIndex >= 0) {
    ClearQueue();
    mBatches.RemoveElementAt(0);
    BT_LOGR("%s: REMOVE. %d remaining", __FUNCTION__, mBatches.Length());
  }

  // Process the next batch
  if (!mBatches.IsEmpty()) {
    ConnectInternal(mBatches[0].mDeviceAddress);
    return true;
  }

  // No more batch to process
  return false;
}

void
BluetoothOppManager::ClearQueue()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!mIsServer);
  MOZ_ASSERT(!mBatches.IsEmpty());
  MOZ_ASSERT(!mBatches[0].mBlobs.IsEmpty());

  mCurrentBlobIndex = -1;
  mBlob = nullptr;
  mBatches[0].mBlobs.Clear();
}

bool
BluetoothOppManager::StopSendingFile()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mIsServer) {
    mAbortFlag = true;
  } else if (mSocket) {
    mSocket->Disconnect();
  } else {
    BT_WARNING("%s: No ongoing file transfer to stop", __FUNCTION__);
  }

  return true;
}

bool
BluetoothOppManager::ConfirmReceivingFile(bool aConfirm)
{
  NS_ENSURE_TRUE(mConnected, false);
  NS_ENSURE_TRUE(mWaitingForConfirmationFlag, false);

  MOZ_ASSERT(mPacketReceivedLength == 0);

  mWaitingForConfirmationFlag = false;

  // For the first packet of first file
  bool success = false;
  if (aConfirm) {
    StartFileTransfer();
    if (CreateFile()) {
      success = WriteToFile(mBodySegment.get(), mBodySegmentLength);
    }
  }

  if (success && mPutFinalFlag) {
    mSuccessFlag = true;
    FileTransferComplete();
    NotifyAboutFileChange();
  }

  ReplyToPut(mPutFinalFlag, success);
  return true;
}

void
BluetoothOppManager::AfterFirstPut()
{
  mUpdateProgressCounter = 1;
  mPutFinalFlag = false;
  mPacketReceivedLength = 0;
  mSentFileLength = 0;
  mWaitingToSendPutFinal = false;
  mSuccessFlag = false;
  mBodySegmentLength = 0;
}

void
BluetoothOppManager::AfterOppConnected()
{
  MOZ_ASSERT(NS_IsMainThread());

  mConnected = true;
  mAbortFlag = false;
  mWaitingForConfirmationFlag = true;
  AfterFirstPut();
  // Get a mount lock to prevent the sdcard from being shared with
  // the PC while we're doing a OPP file transfer. After OPP transaction
  // were done, the mount lock will be freed.
  if (!AcquireSdcardMountLock()) {
    // If we fail to get a mount lock, abort this transaction
    // Directly sending disconnect-request is better than abort-request
    BT_WARNING("BluetoothOPPManager couldn't get a mount lock!");

    MOZ_ASSERT(mSocket);
    mSocket->Disconnect();
  }
}

void
BluetoothOppManager::AfterOppDisconnected()
{
  MOZ_ASSERT(NS_IsMainThread());

  mConnected = false;
  mLastCommand = 0;
  mPacketReceivedLength = 0;
  mDsFile = nullptr;

  // We can't reset mSuccessFlag here since this function may be called
  // before we send system message of transfer complete
  // mSuccessFlag = false;

  if (mInputStream) {
    mInputStream->Close();
    mInputStream = nullptr;
  }

  if (mOutputStream) {
    mOutputStream->Close();
    mOutputStream = nullptr;
  }

  if (mReadFileThread) {
    mReadFileThread->Shutdown();
    mReadFileThread = nullptr;
  }
  // Release the Mount lock if file transfer completed
  if (mMountLock) {
    // The mount lock will be implicitly unlocked
    mMountLock = nullptr;
  }
}

void
BluetoothOppManager::DeleteReceivedFile()
{
  if (mOutputStream) {
    mOutputStream->Close();
    mOutputStream = nullptr;
  }

  if (mDsFile && mDsFile->mFile) {
    mDsFile->mFile->Remove(false);
    mDsFile = nullptr;
  }
}

bool
BluetoothOppManager::CreateFile()
{
  MOZ_ASSERT(mPacketReceivedLength == mPacketLength);

  nsString path;
  path.AssignLiteral(TARGET_SUBDIR);
  path.Append(mFileName);

  mDsFile = DeviceStorageFile::CreateUnique(path, nsIFile::NORMAL_FILE_TYPE, 0644);
  NS_ENSURE_TRUE(mDsFile, false);

  nsCOMPtr<nsIFile> f;
  mDsFile->mFile->Clone(getter_AddRefs(f));

  /*
   * The function CreateUnique() may create a file with a different file
   * name from the original mFileName. Therefore we have to retrieve
   * the file name again.
   */
  f->GetLeafName(mFileName);

  NS_NewLocalFileOutputStream(getter_AddRefs(mOutputStream), f);
  NS_ENSURE_TRUE(mOutputStream, false);

  return true;
}

bool
BluetoothOppManager::WriteToFile(const uint8_t* aData, int aDataLength)
{
  NS_ENSURE_TRUE(mOutputStream, false);

  uint32_t wrote = 0;
  mOutputStream->Write((const char*)aData, aDataLength, &wrote);
  NS_ENSURE_TRUE(aDataLength == (int) wrote, false);

  return true;
}

// Virtual function of class SocketConsumer
void
BluetoothOppManager::ExtractPacketHeaders(const ObexHeaderSet& aHeader)
{
  if (aHeader.Has(ObexHeaderId::Name)) {
    aHeader.GetName(mFileName);
  }

  if (aHeader.Has(ObexHeaderId::Type)) {
    aHeader.GetContentType(mContentType);
  }

  if (aHeader.Has(ObexHeaderId::Length)) {
    aHeader.GetLength(&mFileLength);
  }

  if (aHeader.Has(ObexHeaderId::Body) ||
      aHeader.Has(ObexHeaderId::EndOfBody)) {
    uint8_t* bodyPtr;
    aHeader.GetBody(&bodyPtr);
    mBodySegment = bodyPtr;

    aHeader.GetBodyLength(&mBodySegmentLength);
  }
}

bool
BluetoothOppManager::ExtractBlobHeaders()
{
  RetrieveSentFileName();

  nsresult rv = mBlob->GetType(mContentType);
  if (NS_FAILED(rv)) {
    BT_WARNING("Can't get content type");
    SendDisconnectRequest();
    return false;
  }

  uint64_t fileLength;
  rv = mBlob->GetSize(&fileLength);
  if (NS_FAILED(rv)) {
    BT_WARNING("Can't get file size");
    SendDisconnectRequest();
    return false;
  }

  // Currently we keep the size of files which were sent/received via
  // Bluetooth not exceed UINT32_MAX because the Length header in OBEX
  // is only 4-byte long. Although it is possible to transfer a file
  // larger than UINT32_MAX, it needs to parse another OBEX Header
  // and I would like to leave it as a feature.
  if (fileLength > (uint64_t)UINT32_MAX) {
    BT_WARNING("The file size is too large for now");
    SendDisconnectRequest();
    return false;
  }

  mFileLength = fileLength;
  rv = NS_NewThread(getter_AddRefs(mReadFileThread));
  if (NS_FAILED(rv)) {
    BT_WARNING("Can't create thread");
    SendDisconnectRequest();
    return false;
  }

  return true;
}

void
BluetoothOppManager::RetrieveSentFileName()
{
  mFileName.Truncate();

  nsCOMPtr<nsIDOMFile> file = do_QueryInterface(mBlob);
  if (file) {
    file->GetName(mFileName);
  }

  /**
   * We try our best to get the file extention to avoid interoperability issues.
   * However, once we found that we are unable to get suitable extension or
   * information about the content type, sending a pre-defined file name without
   * extension would be fine.
   */
  if (mFileName.IsEmpty()) {
    mFileName.AssignLiteral("Unknown");
  }

  int32_t offset = mFileName.RFindChar('/');
  if (offset != kNotFound) {
    mFileName = Substring(mFileName, offset + 1);
  }

  offset = mFileName.RFindChar('.');
  if (offset == kNotFound) {
    nsCOMPtr<nsIMIMEService> mimeSvc = do_GetService(NS_MIMESERVICE_CONTRACTID);

    if (mimeSvc) {
      nsString mimeType;
      mBlob->GetType(mimeType);

      nsCString extension;
      nsresult rv =
        mimeSvc->GetPrimaryExtension(NS_LossyConvertUTF16toASCII(mimeType),
                                     EmptyCString(),
                                     extension);
      if (NS_SUCCEEDED(rv)) {
        mFileName.AppendLiteral(".");
        AppendUTF8toUTF16(extension, mFileName);
      }
    }
  }
}

bool
BluetoothOppManager::IsReservedChar(PRUnichar c)
{
  return (c < 0x0020 ||
          c == PRUnichar('?') || c == PRUnichar('|') || c == PRUnichar('<') ||
          c == PRUnichar('>') || c == PRUnichar('"') || c == PRUnichar(':') ||
          c == PRUnichar('/') || c == PRUnichar('*') || c == PRUnichar('\\'));
}

void
BluetoothOppManager::ValidateFileName()
{
  int length = mFileName.Length();

  for (int i = 0; i < length; ++i) {
    // Replace reserved char of fat file system with '_'
    if (IsReservedChar(mFileName.CharAt(i))) {
      mFileName.Replace(i, 1, PRUnichar('_'));
    }
  }
}

bool
BluetoothOppManager::ComposePacket(uint8_t aOpCode, UnixSocketRawData* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aMessage);

  int frameHeaderLength = 0;

  // See if this is the first part of each Put packet
  if (mPacketReceivedLength == 0) {
    // Section 3.3.3 "Put", IrOBEX 1.2
    // [opcode:1][length:2][Headers:var]
    frameHeaderLength = 3;

    mPacketLength = ((((int)aMessage->mData[1]) << 8) | aMessage->mData[2]) -
                      frameHeaderLength;
    /**
     * A PUT request from remote devices may be divided into multiple parts.
     * In other words, one request may need to be received multiple times,
     * so here we keep a variable mPacketLeftLength to indicate if current
     * PUT request is done.
     */
    mReceivedDataBuffer = new uint8_t[mPacketLength];
    mPutFinalFlag = (aOpCode == ObexRequestCode::PutFinal);
  }

  int dataLength = aMessage->mSize - frameHeaderLength;

  // Check length before memcpy to prevent from memory pollution
  if (dataLength < 0 ||
      mPacketReceivedLength + dataLength > mPacketLength) {
    BT_LOGR("%s: Received packet size is unreasonable", __FUNCTION__);

    ReplyToPut(mPutFinalFlag, false);
    DeleteReceivedFile();
    FileTransferComplete();

    return false;
  }

  memcpy(mReceivedDataBuffer.get() + mPacketReceivedLength,
         &aMessage->mData[frameHeaderLength], dataLength);

  mPacketReceivedLength += dataLength;

  return (mPacketReceivedLength == mPacketLength);
}

void
BluetoothOppManager::ServerDataHandler(UnixSocketRawData* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  uint8_t opCode;
  int receivedLength = aMessage->mSize;

  if (mPacketReceivedLength > 0) {
    opCode = mPutFinalFlag ? ObexRequestCode::PutFinal : ObexRequestCode::Put;
  } else {
    opCode = aMessage->mData[0];

    // When there's a Put packet right after a PutFinal packet,
    // which means it's the start point of a new file.
    if (mPutFinalFlag &&
        (opCode == ObexRequestCode::Put ||
         opCode == ObexRequestCode::PutFinal)) {
      mNewFileFlag = true;
      AfterFirstPut();
    }
  }

  ObexHeaderSet pktHeaders(opCode);
  if (opCode == ObexRequestCode::Connect) {
    // Section 3.3.1 "Connect", IrOBEX 1.2
    // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
    // [Headers:var]
    ParseHeaders(&aMessage->mData[7],
                 receivedLength - 7,
                 &pktHeaders);
    ReplyToConnect();
    AfterOppConnected();
  } else if (opCode == ObexRequestCode::Abort) {
    // Section 3.3.5 "Abort", IrOBEX 1.2
    // [opcode:1][length:2][Headers:var]
    ParseHeaders(&aMessage->mData[3],
                receivedLength - 3,
                &pktHeaders);
    ReplyToDisconnectOrAbort();
    DeleteReceivedFile();
  } else if (opCode == ObexRequestCode::Disconnect) {
    // Section 3.3.2 "Disconnect", IrOBEX 1.2
    // [opcode:1][length:2][Headers:var]
    ParseHeaders(&aMessage->mData[3],
                receivedLength - 3,
                &pktHeaders);
    ReplyToDisconnectOrAbort();
    AfterOppDisconnected();
    FileTransferComplete();
  } else if (opCode == ObexRequestCode::Put ||
             opCode == ObexRequestCode::PutFinal) {
    if (!ComposePacket(opCode, aMessage)) {
      return;
    }

    // A Put packet is received completely
    ParseHeaders(mReceivedDataBuffer.get(), mPacketReceivedLength, &pktHeaders);
    ExtractPacketHeaders(pktHeaders);
    ValidateFileName();

    mPacketReceivedLength = 0;

    // When we cancel the transfer, delete the file and notify completion
    if (mAbortFlag) {
      ReplyToPut(mPutFinalFlag, false);
      mSentFileLength += mBodySegmentLength;
      DeleteReceivedFile();
      FileTransferComplete();
      return;
    }

    // Wait until get confirmation from user, then create file and write to it
    if (mWaitingForConfirmationFlag) {
      ReceivingFileConfirmation();
      mSentFileLength += mBodySegmentLength;
      return;
    }

    // Already get confirmation from user, create a new file if needed and
    // write to output stream
    if (mNewFileFlag) {
      StartFileTransfer();
      if (!CreateFile()) {
        ReplyToPut(mPutFinalFlag, false);
        return;
      }
      mNewFileFlag = false;
    }

    if (!WriteToFile(mBodySegment.get(), mBodySegmentLength)) {
      ReplyToPut(mPutFinalFlag, false);
      return;
    }

    ReplyToPut(mPutFinalFlag, true);

    // Send progress update
    mSentFileLength += mBodySegmentLength;
    if (mSentFileLength > kUpdateProgressBase * mUpdateProgressCounter) {
      UpdateProgress();
      mUpdateProgressCounter = mSentFileLength / kUpdateProgressBase + 1;
    }

    // Success to receive a file and notify completion
    if (mPutFinalFlag) {
      mSuccessFlag = true;
      FileTransferComplete();
      NotifyAboutFileChange();
    }
  } else if (opCode == ObexRequestCode::Get ||
             opCode == ObexRequestCode::GetFinal ||
             opCode == ObexRequestCode::SetPath) {
    ReplyError(ObexResponseCode::BadRequest);
    BT_WARNING("Unsupported ObexRequestCode");
  } else {
    ReplyError(ObexResponseCode::NotImplemented);
    BT_WARNING("Unrecognized ObexRequestCode");
  }
}

void
BluetoothOppManager::ClientDataHandler(UnixSocketRawData* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  uint8_t opCode = aMessage->mData[0];

  // Check response code and send out system message as finished if the response
  // code is somehow incorrect.
  uint8_t expectedOpCode = ObexResponseCode::Success;
  if (mLastCommand == ObexRequestCode::Put) {
    expectedOpCode = ObexResponseCode::Continue;
  }

  if (opCode != expectedOpCode) {
    if (mLastCommand == ObexRequestCode::Put ||
        mLastCommand == ObexRequestCode::Abort ||
        mLastCommand == ObexRequestCode::PutFinal) {
      SendDisconnectRequest();
    }
    nsAutoCString str;
    str += "[OPP] 0x";
    str.AppendInt(mLastCommand, 16);
    str += " failed";
    BT_WARNING(str.get());
    FileTransferComplete();
    return;
  }

  if (mLastCommand == ObexRequestCode::PutFinal) {
    mSuccessFlag = true;
    FileTransferComplete();

    if (mInputStream) {
      mInputStream->Close();
      mInputStream = nullptr;
    }

    if (mCurrentBlobIndex + 1 == (int) mBatches[0].mBlobs.Length()) {
      SendDisconnectRequest();
    } else {
      StartSendingNextFile();
    }
  } else if (mLastCommand == ObexRequestCode::Abort) {
    SendDisconnectRequest();
    FileTransferComplete();
  } else if (mLastCommand == ObexRequestCode::Disconnect) {
    AfterOppDisconnected();
    // Most devices will directly terminate connection after receiving
    // Disconnect request, so we make a delay here. If the socket hasn't been
    // disconnected, we will close it.
    if (mSocket) {
      MessageLoop::current()->
        PostDelayedTask(FROM_HERE, new CloseSocketTask(mSocket), 1000);
    }
  } else if (mLastCommand == ObexRequestCode::Connect) {
    MOZ_ASSERT(!mFileName.IsEmpty());
    MOZ_ASSERT(mBlob);

    AfterOppConnected();

    // Keep remote information
    mRemoteObexVersion = aMessage->mData[3];
    mRemoteConnectionFlags = aMessage->mData[4];
    mRemoteMaxPacketLength =
      (((int)(aMessage->mData[5]) << 8) | aMessage->mData[6]);

    SendPutHeaderRequest(mFileName, mFileLength);
  } else if (mLastCommand == ObexRequestCode::Put) {
    if (mWaitingToSendPutFinal) {
      SendPutFinalRequest();
      return;
    }

    if (kUpdateProgressBase * mUpdateProgressCounter < mSentFileLength) {
      UpdateProgress();
      mUpdateProgressCounter = mSentFileLength / kUpdateProgressBase + 1;
    }

    nsresult rv;
    if (!mInputStream) {
      rv = mBlob->GetInternalStream(getter_AddRefs(mInputStream));
      if (NS_FAILED(rv)) {
        BT_WARNING("Can't get internal stream of blob");
        SendDisconnectRequest();
        return;
      }
    }

    nsRefPtr<ReadFileTask> task = new ReadFileTask(mInputStream,
                                                   mRemoteMaxPacketLength);
    rv = mReadFileThread->Dispatch(task, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      BT_WARNING("Cannot dispatch read file task!");
      SendDisconnectRequest();
    }
  } else {
    BT_WARNING("Unhandled ObexRequestCode");
  }
}

// Virtual function of class SocketConsumer
void
BluetoothOppManager::ReceiveSocketData(BluetoothSocket* aSocket,
                                       nsAutoPtr<UnixSocketRawData>& aMessage)
{
  if (mIsServer) {
    ServerDataHandler(aMessage);
  } else {
    ClientDataHandler(aMessage);
  }
}

void
BluetoothOppManager::SendConnectRequest()
{
  if (mConnected) return;

  // Section 3.3.1 "Connect", IrOBEX 1.2
  // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
  // [Headers:var]
  uint8_t req[255];
  int index = 7;

  req[3] = 0x10; // version=1.0
  req[4] = 0x00; // flag=0x00
  req[5] = BluetoothOppManager::MAX_PACKET_LENGTH >> 8;
  req[6] = (uint8_t)BluetoothOppManager::MAX_PACKET_LENGTH;

  SendObexData(req, ObexRequestCode::Connect, index);
}

void
BluetoothOppManager::SendPutHeaderRequest(const nsAString& aFileName,
                                          int aFileSize)
{
  if (!mConnected) return;

  uint8_t* req = new uint8_t[mRemoteMaxPacketLength];

  int len = aFileName.Length();
  uint8_t* fileName = new uint8_t[(len + 1) * 2];
  const PRUnichar* fileNamePtr = aFileName.BeginReading();

  for (int i = 0; i < len; i++) {
    fileName[i * 2] = (uint8_t)(fileNamePtr[i] >> 8);
    fileName[i * 2 + 1] = (uint8_t)fileNamePtr[i];
  }

  fileName[len * 2] = 0x00;
  fileName[len * 2 + 1] = 0x00;

  int index = 3;
  index += AppendHeaderName(&req[index], (char*)fileName, (len + 1) * 2);
  index += AppendHeaderLength(&req[index], aFileSize);

  SendObexData(req, ObexRequestCode::Put, index);

  delete [] fileName;
  delete [] req;
}

void
BluetoothOppManager::SendPutRequest(uint8_t* aFileBody,
                                    int aFileBodyLength)
{
  int packetLeftSpace = mRemoteMaxPacketLength - kPutRequestHeaderSize;

  if (!mConnected) return;
  if (aFileBodyLength > packetLeftSpace) {
    BT_WARNING("Not allowed such a small MaxPacketLength value");
    return;
  }

  // Section 3.3.3 "Put", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t* req = new uint8_t[mRemoteMaxPacketLength];

  int index = 3;
  index += AppendHeaderBody(&req[index], aFileBody, aFileBodyLength);

  SendObexData(req, ObexRequestCode::Put, index);
  delete [] req;

  mSentFileLength += aFileBodyLength;
}

void
BluetoothOppManager::SendPutFinalRequest()
{
  if (!mConnected) return;

  /**
   * Section 2.2.9, "End-of-Body", IrObex 1.2
   * End-of-Body is used to identify the last chunk of the object body.
   * For most platforms, a PutFinal packet is sent with an zero length
   * End-of-Body header.
   */

  // [opcode:1][length:2]
  int index = 3;
  uint8_t* req = new uint8_t[mRemoteMaxPacketLength];
  index += AppendHeaderEndOfBody(&req[index]);

  SendObexData(req, ObexRequestCode::PutFinal, index);
  delete [] req;

  mWaitingToSendPutFinal = false;
}

void
BluetoothOppManager::SendDisconnectRequest()
{
  if (!mConnected) return;

  // Section 3.3.2 "Disconnect", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendObexData(req, ObexRequestCode::Disconnect, index);
}

void
BluetoothOppManager::CheckPutFinal(uint32_t aNumRead)
{
  if (mSentFileLength + aNumRead >= mFileLength) {
    mWaitingToSendPutFinal = true;
  }
}

bool
BluetoothOppManager::IsConnected()
{
  return (mConnected && !mSendTransferCompleteFlag);
}

void
BluetoothOppManager::GetAddress(nsAString& aDeviceAddress)
{
  return mSocket->GetAddress(aDeviceAddress);
}

void
BluetoothOppManager::ReplyToConnect()
{
  if (mConnected) return;

  // Section 3.3.1 "Connect", IrOBEX 1.2
  // [opcode:1][length:2][version:1][flags:1][MaxPktSizeWeCanReceive:2]
  // [Headers:var]
  uint8_t req[255];
  int index = 7;

  req[3] = 0x10; // version=1.0
  req[4] = 0x00; // flag=0x00
  req[5] = BluetoothOppManager::MAX_PACKET_LENGTH >> 8;
  req[6] = (uint8_t)BluetoothOppManager::MAX_PACKET_LENGTH;

  SendObexData(req, ObexResponseCode::Success, index);
}

void
BluetoothOppManager::ReplyToDisconnectOrAbort()
{
  if (!mConnected) return;

  // Section 3.3.2 "Disconnect" and Section 3.3.5 "Abort", IrOBEX 1.2
  // The format of response packet of "Disconnect" and "Abort" are the same
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendObexData(req, ObexResponseCode::Success, index);
}

void
BluetoothOppManager::ReplyToPut(bool aFinal, bool aContinue)
{
  if (!mConnected) return;

  // Section 3.3.2 "Disconnect", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;
  uint8_t opcode;

  if (aContinue) {
    opcode = (aFinal)? ObexResponseCode::Success :
                       ObexResponseCode::Continue;
  } else {
    opcode = (aFinal)? ObexResponseCode::Unauthorized :
                       ObexResponseCode::Unauthorized & (~FINAL_BIT);
  }

  SendObexData(req, opcode, index);
}

void
BluetoothOppManager::ReplyError(uint8_t aError)
{
  if (!mConnected) return;

  // Section 3.2 "Response Format", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendObexData(req, aError, index);
}

void
BluetoothOppManager::SendObexData(uint8_t* aData, uint8_t aOpcode, int aSize)
{
  SetObexPacketInfo(aData, aOpcode, aSize);

  if (!mIsServer) {
    mLastCommand = aOpcode;
  }

  UnixSocketRawData* s = new UnixSocketRawData(aSize);
  memcpy(s->mData, aData, s->mSize);
  mSocket->SendSocketData(s);
}

void
BluetoothOppManager::FileTransferComplete()
{
  if (mSendTransferCompleteFlag) {
    return;
  }

  nsString type, name;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type.AssignLiteral("bluetooth-opp-transfer-complete");

  name.AssignLiteral("address");
  v = mConnectedDeviceAddress;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("success");
  v = mSuccessFlag;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("received");
  v = mIsServer;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("fileName");
  v = mFileName;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("fileLength");
  v = mSentFileLength;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("contentType");
  v = mContentType;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  if (!BroadcastSystemMessage(type, parameters)) {
    BT_WARNING("Failed to broadcast [bluetooth-opp-transfer-complete]");
    return;
  }

  mSendTransferCompleteFlag = true;
}

void
BluetoothOppManager::StartFileTransfer()
{
  nsString type, name;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type.AssignLiteral("bluetooth-opp-transfer-start");

  name.AssignLiteral("address");
  v = mConnectedDeviceAddress;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("received");
  v = mIsServer;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("fileName");
  v = mFileName;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("fileLength");
  v = mFileLength;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("contentType");
  v = mContentType;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  if (!BroadcastSystemMessage(type, parameters)) {
    BT_WARNING("Failed to broadcast [bluetooth-opp-transfer-start]");
    return;
  }

  mSendTransferCompleteFlag = false;
}

void
BluetoothOppManager::UpdateProgress()
{
  nsString type, name;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type.AssignLiteral("bluetooth-opp-update-progress");

  name.AssignLiteral("address");
  v = mConnectedDeviceAddress;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("received");
  v = mIsServer;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("processedLength");
  v = mSentFileLength;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("fileLength");
  v = mFileLength;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  if (!BroadcastSystemMessage(type, parameters)) {
    BT_WARNING("Failed to broadcast [bluetooth-opp-update-progress]");
    return;
  }
}

void
BluetoothOppManager::ReceivingFileConfirmation()
{
  nsString type, name;
  BluetoothValue v;
  InfallibleTArray<BluetoothNamedValue> parameters;
  type.AssignLiteral("bluetooth-opp-receiving-file-confirmation");

  name.AssignLiteral("address");
  v = mConnectedDeviceAddress;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("fileName");
  v = mFileName;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("fileLength");
  v = mFileLength;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  name.AssignLiteral("contentType");
  v = mContentType;
  parameters.AppendElement(BluetoothNamedValue(name, v));

  if (!BroadcastSystemMessage(type, parameters)) {
    BT_WARNING("Failed to send [bluetooth-opp-receiving-file-confirmation]");
    return;
  }
}

void
BluetoothOppManager::NotifyAboutFileChange()
{
  NS_NAMED_LITERAL_STRING(data, "modified");

  nsCOMPtr<nsIObserverService> obs =
    mozilla::services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  obs->NotifyObservers(mDsFile, "file-watcher-notify", data.get());
}

void
BluetoothOppManager::OnSocketConnectSuccess(BluetoothSocket* aSocket)
{
  BT_LOGR("%s: [%s]", __FUNCTION__, (mIsServer)? "server" : "client");
  MOZ_ASSERT(aSocket);

  /**
   * If the created connection is an inbound connection, close another server
   * socket because currently only one file-transfer session is allowed. After
   * that, we need to make sure that both server socket would be nulled out.
   * As for outbound connections, we just notify the controller that it's done.
   */
  if (aSocket == mRfcommSocket) {
    MOZ_ASSERT(!mSocket);
    mRfcommSocket.swap(mSocket);

    mL2capSocket->Disconnect();
    mL2capSocket = nullptr;
  } else if (aSocket == mL2capSocket) {
    MOZ_ASSERT(!mSocket);
    mL2capSocket.swap(mSocket);

    mRfcommSocket->Disconnect();
    mRfcommSocket = nullptr;
  }

  // Cache device address since we can't get socket address when a remote
  // device disconnect with us.
  mSocket->GetAddress(mConnectedDeviceAddress);

  // Start sending file if we connect as a client
  if (!mIsServer) {
    StartSendingNextFile();
  }
}

void
BluetoothOppManager::OnSocketConnectError(BluetoothSocket* aSocket)
{
  BT_LOGR("%s: [%s]", __FUNCTION__, (mIsServer)? "server" : "client");

  mRfcommSocket = nullptr;
  mL2capSocket = nullptr;
  mSocket = nullptr;

  if (!mIsServer) {
    // Inform gaia of remaining blobs' sending failure
    DiscardBlobsToSend();
  }

  // Listen as a server if there's no more batch to process
  if (!ProcessNextBatch()) {
    Listen();
  }
}

void
BluetoothOppManager::OnSocketDisconnect(BluetoothSocket* aSocket)
{
  BT_LOGR("%s: [%s]", __FUNCTION__, (mIsServer)? "server" : "client");
  MOZ_ASSERT(aSocket);

  if (aSocket != mSocket) {
    // Do nothing when a listening server socket is closed.
    return;
  }

  /**
   * It is valid for a bluetooth device which is transfering file via OPP
   * closing socket without sending OBEX disconnect request first. So we
   * delete the broken file when we failed to receive a file from the remote,
   * and notify the transfer has been completed (but failed). We also call
   * AfterOppDisconnected here to ensure all variables will be cleaned.
   */
  if (!mSuccessFlag) {
    if (mIsServer) {
      DeleteReceivedFile();
    }

    FileTransferComplete();
    if (!mIsServer) {
      // Inform gaia of remaining blobs' sending failure
      DiscardBlobsToSend();
    }
  }

  AfterOppDisconnected();
  mConnectedDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
  mSuccessFlag = false;

  mSocket = nullptr;
  // Listen as a server if there's no more batch to process
  if (!ProcessNextBatch()) {
    Listen();
  }
}

void
BluetoothOppManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                         const nsAString& aServiceUuid,
                                         int aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (aChannel < 0) {
    if (mNeedsUpdatingSdpRecords) {
      mNeedsUpdatingSdpRecords = false;
      bs->UpdateSdpRecords(aDeviceAddress, this);
    } else {
      OnSocketConnectError(mSocket);
    }

    return;
  }

  if (!mSocket->Connect(NS_ConvertUTF16toUTF8(aDeviceAddress), aChannel)) {
    OnSocketConnectError(mSocket);
  }
}

void
BluetoothOppManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  nsString uuid;
  BluetoothUuidHelper::GetString(BluetoothServiceClass::OBJECT_PUSH, uuid);

  if (NS_FAILED(bs->GetServiceChannel(aDeviceAddress, uuid, this))) {
    OnSocketConnectError(mSocket);
  }
}

NS_IMPL_ISUPPORTS1(BluetoothOppManager, nsIObserver)

bool
BluetoothOppManager::AcquireSdcardMountLock()
{
  nsCOMPtr<nsIVolumeService> volumeSrv =
    do_GetService(NS_VOLUMESERVICE_CONTRACTID);
  NS_ENSURE_TRUE(volumeSrv, false);
  nsresult rv;
  rv = volumeSrv->CreateMountLock(NS_LITERAL_STRING("sdcard"),
                                  getter_AddRefs(mMountLock));
  NS_ENSURE_SUCCESS(rv, false);
  return true;
}

void
BluetoothOppManager::Connect(const nsAString& aDeviceAddress,
                             BluetoothProfileController* aController)
{
  MOZ_ASSERT(false);
}

void
BluetoothOppManager::Disconnect(BluetoothProfileController* aController)
{
  MOZ_ASSERT(false);
}

void
BluetoothOppManager::OnConnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(false);
}

void
BluetoothOppManager::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(false);
}
