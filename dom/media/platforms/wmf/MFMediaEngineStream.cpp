/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineStream.h"
#include <vcruntime.h>

#include "AudioConverter.h"
#include "MFMediaSource.h"
#include "MFMediaEngineUtils.h"
#include "TimeUnits.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkerTypes.h"
#include "WMF.h"
#include "WMFUtils.h"

namespace mozilla {

// Don't use this log on the task queue, because it would be racy for `mStream`.
#define WLOGV(msg, ...)                                                   \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Verbose,                           \
          ("MFMediaEngineStreamWrapper for stream %p (%s, id=%lu), " msg, \
           mStream.Get(), mStream->GetDescriptionName().get(),            \
           mStream->DescriptorId(), ##__VA_ARGS__))

#define SLOG(msg, ...)                              \
  MOZ_LOG(                                          \
      gMFMediaEngineLog, LogLevel::Debug,           \
      ("MFMediaStream=%p (%s, id=%lu), " msg, this, \
       this->GetDescriptionName().get(), this->DescriptorId(), ##__VA_ARGS__))

#define SLOGV(msg, ...)                             \
  MOZ_LOG(                                          \
      gMFMediaEngineLog, LogLevel::Verbose,         \
      ("MFMediaStream=%p (%s, id=%lu), " msg, this, \
       this->GetDescriptionName().get(), this->DescriptorId(), ##__VA_ARGS__))

using Microsoft::WRL::ComPtr;

RefPtr<MediaDataDecoder::InitPromise> MFMediaEngineStreamWrapper::Init() {
  MOZ_ASSERT(mStream->DescriptorId(), "Stream hasn't been initialized!");
  WLOGV("Init");
  return InitPromise::CreateAndResolve(mStream->TrackType(), __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> MFMediaEngineStreamWrapper::Decode(
    MediaRawData* aSample) {
  WLOGV("Decode");
  if (!mStream || mStream->IsShutdown()) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_FAILURE, "MFMediaEngineStreamWrapper is shutdown"),
        __func__);
  }
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mTaskQueue, mStream.Get(), __func__,
                     &MFMediaEngineStream::OutputData, std::move(sample));
}

RefPtr<MediaDataDecoder::DecodePromise> MFMediaEngineStreamWrapper::Drain() {
  WLOGV("Drain");
  if (!mStream || mStream->IsShutdown()) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_FAILURE, "MFMediaEngineStreamWrapper is shutdown"),
        __func__);
  }
  return InvokeAsync(mTaskQueue, mStream.Get(), __func__,
                     &MFMediaEngineStream::Drain);
}

RefPtr<MediaDataDecoder::FlushPromise> MFMediaEngineStreamWrapper::Flush() {
  WLOGV("Flush");
  if (!mStream || mStream->IsShutdown()) {
    return FlushPromise::CreateAndReject(
        MediaResult(NS_ERROR_FAILURE, "MFMediaEngineStreamWrapper is shutdown"),
        __func__);
  }
  return InvokeAsync(mTaskQueue, mStream.Get(), __func__,
                     &MFMediaEngineStream::Flush);
}

RefPtr<ShutdownPromise> MFMediaEngineStreamWrapper::Shutdown() {
  // Stream shutdown is controlled by the media source, so we don't need to call
  // its shutdown.
  WLOGV("Disconnect wrapper");
  if (!mStream) {
    // This promise must only ever be resolved. See the definition of the
    // original abstract function.
    return ShutdownPromise::CreateAndResolve(false, __func__);
  }
  mStream = nullptr;
  mTaskQueue = nullptr;
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

nsCString MFMediaEngineStreamWrapper::GetDescriptionName() const {
  return mStream ? mStream->GetDescriptionName() : nsLiteralCString("none");
}

nsCString MFMediaEngineStreamWrapper::GetCodecName() const {
  return mStream ? mStream->GetCodecName() : nsLiteralCString("none");
}

MediaDataDecoder::ConversionRequired
MFMediaEngineStreamWrapper::NeedsConversion() const {
  return mStream ? mStream->NeedsConversion()
                 : MediaDataDecoder::ConversionRequired::kNeedNone;
}

MFMediaEngineStream::MFMediaEngineStream()
    : mIsShutdown(false), mIsSelected(false), mReceivedEOS(false) {
  MOZ_COUNT_CTOR(MFMediaEngineStream);
}

MFMediaEngineStream::~MFMediaEngineStream() {
  MOZ_ASSERT(IsShutdown());
  MOZ_COUNT_DTOR(MFMediaEngineStream);
}

HRESULT MFMediaEngineStream::RuntimeClassInitialize(
    uint64_t aStreamId, const TrackInfo& aInfo, MFMediaSource* aParentSource) {
  mParentSource = aParentSource;
  mTaskQueue = aParentSource->GetTaskQueue();
  MOZ_ASSERT(mTaskQueue);
  mStreamId = aStreamId;
  RETURN_IF_FAILED(wmf::MFCreateEventQueue(&mMediaEventQueue));

  ComPtr<IMFMediaType> mediaType;
  // The inherited stream would return different type based on their media info.
  RETURN_IF_FAILED(CreateMediaType(aInfo, mediaType.GetAddressOf()));
  RETURN_IF_FAILED(GenerateStreamDescriptor(mediaType));
  SLOG("Initialized %s (id=%" PRIu64 ", descriptorId=%lu)",
       GetDescriptionName().get(), aStreamId, mStreamDescriptorId);
  return S_OK;
}

HRESULT MFMediaEngineStream::GenerateStreamDescriptor(
    ComPtr<IMFMediaType>& aMediaType) {
  RETURN_IF_FAILED(wmf::MFCreateStreamDescriptor(
      mStreamId, 1 /* stream amount */, aMediaType.GetAddressOf(),
      &mStreamDescriptor));
  RETURN_IF_FAILED(
      mStreamDescriptor->GetStreamIdentifier(&mStreamDescriptorId));
  if (IsEncrypted()) {
    RETURN_IF_FAILED(mStreamDescriptor->SetUINT32(MF_SD_PROTECTED, 1));
  }
  return S_OK;
}

HRESULT MFMediaEngineStream::Start(const PROPVARIANT* aPosition) {
  AssertOnMFThreadPool();
  if (!IsSelected()) {
    SLOG("No need to start non-selected stream");
    return S_OK;
  }
  if (IsShutdown()) {
    return MF_E_SHUTDOWN;
  }
  SLOG("Start");
  const bool isFromCurrentPosition = aPosition->vt == VT_EMPTY;
  RETURN_IF_FAILED(QueueEvent(MEStreamStarted, GUID_NULL, S_OK, aPosition));
  MOZ_ASSERT(mTaskQueue);
  Unused << mTaskQueue->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineStream::Start",
      [self = RefPtr{this}, isFromCurrentPosition, this]() {
        if (!isFromCurrentPosition && IsEnded()) {
          SLOG("Stream restarts again from a new position, reset EOS");
          mReceivedEOS = false;
        }
        // Process pending requests (if any) which happened when the stream
        // wasn't allowed to serve samples. Eg. stream is paused. Or resend the
        // ended event if the stream is ended already.
        ReplySampleRequestIfPossible();
      }));
  return S_OK;
}

