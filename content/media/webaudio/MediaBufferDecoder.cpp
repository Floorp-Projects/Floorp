/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaBufferDecoder.h"
#include "AbstractMediaDecoder.h"
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include <speex/speex_resampler.h>
#include "nsXPCOMCIDInternal.h"
#include "nsComponentManagerUtils.h"
#include "MediaDecoderReader.h"
#include "BufferMediaResource.h"
#include "DecoderTraits.h"
#include "AudioContext.h"
#include "AudioBuffer.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptError.h"
#include "nsMimeTypes.h"
#include "nsCxPusher.h"

namespace mozilla {

using namespace dom;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#endif

/**
 * This class provides a decoder object which decodes a media file that lives in
 * a memory buffer.
 */
class BufferDecoder : public AbstractMediaDecoder
{
public:
  // This class holds a weak pointer to MediaResouce.  It's the responsibility
  // of the caller to manage the memory of the MediaResource object.
  explicit BufferDecoder(MediaResource* aResource);
  virtual ~BufferDecoder();

  NS_DECL_ISUPPORTS

  // This has to be called before decoding begins
  void BeginDecoding(nsIThread* aDecodeThread)
  {
    MOZ_ASSERT(!mDecodeThread && aDecodeThread);
    mDecodeThread = aDecodeThread;
  }

  virtual ReentrantMonitor& GetReentrantMonitor() MOZ_FINAL MOZ_OVERRIDE;

  virtual bool IsShutdown() const MOZ_FINAL MOZ_OVERRIDE;

  virtual bool OnStateMachineThread() const MOZ_FINAL MOZ_OVERRIDE;

  virtual bool OnDecodeThread() const MOZ_FINAL MOZ_OVERRIDE;

  virtual MediaResource* GetResource() const MOZ_FINAL MOZ_OVERRIDE;

  virtual void NotifyBytesConsumed(int64_t aBytes) MOZ_FINAL MOZ_OVERRIDE;

  virtual void NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded) MOZ_FINAL MOZ_OVERRIDE;

  virtual int64_t GetEndMediaTime() const MOZ_FINAL MOZ_OVERRIDE;

  virtual int64_t GetMediaDuration() MOZ_FINAL MOZ_OVERRIDE;

  virtual void SetMediaDuration(int64_t aDuration) MOZ_FINAL MOZ_OVERRIDE;

  virtual void SetMediaSeekable(bool aMediaSeekable) MOZ_OVERRIDE;

  virtual void SetTransportSeekable(bool aTransportSeekable) MOZ_FINAL MOZ_OVERRIDE;

  virtual VideoFrameContainer* GetVideoFrameContainer() MOZ_FINAL MOZ_OVERRIDE;
  virtual mozilla::layers::ImageContainer* GetImageContainer() MOZ_FINAL MOZ_OVERRIDE;

  virtual bool IsTransportSeekable() MOZ_FINAL MOZ_OVERRIDE;

  virtual bool IsMediaSeekable() MOZ_FINAL MOZ_OVERRIDE;

  virtual void MetadataLoaded(int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags) MOZ_FINAL MOZ_OVERRIDE;
  virtual void QueueMetadata(int64_t aTime, int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags) MOZ_FINAL MOZ_OVERRIDE;

  virtual void SetMediaEndTime(int64_t aTime) MOZ_FINAL MOZ_OVERRIDE;

  virtual void UpdatePlaybackPosition(int64_t aTime) MOZ_FINAL MOZ_OVERRIDE;

  virtual void OnReadMetadataCompleted() MOZ_FINAL MOZ_OVERRIDE;

  virtual MediaDecoderOwner* GetOwner() MOZ_FINAL MOZ_OVERRIDE;

private:
  // This monitor object is not really used to synchronize access to anything.
  // It's just there in order for us to be able to override
  // GetReentrantMonitor correctly.
  ReentrantMonitor mReentrantMonitor;
  nsCOMPtr<nsIThread> mDecodeThread;
  nsRefPtr<MediaResource> mResource;
};

NS_IMPL_THREADSAFE_ISUPPORTS0(BufferDecoder)

BufferDecoder::BufferDecoder(MediaResource* aResource)
  : mReentrantMonitor("BufferDecoder")
  , mResource(aResource)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(BufferDecoder);
