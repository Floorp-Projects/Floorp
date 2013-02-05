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

#ifdef MOZ_GSTREAMER
#include "GStreamerReader.h"
#endif
#ifdef MOZ_RAW
#include "RawReader.h"
#endif
#ifdef MOZ_OGG
#include "OggReader.h"
#endif
#ifdef MOZ_WAVE
#include "WaveReader.h"
#endif
#ifdef MOZ_WIDGET_GONK
#include "MediaOmxReader.h"
#endif
#ifdef MOZ_MEDIA_PLUGINS
#include "MediaDecoder.h"
#include "MediaPluginDecoder.h"
#include "MediaPluginReader.h"
#include "MediaPluginHost.h"
#endif
#ifdef MOZ_WEBM
#include "WebMReader.h"
#endif
#ifdef MOZ_WMF
#include "WMFReader.h"
#endif

namespace mozilla {

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

private:
  // This monitor object is not really used to synchronize access to anything.
  // It's just there in order for us to be able to override
  // GetReentrantMonitor correctly.
  ReentrantMonitor mReentrantMonitor;
  nsCOMPtr<nsIThread> mDecodeThread;
  nsAutoPtr<MediaResource> mResource;
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
  CopyBuffer,
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

  void Decode();
  void AllocateBuffer();
  void CopyBuffer();
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
  case PhaseEnum::CopyBuffer:
    CopyBuffer();
    break;
  case PhaseEnum::Done:
    CallbackTheResult();
    break;
  }

  return NS_OK;
}

bool
MediaDecodeTask::CreateReader()
{
  MOZ_ASSERT(NS_IsMainThread());

  BufferMediaResource* resource =
    new BufferMediaResource(static_cast<uint8_t*> (mBuffer),
                            mLength, mPrincipal);

  MOZ_ASSERT(!mBufferDecoder);
  mBufferDecoder = new BufferDecoder(resource);

  // If you change this list to add support for new decoders, please consider
  // updating nsHTMLMediaElement::CreateDecoder as well.

#ifdef MOZ_GSTREAMER
  if (DecoderTraits::IsGStreamerSupportedType(mContentType)) {
    mDecoderReader = new GStreamerReader(mBufferDecoder);
  } else
#endif
#ifdef MOZ_RAW
  if (DecoderTraits::IsRawType(mContentType)) {
    mDecoderReader = new RawReader(mBufferDecoder);
  } else
#endif
#ifdef MOZ_OGG
  if (DecoderTraits::IsOggType(mContentType)) {
    mDecoderReader = new OggReader(mBufferDecoder);
  } else
#endif
#ifdef MOZ_WAVE
  if (DecoderTraits::IsWaveType(mContentType)) {
    mDecoderReader = new WaveReader(mBufferDecoder);
  } else
#endif
#ifdef MOZ_WIDGET_GONK
  if (DecoderTraits::IsOmxSupportedType(mContentType)) {
    mDecoderReader = new MediaOmxReader(mBufferDecoder);
  } else
#endif
#ifdef MOZ_MEDIA_PLUGINS
  if (MediaDecoder::IsMediaPluginsEnabled() &&
      GetMediaPluginHost()->FindDecoder(mContentType, nullptr)) {
    mDecoderReader = new MediaPluginReader(mBufferDecoder, mContentType);
  } else
#endif
#ifdef MOZ_WEBM
  if (DecoderTraits::IsWebMType(mContentType)) {
    mDecoderReader = new WebMReader(mBufferDecoder);
  } else
#endif
#ifdef MOZ_WMF
  if (DecoderTraits::IsWMFSupportedType(mContentType)) {
    mDecoderReader = new WMFReader(mBufferDecoder);
  } else
#endif
#ifdef MOZ_DASH
  // The DASH decoder is not supported.
#endif
  if (false) {} // dummy if to take care of the dangling else

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
MediaDecodeTask::Decode()
{
  MOZ_ASSERT(!NS_IsMainThread());

  mDecoderReader->OnDecodeThreadStart();

  mBufferDecoder->BeginDecoding(NS_GetCurrentThread());

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

  // Ask the main thread to allocate a large enough typed array to fit the data
  mDecodeJob.mFrames = frameCount;
  mDecodeJob.mChannels = channelCount;
  mDecodeJob.mSourceSampleRate = sampleRate;

  const uint32_t destSampleRate = mDecodeJob.mContext->SampleRate();

  mDecodeJob.mResampledFrames = mDecodeJob.mFrames;
  if (mDecodeJob.mSourceSampleRate != destSampleRate) {
    mDecodeJob.mResampledFrames = static_cast<uint32_t>(
        static_cast<uint64_t>(destSampleRate) *
        static_cast<uint64_t>(frameCount) /
        static_cast<uint64_t>(mDecodeJob.mSourceSampleRate)
      );
  }

  mPhase = PhaseEnum::AllocateBuffer;
  NS_DispatchToMainThread(this);
}

void
MediaDecodeTask::AllocateBuffer()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDecodeJob.AllocateBuffer()) {
    ReportFailureOnMainThread(WebAudioDecodeJob::UnknownError);
    return;
  }

  mPhase = PhaseEnum::CopyBuffer;
  mThreadPool->Dispatch(this, nsIThreadPool::DISPATCH_NORMAL);
}

