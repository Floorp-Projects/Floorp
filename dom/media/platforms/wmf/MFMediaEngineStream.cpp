/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineStream.h"
#include <vcruntime.h>

#include "AudioConverter.h"
#include "MFMediaSource.h"
#include "MFMediaEngineUtils.h"
#include "TimeUnits.h"

namespace mozilla {

// Don't use this log on the task queue, because it would be racy for `mStream`.
#define WLOGV(msg, ...)                                           \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Verbose,                   \
          ("MFMediaEngineStreamWrapper for stream %p (%s), " msg, \
           mStream.Get(), mStream->GetDescriptionName().get(), ##__VA_ARGS__))

#define SLOG(msg, ...)                          \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug,   \
          ("MFMediaStream=%p (%s), " msg, this, \
           this->GetDescriptionName().get(), ##__VA_ARGS__))

#define SLOGV(msg, ...)                         \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Verbose, \
          ("MFMediaStream=%p (%s), " msg, this, \
           this->GetDescriptionName().get(), ##__VA_ARGS__))

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
  Microsoft::WRL::ComPtr<MFMediaEngineStream> stream = mStream;
  Unused << mTaskQueue->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineStreamWrapper::Decode",
      [sample = RefPtr{aSample}, stream]() { stream->NotifyNewData(sample); }));

  // We don't return a real data, all data would be processed inside the media
  // engine. We return an empty data back instead.
  DecodedData samples;
  if (mStream->TrackType() == TrackInfo::TrackType::kAudioTrack) {
    AudioSampleBuffer data(nullptr, 0);
    // TODO : use a null data for audio?
    samples.AppendElement(MakeRefPtr<AudioData>(
        aSample->mOffset, aSample->mTime, data.Forget(), 1, 1));
  } else {
    // Remote video decoder parant will transfer null data to a video data.
    MOZ_ASSERT(mStream->TrackType() == TrackInfo::TrackType::kVideoTrack);
    samples.AppendElement(MakeRefPtr<NullData>(aSample->mOffset, aSample->mTime,
                                               aSample->mDuration));
  }
  return DecodePromise::CreateAndResolve(std::move(samples), __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> MFMediaEngineStreamWrapper::Drain() {
  // Nothing to drain, because we're not able to control real outputs.
  WLOGV("Drain");
  if (!mStream || mStream->IsShutdown()) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_FAILURE, "MFMediaEngineStreamWrapper is shutdown"),
        __func__);
  }
  return DecodePromise::CreateAndResolve(DecodedData(), __func__);
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

MFMediaEngineStream::MFMediaEngineStream()
    : mIsShutdown(false),
      mIsSelected(false),
      mReceivedEOS(false),
      mShouldServeSmamples(false) {}

MFMediaEngineStream::~MFMediaEngineStream() { MOZ_ASSERT(IsShutdown()); }

HRESULT MFMediaEngineStream::RuntimeClassInitialize(
    uint64_t aStreamId, const TrackInfo& aInfo, MFMediaSource* aParentSource) {
  mParentSource = aParentSource;
  mTaskQueue = aParentSource->GetTaskQueue();
  RETURN_IF_FAILED(wmf::MFCreateEventQueue(&mMediaEventQueue));
  RETURN_IF_FAILED(GenerateStreamDescriptor(aStreamId, aInfo));
  SLOG("Initialized %s (id=%" PRIu64 ", descriptorId=%lu)",
       GetDescriptionName().get(), aStreamId, mStreamDescriptorId);
  return S_OK;
}

HRESULT MFMediaEngineStream::GenerateStreamDescriptor(uint64_t aStreamId,
                                                      const TrackInfo& aInfo) {
  ComPtr<IMFMediaType> mediaType;
  // The inherited stream would return different type based on their media info.
  RETURN_IF_FAILED(CreateMediaType(aInfo, mediaType.GetAddressOf()));
  RETURN_IF_FAILED(wmf::MFCreateStreamDescriptor(
      aStreamId, 1 /* stream amount */, mediaType.GetAddressOf(),
      &mStreamDescriptor));
  RETURN_IF_FAILED(
      mStreamDescriptor->GetStreamIdentifier(&mStreamDescriptorId));

  // TODO : set MF_SD_PROTECTED on descriptor when it's encrypted
  return S_OK;
}

HRESULT MFMediaEngineStream::Start(const PROPVARIANT* aPosition) {
  AssertOnMFThreadPool();
  if (!IsSelected()) {
    SLOG("No need to start non-selected stream");
    return S_OK;
  }
  SLOG("Start");
  RETURN_IF_FAILED(QueueEvent(MEStreamStarted, GUID_NULL, S_OK, aPosition));
  mShouldServeSmamples = true;
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
  mShouldServeSmamples = true;
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
  mShouldServeSmamples = false;
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
  mShouldServeSmamples = false;
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
  Unused << mTaskQueue->Dispatch(
      NS_NewRunnableFunction("MFMediaEngineStream::Shutdown", [self]() {
        self->mParentSource = nullptr;
        self->mRawDataQueue.Reset();
      }));
}

IFACEMETHODIMP
MFMediaEngineStream::GetMediaSource(IMFMediaSource** aMediaSource) {
  AssertOnMFThreadPool();
  if (IsShutdown()) {
    return MF_E_SHUTDOWN;
  }
  SLOG("GetMediaSource");
  RETURN_IF_FAILED(mParentSource.CopyTo(aMediaSource));
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::GetStreamDescriptor(
    IMFStreamDescriptor** aStreamDescriptor) {
  AssertOnMFThreadPool();
  if (IsShutdown()) {
    return MF_E_SHUTDOWN;
  }
  SLOG("GetStreamDescriptor");

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

  if (IsEnded()) {
    SLOG("RequestSample, EOS");
    MOZ_ASSERT(mRawDataQueue.GetSize() == 0);
    RETURN_IF_FAILED(mMediaEventQueue->QueueEventParamUnk(
        MEEndOfStream, GUID_NULL, S_OK, nullptr));
    mEndedEvent.Notify(TrackType());
    return S_OK;
  }

  ComPtr<IUnknown> token = aToken;
  ComPtr<MFMediaEngineStream> self = this;
  Unused << mTaskQueue->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineStream::RequestSample", [token, self, this]() {
        AssertOnTaskQueue();
        mSampleRequestTokens.push(token);
        SLOGV("RequestSample, token amount=%zu", mSampleRequestTokens.size());
        ReplySampleRequestIfPossible();
        if (!HasEnoughRawData() && mParentSource) {
          SLOGV("Dispatch a sample request, queue duration=%" PRId64,
                mRawDataQueue.Duration());
          mParentSource->mRequestSampleEvent.Notify(
              SampleRequest{TrackType(), false /* isEnough */});
        }
      }));
  return S_OK;
}

