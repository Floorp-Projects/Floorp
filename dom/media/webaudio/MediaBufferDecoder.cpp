/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaBufferDecoder.h"

#include <speex/speex_resampler.h>

#include "AudioBuffer.h"
#include "AudioContext.h"
#include "AudioNodeEngine.h"
#include "BufferMediaResource.h"
#include "DecoderTraits.h"
#include "MediaContainerType.h"
#include "MediaDataDecoderProxy.h"
#include "MediaDataDemuxer.h"
#include "MediaQueue.h"
#include "PDMFactory.h"
#include "VideoUtils.h"
#include "WebAudioUtils.h"
#include "js/MemoryFunctions.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/AudioContextBinding.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsMimeTypes.h"
#include "nsPrintfCString.h"
#include "nsXPCOMCIDInternal.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;

#define LOG(x, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, (x, ##__VA_ARGS__))

using namespace dom;

class ReportResultTask final : public Runnable {
 public:
  ReportResultTask(WebAudioDecodeJob& aDecodeJob,
                   WebAudioDecodeJob::ResultFn aFunction,
                   WebAudioDecodeJob::ErrorCode aErrorCode)
      : Runnable("ReportResultTask"),
        mDecodeJob(aDecodeJob),
        mFunction(aFunction),
        mErrorCode(aErrorCode) {
    MOZ_ASSERT(aFunction);
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    (mDecodeJob.*mFunction)(mErrorCode);

    return NS_OK;
  }

 private:
  // Note that the mDecodeJob member will probably die when mFunction is run.
  // Therefore, it is not safe to do anything fancy with it in this class.
  // Really, this class is only used because nsRunnableMethod doesn't support
  // methods accepting arguments.
  WebAudioDecodeJob& mDecodeJob;
  WebAudioDecodeJob::ResultFn mFunction;
  WebAudioDecodeJob::ErrorCode mErrorCode;
};

enum class PhaseEnum : int { Decode, AllocateBuffer, Done };

class MediaDecodeTask final : public Runnable {
 public:
  MediaDecodeTask(const MediaContainerType& aContainerType, uint8_t* aBuffer,
                  uint32_t aLength, WebAudioDecodeJob& aDecodeJob)
      : Runnable("MediaDecodeTask"),
        mContainerType(aContainerType),
        mBuffer(aBuffer),
        mLength(aLength),
        mBatchSize(StaticPrefs::media_rdd_webaudio_batch_size()),
        mDecodeJob(aDecodeJob),
        mPhase(PhaseEnum::Decode) {
    MOZ_ASSERT(aBuffer);
    MOZ_ASSERT(NS_IsMainThread());
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.  See
  // bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD Run() override;
  bool Init();
  TaskQueue* PSupervisorTaskQueue() { return mPSupervisorTaskQueue; }
  bool OnPSupervisorTaskQueue() const {
    return mPSupervisorTaskQueue->IsCurrentThreadIn();
  }

 private:
  MOZ_CAN_RUN_SCRIPT
  void ReportFailureOnMainThread(WebAudioDecodeJob::ErrorCode aErrorCode) {
    if (NS_IsMainThread()) {
      Cleanup();
      mDecodeJob.OnFailure(aErrorCode);
    } else {
      // Take extra care to cleanup on the main thread
      mMainThread->Dispatch(NewRunnableMethod("MediaDecodeTask::Cleanup", this,
                                              &MediaDecodeTask::Cleanup));

      nsCOMPtr<nsIRunnable> event = new ReportResultTask(
          mDecodeJob, &WebAudioDecodeJob::OnFailure, aErrorCode);
      mMainThread->Dispatch(event.forget());
    }
  }

  void Decode();

  void OnCreateDecoderCompleted(RefPtr<MediaDataDecoder> aDecoder);
  MOZ_CAN_RUN_SCRIPT void OnCreateDecoderFailed(const MediaResult& aError);

  MOZ_CAN_RUN_SCRIPT void OnInitDemuxerCompleted();
  MOZ_CAN_RUN_SCRIPT void OnInitDemuxerFailed(const MediaResult& aError);

  void InitDecoder();
  void OnInitDecoderCompleted();
  MOZ_CAN_RUN_SCRIPT void OnInitDecoderFailed();

  void DoDemux();
  void OnAudioDemuxCompleted(RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples);
  MOZ_CAN_RUN_SCRIPT void OnAudioDemuxFailed(const MediaResult& aError);

  void DoDecode();
  void OnAudioDecodeCompleted(MediaDataDecoder::DecodedData&& aResults);
  MOZ_CAN_RUN_SCRIPT void OnAudioDecodeFailed(const MediaResult& aError);

  void DoDrain();
  MOZ_CAN_RUN_SCRIPT void OnAudioDrainCompleted(
      MediaDataDecoder::DecodedData&& aResults);
  MOZ_CAN_RUN_SCRIPT void OnAudioDrainFailed(const MediaResult& aError);

  void ShutdownDecoder();

  MOZ_CAN_RUN_SCRIPT void FinishDecode();
  MOZ_CAN_RUN_SCRIPT void AllocateBuffer();
  MOZ_CAN_RUN_SCRIPT void CallbackTheResult();

  void Cleanup() {
    MOZ_ASSERT(NS_IsMainThread());
    JS_free(nullptr, mBuffer);
    if (mTrackDemuxer) {
      mTrackDemuxer->BreakCycles();
    }
    mTrackDemuxer = nullptr;
    mDemuxer = nullptr;
    mPSupervisorTaskQueue = nullptr;
    mPDecoderTaskQueue = nullptr;
  }

 private:
  MediaContainerType mContainerType;
  uint8_t* mBuffer;
  const uint32_t mLength;
  const uint32_t mBatchSize;
  WebAudioDecodeJob& mDecodeJob;
  PhaseEnum mPhase;
  RefPtr<TaskQueue> mPSupervisorTaskQueue;
  RefPtr<TaskQueue> mPDecoderTaskQueue;
  RefPtr<MediaDataDemuxer> mDemuxer;
  RefPtr<MediaTrackDemuxer> mTrackDemuxer;
  RefPtr<MediaDataDecoder> mDecoder;
  nsTArray<RefPtr<MediaRawData>> mRawSamples;
  MediaInfo mMediaInfo;
  MediaQueue<AudioData> mAudioQueue;
  RefPtr<AbstractThread> mMainThread;
};

NS_IMETHODIMP
MediaDecodeTask::Run() {
  switch (mPhase) {
    case PhaseEnum::Decode:
      Decode();
      break;
    case PhaseEnum::AllocateBuffer:
      AllocateBuffer();
      break;
    case PhaseEnum::Done:
      break;
  }

  return NS_OK;
}

bool MediaDecodeTask::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<BufferMediaResource> resource =
      new BufferMediaResource(static_cast<uint8_t*>(mBuffer), mLength);

