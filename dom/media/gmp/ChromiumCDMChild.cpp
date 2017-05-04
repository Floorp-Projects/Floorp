/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMChild.h"
#include "GMPContentChild.h"
#include "WidevineUtils.h"
#include "WidevineFileIO.h"
#include "WidevineVideoFrame.h"
#include "GMPLog.h"
#include "GMPPlatform.h"
#include "mozilla/Unused.h"
#include "nsPrintfCString.h"
#include "base/time.h"
#include "GMPUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SizePrintfMacros.h"

namespace mozilla {
namespace gmp {

ChromiumCDMChild::ChromiumCDMChild(GMPContentChild* aPlugin)
  : mPlugin(aPlugin)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild:: ctor this=%p", this);
}

void
ChromiumCDMChild::Init(cdm::ContentDecryptionModule_8* aCDM)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  mCDM = aCDM;
  MOZ_ASSERT(mCDM);
}

void
ChromiumCDMChild::TimerExpired(void* aContext)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::TimerExpired(context=0x%p)", aContext);
  if (mCDM) {
    mCDM->TimerExpired(aContext);
  }
}

class CDMShmemBuffer : public CDMBuffer
{
public:
  CDMShmemBuffer(ChromiumCDMChild* aProtocol, ipc::Shmem aShmem)
    : mProtocol(aProtocol)
    , mSize(aShmem.Size<uint8_t>())
    , mShmem(aShmem)
  {
    CDM_LOG("CDMShmemBuffer(size=%" PRIu32 ") created", Size());
    // Note: Chrome initializes the size of a buffer to it capacity. We do the same.
  }

  CDMShmemBuffer(ChromiumCDMChild* aProtocol,
                 ipc::Shmem aShmem,
                 WidevineBuffer* aLocalBuffer)
    : CDMShmemBuffer(aProtocol, aShmem)
  {
    MOZ_ASSERT(aLocalBuffer->Size() == Size());
    memcpy(Data(),
           aLocalBuffer->Data(),
           std::min<uint32_t>(aLocalBuffer->Size(), Size()));
  }

  ~CDMShmemBuffer() override
  {
    CDM_LOG("CDMShmemBuffer(size=%" PRIu32 ") destructed writable=%d",
            Size(),
            mShmem.IsWritable());
    if (mShmem.IsWritable()) {
      // The shmem wasn't extracted to send its data back up to the parent process,
      // so we can reuse the shmem.
      mProtocol->GiveBuffer(Move(mShmem));
    }
  }

  void Destroy() override
  {
    CDM_LOG("CDMShmemBuffer::Destroy(size=%" PRIu32 ")", Size());
    delete this;
  }
  uint32_t Capacity() const override { return mShmem.Size<uint8_t>(); }

  uint8_t* Data() override { return mShmem.get<uint8_t>(); }

  void SetSize(uint32_t aSize) override
  {
    MOZ_ASSERT(aSize <= Capacity());
    // Note: We can't use the shmem's size member after ExtractShmem(),
    // has been called, so we track the size exlicitly so that we can use
    // Size() in logging after we've called ExtractShmem().
    CDM_LOG("CDMShmemBuffer::SetSize(size=%" PRIu32 ")", Size());
    mSize = aSize;
  }

  uint32_t Size() const override { return mSize; }

  ipc::Shmem ExtractShmem()
  {
    ipc::Shmem shmem = mShmem;
    mShmem = ipc::Shmem();
    return shmem;
  }

  CDMShmemBuffer* AsShmemBuffer() override { return this; }

private:
  RefPtr<ChromiumCDMChild> mProtocol;
  uint32_t mSize;
  mozilla::ipc::Shmem mShmem;
  CDMShmemBuffer(const CDMShmemBuffer&);
  void operator=(const CDMShmemBuffer&);
};

static nsCString
ToString(const nsTArray<ipc::Shmem>& aBuffers)
{
  nsCString s;
  for (const ipc::Shmem& shmem : aBuffers) {
    if (!s.IsEmpty()) {
      s.AppendLiteral(",");
    }
    s.AppendInt(static_cast<uint32_t>(shmem.Size<uint8_t>()));
  }
  return s;
}