void MFMediaEngineStream::ReplySampleRequestIfPossible() {
  AssertOnTaskQueue();
  if (mSampleRequestTokens.empty() || mRawDataQueue.GetSize() == 0) {
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

  MOZ_ASSERT(mRawDataQueue.GetSize() != 0);
  RefPtr<MediaRawData> data = mRawDataQueue.PopFront();
  SLOGV("CreateInputSample, pop data [%" PRId64 ", %" PRId64
        "] (duration=%" PRId64 ", kf=%d), queue size=%zu",
        data->mTime.ToMicroseconds(), data->GetEndTime().ToMicroseconds(),
        data->mDuration.ToMicroseconds(), data->mKeyframe,
        mRawDataQueue.GetSize());

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

  // TODO : set up encrypt attributes
  *aSample = sample.Detach();
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::GetEvent(DWORD aFlags,
                                             IMFMediaEvent** aEvent) {
  AssertOnMFThreadPool();
  MOZ_ASSERT(mMediaEventQueue);
  SLOG("GetEvent");
  RETURN_IF_FAILED(mMediaEventQueue->GetEvent(aFlags, aEvent));
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::BeginGetEvent(IMFAsyncCallback* aCallback,
                                                  IUnknown* aState) {
  AssertOnMFThreadPool();
  MOZ_ASSERT(mMediaEventQueue);
  SLOG("BeginGetEvent");
  RETURN_IF_FAILED(mMediaEventQueue->BeginGetEvent(aCallback, aState));
  return S_OK;
}

IFACEMETHODIMP MFMediaEngineStream::EndGetEvent(IMFAsyncResult* aResult,
                                                IMFMediaEvent** aEvent) {
  AssertOnMFThreadPool();
  MOZ_ASSERT(mMediaEventQueue);
  SLOG("EndGetEvent");
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
  bool wasEnough = HasEnoughRawData();
  mRawDataQueue.Push(aSample);
  SLOGV("NotifyNewData, push data [%" PRId64 ", %" PRId64
        "], queue size=%zu, queue duration=%" PRId64,
        aSample->mTime.ToMicroseconds(), aSample->GetEndTime().ToMicroseconds(),
        mRawDataQueue.GetSize(), mRawDataQueue.Duration());
  if (mReceivedEOS) {
    SLOG("Receive a new data, cancel old EOS flag");
    mReceivedEOS = false;
  }
  ReplySampleRequestIfPossible();
  if (!wasEnough && HasEnoughRawData()) {
    SLOGV("data is enough");
    mParentSource->mRequestSampleEvent.Notify(
        SampleRequest{TrackType(), true /* isEnough */});
  }
}

void MFMediaEngineStream::NotifyEndOfStream() {
  AssertOnTaskQueue();
  if (mReceivedEOS) {
    return;
  }
  SLOG("EOS");
  mReceivedEOS = true;
}

bool MFMediaEngineStream::IsEnded() const {
  return mReceivedEOS && mRawDataQueue.GetSize() == 0;
}

RefPtr<MediaDataDecoder::FlushPromise> MFMediaEngineStream::Flush() {
  AssertOnTaskQueue();
  if (IsShutdown()) {
    return MediaDataDecoder::FlushPromise::CreateAndReject(
        MediaResult(NS_ERROR_FAILURE,
                    RESULT_DETAIL("MFMediaEngineStream is shutdown")),
        __func__);
  }
  SLOG("Flush");
  mRawDataQueue.Reset();
  return MediaDataDecoder::FlushPromise::CreateAndResolve(true, __func__);
}

void MFMediaEngineStream::AssertOnTaskQueue() const {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
}

void MFMediaEngineStream::AssertOnMFThreadPool() const {
  // We can't really assert the thread id from thread pool, because it would
  // change any time. So we just assert this is not the task queue, and use the
  // explicit function name to indicate what thread we should run on.
  MOZ_ASSERT(!mTaskQueue->IsCurrentThreadIn());
}

#undef WLOGV
#undef SLOG
#undef SLOGV

}  // namespace mozilla
