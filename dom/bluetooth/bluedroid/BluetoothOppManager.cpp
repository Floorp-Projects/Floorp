/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsCExternalHandlerService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
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
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {
  // Sending system message "bluetooth-opp-update-progress" every 50kb
  static const uint32_t kUpdateProgressBase = 50 * 1024;

  /*
   * The format of the header of an PUT request is
   * [opcode:1][packet length:2][headerId:1][header length:2]
   */
  static const uint32_t kPutRequestHeaderSize = 6;

  /*
   * The format of the appended header of an PUT request is
   * [headerId:1][header length:4]
   * P.S. Length of name header is 4 since unicode is 2 bytes per char.
   */
  static const uint32_t kPutRequestAppendHeaderSize = 5;

  // UUID of OBEX Object Push
  static const BluetoothUuid kObexObjectPush = {
    {
      0x00, 0x00, 0x11, 0x05, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
    }
  };

  StaticRefPtr<BluetoothOppManager> sBluetoothOppManager;
  static bool sInShutdown = false;
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothOppManager::SendFileBatch final
{
public:
  SendFileBatch(const nsAString& aDeviceAddress, nsIDOMBlob* aBlob)
    : mDeviceAddress(aDeviceAddress)
  {
    mBlobs.AppendElement(aBlob);
  }

  nsString mDeviceAddress;
  nsCOMArray<nsIDOMBlob> mBlobs;
};

NS_IMETHODIMP
BluetoothOppManager::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  MOZ_ASSERT(sBluetoothOppManager);

  // if state of any volume was changed
  if (!strcmp(aTopic, NS_VOLUME_STATE_CHANGED)) {
    HandleVolumeStateChanged(aSubject);
    return NS_OK;
  }

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothOppManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

class BluetoothOppManager::SendSocketDataTask final : public nsRunnable
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

class BluetoothOppManager::ReadFileTask final : public nsRunnable
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

class BluetoothOppManager::CloseSocketTask final : public Task
{
public:
  CloseSocketTask(BluetoothSocket* aSocket) : mSocket(aSocket)
  {
    MOZ_ASSERT(aSocket);
  }

  void Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    mSocket->CloseSocket();
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
                                           , mPutPacketReceivedLength(0)
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
  mDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
}

BluetoothOppManager::~BluetoothOppManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }

  if (NS_FAILED(obs->RemoveObserver(this, NS_VOLUME_STATE_CHANGED))) {
    BT_WARNING("Failed to remove volume observer!");
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

  if (NS_FAILED(obs->AddObserver(this, NS_VOLUME_STATE_CHANGED, false))) {
    BT_WARNING("Failed to add ns volume observer!");
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
  if (mServerSocket) {
    mServerSocket->CloseSocket();
    mServerSocket = nullptr;
  }

  mIsServer = false;

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || sInShutdown || mSocket) {
    OnSocketConnectError(mSocket);
    return;
  }

  mSocket =
    new BluetoothSocket(this, BluetoothSocketType::RFCOMM, false, true);
  mSocket->ConnectSocket(aDeviceAddress, kObexObjectPush, -1);
}

void
BluetoothOppManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  Disconnect(nullptr);
  sBluetoothOppManager = nullptr;
}

void
BluetoothOppManager::HandleVolumeStateChanged(nsISupports* aSubject)
{
  MOZ_ASSERT(NS_IsMainThread());

  // We will disconnect OPP connection no matter which volume becomes non
  // STATE_MOUNTED, since user may select files scattered in multiple volumes
  // in a single transfer, (e.g., user selects 2 files to transfer, one is in
  // internal volume and the other is in external volume).

  if (!mConnected) {
    return;
  }

  // get volume interface and state
  nsCOMPtr<nsIVolume> vol = do_QueryInterface(aSubject);
  if (!vol) {
    return;
  }
  int32_t state;
  vol->GetState(&state);
  if (nsIVolume::STATE_MOUNTED != state) {
    // Here is for unexpected physical removal of any storage.
    // By checking the volume state change, we should release the unavailable
    // storage to prevent being killed by vold.
    // Disconnect any connected OPP connection since volume state becomes
    // non STATE_MOUNTED. Then |OnSocketDisconnect| would also be called to
    // handle remaining files during send(|DiscardBlobsToSend|) or
    // receive(|DeleteReceivedFile|).
    BT_LOGR("Volume state is not STATE_MOUNTED. Abort any ongoing OPP connection.");
    Disconnect(nullptr);
  }
}