  mMainThread = mDecodeJob.mContext->GetOwnerGlobal()->AbstractMainThreadFor(
      TaskCategory::Other);

  mPSupervisorTaskQueue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                        "MediaBufferDecoder::mPSupervisorTaskQueue");
  mPDecoderTaskQueue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                        "MediaBufferDecoder::mPDecoderTaskQueue");

  // If you change this list to add support for new decoders, please consider
  // updating HTMLMediaElement::CreateDecoder as well.
  mDemuxer = DecoderTraits::CreateDemuxer(mContainerType, resource);
  if (!mDemuxer) {
    return false;
  }

  return true;
}

class AutoResampler final {
 public:
  AutoResampler() : mResampler(nullptr) {}
  ~AutoResampler() {
    if (mResampler) {
      speex_resampler_destroy(mResampler);
    }
  }
  operator SpeexResamplerState*() const {
    MOZ_ASSERT(mResampler);
    return mResampler;
  }
  void operator=(SpeexResamplerState* aResampler) { mResampler = aResampler; }

 private:
  SpeexResamplerState* mResampler;
};

void MediaDecodeTask::Decode() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  mDemuxer->Init()->Then(PSupervisorTaskQueue(), __func__, this,
                         &MediaDecodeTask::OnInitDemuxerCompleted,
                         &MediaDecodeTask::OnInitDemuxerFailed);
}

void MediaDecodeTask::OnInitDemuxerCompleted() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  if (!!mDemuxer->GetNumberTracks(TrackInfo::kAudioTrack)) {
    mTrackDemuxer = mDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
    if (!mTrackDemuxer) {
      LOG("MediaDecodeTask: Could not get a track demuxer.");
      ReportFailureOnMainThread(WebAudioDecodeJob::UnknownContent);
      return;
    }

    RefPtr<PDMFactory> platform = new PDMFactory();
    UniquePtr<TrackInfo> audioInfo = mTrackDemuxer->GetInfo();
    // We actively ignore audio tracks that we know we can't play.
    if (audioInfo && audioInfo->IsValid() &&
        platform->SupportsMimeType(audioInfo->mMimeType) !=
            media::DecodeSupport::Unsupported) {
      mMediaInfo.mAudio = *audioInfo->GetAsAudioInfo();
    }
  }

  RefPtr<PDMFactory> pdm = new PDMFactory();
  pdm->CreateDecoder(
         {*mMediaInfo.mAudio.GetAsAudioInfo(), TrackInfo::kAudioTrack})
      ->Then(PSupervisorTaskQueue(), __func__, this,
             &MediaDecodeTask::OnCreateDecoderCompleted,
             &MediaDecodeTask::OnCreateDecoderFailed);
}