void
MediaDecodeTask::CopyBuffer()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mDecodeJob.mOutput);
  MOZ_ASSERT(mDecodeJob.mChannels);
  MOZ_ASSERT(mDecoderReader);

  SpeexResamplerState* resampler = nullptr;

  MediaQueue<AudioData>& audioQueue = mDecoderReader->AudioQueue();

  const uint32_t destSampleRate = mDecodeJob.mContext->SampleRate();
  if (mDecodeJob.mSourceSampleRate != destSampleRate) {
    resampler = speex_resampler_init(mDecodeJob.mChannels,
                                     mDecodeJob.mSourceSampleRate,
                                     destSampleRate,
                                     SPEEX_RESAMPLER_QUALITY_DEFAULT, nullptr);
  }

  uint32_t framesCopied = 0;

  nsAutoPtr<AudioData> audioData;
  while ((audioData = audioQueue.PopFront())) {
    audioData->EnsureAudioBuffer(); // could lead to a copy :(
    AudioDataValue* bufferData = static_cast<AudioDataValue*>
      (audioData->mAudioBuffer->Data());

    AudioDataValue* resampledBuffer = bufferData;

    // We cannot use mDecodeJob.mResampledFrames here because we are dealing
    // with only part of the audio frames, not all of it.
    const uint32_t expectedOutSamples = static_cast<uint32_t>(
        static_cast<uint64_t>(destSampleRate) *
        static_cast<uint64_t>(audioData->mFrames) /
        static_cast<uint64_t>(mDecodeJob.mSourceSampleRate)
      );
    if (mDecodeJob.mSourceSampleRate != destSampleRate) {
      static const fallible_t fallible = fallible_t();
      resampledBuffer = new(fallible) AudioDataValue[mDecodeJob.mChannels * expectedOutSamples];

      if (!resampledBuffer) {
        // Out of memory!
        ReportFailureOnMainThread(WebAudioDecodeJob::UnknownError);
        return;
      }

      for (uint32_t i = 0; i < audioData->mChannels; ++i) {
        uint32_t inSamples = audioData->mFrames;
        uint32_t outSamples = expectedOutSamples;

#ifdef MOZ_SAMPLE_TYPE_S16
        speex_resampler_process_int(resampler, i, bufferData, &inSamples,
                                    &resampledBuffer[i * expectedOutSamples],
                                    &outSamples);
#else
        speex_resampler_process_float(resampler, i, bufferData, &inSamples,
                                      &resampledBuffer[i * expectedOutSamples],
                                      &outSamples);
#endif
      }
    }

    for (uint32_t i = 0; i < audioData->mChannels; ++i) {
      ConvertAudioSamples(&resampledBuffer[i * expectedOutSamples],
                          &mDecodeJob.mChannelBuffers[i].second[framesCopied],
                          expectedOutSamples);
    }
    framesCopied += expectedOutSamples;

    if (resampledBuffer != bufferData) {
      delete[] resampledBuffer;
    }
  }

  if (resampler) {
    speex_resampler_destroy(resampler);
  }

  mPhase = PhaseEnum::Done;
  NS_DispatchToMainThread(this);
}