bool
BluetoothOppManager::Listen()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mSocket) {
    BT_WARNING("mSocket exists. Failed to listen.");
    return false;
  }

  /**
   * Restart server socket since its underlying fd becomes invalid when
   * BT stops; otherwise no more read events would be received even if
   * BT restarts.
   */
  if (mServerSocket) {
    mServerSocket->CloseSocket();
    mServerSocket = nullptr;
  }

  mServerSocket =
    new BluetoothSocket(this, BluetoothSocketType::RFCOMM, false, true);

  if (!mServerSocket->ListenSocket(NS_LITERAL_STRING("OBEX Object Push"),
                                   kObexObjectPush,
                                   BluetoothReservedChannels::CHANNEL_OPUSH)) {
    BT_WARNING("[OPP] Can't listen on RFCOMM socket!");
    mServerSocket = nullptr;
    return false;
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
  MOZ_ASSERT((int)mBatches[0].mBlobs.Length() > mCurrentBlobIndex + 1);

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

  nsRefPtr<BlobImpl> impl = aActor->GetBlobImpl();
  nsRefPtr<Blob> blob = Blob::Create(nullptr, impl);

  return SendFile(aDeviceAddress, blob.get());
}

bool
BluetoothOppManager::SendFile(const nsAString& aDeviceAddress,
                              nsIDOMBlob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());

  AppendBlobToSend(aDeviceAddress, aBlob);
  if (!mSocket) {
    ProcessNextBatch();
  }

  return true;
}

void
BluetoothOppManager::AppendBlobToSend(const nsAString& aDeviceAddress,
                                      nsIDOMBlob* aBlob)
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
    SendFileBatch batch(aDeviceAddress, aBlob);
    mBatches.AppendElement(batch);
  } else {
    mBatches[indexTail].mBlobs.AppendElement(aBlob);
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

    BT_LOGR("idx %d", mCurrentBlobIndex);
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
    BT_LOGR("REMOVE. %d remaining", mBatches.Length());
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
  } else {
    Disconnect(nullptr);
  }

  return true;
}

bool
BluetoothOppManager::ConfirmReceivingFile(bool aConfirm)
{
  NS_ENSURE_TRUE(mConnected, false);
  NS_ENSURE_TRUE(mWaitingForConfirmationFlag, false);

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
    RestoreReceivedFileAndNotify();
    FileTransferComplete();
  }

  ReplyToPut(mPutFinalFlag, success);

  return true;
}

void
BluetoothOppManager::AfterFirstPut()
{
  mUpdateProgressCounter = 1;
  mPutFinalFlag = false;
  mPutPacketReceivedLength = 0;
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
    Disconnect(nullptr);
  }
}

void
BluetoothOppManager::AfterOppDisconnected()
{
  MOZ_ASSERT(NS_IsMainThread());

  mConnected = false;
  mLastCommand = 0;
  mPutPacketReceivedLength = 0;
  mDsFile = nullptr;
  mDummyDsFile = nullptr;

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

  // Release the mount lock if file transfer completed
  if (mMountLock) {
    // The mount lock will be implicitly unlocked
    mMountLock = nullptr;
  }
}

void
BluetoothOppManager::RestoreReceivedFileAndNotify()
{
  // Remove the empty dummy file
  if (mDummyDsFile && mDummyDsFile->mFile) {
    mDummyDsFile->mFile->Remove(false);
    mDummyDsFile = nullptr;
  }

  // Remove the trailing ".part" file name from mDsFile by two steps
  // 1. mDsFile->SetPath() so that the notification sent to Gaia will carry
  //    correct information of the file.
  // 2. mDsFile->mFile->RenameTo() so that the file name would actually be
  //    changed in file system.
  if (mDsFile && mDsFile->mFile) {
    nsString path;
    path.AssignLiteral(TARGET_SUBDIR);
    path.Append(mFileName);

    mDsFile->SetPath(path);
    mDsFile->mFile->RenameTo(nullptr, mFileName);
  }

  // Notify about change of received file
  NotifyAboutFileChange();
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

  // Remove the empty dummy file
  if (mDummyDsFile && mDummyDsFile->mFile) {
    mDummyDsFile->mFile->Remove(false);
    mDummyDsFile = nullptr;
  }
}