#ifdef PR_LOGGING
  if (!gMediaDecoderLog) {
    gMediaDecoderLog = PR_NewLogModule("MediaDecoder");
  }
#endif
}

BufferDecoder::~BufferDecoder()
{
  // The dtor may run on any thread, we cannot be sure.
  MOZ_COUNT_DTOR(BufferDecoder);
}

ReentrantMonitor&
BufferDecoder::GetReentrantMonitor()
{
  return mReentrantMonitor;
}

bool
BufferDecoder::IsShutdown() const
{
  // BufferDecoder cannot be shut down.
  return false;
}

bool
BufferDecoder::OnStateMachineThread() const
{
  // BufferDecoder doesn't have the concept of a state machine.
  return true;
}

bool
BufferDecoder::OnDecodeThread() const
{
  MOZ_ASSERT(mDecodeThread, "Forgot to call BeginDecoding?");
  return IsCurrentThread(mDecodeThread);
}

MediaResource*
BufferDecoder::GetResource() const
{
  return mResource;
}

void
BufferDecoder::NotifyBytesConsumed(int64_t aBytes)
{
  // ignore
}

void
BufferDecoder::NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded)
{
  // ignore
}

int64_t
BufferDecoder::GetEndMediaTime() const
{
  // unknown
  return -1;
}

int64_t
BufferDecoder::GetMediaDuration()
{
  // unknown
  return -1;
}

void
BufferDecoder::SetMediaDuration(int64_t aDuration)
{
  // ignore
}

void
BufferDecoder::SetMediaSeekable(bool aMediaSeekable)
{
  // ignore
}

void
BufferDecoder::SetTransportSeekable(bool aTransportSeekable)
{
  // ignore
}

VideoFrameContainer*
BufferDecoder::GetVideoFrameContainer()
{
  // no video frame
  return nullptr;
}

layers::ImageContainer*
BufferDecoder::GetImageContainer()
{
  // no image container
  return nullptr;
}

bool
BufferDecoder::IsTransportSeekable()
{
  return false;
}

bool
BufferDecoder::IsMediaSeekable()
{
  return false;
}

void
BufferDecoder::MetadataLoaded(int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags)
{
  // ignore
}

void
BufferDecoder::QueueMetadata(int64_t aTime, int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags)
{
  // ignore
}

void
BufferDecoder::SetMediaEndTime(int64_t aTime)
{
  // ignore
}

void
BufferDecoder::UpdatePlaybackPosition(int64_t aTime)
{
  // ignore
}

void
BufferDecoder::OnReadMetadataCompleted()
{
  // ignore
}

MediaDecoderOwner*
BufferDecoder::GetOwner()
{
  // unknown
  return nullptr;
}

class ReportResultTask : public nsRunnable
{
public:
  ReportResultTask(WebAudioDecodeJob& aDecodeJob,
                   WebAudioDecodeJob::ResultFn aFunction,
                   WebAudioDecodeJob::ErrorCode aErrorCode)
    : mDecodeJob(aDecodeJob)
    , mFunction(aFunction)
    , mErrorCode(aErrorCode)
  {
    MOZ_ASSERT(aFunction);
  }