HRESULT MFMediaEngineStream::Seek(const PROPVARIANT* aPosition) {
  AssertOnMFThreadPool();
  if (!IsSelected()) {
    SLOG("No need to seek non-selected stream");
    return S_OK;
  }
  SLOG("Seek");
  RETURN_IF_FAILED(QueueEvent(MEStreamSeeked, GUID_NULL, S_OK, aPosition));
  return S_OK;
}

HRESULT MFMediaEngineStream::Stop() {
  AssertOnMFThreadPool();
  if (!IsSelected()) {
    SLOG("No need to stop non-selected stream");
    return S_OK;
  }
  SLOG("Stop");
  RETURN_IF_FAILED(QueueEvent(MEStreamStopped, GUID_NULL, S_OK, nullptr));
  return S_OK;
}

HRESULT MFMediaEngineStream::Pause() {
  AssertOnMFThreadPool();
  if (!IsSelected()) {
    SLOG("No need to pause non-selected stream");
    return S_OK;
  }
  SLOG("Pause");
  RETURN_IF_FAILED(QueueEvent(MEStreamPaused, GUID_NULL, S_OK, nullptr));
  return S_OK;
}

void MFMediaEngineStream::Shutdown() {
  AssertOnMFThreadPool();
  if (IsShutdown()) {
    return;
  }
  SLOG("Shutdown");
  mIsShutdown = true;
  // After this method is called, all IMFMediaEventQueue methods return
  // MF_E_SHUTDOWN.
  RETURN_VOID_IF_FAILED(mMediaEventQueue->Shutdown());
  ComPtr<MFMediaEngineStream> self = this;
  MOZ_ASSERT(mTaskQueue);
  Unused << mTaskQueue->Dispatch(
      NS_NewRunnableFunction("MFMediaEngineStream::Shutdown", [self]() {
        self->mParentSource = nullptr;
        self->mRawDataQueueForFeedingEngine.Reset();
        self->mRawDataQueueForGeneratingOutput.Reset();
        self->ShutdownCleanUpOnTaskQueue();
        self->mTaskQueue = nullptr;
      }));
}