void MediaDecodeTask::OnCreateDecoderCompleted(
    RefPtr<MediaDataDecoder> aDecoder) {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  mDecoder = new MediaDataDecoderProxy(aDecoder.forget(),
                                       do_AddRef(mPDecoderTaskQueue.get()));
  InitDecoder();
}

void MediaDecodeTask::OnCreateDecoderFailed(const MediaResult& aError) {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  LOG("MediaDecodeTask: Could not create a decoder.");
  ReportFailureOnMainThread(WebAudioDecodeJob::UnknownContent);
}

void MediaDecodeTask::OnInitDemuxerFailed(const MediaResult& aError) {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  LOG("MediaDecodeTask: Could not initialize the demuxer.");
  ReportFailureOnMainThread(WebAudioDecodeJob::InvalidContent);
}

void MediaDecodeTask::InitDecoder() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  mDecoder->Init()->Then(PSupervisorTaskQueue(), __func__, this,
                         &MediaDecodeTask::OnInitDecoderCompleted,
                         &MediaDecodeTask::OnInitDecoderFailed);
}

void MediaDecodeTask::OnInitDecoderCompleted() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  DoDemux();
}

void MediaDecodeTask::OnInitDecoderFailed() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  ShutdownDecoder();
  LOG("MediaDecodeTask: Could not initialize the decoder");
  ReportFailureOnMainThread(WebAudioDecodeJob::InvalidContent);
}

void MediaDecodeTask::DoDemux() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  mTrackDemuxer->GetSamples(mBatchSize)
      ->Then(PSupervisorTaskQueue(), __func__, this,
             &MediaDecodeTask::OnAudioDemuxCompleted,
             &MediaDecodeTask::OnAudioDemuxFailed);
}

void MediaDecodeTask::OnAudioDemuxCompleted(
    RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples) {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  mRawSamples.AppendElements(aSamples->GetSamples());

  DoDemux();
}

void MediaDecodeTask::OnAudioDemuxFailed(const MediaResult& aError) {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  if (aError.Code() == NS_ERROR_DOM_MEDIA_END_OF_STREAM) {
    DoDecode();
  } else {
    ShutdownDecoder();
    LOG("MediaDecodeTask: Audio demux failed");
    ReportFailureOnMainThread(WebAudioDecodeJob::InvalidContent);
  }
}

void MediaDecodeTask::DoDecode() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  if (mRawSamples.IsEmpty()) {
    DoDrain();
    return;
  }

  if (mBatchSize > 1 && mDecoder->CanDecodeBatch()) {
    nsTArray<RefPtr<MediaRawData>> rawSampleBatch;
    const int batchSize = std::min((unsigned long)mBatchSize,
                                   (unsigned long)mRawSamples.Length());
    for (int i = 0; i < batchSize; ++i) {
      rawSampleBatch.AppendElement(std::move(mRawSamples[i]));
    }

    mDecoder->DecodeBatch(std::move(rawSampleBatch))
        ->Then(PSupervisorTaskQueue(), __func__, this,
               &MediaDecodeTask::OnAudioDecodeCompleted,
               &MediaDecodeTask::OnAudioDecodeFailed);

    mRawSamples.RemoveElementsAt(0, batchSize);
  } else {
    RefPtr<MediaRawData> sample = std::move(mRawSamples[0]);

    mDecoder->Decode(sample)->Then(PSupervisorTaskQueue(), __func__, this,
                                   &MediaDecodeTask::OnAudioDecodeCompleted,
                                   &MediaDecodeTask::OnAudioDecodeFailed);

    mRawSamples.RemoveElementAt(0);
  }
}