  NS_IMETHOD Run()
  {
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

MOZ_BEGIN_ENUM_CLASS(PhaseEnum, int)
  Decode,
  AllocateBuffer,
  Done
MOZ_END_ENUM_CLASS(PhaseEnum)

class MediaDecodeTask : public nsRunnable
{
public:
  MediaDecodeTask(const char* aContentType, uint8_t* aBuffer,
                  uint32_t aLength,
                  WebAudioDecodeJob& aDecodeJob,
                  nsIThreadPool* aThreadPool)
    : mContentType(aContentType)
    , mBuffer(aBuffer)
    , mLength(aLength)
    , mDecodeJob(aDecodeJob)
    , mPhase(PhaseEnum::Decode)
    , mThreadPool(aThreadPool)
  {
    MOZ_ASSERT(aBuffer);
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(mDecodeJob.mContext->GetParentObject());
    nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal =
      do_QueryInterface(pWindow);
    if (scriptPrincipal) {
      mPrincipal = scriptPrincipal->GetPrincipal();
    }
  }

  NS_IMETHOD Run();
  bool CreateReader();

private:
  void ReportFailureOnMainThread(WebAudioDecodeJob::ErrorCode aErrorCode) {
    if (NS_IsMainThread()) {
      Cleanup();
      mDecodeJob.OnFailure(aErrorCode);
    } else {
      // Take extra care to cleanup on the main thread
      NS_DispatchToMainThread(NS_NewRunnableMethod(this, &MediaDecodeTask::Cleanup),
                              NS_DISPATCH_NORMAL);

      nsCOMPtr<nsIRunnable> event =
        new ReportResultTask(mDecodeJob, &WebAudioDecodeJob::OnFailure, aErrorCode);
      NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    }
  }

  void RunNextPhase();

  void Decode();
  void AllocateBuffer();
  void CallbackTheResult();

  void Cleanup()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mBufferDecoder = nullptr;
    mDecoderReader = nullptr;
  }

private:
  nsCString mContentType;
  uint8_t* mBuffer;
  uint32_t mLength;
  WebAudioDecodeJob& mDecodeJob;
  PhaseEnum mPhase;
  nsCOMPtr<nsIThreadPool> mThreadPool;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsRefPtr<BufferDecoder> mBufferDecoder;
  nsAutoPtr<MediaDecoderReader> mDecoderReader;
};

NS_IMETHODIMP
MediaDecodeTask::Run()
{
  MOZ_ASSERT(mBufferDecoder);
  MOZ_ASSERT(mDecoderReader);
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

bool
MediaDecodeTask::CreateReader()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<BufferMediaResource> resource =
    new BufferMediaResource(static_cast<uint8_t*> (mBuffer),
                            mLength, mPrincipal, mContentType);

  MOZ_ASSERT(!mBufferDecoder);
  mBufferDecoder = new BufferDecoder(resource);

  // If you change this list to add support for new decoders, please consider
  // updating HTMLMediaElement::CreateDecoder as well.

  mDecoderReader = DecoderTraits::CreateReader(mContentType, mBufferDecoder);

  if (!mDecoderReader) {
    return false;
  }

  nsresult rv = mDecoderReader->Init(nullptr);
  if (NS_FAILED(rv)) {
    return false;
  }

  return true;
}

void
MediaDecodeTask::RunNextPhase()
{
  // This takes care of handling the logic of where to run the next phase.
  // If we were invoked synchronously, we do not have a thread pool and
  // everything happens on the main thread. Just invoke Run() in that case.
  // Otherwise, some things happen on the main thread and others are run
  // in the thread pool.
  if (!mThreadPool) {
    Run();
    return;
  }

  switch (mPhase) {
  case PhaseEnum::AllocateBuffer:
    MOZ_ASSERT(!NS_IsMainThread());
    NS_DispatchToMainThread(this);
    break;
  case PhaseEnum::Decode:
  case PhaseEnum::Done:
    MOZ_CRASH("Invalid phase Decode");
  }
}

class AutoResampler {
public:
  AutoResampler()
    : mResampler(nullptr)
  {}
  ~AutoResampler()
  {
    if (mResampler) {
      speex_resampler_destroy(mResampler);
    }
  }
  operator SpeexResamplerState*() const
  {
    MOZ_ASSERT(mResampler);
    return mResampler;
  }
  void operator=(SpeexResamplerState* aResampler)
  {
    mResampler = aResampler;
  }

private:
  SpeexResamplerState* mResampler;
};

void
MediaDecodeTask::Decode()
{
  MOZ_ASSERT(!mThreadPool == NS_IsMainThread(),
             "We should be on the main thread only if we don't have a thread pool");

  mBufferDecoder->BeginDecoding(NS_GetCurrentThread());

  mDecoderReader->OnDecodeThreadStart();

  VideoInfo videoInfo;
  nsAutoPtr<MetadataTags> tags;
  nsresult rv = mDecoderReader->ReadMetadata(&videoInfo, getter_Transfers(tags));
  if (NS_FAILED(rv)) {
    ReportFailureOnMainThread(WebAudioDecodeJob::InvalidContent);
    return;
  }

  if (!mDecoderReader->HasAudio()) {
    ReportFailureOnMainThread(WebAudioDecodeJob::NoAudio);
    return;
  }

  while (mDecoderReader->DecodeAudioData()) {
    // consume all of the buffer
    continue;
  }

  mDecoderReader->OnDecodeThreadFinish();

  MediaQueue<AudioData>& audioQueue = mDecoderReader->AudioQueue();
  uint32_t frameCount = audioQueue.FrameCount();
  uint32_t channelCount = videoInfo.mAudioChannels;
  uint32_t sampleRate = videoInfo.mAudioRate;

  if (!frameCount || !channelCount || !sampleRate) {
    ReportFailureOnMainThread(WebAudioDecodeJob::InvalidContent);
    return;
  }

  const uint32_t destSampleRate = mDecodeJob.mContext->SampleRate();
  AutoResampler resampler;

  uint32_t resampledFrames = frameCount;
  if (sampleRate != destSampleRate) {
    resampledFrames = static_cast<uint32_t>(
        static_cast<uint64_t>(destSampleRate) *
        static_cast<uint64_t>(frameCount) /
        static_cast<uint64_t>(sampleRate)
      );

    resampler = speex_resampler_init(channelCount,
                                     sampleRate,
                                     destSampleRate,
                                     SPEEX_RESAMPLER_QUALITY_DEFAULT, nullptr);
    speex_resampler_skip_zeros(resampler);
    resampledFrames += speex_resampler_get_output_latency(resampler);
  }

  // Allocate the channel buffers.  Note that if we end up resampling, we may
  // write fewer bytes than mResampledFrames to the output buffer, in which
  // case mWriteIndex will tell us how many valid samples we have.
  static const fallible_t fallible = fallible_t();
  bool memoryAllocationSuccess = true;
  if (!mDecodeJob.mChannelBuffers.SetLength(channelCount)) {
    memoryAllocationSuccess = false;
  } else {
    for (uint32_t i = 0; i < channelCount; ++i) {
      mDecodeJob.mChannelBuffers[i] = new(fallible) float[resampledFrames];
      if (!mDecodeJob.mChannelBuffers[i]) {
        memoryAllocationSuccess = false;
        break;
      }
    }
  }
  if (!memoryAllocationSuccess) {
    ReportFailureOnMainThread(WebAudioDecodeJob::UnknownError);
    return;
  }

  nsAutoPtr<AudioData> audioData;
  while ((audioData = audioQueue.PopFront())) {
    audioData->EnsureAudioBuffer(); // could lead to a copy :(
    AudioDataValue* bufferData = static_cast<AudioDataValue*>
      (audioData->mAudioBuffer->Data());

    if (sampleRate != destSampleRate) {
      const uint32_t expectedOutSamples = static_cast<uint32_t>(
          static_cast<uint64_t>(destSampleRate) *
          static_cast<uint64_t>(audioData->mFrames) /
          static_cast<uint64_t>(sampleRate)
        );
#ifdef MOZ_SAMPLE_TYPE_S16
      AudioDataValue* resampledBuffer = new(fallible) AudioDataValue[channelCount * expectedOutSamples];
#endif

      for (uint32_t i = 0; i < audioData->mChannels; ++i) {
        uint32_t inSamples = audioData->mFrames;
        uint32_t outSamples = expectedOutSamples;

#ifdef MOZ_SAMPLE_TYPE_S16
        speex_resampler_process_int(resampler, i, &bufferData[i * audioData->mFrames], &inSamples,
                                    &resampledBuffer[i * expectedOutSamples],
                                    &outSamples);

        ConvertAudioSamples(&resampledBuffer[i * expectedOutSamples],
                            mDecodeJob.mChannelBuffers[i] + mDecodeJob.mWriteIndex,
                            outSamples);
#else
        speex_resampler_process_float(resampler, i, &bufferData[i * audioData->mFrames], &inSamples,
                                      mDecodeJob.mChannelBuffers[i] + mDecodeJob.mWriteIndex,
                                      &outSamples);
#endif

        if (i == audioData->mChannels - 1) {
          mDecodeJob.mWriteIndex += outSamples;
          MOZ_ASSERT(mDecodeJob.mWriteIndex <= resampledFrames);
        }
      }

#ifdef MOZ_SAMPLE_TYPE_S16
      delete[] resampledBuffer;
#endif
    } else {
      for (uint32_t i = 0; i < audioData->mChannels; ++i) {
        ConvertAudioSamples(&bufferData[i * audioData->mFrames],
                            mDecodeJob.mChannelBuffers[i] + mDecodeJob.mWriteIndex,
                            audioData->mFrames);

        if (i == audioData->mChannels - 1) {
          mDecodeJob.mWriteIndex += audioData->mFrames;
        }
      }
    }
  }

  if (sampleRate != destSampleRate) {
    int inputLatency = speex_resampler_get_input_latency(resampler);
    int outputLatency = speex_resampler_get_output_latency(resampler);
    AudioDataValue* zero = (AudioDataValue*)calloc(inputLatency, sizeof(AudioDataValue));

#ifdef MOZ_SAMPLE_TYPE_S16
    AudioDataValue* resampledBuffer = new(fallible) AudioDataValue[channelCount * outputLatency];
    if (!resampledBuffer || !zero) {
#else
    if (!zero) {
#endif
      // Out of memory!
      ReportFailureOnMainThread(WebAudioDecodeJob::UnknownError);
      return;
    }

    for (uint32_t i = 0; i < channelCount; ++i) {
      uint32_t inSamples = inputLatency;
      uint32_t outSamples = outputLatency;

#ifdef MOZ_SAMPLE_TYPE_S16
      speex_resampler_process_int(resampler, i, zero, &inSamples,
                                  &resampledBuffer[i * outputLatency],
                                  &outSamples);

      ConvertAudioSamples(&resampledBuffer[i * outputLatency],
                          mDecodeJob.mChannelBuffers[i] + mDecodeJob.mWriteIndex,
                          outSamples);
#else
      speex_resampler_process_float(resampler, i, zero, &inSamples,
                                    mDecodeJob.mChannelBuffers[i] + mDecodeJob.mWriteIndex,
                                    &outSamples);
#endif

      if (i == channelCount - 1) {
        mDecodeJob.mWriteIndex += outSamples;
        MOZ_ASSERT(mDecodeJob.mWriteIndex <= resampledFrames);
      }
    }

    free(zero);

#ifdef MOZ_SAMPLE_TYPE_S16
    delete[] resampledBuffer;
#endif
  }

  mPhase = PhaseEnum::AllocateBuffer;
  RunNextPhase();
}

void
MediaDecodeTask::AllocateBuffer()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDecodeJob.AllocateBuffer()) {
    ReportFailureOnMainThread(WebAudioDecodeJob::UnknownError);
    return;
  }