cdm::Buffer*
ChromiumCDMChild::Allocate(uint32_t aCapacity)
{
  GMP_LOG("ChromiumCDMChild::Allocate(capacity=%" PRIu32 ") bufferSizes={%s}",
          aCapacity,
          ToString(mBuffers).get());
  MOZ_ASSERT(IsOnMessageLoopThread());

  // Find the shmem with the least amount of wasted space if we were to
  // select it for this sized allocation. We need to do this because shmems
  // for decrypted audio as well as video frames are both stored in this
  // list, and we don't want to use the video frame shmems for audio samples.
  const size_t invalid = std::numeric_limits<size_t>::max();
  size_t best = invalid;
  auto wastedSpace = [this, aCapacity](size_t index) {
    return mBuffers[index].Size<uint8_t>() - aCapacity;
  };
  for (size_t i = 0; i < mBuffers.Length(); i++) {
    if (mBuffers[i].Size<uint8_t>() >= aCapacity &&
        (best == invalid || wastedSpace(i) < wastedSpace(best))) {
      best = i;
    }
  }
  if (best == invalid) {
    // The parent process should have bestowed upon us a shmem of appropriate
    // size, but did not! Do a "dive and catch", and create an non-shared
    // memory buffer. The parent will detect this and send us an extra shmem
    // so future frames can be in shmems, i.e. returned on the fast path.
    return new WidevineBuffer(aCapacity);
  }
  ipc::Shmem shmem = mBuffers[best];
  mBuffers.RemoveElementAt(best);
  return new CDMShmemBuffer(this, shmem);
}

void
ChromiumCDMChild::SetTimer(int64_t aDelayMs, void* aContext)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::SetTimer(delay=%" PRId64 ", context=0x%p)",
          aDelayMs,
          aContext);
  RefPtr<ChromiumCDMChild> self(this);
  SetTimerOnMainThread(NewGMPTask([self, aContext]() {
    self->TimerExpired(aContext);
  }), aDelayMs);
}

cdm::Time
ChromiumCDMChild::GetCurrentWallTime()
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  return base::Time::Now().ToDoubleT();
}

void
ChromiumCDMChild::OnResolveNewSessionPromise(uint32_t aPromiseId,
                                             const char* aSessionId,
                                             uint32_t aSessionIdSize)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::OnResolveNewSessionPromise(pid=%" PRIu32
          ", sid=%s)",
          aPromiseId,
          aSessionId);

  if (mLoadSessionPromiseIds.Contains(aPromiseId)) {
    // As laid out in the Chromium CDM API, if the CDM fails to load
    // a session it calls OnResolveNewSessionPromise with nullptr as the sessionId.
    // We can safely assume this means that we have failed to load a session
    // as the other methods specify calling 'OnRejectPromise' when they fail.
    bool loadSuccessful = aSessionId != nullptr;
    GMP_LOG("ChromiumCDMChild::OnResolveNewSessionPromise(pid=%u, sid=%s) "
            "resolving %s load session ",
            aPromiseId,
            aSessionId,
            (loadSuccessful ? "successful" : "failed"));
    Unused << SendResolveLoadSessionPromise(aPromiseId, loadSuccessful);
    mLoadSessionPromiseIds.RemoveElement(aPromiseId);
    return;
  }

  Unused << SendOnResolveNewSessionPromise(aPromiseId,
                                           nsCString(aSessionId, aSessionIdSize));
}

void ChromiumCDMChild::OnResolvePromise(uint32_t aPromiseId)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::OnResolvePromise(pid=%" PRIu32 ")", aPromiseId);
  Unused << SendOnResolvePromise(aPromiseId);
}