void MediaDecodeTask::OnAudioDecodeCompleted(
    MediaDataDecoder::DecodedData&& aResults) {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  for (auto&& sample : aResults) {
    MOZ_ASSERT(sample->mType == MediaData::Type::AUDIO_DATA);
    RefPtr<AudioData> audioData = sample->As<AudioData>();

    mMediaInfo.mAudio.mRate = audioData->mRate;
    mMediaInfo.mAudio.mChannels = audioData->mChannels;

    mAudioQueue.Push(audioData.forget());
  }

  DoDecode();
}

void MediaDecodeTask::OnAudioDecodeFailed(const MediaResult& aError) {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  ShutdownDecoder();
  LOG("MediaDecodeTask: decode audio failed.");
  ReportFailureOnMainThread(WebAudioDecodeJob::InvalidContent);
}

void MediaDecodeTask::DoDrain() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  mDecoder->Drain()->Then(PSupervisorTaskQueue(), __func__, this,
                          &MediaDecodeTask::OnAudioDrainCompleted,
                          &MediaDecodeTask::OnAudioDrainFailed);
}

void MediaDecodeTask::OnAudioDrainCompleted(
    MediaDataDecoder::DecodedData&& aResults) {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  if (aResults.IsEmpty()) {
    FinishDecode();
    return;
  }

  for (auto&& sample : aResults) {
    MOZ_ASSERT(sample->mType == MediaData::Type::AUDIO_DATA);
    RefPtr<AudioData> audioData = sample->As<AudioData>();

    mAudioQueue.Push(audioData.forget());
  }
  DoDrain();
}

void MediaDecodeTask::OnAudioDrainFailed(const MediaResult& aError) {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  ShutdownDecoder();
  LOG("MediaDecodeTask: Drain audio failed");
  ReportFailureOnMainThread(WebAudioDecodeJob::InvalidContent);
}

void MediaDecodeTask::ShutdownDecoder() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  if (!mDecoder) {
    return;
  }

  RefPtr<MediaDecodeTask> self = this;
  mDecoder->Shutdown();
  mDecoder = nullptr;
}