  mPhase = PhaseEnum::Done;
  CallbackTheResult();
}

void
MediaDecodeTask::CallbackTheResult()
{
  MOZ_ASSERT(NS_IsMainThread());

  Cleanup();

  // Now, we're ready to call the script back with the resulting buffer
  mDecodeJob.OnSuccess(WebAudioDecodeJob::NoError);
}

bool
WebAudioDecodeJob::AllocateBuffer()
{
  MOZ_ASSERT(!mOutput);
  MOZ_ASSERT(NS_IsMainThread());

  // First, get a JSContext
  AutoPushJSContext cx(mContext->GetJSContext());
  if (!cx) {
    return false;
  }

  // Now create the AudioBuffer
  mOutput = new AudioBuffer(mContext, mWriteIndex, mContext->SampleRate());
  if (!mOutput->InitializeBuffers(mChannelBuffers.Length(), cx)) {
    return false;
  }

  for (uint32_t i = 0; i < mChannelBuffers.Length(); ++i) {
    mOutput->SetRawChannelContents(cx, i, mChannelBuffers[i]);
  }

  return true;
}

void
MediaBufferDecoder::AsyncDecodeMedia(const char* aContentType, uint8_t* aBuffer,
                                     uint32_t aLength,
                                     WebAudioDecodeJob& aDecodeJob)
{
  // Do not attempt to decode the media if we were not successful at sniffing
  // the content type.
  if (!*aContentType ||
      strcmp(aContentType, APPLICATION_OCTET_STREAM) == 0) {
    nsCOMPtr<nsIRunnable> event =
      new ReportResultTask(aDecodeJob,
                           &WebAudioDecodeJob::OnFailure,
                           WebAudioDecodeJob::UnknownContent);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    return;
  }

  if (!EnsureThreadPoolInitialized()) {
    nsCOMPtr<nsIRunnable> event =
      new ReportResultTask(aDecodeJob,
                           &WebAudioDecodeJob::OnFailure,
                           WebAudioDecodeJob::UnknownError);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    return;
  }

  MOZ_ASSERT(mThreadPool);

  nsRefPtr<MediaDecodeTask> task =
    new MediaDecodeTask(aContentType, aBuffer, aLength, aDecodeJob, mThreadPool);
  if (!task->CreateReader()) {
    nsCOMPtr<nsIRunnable> event =
      new ReportResultTask(aDecodeJob,
                           &WebAudioDecodeJob::OnFailure,
                           WebAudioDecodeJob::UnknownError);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  } else {
    mThreadPool->Dispatch(task, nsIThreadPool::DISPATCH_NORMAL);
  }
}