void
ChromiumCDMChild::OnRejectPromise(uint32_t aPromiseId,
                                  cdm::Error aError,
                                  uint32_t aSystemCode,
                                  const char* aErrorMessage,
                                  uint32_t aErrorMessageSize)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::OnRejectPromise(pid=%" PRIu32 ", err=%" PRIu32
          " code=%" PRIu32 ", msg='%s')",
          aPromiseId,
          aError,
          aSystemCode,
          aErrorMessage);
  Unused << SendOnRejectPromise(aPromiseId,
                                static_cast<uint32_t>(aError),
                                aSystemCode,
                                nsCString(aErrorMessage, aErrorMessageSize));
}

void
ChromiumCDMChild::OnSessionMessage(const char* aSessionId,
                                   uint32_t aSessionIdSize,
                                   cdm::MessageType aMessageType,
                                   const char* aMessage,
                                   uint32_t aMessageSize,
                                   const char* aLegacyDestinationUrl,
                                   uint32_t aLegacyDestinationUrlLength)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::OnSessionMessage(sid=%s, type=%" PRIu32
          " size=%" PRIu32 ")",
          aSessionId,
          aMessageType,
          aMessageSize);
  nsTArray<uint8_t> message;
  message.AppendElements(aMessage, aMessageSize);
  Unused << SendOnSessionMessage(nsCString(aSessionId, aSessionIdSize),
                                 static_cast<uint32_t>(aMessageType),
                                 message);
}

static nsCString
ToString(const cdm::KeyInformation* aKeysInfo, uint32_t aKeysInfoCount)
{
  nsCString str;
  for (uint32_t i = 0; i < aKeysInfoCount; i++) {
    if (!str.IsEmpty()) {
      str.AppendLiteral(",");
    }
    const cdm::KeyInformation& key = aKeysInfo[i];
    str.Append(ToHexString(key.key_id, key.key_id_size));
    str.AppendLiteral("=");
    str.AppendInt(key.status);
  }
  return str;
}

void
ChromiumCDMChild::OnSessionKeysChange(const char *aSessionId,
                                      uint32_t aSessionIdSize,
                                      bool aHasAdditionalUsableKey,
                                      const cdm::KeyInformation* aKeysInfo,
                                      uint32_t aKeysInfoCount)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::OnSessionKeysChange(sid=%s) keys={%s}",
          aSessionId,
          ToString(aKeysInfo, aKeysInfoCount).get());

  nsTArray<CDMKeyInformation> keys;
  keys.SetCapacity(aKeysInfoCount);
  for (uint32_t i = 0; i < aKeysInfoCount; i++) {
    const cdm::KeyInformation& key = aKeysInfo[i];
    nsTArray<uint8_t> kid;
    kid.AppendElements(key.key_id, key.key_id_size);
    keys.AppendElement(CDMKeyInformation(kid, key.status, key.system_code));
  }
  Unused << SendOnSessionKeysChange(nsCString(aSessionId, aSessionIdSize),
                                    keys);
}

void
ChromiumCDMChild::OnExpirationChange(const char* aSessionId,
                                     uint32_t aSessionIdSize,
                                     cdm::Time aNewExpiryTime)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::OnExpirationChange(sid=%s, time=%lf)",
          aSessionId,
          aNewExpiryTime);
  Unused << SendOnExpirationChange(nsCString(aSessionId, aSessionIdSize),
                                   aNewExpiryTime);
}

void
ChromiumCDMChild::OnSessionClosed(const char* aSessionId,
                                  uint32_t aSessionIdSize)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::OnSessionClosed(sid=%s)", aSessionId);
  Unused << SendOnSessionClosed(nsCString(aSessionId, aSessionIdSize));
}

void
ChromiumCDMChild::OnLegacySessionError(const char* aSessionId,
                                       uint32_t aSessionIdLength,
                                       cdm::Error aError,
                                       uint32_t aSystemCode,
                                       const char* aErrorMessage,
                                       uint32_t aErrorMessageLength)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::OnLegacySessionError(sid=%s, error=%" PRIu32
          " msg='%s')",
          aSessionId,
          aError,
          aErrorMessage);
  Unused << SendOnLegacySessionError(
    nsCString(aSessionId, aSessionIdLength),
    static_cast<uint32_t>(aError),
    aSystemCode,
    nsCString(aErrorMessage, aErrorMessageLength));
}