IFACEMETHODIMP
MFMediaEngineStream::GetMediaSource(IMFMediaSource** aMediaSource) {
  AssertOnMFThreadPool();
  if (IsShutdown()) {
    return MF_E_SHUTDOWN;
  }
  RETURN_IF_FAILED(mParentSource.CopyTo(aMediaSource));
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::GetStreamDescriptor(
    IMFStreamDescriptor** aStreamDescriptor) {
  AssertOnMFThreadPool();
  if (IsShutdown()) {
    return MF_E_SHUTDOWN;
  }
  if (!mStreamDescriptor) {
    SLOG("Hasn't initialized stream descriptor");
    return MF_E_NOT_INITIALIZED;
  }
  RETURN_IF_FAILED(mStreamDescriptor.CopyTo(aStreamDescriptor));
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::RequestSample(IUnknown* aToken) {
  AssertOnMFThreadPool();
  if (IsShutdown()) {
    return MF_E_SHUTDOWN;
  }

  ComPtr<IUnknown> token = aToken;
  ComPtr<MFMediaEngineStream> self = this;
  MOZ_ASSERT(mTaskQueue);
  Unused << mTaskQueue->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineStream::RequestSample", [token, self, this]() {
        AssertOnTaskQueue();
        mSampleRequestTokens.push(token);
        SLOGV("RequestSample, token amount=%zu", mSampleRequestTokens.size());
        ReplySampleRequestIfPossible();
        if (!HasEnoughRawData() && mParentSource && !IsEnded()) {
          SendRequestSampleEvent(false /* isEnough */);
        }
      }));
  return S_OK;
}

void MFMediaEngineStream::ReplySampleRequestIfPossible() {
  AssertOnTaskQueue();
  if (IsEnded()) {
    // We have no more sample to return, clean all pending requests.
    while (!mSampleRequestTokens.empty()) {
      mSampleRequestTokens.pop();
    }

    SLOG("Notify end events");
    MOZ_ASSERT(mRawDataQueueForFeedingEngine.GetSize() == 0);
    MOZ_ASSERT(mSampleRequestTokens.empty());
    RETURN_VOID_IF_FAILED(mMediaEventQueue->QueueEventParamUnk(
        MEEndOfStream, GUID_NULL, S_OK, nullptr));
    mEndedEvent.Notify(TrackType());
    PROFILER_MARKER_TEXT(
        "MFMediaEngineStream:NotifyEnd", MEDIA_PLAYBACK, {},
        nsPrintfCString("stream=%s, id=%" PRIu64, GetDescriptionName().get(),
                        mStreamId));
    return;
  }

  if (mSampleRequestTokens.empty() ||
      mRawDataQueueForFeedingEngine.GetSize() == 0) {
    return;
  }

  if (!ShouldServeSamples()) {
    SLOGV("Not deliver samples if the stream is not started");
    return;
  }

  // Push data into the mf media event queue if the media engine is already
  // waiting for data.
  ComPtr<IMFSample> inputSample;
  RETURN_VOID_IF_FAILED(CreateInputSample(inputSample.GetAddressOf()));
  ComPtr<IUnknown> token = mSampleRequestTokens.front();
  RETURN_VOID_IF_FAILED(
      inputSample->SetUnknown(MFSampleExtension_Token, token.Get()));
  mSampleRequestTokens.pop();
  RETURN_VOID_IF_FAILED(mMediaEventQueue->QueueEventParamUnk(
      MEMediaSample, GUID_NULL, S_OK, inputSample.Get()));
}