void MediaDecodeTask::FinishDecode() {
  MOZ_ASSERT(OnPSupervisorTaskQueue());

  ShutdownDecoder();

  uint32_t frameCount = mAudioQueue.AudioFramesCount();
  uint32_t channelCount = mMediaInfo.mAudio.mChannels;
  uint32_t sampleRate = mMediaInfo.mAudio.mRate;

  if (!frameCount || !channelCount || !sampleRate) {
    LOG("MediaDecodeTask: invalid content frame count, channel count or "
        "sample-rate");
    ReportFailureOnMainThread(WebAudioDecodeJob::InvalidContent);
    return;
  }

  const uint32_t destSampleRate = mDecodeJob.mContext->SampleRate();
  AutoResampler resampler;

  uint32_t resampledFrames = frameCount;
  if (sampleRate != destSampleRate) {
    resampledFrames = static_cast<uint32_t>(
        static_cast<uint64_t>(destSampleRate) *
        static_cast<uint64_t>(frameCount) / static_cast<uint64_t>(sampleRate));

    resampler = speex_resampler_init(channelCount, sampleRate, destSampleRate,
                                     SPEEX_RESAMPLER_QUALITY_DEFAULT, nullptr);
    speex_resampler_skip_zeros(resampler);
    resampledFrames += speex_resampler_get_output_latency(resampler);
  }

  // Allocate contiguous channel buffers.  Note that if we end up resampling,
  // we may write fewer bytes than mResampledFrames to the output buffer, in
  // which case writeIndex will tell us how many valid samples we have.
  mDecodeJob.mBuffer.mChannelData.SetLength(channelCount);
#if AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_FLOAT32
  // This buffer has separate channel arrays that could be transferred to
  // JS::NewArrayBufferWithContents(), but AudioBuffer::RestoreJSChannelData()
  // does not yet take advantage of this.
  RefPtr<ThreadSharedFloatArrayBufferList> buffer =
      ThreadSharedFloatArrayBufferList::Create(channelCount, resampledFrames,
                                               fallible);
  if (!buffer) {
    LOG("MediaDecodeTask: Could not create final buffer (f32)");
    ReportFailureOnMainThread(WebAudioDecodeJob::UnknownError);
    return;
  }
  for (uint32_t i = 0; i < channelCount; ++i) {
    mDecodeJob.mBuffer.mChannelData[i] = buffer->GetData(i);
  }
#else
  CheckedInt<size_t> bufferSize(sizeof(AudioDataValue));
  bufferSize *= resampledFrames;
  bufferSize *= channelCount;
  RefPtr<SharedBuffer> buffer = SharedBuffer::Create(bufferSize);
  if (!buffer) {
    LOG("MediaDecodeTask: Could not create final buffer (i16)");
    ReportFailureOnMainThread(WebAudioDecodeJob::UnknownError);
    return;
  }
  auto data = static_cast<AudioDataValue*>(floatBuffer->Data());
  for (uint32_t i = 0; i < channelCount; ++i) {
    mDecodeJob.mBuffer.mChannelData[i] = data;
    data += resampledFrames;
  }
#endif
  mDecodeJob.mBuffer.mBuffer = std::move(buffer);
  mDecodeJob.mBuffer.mVolume = 1.0f;
  mDecodeJob.mBuffer.mBufferFormat = AUDIO_OUTPUT_FORMAT;

  uint32_t writeIndex = 0;
  RefPtr<AudioData> audioData;
  while ((audioData = mAudioQueue.PopFront())) {
    if (!audioData->Frames()) {
      // The packet contains no audio frames, skip it.
      continue;
    }
    audioData->EnsureAudioBuffer();  // could lead to a copy :(
    const AudioDataValue* bufferData =
        static_cast<AudioDataValue*>(audioData->mAudioBuffer->Data());

    if (sampleRate != destSampleRate) {
      const uint32_t maxOutSamples = resampledFrames - writeIndex;

      for (uint32_t i = 0; i < audioData->mChannels; ++i) {
        uint32_t inSamples = audioData->Frames();
        uint32_t outSamples = maxOutSamples;
        AudioDataValue* outData =
            mDecodeJob.mBuffer.ChannelDataForWrite<AudioDataValue>(i) +
            writeIndex;

        WebAudioUtils::SpeexResamplerProcess(
            resampler, i, &bufferData[i * audioData->Frames()], &inSamples,
            outData, &outSamples);

        if (i == audioData->mChannels - 1) {
          writeIndex += outSamples;
          MOZ_ASSERT(writeIndex <= resampledFrames);
          MOZ_ASSERT(inSamples == audioData->Frames());
        }
      }
    } else {
      for (uint32_t i = 0; i < audioData->mChannels; ++i) {
        AudioDataValue* outData =
            mDecodeJob.mBuffer.ChannelDataForWrite<AudioDataValue>(i) +
            writeIndex;
        PodCopy(outData, &bufferData[i * audioData->Frames()],
                audioData->Frames());

        if (i == audioData->mChannels - 1) {
          writeIndex += audioData->Frames();
        }
      }
    }
  }

  if (sampleRate != destSampleRate) {
    uint32_t inputLatency = speex_resampler_get_input_latency(resampler);
    const uint32_t maxOutSamples = resampledFrames - writeIndex;
    for (uint32_t i = 0; i < channelCount; ++i) {
      uint32_t inSamples = inputLatency;
      uint32_t outSamples = maxOutSamples;
      AudioDataValue* outData =
          mDecodeJob.mBuffer.ChannelDataForWrite<AudioDataValue>(i) +
          writeIndex;

      WebAudioUtils::SpeexResamplerProcess(resampler, i,
                                           (AudioDataValue*)nullptr, &inSamples,
                                           outData, &outSamples);

      if (i == channelCount - 1) {
        writeIndex += outSamples;
        MOZ_ASSERT(writeIndex <= resampledFrames);
        MOZ_ASSERT(inSamples == inputLatency);
      }
    }
  }

  mDecodeJob.mBuffer.mDuration = writeIndex;
  mPhase = PhaseEnum::AllocateBuffer;
  mMainThread->Dispatch(do_AddRef(this));
}

void MediaDecodeTask::AllocateBuffer() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDecodeJob.AllocateBuffer()) {
    LOG("MediaDecodeTask: Could not allocate final buffer");
    ReportFailureOnMainThread(WebAudioDecodeJob::UnknownError);
    return;
  }

  mPhase = PhaseEnum::Done;
  CallbackTheResult();
}

void MediaDecodeTask::CallbackTheResult() {
  MOZ_ASSERT(NS_IsMainThread());

  Cleanup();

  // Now, we're ready to call the script back with the resulting buffer
  mDecodeJob.OnSuccess(WebAudioDecodeJob::NoError);
}

bool WebAudioDecodeJob::AllocateBuffer() {
  MOZ_ASSERT(!mOutput);
  MOZ_ASSERT(NS_IsMainThread());

  // Now create the AudioBuffer
  mOutput = AudioBuffer::Create(mContext->GetOwner(), mContext->SampleRate(),
                                std::move(mBuffer));
  return mOutput != nullptr;
}