bool
BluetoothOppManager::CreateFile()
{
  MOZ_ASSERT(mPutPacketReceivedLength == mPacketLength);

  // Create one dummy file to be a placeholder for the target file name, and
  // create another file with a meaningless file extension to write the received
  // data. By doing this, we can prevent applications from parsing incomplete
  // data in the middle of the receiving process.
  nsString path;
  path.AssignLiteral(TARGET_SUBDIR);
  path.Append(mFileName);

  // Use an empty dummy file object to occupy the file name, so that after the
  // whole file has been received successfully by using mDsFile, we could just
  // remove mDummyDsFile and rename mDsFile to the file name of mDummyDsFile.
  mDummyDsFile =
    DeviceStorageFile::CreateUnique(path, nsIFile::NORMAL_FILE_TYPE, 0644);
  NS_ENSURE_TRUE(mDummyDsFile, false);

  // The function CreateUnique() may create a file with a different file
  // name from the original mFileName. Therefore we have to retrieve
  // the file name again.
  mDummyDsFile->mFile->GetLeafName(mFileName);

  BT_LOGR("mFileName: %s", NS_ConvertUTF16toUTF8(mFileName).get());

  // Prepare the entire file path for the .part file
  path.Truncate();
  path.AssignLiteral(TARGET_SUBDIR);
  path.Append(mFileName);
  path.AppendLiteral(".part");

  mDsFile =
    DeviceStorageFile::CreateUnique(path, nsIFile::NORMAL_FILE_TYPE, 0644);
  NS_ENSURE_TRUE(mDsFile, false);

  NS_NewLocalFileOutputStream(getter_AddRefs(mOutputStream), mDsFile->mFile);
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
        mFileName.Append('.');
        AppendUTF8toUTF16(extension, mFileName);
      }
    }
  }
}

bool
BluetoothOppManager::IsReservedChar(char16_t c)
{
  return (c < 0x0020 ||
          c == char16_t('?') || c == char16_t('|') || c == char16_t('<') ||
          c == char16_t('>') || c == char16_t('"') || c == char16_t(':') ||
          c == char16_t('/') || c == char16_t('*') || c == char16_t('\\'));
}

void
BluetoothOppManager::ValidateFileName()
{
  int length = mFileName.Length();

  for (int i = 0; i < length; ++i) {
    // Replace reserved char of fat file system with '_'
    if (IsReservedChar(mFileName.CharAt(i))) {
      mFileName.Replace(i, 1, char16_t('_'));
    }
  }
}

bool
BluetoothOppManager::ComposePacket(uint8_t aOpCode, UnixSocketBuffer* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aMessage);

  const uint8_t* data = aMessage->GetData();
  int frameHeaderLength = 0;

  // See if this is the first part of each Put packet
  if (mPutPacketReceivedLength == 0) {
    // Section 3.3.3 "Put", IrOBEX 1.2
    // [opcode:1][length:2][Headers:var]
    frameHeaderLength = 3;

    mPacketLength = ((static_cast<int>(data[1]) << 8) | data[2]) -
                    frameHeaderLength;

    /**
     * A PUT request from remote devices may be divided into multiple parts.
     * In other words, one request may need to be received multiple times,
     * so here we keep a variable mPutPacketReceivedLength to indicate if
     * current PUT request is done.
     */
    mReceivedDataBuffer = new uint8_t[mPacketLength];
    mPutFinalFlag = (aOpCode == ObexRequestCode::PutFinal);
  }

  int dataLength = aMessage->GetSize() - frameHeaderLength;

  // Check length before memcpy to prevent from memory pollution
  if (dataLength < 0 ||
      mPutPacketReceivedLength + dataLength > mPacketLength) {
    BT_LOGR("Received packet size is unreasonable");

    ReplyToPut(mPutFinalFlag, false);
    DeleteReceivedFile();
    FileTransferComplete();

    return false;
  }

  memcpy(mReceivedDataBuffer.get() + mPutPacketReceivedLength,
         &data[frameHeaderLength], dataLength);

  mPutPacketReceivedLength += dataLength;

  return (mPutPacketReceivedLength == mPacketLength);
}