void
MediaDecodeTask::CallbackTheResult()
{
  MOZ_ASSERT(NS_IsMainThread());

  Cleanup();

  // Now, we're ready to call the script back with the resulting buffer
  if (!mDecodeJob.FinalizeBufferData()) {
    ReportFailureOnMainThread(WebAudioDecodeJob::UnknownError);
  }

  mDecodeJob.OnSuccess(WebAudioDecodeJob::NoError);
}

bool
WebAudioDecodeJob::FinalizeBufferData()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOutput);
  MOZ_ASSERT(mChannels == mChannelBuffers.Length());

  JSContext* cx = GetJSContext();
  if (!cx) {
    return false;
  }

  for (uint32_t i = 0; i < mChannels; ++i) {
    mOutput->SetChannelDataFromArrayBufferContents(cx, i, mChannelBuffers[i].first);
  }

  return true;
}

JSContext*
WebAudioDecodeJob::GetJSContext() const
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobal =
    do_QueryInterface(mContext->GetParentObject());
  nsIScriptContext* scriptContext = scriptGlobal->GetContext();
  if (!scriptContext) {
    return nullptr;
  }
  return scriptContext->GetNativeContext();
}

bool
WebAudioDecodeJob::AllocateBuffer()
{
  MOZ_ASSERT(!mOutput);
  MOZ_ASSERT(NS_IsMainThread());

  // First, get a JSContext
  JSContext* cx = GetJSContext();
  if (!cx) {
    return false;
  }

  // Now create the AudioBuffer
  mOutput = new AudioBuffer(mContext, mResampledFrames, mContext->SampleRate());
  if (!mOutput->InitializeBuffers(mChannels, cx)) {
    return false;
  }

  if (!mChannelBuffers.SetCapacity(mChannels)) {
    return false;
  }
  for (uint32_t i = 0; i < mChannels; ++i) {
    JSObject* channelObj = mOutput->GetChannelData(i);
    JSObject* arrayBuffer = JS_GetArrayBufferViewBuffer(channelObj);
    void* contents;
    uint8_t* data;
    if (JS_FALSE == JS_StealArrayBufferContents(cx, arrayBuffer, &contents, &data)) {
      return false;
    }
    mChannelBuffers.AppendElement(
      std::make_pair(contents, reinterpret_cast<float*>(data)));
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
                                     const ArrayBuffer& aBuffer,
                                     AudioContext* aContext,
                                     DecodeSuccessCallback* aSuccessCallback,
                                     DecodeErrorCallback* aFailureCallback)
  : mContentType(aContentType)
  , mBuffer(aBuffer.Data())
  , mLength(aBuffer.Length())
  , mChannels(0)
  , mSourceSampleRate(0)
  , mFrames(0)
  , mResampledFrames(0)
  , mContext(aContext)
  , mSuccessCallback(aSuccessCallback)
  , mFailureCallback(aFailureCallback)
{
  MOZ_ASSERT(aContext);
  MOZ_ASSERT(aSuccessCallback);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(WebAudioDecodeJob);
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
  ErrorResult rv;
  mSuccessCallback->Call(*mOutput, rv);

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
  ErrorResult rv;
  mFailureCallback->Call(rv);

  mContext->RemoveFromDecodeQueue(this);
}

}