void AsyncDecodeWebAudio(const char* aContentType, uint8_t* aBuffer,
                         uint32_t aLength, WebAudioDecodeJob& aDecodeJob) {
  Maybe<MediaContainerType> containerType =
      MakeMediaContainerType(aContentType);
  // Do not attempt to decode the media if we were not successful at sniffing
  // the container type.
  if (!*aContentType || strcmp(aContentType, APPLICATION_OCTET_STREAM) == 0 ||
      !containerType) {
    nsCOMPtr<nsIRunnable> event =
        new ReportResultTask(aDecodeJob, &WebAudioDecodeJob::OnFailure,
                             WebAudioDecodeJob::UnknownContent);
    JS_free(nullptr, aBuffer);
    aDecodeJob.mContext->Dispatch(event.forget());
    return;
  }

  RefPtr<MediaDecodeTask> task =
      new MediaDecodeTask(*containerType, aBuffer, aLength, aDecodeJob);
  if (!task->Init()) {
    nsCOMPtr<nsIRunnable> event =
        new ReportResultTask(aDecodeJob, &WebAudioDecodeJob::OnFailure,
                             WebAudioDecodeJob::UnknownError);
    aDecodeJob.mContext->Dispatch(event.forget());
  } else {
    nsresult rv = task->PSupervisorTaskQueue()->Dispatch(task.forget());
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }
}

WebAudioDecodeJob::WebAudioDecodeJob(AudioContext* aContext, Promise* aPromise,
                                     DecodeSuccessCallback* aSuccessCallback,
                                     DecodeErrorCallback* aFailureCallback)
    : mContext(aContext),
      mPromise(aPromise),
      mSuccessCallback(aSuccessCallback),
      mFailureCallback(aFailureCallback) {
  MOZ_ASSERT(aContext);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(WebAudioDecodeJob);
}

WebAudioDecodeJob::~WebAudioDecodeJob() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(WebAudioDecodeJob);
}

void WebAudioDecodeJob::OnSuccess(ErrorCode aErrorCode) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aErrorCode == NoError);

  RefPtr<AudioBuffer> output(mOutput);
  if (mSuccessCallback) {
    RefPtr<DecodeSuccessCallback> callback(mSuccessCallback);
    // Ignore errors in calling the callback, since there is not much that we
    // can do about it here.
    callback->Call(*output);
  }
  mPromise->MaybeResolve(output);

  mContext->RemoveFromDecodeQueue(this);
}

void WebAudioDecodeJob::OnFailure(ErrorCode aErrorCode) {
  MOZ_ASSERT(NS_IsMainThread());

  const char* errorMessage;
  switch (aErrorCode) {
    case UnknownContent:
      errorMessage =
          "The buffer passed to decodeAudioData contains an unknown content "
          "type.";
      break;
    case InvalidContent:
      errorMessage =
          "The buffer passed to decodeAudioData contains invalid content which "
          "cannot be decoded successfully.";
      break;
    case NoAudio:
      errorMessage =
          "The buffer passed to decodeAudioData does not contain any audio.";
      break;
    case NoError:
      MOZ_FALLTHROUGH_ASSERT("Who passed NoError to OnFailure?");
      // Fall through to get some sort of a sane error message if this actually
      // happens at runtime.
    case UnknownError:
      [[fallthrough]];
    default:
      errorMessage =
          "An unknown error occurred while processing decodeAudioData.";
      break;
  }

  // Ignore errors in calling the callback, since there is not much that we can
  // do about it here.
  nsAutoCString errorString(errorMessage);
  if (mFailureCallback) {
    RefPtr<DOMException> exception = DOMException::Create(
        NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR, errorString);
    RefPtr<DecodeErrorCallback> callback(mFailureCallback);
    callback->Call(*exception);
  }

  mPromise->MaybeRejectWithEncodingError(errorString);

  mContext->RemoveFromDecodeQueue(this);
}

size_t WebAudioDecodeJob::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t amount = 0;
  if (mSuccessCallback) {
    amount += mSuccessCallback->SizeOfIncludingThis(aMallocSizeOf);
  }
  if (mFailureCallback) {
    amount += mFailureCallback->SizeOfIncludingThis(aMallocSizeOf);
  }
  if (mOutput) {
    amount += mOutput->SizeOfIncludingThis(aMallocSizeOf);
  }
  amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf, false);
  return amount;
}

size_t WebAudioDecodeJob::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

}  // namespace mozilla