void
BluetoothOppManager::ServerDataHandler(UnixSocketBuffer* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  uint8_t opCode;
  int receivedLength = aMessage->GetSize();
  const uint8_t* data = aMessage->GetData();

  if (mPutPacketReceivedLength > 0) {
    opCode = mPutFinalFlag ? ObexRequestCode::PutFinal : ObexRequestCode::Put;
  } else {
    opCode = data[0];

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
    if (!ParseHeaders(&data[7], receivedLength - 7, &pktHeaders)) {
      ReplyError(ObexResponseCode::BadRequest);
      return;
    }

    ReplyToConnect();
    AfterOppConnected();
  } else if (opCode == ObexRequestCode::Abort) {
    // Section 3.3.5 "Abort", IrOBEX 1.2
    // [opcode:1][length:2][Headers:var]
    if (!ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
      ReplyError(ObexResponseCode::BadRequest);
      return;
    }

    ReplyToDisconnectOrAbort();
    DeleteReceivedFile();
  } else if (opCode == ObexRequestCode::Disconnect) {
    // Section 3.3.2 "Disconnect", IrOBEX 1.2
    // [opcode:1][length:2][Headers:var]
    if (!ParseHeaders(&data[3], receivedLength - 3, &pktHeaders)) {
      ReplyError(ObexResponseCode::BadRequest);
      return;
    }

    ReplyToDisconnectOrAbort();
    AfterOppDisconnected();
    FileTransferComplete();
  } else if (opCode == ObexRequestCode::Put ||
             opCode == ObexRequestCode::PutFinal) {
    if (!ComposePacket(opCode, aMessage)) {
      return;
    }

    // A Put packet is received completely
    ParseHeaders(mReceivedDataBuffer.get(),
                 mPutPacketReceivedLength, &pktHeaders);
    ExtractPacketHeaders(pktHeaders);
    ValidateFileName();

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
      RestoreReceivedFileAndNotify();
      FileTransferComplete();
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
BluetoothOppManager::ClientDataHandler(UnixSocketBuffer* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  const uint8_t* data = aMessage->GetData();
  uint8_t opCode = data[0];

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
    mRemoteObexVersion = data[3];
    mRemoteConnectionFlags = data[4];
    mRemoteMaxPacketLength = ((static_cast<int>(data[5]) << 8) | data[6]);

    // The length of file name exceeds maximum length.
    int fileNameByteLen = (mFileName.Length() + 1) * 2;
    int headerLen = kPutRequestHeaderSize + kPutRequestAppendHeaderSize;
    if (fileNameByteLen > mRemoteMaxPacketLength - headerLen) {
      BT_WARNING("The length of file name is aberrant.");
      SendDisconnectRequest();
      return;
    }

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
                                       nsAutoPtr<UnixSocketBuffer>& aBuffer)
{
  if (mIsServer) {
    ServerDataHandler(aBuffer);
  } else {
    ClientDataHandler(aBuffer);
  }
}

void
BluetoothOppManager::SendConnectRequest()
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
  req[5] = BluetoothOppManager::MAX_PACKET_LENGTH >> 8;
  req[6] = (uint8_t)BluetoothOppManager::MAX_PACKET_LENGTH;

  SendObexData(req, ObexRequestCode::Connect, index);
}

void
BluetoothOppManager::SendPutHeaderRequest(const nsAString& aFileName,
                                          int aFileSize)
{
  if (!mConnected) {
    return;
  }

  uint8_t* req = new uint8_t[mRemoteMaxPacketLength];

  int len = aFileName.Length();
  uint8_t* fileName = new uint8_t[(len + 1) * 2];
  const char16_t* fileNamePtr = aFileName.BeginReading();

  for (int i = 0; i < len; i++) {
    fileName[i * 2] = (uint8_t)(fileNamePtr[i] >> 8);
    fileName[i * 2 + 1] = (uint8_t)fileNamePtr[i];
  }

  fileName[len * 2] = 0x00;
  fileName[len * 2 + 1] = 0x00;

  int index = 3;
  index += AppendHeaderName(&req[index], mRemoteMaxPacketLength - index,
                            (char*)fileName, (len + 1) * 2);
  index += AppendHeaderLength(&req[index], aFileSize);

  // This is final put packet if file size equals to 0
  uint8_t opcode = (aFileSize > 0) ? ObexRequestCode::Put
                                   : ObexRequestCode::PutFinal;
  SendObexData(req, opcode, index);

  delete [] fileName;
  delete [] req;
}

void
BluetoothOppManager::SendPutRequest(uint8_t* aFileBody,
                                    int aFileBodyLength)
{
  if (!mConnected) {
    return;
  }

  int packetLeftSpace = mRemoteMaxPacketLength - kPutRequestHeaderSize;
  if (aFileBodyLength > packetLeftSpace) {
    BT_WARNING("Not allowed such a small MaxPacketLength value");
    return;
  }

  // Section 3.3.3 "Put", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t* req = new uint8_t[mRemoteMaxPacketLength];

  int index = 3;
  index += AppendHeaderBody(&req[index], mRemoteMaxPacketLength - index,
                            aFileBody, aFileBodyLength);

  SendObexData(req, ObexRequestCode::Put, index);
  delete [] req;

  mSentFileLength += aFileBodyLength;
}

void
BluetoothOppManager::SendPutFinalRequest()
{
  if (!mConnected) {
    return;
  }

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
  if (!mConnected) {
    return;
  }

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
  return mConnected;
}

void
BluetoothOppManager::GetAddress(nsAString& aDeviceAddress)
{
  return mSocket->GetAddress(aDeviceAddress);
}

void
BluetoothOppManager::ReplyToConnect()
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
  req[5] = BluetoothOppManager::MAX_PACKET_LENGTH >> 8;
  req[6] = (uint8_t)BluetoothOppManager::MAX_PACKET_LENGTH;

  SendObexData(req, ObexResponseCode::Success, index);
}