bool MFMediaEngineStream::ShouldServeSamples() const {
  AssertOnTaskQueue();
  return mParentSource &&
         mParentSource->GetState() == MFMediaSource::State::Started &&
         mIsSelected;
}

HRESULT MFMediaEngineStream::CreateInputSample(IMFSample** aSample) {
  AssertOnTaskQueue();

  ComPtr<IMFSample> sample;
  RETURN_IF_FAILED(wmf::MFCreateSample(&sample));

  MOZ_ASSERT(mRawDataQueueForFeedingEngine.GetSize() != 0);
  RefPtr<MediaRawData> data = mRawDataQueueForFeedingEngine.PopFront();
  SLOGV("CreateInputSample, pop data [%" PRId64 ", %" PRId64
        "] (duration=%" PRId64 ", kf=%d), queue size=%zu",
        data->mTime.ToMicroseconds(), data->GetEndTime().ToMicroseconds(),
        data->mDuration.ToMicroseconds(), data->mKeyframe,
        mRawDataQueueForFeedingEngine.GetSize());
  PROFILER_MARKER(
      nsPrintfCString(
          "pop %s (stream=%" PRIu64 ")",
          TrackType() == TrackInfo::TrackType::kVideoTrack ? "video" : "audio",
          mStreamId),
      MEDIA_PLAYBACK, {}, MediaSampleMarker, data->mTime.ToMicroseconds(),
      data->GetEndTime().ToMicroseconds(),
      mRawDataQueueForFeedingEngine.GetSize());

  // Copy data into IMFMediaBuffer
  ComPtr<IMFMediaBuffer> buffer;
  BYTE* dst = nullptr;
  DWORD maxLength = 0;
  RETURN_IF_FAILED(
      wmf::MFCreateMemoryBuffer(data->Size(), buffer.GetAddressOf()));
  RETURN_IF_FAILED(buffer->Lock(&dst, &maxLength, 0));
  memcpy(dst, data->Data(), data->Size());
  RETURN_IF_FAILED(buffer->Unlock());
  RETURN_IF_FAILED(buffer->SetCurrentLength(data->Size()));

  // Setup sample attributes
  RETURN_IF_FAILED(sample->AddBuffer(buffer.Get()));
  RETURN_IF_FAILED(
      sample->SetSampleTime(UsecsToHNs(data->mTime.ToMicroseconds())));
  RETURN_IF_FAILED(
      sample->SetSampleDuration(UsecsToHNs(data->mDuration.ToMicroseconds())));
  if (data->mKeyframe) {
    RETURN_IF_FAILED(sample->SetUINT32(MFSampleExtension_CleanPoint, 1));
  }

  // Setup encrypt attributes
  if (data->mCrypto.IsEncrypted()) {
    RETURN_IF_FAILED(AddEncryptAttributes(sample.Get(), data->mCrypto));
  }

  *aSample = sample.Detach();
  return S_OK;
}