bool
MediaBufferDecoder::SyncDecodeMedia(const char* aContentType, uint8_t* aBuffer,
                                    uint32_t aLength,
                                    WebAudioDecodeJob& aDecodeJob)
{
  // Do not attempt to decode the media if we were not successful at sniffing
  // the content type.
  if (!*aContentType ||
      strcmp(aContentType, APPLICATION_OCTET_STREAM) == 0) {
    return false;
  }

  nsRefPtr<MediaDecodeTask> task =
    new MediaDecodeTask(aContentType, aBuffer, aLength, aDecodeJob, nullptr);
  if (!task->CreateReader()) {
    return false;
  }

  task->Run();
  return true;
}


bool
MediaBufferDecoder::EnsureThreadPoolInitialized()
{
  if (!mThreadPool) {
    mThreadPool = do_CreateInstance(NS_THREADPOOL_CONTRACTID);
    if (!mThreadPool) {
      return false;
    }
    mThreadPool->SetName(NS_LITERAL_CSTRING("MediaBufferDecoder"));
  }
  return true;
}

void
MediaBufferDecoder::Shutdown() {
  if (mThreadPool) {
    mThreadPool->Shutdown();
    mThreadPool = nullptr;
  }
  MOZ_ASSERT(!mThreadPool);
}