void
BluetoothOppManager::ReplyToDisconnectOrAbort()
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
BluetoothOppManager::ReplyToPut(bool aFinal, bool aContinue)
{
  if (!mConnected) {
    return;
  }

  // The received length can be reset here because this is where we reply to a
  // complete put packet.
  mPutPacketReceivedLength = 0;

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
  BT_LOGR("error: %d", aError);

  // Section 3.2 "Response Format", IrOBEX 1.2
  // [opcode:1][length:2][Headers:var]
  uint8_t req[255];
  int index = 3;

  SendObexData(req, aError, index);
}

void
BluetoothOppManager::SendObexData(uint8_t* aData, uint8_t aOpcode, int aSize)
{
  if (!mIsServer) {
    mLastCommand = aOpcode;
  }

  SetObexPacketInfo(aData, aOpcode, aSize);
  mSocket->SendSocketData(new UnixSocketRawData(aData, aSize));
}

void
BluetoothOppManager::FileTransferComplete()
{
  if (mSendTransferCompleteFlag) {
    return;
  }

  NS_NAMED_LITERAL_STRING(type, "bluetooth-opp-transfer-complete");
  InfallibleTArray<BluetoothNamedValue> parameters;

  BT_APPEND_NAMED_VALUE(parameters, "address", mDeviceAddress);
  BT_APPEND_NAMED_VALUE(parameters, "success", mSuccessFlag);
  BT_APPEND_NAMED_VALUE(parameters, "received", mIsServer);
  BT_APPEND_NAMED_VALUE(parameters, "fileName", mFileName);
  BT_APPEND_NAMED_VALUE(parameters, "fileLength", mSentFileLength);
  BT_APPEND_NAMED_VALUE(parameters, "contentType", mContentType);

  BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(type, parameters);

  mSendTransferCompleteFlag = true;
}