HRESULT MFMediaEngineStream::AddEncryptAttributes(
    IMFSample* aSample, const CryptoSample& aCryptoConfig) {
  // Scheme
  MFSampleEncryptionProtectionScheme protectionScheme;
  if (aCryptoConfig.mCryptoScheme == CryptoScheme::Cenc) {
    protectionScheme = MFSampleEncryptionProtectionScheme::
        MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CTR;
  } else if (aCryptoConfig.mCryptoScheme == CryptoScheme::Cbcs) {
    protectionScheme = MFSampleEncryptionProtectionScheme::
        MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CBC;
  } else {
    SLOG("Unexpected encryption scheme");
    return MF_E_UNEXPECTED;
  }
  RETURN_IF_FAILED(aSample->SetUINT32(
      MFSampleExtension_Encryption_ProtectionScheme, protectionScheme));

  // KID
  if (aCryptoConfig.mKeyId.Length() != sizeof(GUID)) {
    SLOG("Unsupported key ID size (%zu)", aCryptoConfig.mKeyId.Length());
    return MF_E_UNEXPECTED;
  }
  GUID keyId;
  GUIDFromByteArray(aCryptoConfig.mKeyId, keyId);
  RETURN_IF_FAILED(aSample->SetGUID(MFSampleExtension_Content_KeyID, keyId));
  // TODO : if we want to suspend/resume the media engine, then we can consider
  // to store last key id and set it in CDM to refresh the decryptor.

  // IV
  RETURN_IF_FAILED(aSample->SetBlob(
      MFSampleExtension_Encryption_SampleID,
      reinterpret_cast<const uint8_t*>(aCryptoConfig.mIV.Elements()),
      aCryptoConfig.mIVSize));

  // Subsample entries.
  MOZ_ASSERT(aCryptoConfig.mEncryptedSizes.Length() ==
             aCryptoConfig.mPlainSizes.Length());
  size_t numSubsamples = aCryptoConfig.mEncryptedSizes.Length();
  if (numSubsamples != 0) {
    std::vector<MediaFoundationSubsampleEntry> subsampleEntries;
    for (size_t idx = 0; idx < numSubsamples; idx++) {
      subsampleEntries.push_back(MediaFoundationSubsampleEntry{
          aCryptoConfig.mPlainSizes[idx], aCryptoConfig.mEncryptedSizes[idx]});
    }
    const uint32_t entriesSize =
        sizeof(MediaFoundationSubsampleEntry) * numSubsamples;
    RETURN_IF_FAILED(aSample->SetBlob(
        MFSampleExtension_Encryption_SubSample_Mapping,
        reinterpret_cast<const uint8_t*>(subsampleEntries.data()),
        entriesSize));
  }

  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::GetEvent(DWORD aFlags,
                                             IMFMediaEvent** aEvent) {
  AssertOnMFThreadPool();
  MOZ_ASSERT(mMediaEventQueue);
  RETURN_IF_FAILED(mMediaEventQueue->GetEvent(aFlags, aEvent));
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::BeginGetEvent(IMFAsyncCallback* aCallback,
                                                  IUnknown* aState) {
  AssertOnMFThreadPool();
  MOZ_ASSERT(mMediaEventQueue);
  RETURN_IF_FAILED(mMediaEventQueue->BeginGetEvent(aCallback, aState));
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::EndGetEvent(IMFAsyncResult* aResult,
                                                IMFMediaEvent** aEvent) {
  AssertOnMFThreadPool();
  MOZ_ASSERT(mMediaEventQueue);
  RETURN_IF_FAILED(mMediaEventQueue->EndGetEvent(aResult, aEvent));
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::QueueEvent(MediaEventType aType,
                                               REFGUID aExtendedType,
                                               HRESULT aStatus,
                                               const PROPVARIANT* aValue) {
  AssertOnMFThreadPool();
  MOZ_ASSERT(mMediaEventQueue);
  RETURN_IF_FAILED(mMediaEventQueue->QueueEventParamVar(aType, aExtendedType,
                                                        aStatus, aValue));
  SLOG("Queued event %s", MediaEventTypeToStr(aType));
  return S_OK;
}

void MFMediaEngineStream::SetSelected(bool aSelected) {
  AssertOnMFThreadPool();
  SLOG("Select=%d", aSelected);
  mIsSelected = aSelected;
}

void MFMediaEngineStream::NotifyNewData(MediaRawData* aSample) {
  AssertOnTaskQueue();
  if (IsShutdown()) {
    return;
  }
  const bool wasEnough = HasEnoughRawData();
  mRawDataQueueForFeedingEngine.Push(aSample);
  mRawDataQueueForGeneratingOutput.Push(aSample);
  SLOGV("NotifyNewData, push data [%" PRId64 ", %" PRId64
        "], queue size=%zu, queue duration=%" PRId64,
        aSample->mTime.ToMicroseconds(), aSample->GetEndTime().ToMicroseconds(),
        mRawDataQueueForFeedingEngine.GetSize(),
        mRawDataQueueForFeedingEngine.Duration());
  if (mReceivedEOS) {
    SLOG("Receive a new data, cancel old EOS flag");
    mReceivedEOS = false;
  }
  ReplySampleRequestIfPossible();
  if (!wasEnough && HasEnoughRawData()) {
    SendRequestSampleEvent(true /* isEnough */);
  }
}

void MFMediaEngineStream::SendRequestSampleEvent(bool aIsEnough) {
  AssertOnTaskQueue();
  SLOGV("data is %s, queue duration=%" PRId64,
        aIsEnough ? "enough" : "not enough",
        mRawDataQueueForFeedingEngine.Duration());
  mParentSource->mRequestSampleEvent.Notify(
      SampleRequest{TrackType(), aIsEnough});
}

void MFMediaEngineStream::NotifyEndOfStreamInternal() {
  AssertOnTaskQueue();
  if (mReceivedEOS) {
    return;
  }
  SLOG("EOS");
  mReceivedEOS = true;
  ReplySampleRequestIfPossible();
}

bool MFMediaEngineStream::IsEnded() const {
  AssertOnTaskQueue();
  return mReceivedEOS && mRawDataQueueForFeedingEngine.GetSize() == 0;
}

RefPtr<MediaDataDecoder::FlushPromise> MFMediaEngineStream::Flush() {
  if (IsShutdown()) {
    return MediaDataDecoder::FlushPromise::CreateAndReject(
        MediaResult(NS_ERROR_FAILURE,
                    RESULT_DETAIL("MFMediaEngineStream is shutdown")),
        __func__);
  }
  AssertOnTaskQueue();
  SLOG("Flush");
  mRawDataQueueForFeedingEngine.Reset();
  mRawDataQueueForGeneratingOutput.Reset();
  mReceivedEOS = false;
  return MediaDataDecoder::FlushPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> MFMediaEngineStream::OutputData(
    RefPtr<MediaRawData> aSample) {
  if (IsShutdown()) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_FAILURE,
                    RESULT_DETAIL("MFMediaEngineStream is shutdown")),
        __func__);
  }
  AssertOnTaskQueue();
  NotifyNewData(aSample);
  MediaDataDecoder::DecodedData outputs;
  if (RefPtr<MediaData> outputData = OutputDataInternal()) {
    outputs.AppendElement(outputData);
    SLOGV("Output data [%" PRId64 ",%" PRId64 "]",
          outputData->mTime.ToMicroseconds(),
          outputData->GetEndTime().ToMicroseconds());
  }
  return MediaDataDecoder::DecodePromise::CreateAndResolve(std::move(outputs),
                                                           __func__);
};

RefPtr<MediaDataDecoder::DecodePromise> MFMediaEngineStream::Drain() {
  if (IsShutdown()) {
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_FAILURE,
                    RESULT_DETAIL("MFMediaEngineStream is shutdown")),
        __func__);
  }
  AssertOnTaskQueue();
  MediaDataDecoder::DecodedData outputs;
  while (RefPtr<MediaData> outputData = OutputDataInternal()) {
    outputs.AppendElement(outputData);
    SLOGV("Output data [%" PRId64 ",%" PRId64 "]",
          outputData->mTime.ToMicroseconds(),
          outputData->GetEndTime().ToMicroseconds());
  }
  return MediaDataDecoder::DecodePromise::CreateAndResolve(std::move(outputs),
                                                           __func__);
}

void MFMediaEngineStream::AssertOnTaskQueue() const {
  MOZ_ASSERT(mTaskQueue && mTaskQueue->IsCurrentThreadIn());
}

void MFMediaEngineStream::AssertOnMFThreadPool() const {
  // We can't really assert the thread id from thread pool, because it would
  // change any time. So we just assert this is not the task queue, and use the
  // explicit function name to indicate what thread we should run on.
  // TODO : this assertion is not precise, because the running thread could be
  // the stream wrapper thread as well,
  MOZ_ASSERT(!mTaskQueue || !mTaskQueue->IsCurrentThreadIn());
}

#undef WLOGV
#undef SLOG
#undef SLOGV

}  // namespace mozilla
