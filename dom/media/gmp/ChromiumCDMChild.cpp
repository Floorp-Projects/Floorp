/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMChild.h"
#include "GMPContentChild.h"
#include "WidevineUtils.h"
#include "WidevineVideoFrame.h"
#include "GMPLog.h"
#include "GMPPlatform.h"
#include "mozilla/Unused.h"
#include "nsPrintfCString.h"
#include "base/time.h"

namespace mozilla {
namespace gmp {

ChromiumCDMChild::ChromiumCDMChild(GMPContentChild* aPlugin)
  : mPlugin(aPlugin)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
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

cdm::Buffer*
ChromiumCDMChild::Allocate(uint32_t aCapacity)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::Allocate(capacity=%" PRIu32 ")", aCapacity);
  return new WidevineBuffer(aCapacity);
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
    nsCString keyId;
    const cdm::KeyInformation& key = aKeysInfo[i];
    for (size_t k = 0; k < key.key_id_size; k++) {
      keyId.Append(nsPrintfCString("%hhX", key.key_id[k]));
    }
    if (!str.IsEmpty()) {
      str.AppendLiteral(",");
    }
    str.Append(keyId);
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
  return nullptr;
}

bool
ChromiumCDMChild::IsOnMessageLoopThread()
{
  return mPlugin && mPlugin->GMPMessageLoop() == MessageLoop::current();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvInit(const bool& aAllowDistinctiveIdentifier,
                           const bool& aAllowPersistentState)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvInit(distinctiveId=%d, persistentState=%d)",
          aAllowDistinctiveIdentifier,
          aAllowPersistentState);
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

void
ChromiumCDMChild::DecryptFailed(uint32_t aId, cdm::Status aStatus)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  Unused << SendDecrypted(aId, aStatus, nsTArray<uint8_t>());
}

static void
InitInputBuffer(const CDMInputBuffer& aBuffer,
                nsTArray<cdm::SubsampleEntry>& aSubSamples,
                cdm::InputBuffer& aInputBuffer)
{
  aInputBuffer.data = aBuffer.mData().Elements();
  aInputBuffer.data_size = aBuffer.mData().Length();

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

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDecrypt(const uint32_t& aId,
                              const CDMInputBuffer& aBuffer)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvDecrypt()");
  if (!mCDM) {
    GMP_LOG("ChromiumCDMChild::RecvDecrypt() no CDM");
    DecryptFailed(aId, cdm::kDecryptError);
    return IPC_OK();
  }
  if (aBuffer.mClearBytes().Length() != aBuffer.mCipherBytes().Length()) {
    GMP_LOG("ChromiumCDMChild::RecvDecrypt() clear/cipher bytes length doesn't "
            "match");
    DecryptFailed(aId, cdm::kDecryptError);
    return IPC_OK();
  }

  cdm::InputBuffer input;
  nsTArray<cdm::SubsampleEntry> subsamples;
  InitInputBuffer(aBuffer, subsamples, input);

  WidevineDecryptedBlock output;
  cdm::Status status = mCDM->Decrypt(input, &output);

  if (status != cdm::kSuccess) {
    DecryptFailed(aId, status);
    return IPC_OK();
  }

  if (!output.DecryptedBuffer() ||
      output.DecryptedBuffer()->Size() != aBuffer.mData().Length()) {
    // The sizes of the input and output should exactly match.
    DecryptFailed(aId, cdm::kDecryptError);
    return IPC_OK();
  }

  nsTArray<uint8_t> buf =
    static_cast<WidevineBuffer*>(output.DecryptedBuffer())->ExtractBuffer();
  Unused << SendDecrypted(aId, cdm::kSuccess, buf);

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
  config.coded_size = { aConfig.mImageWidth(), aConfig.mImageHeight() };
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
  return IPC_OK();
}

mozilla::ipc::IPCResult
ChromiumCDMChild::RecvDecryptAndDecodeFrame(const CDMInputBuffer& aBuffer)
{
  MOZ_ASSERT(IsOnMessageLoopThread());
  GMP_LOG("ChromiumCDMChild::RecvDecryptAndDecodeFrame()");
  MOZ_ASSERT(mDecoderInitialized);

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
  GMP_LOG("WidevineVideoDecoder::Decode(timestamp=%" PRId64 ") rv=%d",
          input.timestamp,
          rv);

  if (rv == cdm::kSuccess) {
    // TODO: WidevineBuffers should hold a shmem instead of a array, and we can
    // send the handle instead of copying the array here.

    gmp::CDMVideoFrame output;
    output.mFormat() = static_cast<cdm::VideoFormat>(frame.Format());
    output.mImageWidth() = frame.Size().width;
    output.mImageHeight() = frame.Size().height;
    output.mData() = Move(
      reinterpret_cast<WidevineBuffer*>(frame.FrameBuffer())->ExtractBuffer());
    output.mYPlane() = { frame.PlaneOffset(cdm::VideoFrame::kYPlane),
                         frame.Stride(cdm::VideoFrame::kYPlane) };
    output.mUPlane() = { frame.PlaneOffset(cdm::VideoFrame::kUPlane),
                         frame.Stride(cdm::VideoFrame::kUPlane) };
    output.mVPlane() = { frame.PlaneOffset(cdm::VideoFrame::kVPlane),
                         frame.Stride(cdm::VideoFrame::kVPlane) };
    output.mTimestamp() = frame.Timestamp();

    uint64_t duration = 0;
    if (mFrameDurations.Find(frame.Timestamp(), duration)) {
      output.mDuration() = duration;
    }

    Unused << SendDecoded(output);
  } else if (rv == cdm::kNeedMoreData) {
    Unused << SendDecoded(gmp::CDMVideoFrame());
  } else {
    Unused << SendDecodeFailed(rv);
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

} // namespace gmp
} // namespace mozilla