void
BluetoothOppManager::StartFileTransfer()
{
  NS_NAMED_LITERAL_STRING(type, "bluetooth-opp-transfer-start");
  InfallibleTArray<BluetoothNamedValue> parameters;

  BT_APPEND_NAMED_VALUE(parameters, "address", mDeviceAddress);
  BT_APPEND_NAMED_VALUE(parameters, "received", mIsServer);
  BT_APPEND_NAMED_VALUE(parameters, "fileName", mFileName);
  BT_APPEND_NAMED_VALUE(parameters, "fileLength", mFileLength);
  BT_APPEND_NAMED_VALUE(parameters, "contentType", mContentType);

  BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(type, parameters);

  mSendTransferCompleteFlag = false;
}

void
BluetoothOppManager::UpdateProgress()
{
  NS_NAMED_LITERAL_STRING(type, "bluetooth-opp-update-progress");
  InfallibleTArray<BluetoothNamedValue> parameters;

  BT_APPEND_NAMED_VALUE(parameters, "address", mDeviceAddress);
  BT_APPEND_NAMED_VALUE(parameters, "received", mIsServer);
  BT_APPEND_NAMED_VALUE(parameters, "processedLength", mSentFileLength);
  BT_APPEND_NAMED_VALUE(parameters, "fileLength", mFileLength);

  BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(type, parameters);
}

void
BluetoothOppManager::ReceivingFileConfirmation()
{
  NS_NAMED_LITERAL_STRING(type, "bluetooth-opp-receiving-file-confirmation");
  InfallibleTArray<BluetoothNamedValue> parameters;

  BT_APPEND_NAMED_VALUE(parameters, "address", mDeviceAddress);
  BT_APPEND_NAMED_VALUE(parameters, "fileName", mFileName);
  BT_APPEND_NAMED_VALUE(parameters, "fileLength", mFileLength);
  BT_APPEND_NAMED_VALUE(parameters, "contentType", mContentType);

  BT_ENSURE_TRUE_VOID_BROADCAST_SYSMSG(type, parameters);
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
  BT_LOGR("[%s]", (mIsServer)? "server" : "client");
  MOZ_ASSERT(aSocket);

  /**
   * If the created connection is an inbound connection, close server socket
   * because currently only one file-transfer session is allowed. After that,
   * we need to make sure that server socket would be nulled out.
   * As for outbound connections, we just notify the controller that it's done.
   */
  if (aSocket == mServerSocket) {
    MOZ_ASSERT(!mSocket);
    mServerSocket.swap(mSocket);
  }

  // Cache device address since we can't get socket address when a remote
  // device disconnect with us.
  mSocket->GetAddress(mDeviceAddress);

  // Start sending file if we connect as a client
  if (!mIsServer) {
    StartSendingNextFile();
  }
}

void
BluetoothOppManager::OnSocketConnectError(BluetoothSocket* aSocket)
{
  BT_LOGR("[%s]", (mIsServer)? "server" : "client");

  mServerSocket = nullptr;
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
  MOZ_ASSERT(aSocket);
  if (aSocket != mSocket) {
    // Do nothing when a listening server socket is closed.
    return;
  }
  BT_LOGR("[%s]", (mIsServer) ? "server" : "client");

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
  mDeviceAddress.AssignLiteral(BLUETOOTH_ADDRESS_NONE);
  mSuccessFlag = false;

  mSocket = nullptr;
  // Listen as a server if there's no more batch to process
  if (!ProcessNextBatch()) {
    Listen();
  }
}

void
BluetoothOppManager::Disconnect(BluetoothProfileController* aController)
{
  if (mSocket) {
    mSocket->CloseSocket();
  } else {
    BT_WARNING("%s: No ongoing file transfer to stop", __FUNCTION__);
  }
}

NS_IMPL_ISUPPORTS(BluetoothOppManager, nsIObserver)

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
BluetoothOppManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                         const nsAString& aServiceUuid,
                                         int aChannel)
{
  MOZ_ASSERT(false);
}

void
BluetoothOppManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(false);
}

void
BluetoothOppManager::Connect(const nsAString& aDeviceAddress,
                             BluetoothProfileController* aController)
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

void
BluetoothOppManager::Reset()
{
  MOZ_ASSERT(false);
}

END_BLUETOOTH_NAMESPACE