cdm::FileIO*
ChromiumCDMChild::CreateFileIO(cdm::FileIOClient * aClient)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::CreateFileIO()");
  if (!mPersistentStateAllowed) {
    return nullptr;
  }
  return new WidevineFileIO(aClient);
}

ChromiumCDMChild::~ChromiumCDMChild()
{
  GMP_LOG("ChromiumCDMChild:: dtor this=%p", this);
}

bool
ChromiumCDMChild::IsOnMessageLoopThread()
{
  return mPlugin && mPlugin->GMPMessageLoop() == MessageLoop::current();
}

void
ChromiumCDMChild::PurgeShmems()
{
  for (ipc::Shmem& shmem : mBuffers) {
    DeallocShmem(shmem);
  }
  mBuffers.Clear();
}

ipc::IPCResult
ChromiumCDMChild::RecvPurgeShmems()
{
  PurgeShmems();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvInit(const bool& aAllowDistinctiveIdentifier,
                           const bool& aAllowPersistentState)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvInit(distinctiveId=%d, persistentState=%d)",
          aAllowDistinctiveIdentifier,
          aAllowPersistentState);
  mPersistentStateAllowed = aAllowPersistentState;
  if (mCDM) {
    mCDM->Initialize(aAllowDistinctiveIdentifier, aAllowPersistentState);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvSetServerCertificate(const uint32_t& aPromiseId,
                                           nsTArray<uint8_t>&& aServerCert)

{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvSetServerCertificate() certlen=%zu",
          aServerCert.Length());
  if (mCDM) {
    mCDM->SetServerCertificate(aPromiseId,
                               aServerCert.Elements(),
                               aServerCert.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvCreateSessionAndGenerateRequest(
  const uint32_t& aPromiseId,
  const uint32_t& aSessionType,
  const uint32_t& aInitDataType,
  nsTArray<uint8_t>&& aInitData)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvCreateSessionAndGenerateRequest("
          "pid=%" PRIu32 ", sessionType=%" PRIu32 ", initDataType=%" PRIu32
          ") initDataLen=%zu",
          aPromiseId,
          aSessionType,
          aInitDataType,
          aInitData.Length());
  MOZ_ASSERT(aSessionType <= cdm::SessionType::kPersistentKeyRelease);
  MOZ_ASSERT(aInitDataType <= cdm::InitDataType::kWebM);
  if (mCDM) {
    mCDM->CreateSessionAndGenerateRequest(aPromiseId,
                                          static_cast<cdm::SessionType>(aSessionType),
                                          static_cast<cdm::InitDataType>(aInitDataType),
                                          aInitData.Elements(),
                                          aInitData.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvLoadSession(const uint32_t& aPromiseId,
                                  const uint32_t& aSessionType,
                                  const nsCString& aSessionId)
{
  GMP_LOG("ChromiumCDMChild::RecvLoadSession(pid=%u, type=%u, sessionId=%s)",
          aPromiseId,
          aSessionType,
          aSessionId.get());
  if (mCDM) {
    mLoadSessionPromiseIds.AppendElement(aPromiseId);
    mCDM->LoadSession(aPromiseId,
                      static_cast<cdm::SessionType>(aSessionType),
                      aSessionId.get(),
                      aSessionId.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvUpdateSession(const uint32_t& aPromiseId,
                                    const nsCString& aSessionId,
                                    nsTArray<uint8_t>&& aResponse)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvUpdateSession(pid=%" PRIu32
          ", sid=%s) responseLen=%zu",
          aPromiseId,
          aSessionId.get(),
          aResponse.Length());
  if (mCDM) {
    mCDM->UpdateSession(aPromiseId,
                        aSessionId.get(),
                        aSessionId.Length(),
                        aResponse.Elements(),
                        aResponse.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvCloseSession(const uint32_t& aPromiseId,
                                   const nsCString& aSessionId)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvCloseSession(pid=%" PRIu32 ", sid=%s)",
          aPromiseId,
          aSessionId.get());
  if (mCDM) {
    mCDM->CloseSession(aPromiseId, aSessionId.get(), aSessionId.Length());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvRemoveSession(const uint32_t& aPromiseId,
                                    const nsCString& aSessionId)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvRemoveSession(pid=%" PRIu32 ", sid=%s)",
          aPromiseId,
          aSessionId.get());
  if (mCDM) {
    mCDM->RemoveSession(aPromiseId, aSessionId.get(), aSessionId.Length());
  }
  return IPC_OK();
}

static void
InitInputBuffer(const CDMInputBuffer& aBuffer,
                nsTArray<cdm::SubsampleEntry>& aSubSamples,
                cdm::InputBuffer& aInputBuffer)
{
  aInputBuffer.data = aBuffer.mData().get<uint8_t>();
  aInputBuffer.data_size = aBuffer.mData().Size<uint8_t>();

  if (aBuffer.mIsEncrypted()) {
    aInputBuffer.key_id = aBuffer.mKeyId().Elements();
    aInputBuffer.key_id_size = aBuffer.mKeyId().Length();

    aInputBuffer.iv = aBuffer.mIV().Elements();
    aInputBuffer.iv_size = aBuffer.mIV().Length();

    aSubSamples.SetCapacity(aBuffer.mClearBytes().Length());
    for (size_t i = 0; i < aBuffer.mCipherBytes().Length(); i++) {
      aSubSamples.AppendElement(cdm::SubsampleEntry(aBuffer.mClearBytes()[i],
                                                    aBuffer.mCipherBytes()[i]));
    }
    aInputBuffer.subsamples = aSubSamples.Elements();
    aInputBuffer.num_subsamples = aSubSamples.Length();
  }
  aInputBuffer.timestamp = aBuffer.mTimestamp();
}

bool
ChromiumCDMChild::HasShmemOfSize(size_t aSize) const
{
  for (const ipc::Shmem& shmem : mBuffers) {
    if (shmem.Size<uint8_t>() == aSize) {
      return true;
    }
  }
  return false;
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDecrypt(const uint32_t& aId,
                              const CDMInputBuffer& aBuffer)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvDecrypt()");

  // Parent should have already gifted us a shmem to use as output.
  size_t outputShmemSize = aBuffer.mData().Size<uint8_t>();
  MOZ_ASSERT(HasShmemOfSize(outputShmemSize));

  // Ensure we deallocate the shmem used to send input.
  RefPtr<ChromiumCDMChild> self = this;
  auto autoDeallocateInputShmem =
    MakeScopeExit([&, self] { self->DeallocShmem(aBuffer.mData()); });

  // On failure, we need to ensure that the shmem that the parent sent
  // for the CDM to use to return output back to the parent is deallocated.
  // Otherwise, it will leak.
  auto autoDeallocateOutputShmem = MakeScopeExit([self, outputShmemSize] {
    self->mBuffers.RemoveElementsBy([outputShmemSize, self](ipc::Shmem& aShmem) {
      if (aShmem.Size<uint8_t>() != outputShmemSize) {
        return false;
      }
      self->DeallocShmem(aShmem);
      return true;
    });
  });

  if (!mCDM) {
    GMP_LOG("ChromiumCDMChild::RecvDecrypt() no CDM");
    Unused << SendDecryptFailed(aId, cdm::kDecryptError);
    return IPC_OK();
  }
  if (aBuffer.mClearBytes().Length() != aBuffer.mCipherBytes().Length()) {
    GMP_LOG("ChromiumCDMChild::RecvDecrypt() clear/cipher bytes length doesn't "
            "match");
    Unused << SendDecryptFailed(aId, cdm::kDecryptError);
    return IPC_OK();
  }

  cdm::InputBuffer input;
  nsTArray<cdm::SubsampleEntry> subsamples;
  InitInputBuffer(aBuffer, subsamples, input);

  WidevineDecryptedBlock output;
  cdm::Status status = mCDM->Decrypt(input, &output);

  // CDM should have allocated a cdm::Buffer for output.
  CDMShmemBuffer* buffer =
    output.DecryptedBuffer()
      ? static_cast<CDMShmemBuffer*>(output.DecryptedBuffer())
      : nullptr;
  MOZ_ASSERT_IF(buffer, buffer->AsShmemBuffer());
  if (status != cdm::kSuccess || !buffer) {
    Unused << SendDecryptFailed(aId, status);
    return IPC_OK();
  }

  // Success! Return the decrypted sample to parent.
  MOZ_ASSERT(!HasShmemOfSize(outputShmemSize));
  ipc::Shmem shmem = buffer->ExtractShmem();
  if (SendDecrypted(aId, cdm::kSuccess, shmem)) {
    // No need to deallocate the output shmem; it should have been returned
    // to the content process.
    autoDeallocateOutputShmem.release();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvInitializeVideoDecoder(
  const CDMVideoDecoderConfig& aConfig)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  MOZ_ASSERT(!mDecoderInitialized);
  cdm::VideoDecoderConfig config;
  config.codec =
    static_cast<cdm::VideoDecoderConfig::VideoCodec>(aConfig.mCodec());
  config.profile =
    static_cast<cdm::VideoDecoderConfig::VideoCodecProfile>(aConfig.mProfile());
  config.format = static_cast<cdm::VideoFormat>(aConfig.mFormat());
  config.coded_size =
    mCodedSize = { aConfig.mImageWidth(), aConfig.mImageHeight() };
  nsTArray<uint8_t> extraData(aConfig.mExtraData());
  config.extra_data = extraData.Elements();
  config.extra_data_size = extraData.Length();
  cdm::Status status = mCDM->InitializeVideoDecoder(config);
  GMP_LOG("ChromiumCDMChild::RecvInitializeVideoDecoder() status=%u", status);
  Unused << SendOnDecoderInitDone(status);
  mDecoderInitialized = status == cdm::kSuccess;
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDeinitializeVideoDecoder()
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvDeinitializeVideoDecoder()");
  MOZ_ASSERT(mDecoderInitialized);
  if (mDecoderInitialized) {
    mDecoderInitialized = false;
    mCDM->DeinitializeDecoder(cdm::kStreamTypeVideo);
  }
  PurgeShmems();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvResetVideoDecoder()
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvResetVideoDecoder()");
  if (mDecoderInitialized) {
    mCDM->ResetDecoder(cdm::kStreamTypeVideo);
  }
  Unused << SendResetVideoDecoderComplete();
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDecryptAndDecodeFrame(const CDMInputBuffer& aBuffer)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvDecryptAndDecodeFrame() t=%" PRId64 ")",
          aBuffer.mTimestamp());
  MOZ_ASSERT(mDecoderInitialized);

  RefPtr<ChromiumCDMChild> self = this;
  auto autoDeallocateShmem = MakeScopeExit([&, self] {
    self->DeallocShmem(aBuffer.mData());
  });

  // The output frame may not have the same timestamp as the frame we put in.
  // We may need to input a number of frames before we receive output. The
  // CDM's decoder reorders to ensure frames output are in presentation order.
  // So we need to store the durations of the frames input, and retrieve them
  // on output.
  mFrameDurations.Insert(aBuffer.mTimestamp(), aBuffer.mDuration());

  cdm::InputBuffer input;
  nsTArray<cdm::SubsampleEntry> subsamples;
  InitInputBuffer(aBuffer, subsamples, input);

  WidevineVideoFrame frame;
  cdm::Status rv = mCDM->DecryptAndDecodeFrame(input, &frame);
  GMP_LOG("ChromiumCDMChild::RecvDecryptAndDecodeFrame() t=%" PRId64
          " CDM decoder rv=%d",
          aBuffer.mTimestamp(),
          rv);

  switch (rv) {
    case cdm::kNeedMoreData:
      Unused << SendDecodeFailed(rv);
      break;
    case cdm::kNoKey:
      GMP_LOG("NoKey for sample at time=%" PRId64 "!", input.timestamp);
      // Somehow our key became unusable. Typically this would happen when
      // a stream requires output protection, and the configuration changed
      // such that output protection is no longer available. For example, a
      // non-compliant monitor was attached. The JS player should notice the
      // key status changing to "output-restricted", and is supposed to switch
      // to a stream that doesn't require OP. In order to keep the playback
      // pipeline rolling, just output a black frame. See bug 1343140.
      if (!frame.InitToBlack(mCodedSize.width, mCodedSize.height,
                             input.timestamp)) {
        Unused << SendDecodeFailed(cdm::kDecodeError);
        break;
      }
      MOZ_FALLTHROUGH;
    case cdm::kSuccess:
      if (frame.FrameBuffer()) {
        ReturnOutput(frame);
        break;
      }
      // CDM didn't set a frame buffer on the sample, report it as an error.
      MOZ_FALLTHROUGH;
    default:
      Unused << SendDecodeFailed(rv);
      break;
  }

  return IPC_OK();
}

void
ChromiumCDMChild::ReturnOutput(WidevineVideoFrame& aFrame)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  MOZ_ASSERT(aFrame.FrameBuffer());
  gmp::CDMVideoFrame output;
  output.mFormat() = static_cast<cdm::VideoFormat>(aFrame.Format());
  output.mImageWidth() = aFrame.Size().width;
  output.mImageHeight() = aFrame.Size().height;
  output.mYPlane() = { aFrame.PlaneOffset(cdm::VideoFrame::kYPlane),
                       aFrame.Stride(cdm::VideoFrame::kYPlane) };
  output.mUPlane() = { aFrame.PlaneOffset(cdm::VideoFrame::kUPlane),
                       aFrame.Stride(cdm::VideoFrame::kUPlane) };
  output.mVPlane() = { aFrame.PlaneOffset(cdm::VideoFrame::kVPlane),
                       aFrame.Stride(cdm::VideoFrame::kVPlane) };
  output.mTimestamp() = aFrame.Timestamp();

  uint64_t duration = 0;
  if (mFrameDurations.Find(aFrame.Timestamp(), duration)) {
    output.mDuration() = duration;
  }

  CDMBuffer* base = reinterpret_cast<CDMBuffer*>(aFrame.FrameBuffer());
  if (base->AsShmemBuffer()) {
    ipc::Shmem shmem = base->AsShmemBuffer()->ExtractShmem();
    Unused << SendDecodedShmem(output, shmem);
  } else {
    MOZ_ASSERT(base->AsArrayBuffer());
    Unused << SendDecodedData(output, base->AsArrayBuffer()->ExtractBuffer());
  }
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDrain()
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  WidevineVideoFrame frame;
  cdm::InputBuffer sample;
  cdm::Status rv = mCDM->DecryptAndDecodeFrame(sample, &frame);
  CDM_LOG("ChromiumCDMChild::RecvDrain();  DecryptAndDecodeFrame() rv=%d", rv);
  if (rv == cdm::kSuccess) {
    MOZ_ASSERT(frame.Format() != cdm::kUnknownVideoFormat);
    ReturnOutput(frame);
  } else {
    Unused << SendDrainComplete();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDestroy()
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvDestroy()");

  MOZ_ASSERT(!mDecoderInitialized);

  if (mCDM) {
    mCDM->Destroy();
    mCDM = nullptr;
  }

  Unused << Send__delete__(this);

  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvGiveBuffer(ipc::Shmem&& aBuffer)
{
  MOZ_ASSERT(IsOnMessageLoopThread());

  GiveBuffer(Move(aBuffer));
  return IPC_OK();
}

void
ChromiumCDMChild::GiveBuffer(ipc::Shmem&& aBuffer)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  size_t sz = aBuffer.Size<uint8_t>();
  mBuffers.AppendElement(Move(aBuffer));
  GMP_LOG("ChromiumCDMChild::RecvGiveBuffer(capacity=%" PRIuSIZE
          ") bufferSizes={%s} mDecoderInitialized=%d",
          sz,
          ToString(mBuffers).get(),
          mDecoderInitialized);
}

} // namespace gmp
} // namespace mozilla