WebAudioDecodeJob::WebAudioDecodeJob(const nsACString& aContentType,
                                     AudioContext* aContext,
                                     DecodeSuccessCallback* aSuccessCallback,
                                     DecodeErrorCallback* aFailureCallback)
  : mContentType(aContentType)
  , mWriteIndex(0)
  , mContext(aContext)
  , mSuccessCallback(aSuccessCallback)
  , mFailureCallback(aFailureCallback)
{
  MOZ_ASSERT(aContext);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(WebAudioDecodeJob);

  MOZ_ASSERT(aSuccessCallback ||
             (!aSuccessCallback && !aFailureCallback),
             "If a success callback is not passed, no failure callback should be passed either");
}

WebAudioDecodeJob::~WebAudioDecodeJob()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(WebAudioDecodeJob);
}

void
WebAudioDecodeJob::OnSuccess(ErrorCode aErrorCode)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aErrorCode == NoError);

  // Ignore errors in calling the callback, since there is not much that we can
  // do about it here.
  if (mSuccessCallback) {
    ErrorResult rv;
    mSuccessCallback->Call(*mOutput, rv);
  }

  mContext->RemoveFromDecodeQueue(this);
}

void
WebAudioDecodeJob::OnFailure(ErrorCode aErrorCode)
{
  MOZ_ASSERT(NS_IsMainThread());

  const char* errorMessage;
  switch (aErrorCode) {
  case NoError:
    MOZ_ASSERT(false, "Who passed NoError to OnFailure?");
    // Fall through to get some sort of a sane error message if this actually
    // happens at runtime.
  case UnknownError:
    errorMessage = "MediaDecodeAudioDataUnknownError";
    break;
  case UnknownContent:
    errorMessage = "MediaDecodeAudioDataUnknownContentType";
    break;
  case InvalidContent:
    errorMessage = "MediaDecodeAudioDataInvalidContent";
    break;
  case NoAudio:
    errorMessage = "MediaDecodeAudioDataNoAudio";
    break;
  }

  nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(mContext->GetParentObject());
  nsIDocument* doc = nullptr;
  if (pWindow) {
    doc = pWindow->GetExtantDoc();
  }
  nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                  "Media",
                                  doc,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  errorMessage);

  // Ignore errors in calling the callback, since there is not much that we can
  // do about it here.
  if (mFailureCallback) {
    ErrorResult rv;
    mFailureCallback->Call(rv);
  }

  mContext->RemoveFromDecodeQueue(this);
}

}

