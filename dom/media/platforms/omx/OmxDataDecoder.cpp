/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxDataDecoder.h"
#include "OMX_Types.h"
#include "OMX_Component.h"
#include "OMX_Audio.h"

extern mozilla::LogModule* GetPDMLog();

#ifdef LOG
#undef LOG
#endif

#define LOG(arg, ...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, ("OmxDataDecoder::%s: " arg, __func__, ##__VA_ARGS__))

#define CHECK_OMX_ERR(err)     \
  if (err != OMX_ErrorNone) {  \
    NotifyError(err, __func__);\
    return;                    \
  }                            \


namespace mozilla {

static const char*
StateTypeToStr(OMX_STATETYPE aType)
{
  MOZ_ASSERT(aType == OMX_StateLoaded ||
             aType == OMX_StateIdle ||
             aType == OMX_StateExecuting ||
             aType == OMX_StatePause ||
             aType == OMX_StateWaitForResources ||
             aType == OMX_StateInvalid);

  switch (aType) {
    case OMX_StateLoaded:
      return "OMX_StateLoaded";
    case OMX_StateIdle:
      return "OMX_StateIdle";
    case OMX_StateExecuting:
      return "OMX_StateExecuting";
    case OMX_StatePause:
      return "OMX_StatePause";
    case OMX_StateWaitForResources:
      return "OMX_StateWaitForResources";
    case OMX_StateInvalid:
      return "OMX_StateInvalid";
    default:
      return "Unknown";
  }
}

// There should be 2 ports and port number start from 0.
void GetPortIndex(nsTArray<uint32_t>& aPortIndex) {
  aPortIndex.AppendElement(0);
  aPortIndex.AppendElement(1);
}

OmxDataDecoder::OmxDataDecoder(const TrackInfo& aTrackInfo,
                               MediaDataDecoderCallback* aCallback)
  : mMonitor("OmxDataDecoder")
  , mOmxTaskQueue(CreateMediaDecodeTaskQueue())
  , mWatchManager(this, mOmxTaskQueue)
  , mOmxState(OMX_STATETYPE::OMX_StateInvalid, "OmxDataDecoder::mOmxState")
  , mTrackInfo(aTrackInfo.Clone())
  , mFlushing(false)
  , mShutdown(false)
  , mCheckingInputExhausted(false)
  , mPortSettingsChanged(-1, "OmxDataDecoder::mPortSettingsChanged")
  , mAudioCompactor(mAudioQueue)
  , mCallback(aCallback)
{
  LOG("(%p)", this);
  mOmxLayer = new OmxPromiseLayer(mOmxTaskQueue, this);

  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(this, &OmxDataDecoder::InitializationTask);
  mOmxTaskQueue->Dispatch(r.forget());
}

OmxDataDecoder::~OmxDataDecoder()
{
  LOG("(%p)", this);
  mWatchManager.Shutdown();
  mOmxTaskQueue->AwaitShutdownAndIdle();
}

void
OmxDataDecoder::InitializationTask()
{
  mWatchManager.Watch(mOmxState, &OmxDataDecoder::OmxStateRunner);
  mWatchManager.Watch(mPortSettingsChanged, &OmxDataDecoder::PortSettingsChanged);
}

void
OmxDataDecoder::EndOfStream()
{
  LOG("(%p)", this);
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  RefPtr<OmxDataDecoder> self = this;
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction([self] () {
      self->mCallback->DrainComplete();
    });
  mReaderTaskQueue->Dispatch(r.forget());
}

RefPtr<MediaDataDecoder::InitPromise>
OmxDataDecoder::Init()
{
  LOG("(%p)", this);
  mReaderTaskQueue = AbstractThread::GetCurrent()->AsTaskQueue();
  MOZ_ASSERT(mReaderTaskQueue);

  RefPtr<InitPromise> p = mInitPromise.Ensure(__func__);
  RefPtr<OmxDataDecoder> self = this;

  // TODO: it needs to get permission from resource manager before allocating
  //       Omx component.
  InvokeAsync(mOmxTaskQueue, mOmxLayer.get(), __func__, &OmxPromiseLayer::Init,
              mOmxTaskQueue, mTrackInfo.get())
    ->Then(mReaderTaskQueue, __func__,
      [self] () {
        // Omx state should be OMX_StateIdle.
        nsCOMPtr<nsIRunnable> r =
          NS_NewRunnableFunction([self] () {
            self->mOmxState = self->mOmxLayer->GetState();
            MOZ_ASSERT(self->mOmxState != OMX_StateIdle);
          });
        self->mOmxTaskQueue->Dispatch(r.forget());
      },
      [self] () {
        self->RejectInitPromise(DecoderFailureReason::INIT_ERROR, __func__);
      });

  return p;
}

nsresult
OmxDataDecoder::Input(MediaRawData* aSample)
{
  LOG("(%p) sample %p", this, aSample);
  MOZ_ASSERT(mInitPromise.IsEmpty());

  RefPtr<OmxDataDecoder> self = this;
  RefPtr<MediaRawData> sample = aSample;

  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction([self, sample] () {
      self->mMediaRawDatas.AppendElement(sample);

      // Start to fill/empty buffers.
      if (self->mOmxState == OMX_StateIdle ||
          self->mOmxState == OMX_StateExecuting) {
        self->FillAndEmptyBuffers();
      }
    });
  mOmxTaskQueue->Dispatch(r.forget());

  return NS_OK;
}

nsresult
OmxDataDecoder::Flush()
{
  LOG("(%p)", this);

  mFlushing = true;

  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(this, &OmxDataDecoder::DoFlush);
  mOmxTaskQueue->Dispatch(r.forget());

  // According to the definition of Flush() in PDM:
  // "the decoder must be ready to accept new input for decoding".
  // So it needs to wait for the Omx to complete the flush command.
  MonitorAutoLock lock(mMonitor);
  while (mFlushing) {
    lock.Wait();
  }

  return NS_OK;
}

nsresult
OmxDataDecoder::Drain()
{
  LOG("(%p)", this);

  // TODO: For video decoding, it needs to copy the latest video frame to yuv
  //       and output to layer again, because all video buffers will be released
  //       later.
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(this, &OmxDataDecoder::SendEosBuffer);
  mOmxTaskQueue->Dispatch(r.forget());

  return NS_OK;
}

nsresult
OmxDataDecoder::Shutdown()
{
  LOG("(%p)", this);

  mShutdown = true;

  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableMethod(this, &OmxDataDecoder::DoAsyncShutdown);
  mOmxTaskQueue->Dispatch(r.forget());

  return NS_OK;
}

void
OmxDataDecoder::DoAsyncShutdown()
{
  LOG("(%p)", this);
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(mFlushing);

  mWatchManager.Unwatch(mOmxState, &OmxDataDecoder::OmxStateRunner);
  mWatchManager.Unwatch(mPortSettingsChanged, &OmxDataDecoder::PortSettingsChanged);

  // Do flush so all port can be returned to client.
  RefPtr<OmxDataDecoder> self = this;
  mOmxLayer->SendCommand(OMX_CommandFlush, OMX_ALL, nullptr)
    ->Then(mOmxTaskQueue, __func__,
           [self] () -> RefPtr<OmxCommandPromise> {
             LOG("DoAsyncShutdown: flush complete, collecting buffers...");
             self->CollectBufferPromises(OMX_DirMax)
               ->Then(self->mOmxTaskQueue, __func__,
                   [self] () {
                     LOG("DoAsyncShutdown: releasing all buffers.");
                     self->ReleaseBuffers(OMX_DirInput);
                     self->ReleaseBuffers(OMX_DirOutput);
                   },
                   [self] () {
                     self->mOmxLayer->Shutdown();
                   });

             return self->mOmxLayer->SendCommand(OMX_CommandStateSet, OMX_StateIdle, nullptr);
           },
           [self] () {
             self->mOmxLayer->Shutdown();
           })
    ->CompletionPromise()
    ->Then(mOmxTaskQueue, __func__,
           [self] () -> RefPtr<OmxCommandPromise> {
             LOG("DoAsyncShutdown: OMX_StateIdle");
             return self->mOmxLayer->SendCommand(OMX_CommandStateSet, OMX_StateLoaded, nullptr);
           },
           [self] () {
             self->mOmxLayer->Shutdown();
           })
    ->CompletionPromise()
    ->Then(mOmxTaskQueue, __func__,
           [self] () {
             LOG("DoAsyncShutdown: OMX_StateLoaded, it is safe to shutdown omx");
             self->mOmxLayer->Shutdown();
           },
           [self] () {
             self->mOmxLayer->Shutdown();
           });
}

void
OmxDataDecoder::CheckIfInputExhausted()
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(!mCheckingInputExhausted);

  mCheckingInputExhausted = false;

  if (mMediaRawDatas.Length()) {
    return;
  }

  // When all input buffers are not in omx component, it means all samples have
  // been fed into OMX component.
  for (auto buf : mInPortBuffers) {
    if (buf->mStatus == BufferData::BufferStatus::OMX_COMPONENT) {
      return;
    }
  }

  // When all output buffers are held by component, it means client is waiting for output.
  for (auto buf : mOutPortBuffers) {
    if (buf->mStatus != BufferData::BufferStatus::OMX_COMPONENT) {
      return;
    }
  }

  LOG("Call InputExhausted()");
  mCallback->InputExhausted();
}

void
OmxDataDecoder::OutputAudio(BufferData* aBufferData)
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  OMX_BUFFERHEADERTYPE* buf = aBufferData->mBuffer;
  AudioInfo* info = mTrackInfo->GetAsAudioInfo();
  if (buf->nFilledLen) {
    uint64_t offset = 0;
    uint32_t frames = buf->nFilledLen / (2 * info->mChannels);
    if (aBufferData->mRawData) {
      offset = aBufferData->mRawData->mOffset;
    }
    typedef AudioCompactor::NativeCopy OmxCopy;
    mAudioCompactor.Push(offset,
                         buf->nTimeStamp,
                         info->mRate,
                         frames,
                         info->mChannels,
                         OmxCopy(buf->pBuffer + buf->nOffset,
                                 buf->nFilledLen,
                                 info->mChannels));
    RefPtr<AudioData> audio = mAudioQueue.PopFront();
    mCallback->Output(audio);
  }
  aBufferData->mStatus = BufferData::BufferStatus::FREE;
}

void
OmxDataDecoder::FillBufferDone(BufferData* aData)
{
  MOZ_ASSERT(!aData || aData->mStatus == BufferData::BufferStatus::OMX_CLIENT);

  if (mTrackInfo->IsAudio()) {
    OutputAudio(aData);
  } else {
    MOZ_ASSERT(0);
  }

  if (aData->mBuffer->nFlags & OMX_BUFFERFLAG_EOS) {
    EndOfStream();
  } else {
    FillAndEmptyBuffers();

    // If the latest decoded sample's MediaRawData is also the latest input
    // sample, it means there is no input data in queue and component, calling
    // CheckIfInputExhausted().
    if (aData->mRawData == mLatestInputRawData && !mCheckingInputExhausted) {
      mCheckingInputExhausted = true;
      nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableMethod(this, &OmxDataDecoder::CheckIfInputExhausted);
      mOmxTaskQueue->Dispatch(r.forget());
    }
  }
}

void
OmxDataDecoder::FillBufferFailure(OmxBufferFailureHolder aFailureHolder)
{
  NotifyError(aFailureHolder.mError, __func__);
}

void
OmxDataDecoder::EmptyBufferDone(BufferData* aData)
{
  MOZ_ASSERT(!aData || aData->mStatus == BufferData::BufferStatus::OMX_CLIENT);

  // Nothing to do when status of input buffer is OMX_CLIENT.
  aData->mStatus = BufferData::BufferStatus::FREE;
  FillAndEmptyBuffers();
}

void
OmxDataDecoder::EmptyBufferFailure(OmxBufferFailureHolder aFailureHolder)
{
  NotifyError(aFailureHolder.mError, __func__);
}

void
OmxDataDecoder::NotifyError(OMX_ERRORTYPE aError, const char* aLine)
{
  LOG("NotifyError %d at %s", aError, aLine);
  mCallback->Error();
}

void
OmxDataDecoder::FillAndEmptyBuffers()
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(mOmxState == OMX_StateExecuting);

  // During the port setting changed, it is forbided to do any buffer operations.
  if (mPortSettingsChanged != -1 || mShutdown) {
    return;
  }

  if (mFlushing) {
    return;
  }

  // Trigger input port.
  while (!!mMediaRawDatas.Length()) {
    // input buffer must be usedi by component if there is data available.
    RefPtr<BufferData> inbuf = FindAvailableBuffer(OMX_DirInput);
    if (!inbuf) {
      LOG("no input buffer!");
      break;
    }

    RefPtr<MediaRawData> data = mMediaRawDatas[0];
    memcpy(inbuf->mBuffer->pBuffer, data->Data(), data->Size());
    inbuf->mBuffer->nFilledLen = data->Size();
    inbuf->mBuffer->nOffset = 0;
    // TODO: the frame size could larger than buffer size in video case.
    inbuf->mBuffer->nFlags = inbuf->mBuffer->nAllocLen > data->Size() ?
                             OMX_BUFFERFLAG_ENDOFFRAME : 0;
    inbuf->mBuffer->nTimeStamp = data->mTime;
    if (data->Size()) {
      inbuf->mRawData = mMediaRawDatas[0];
    } else {
       LOG("send EOS buffer");
      inbuf->mBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
    }

    LOG("feed sample %p to omx component, len %d, flag %X", data.get(),
        inbuf->mBuffer->nFilledLen, inbuf->mBuffer->nFlags);
    mOmxLayer->EmptyBuffer(inbuf)->Then(mOmxTaskQueue, __func__, this,
                                        &OmxDataDecoder::EmptyBufferDone,
                                        &OmxDataDecoder::EmptyBufferFailure);
    mLatestInputRawData.swap(mMediaRawDatas[0]);
    mMediaRawDatas.RemoveElementAt(0);
  }

  // Trigger output port.
  while (true) {
    RefPtr<BufferData> outbuf = FindAvailableBuffer(OMX_DirOutput);
    if (!outbuf) {
      break;
    }

    mOmxLayer->FillBuffer(outbuf)->Then(mOmxTaskQueue, __func__, this,
                                        &OmxDataDecoder::FillBufferDone,
                                        &OmxDataDecoder::FillBufferFailure);
  }
}

OmxPromiseLayer::BufferData*
OmxDataDecoder::FindAvailableBuffer(OMX_DIRTYPE aType)
{
  BUFFERLIST* buffers = GetBuffers(aType);

  for (uint32_t i = 0; i < buffers->Length(); i++) {
    BufferData* buf = buffers->ElementAt(i);
    if (buf->mStatus == BufferData::BufferStatus::FREE) {
      return buf;
    }
  }

  return nullptr;
}

nsresult
OmxDataDecoder::AllocateBuffers(OMX_DIRTYPE aType)
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  return mOmxLayer->AllocateOmxBuffer(aType, GetBuffers(aType));
}

nsresult
OmxDataDecoder::ReleaseBuffers(OMX_DIRTYPE aType)
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  return mOmxLayer->ReleaseOmxBuffer(aType, GetBuffers(aType));
}

nsTArray<RefPtr<OmxPromiseLayer::BufferData>>*
OmxDataDecoder::GetBuffers(OMX_DIRTYPE aType)
{
  MOZ_ASSERT(aType == OMX_DIRTYPE::OMX_DirInput ||
             aType == OMX_DIRTYPE::OMX_DirOutput);

  if (aType == OMX_DIRTYPE::OMX_DirInput) {
    return &mInPortBuffers;
  }
  return &mOutPortBuffers;
}

void
OmxDataDecoder::ResolveInitPromise(const char* aMethodName)
{
  LOG("Resolved InitPromise");
  RefPtr<OmxDataDecoder> self = this;
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction([self, aMethodName] () {
      MOZ_ASSERT(self->mReaderTaskQueue->IsCurrentThreadIn());
      self->mInitPromise.ResolveIfExists(self->mTrackInfo->GetType(), aMethodName);
    });
  mReaderTaskQueue->Dispatch(r.forget());
}

void
OmxDataDecoder::RejectInitPromise(DecoderFailureReason aReason, const char* aMethodName)
{
  RefPtr<OmxDataDecoder> self = this;
  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction([self, aReason, aMethodName] () {
      MOZ_ASSERT(self->mReaderTaskQueue->IsCurrentThreadIn());
      self->mInitPromise.RejectIfExists(aReason, aMethodName);
    });
  mReaderTaskQueue->Dispatch(r.forget());
}

void
OmxDataDecoder::OmxStateRunner()
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  LOG("OMX state: %s", StateTypeToStr(mOmxState));

  // TODO: maybe it'd be better to use promise CompletionPromise() to replace
  //       this state machine.
  if (mOmxState == OMX_StateLoaded) {
    // Config codec parameters by minetype.
    if (mTrackInfo->IsAudio()) {
      ConfigAudioCodec();
    }

    // Send OpenMax state commane to OMX_StateIdle.
    RefPtr<OmxDataDecoder> self = this;
    mOmxLayer->SendCommand(OMX_CommandStateSet, OMX_StateIdle, nullptr)
      ->Then(mOmxTaskQueue, __func__,
             [self] () {
               // Current state should be OMX_StateIdle.
               self->mOmxState = self->mOmxLayer->GetState();
               MOZ_ASSERT(self->mOmxState == OMX_StateIdle);
             },
             [self] () {
               self->RejectInitPromise(DecoderFailureReason::INIT_ERROR, __func__);
             });

    // Allocate input and output buffers.
    OMX_DIRTYPE types[] = {OMX_DIRTYPE::OMX_DirInput, OMX_DIRTYPE::OMX_DirOutput};
    for(const auto id : types) {
      if (NS_FAILED(AllocateBuffers(id))) {
        LOG("Failed to allocate buffer on port %d", id);
        RejectInitPromise(DecoderFailureReason::INIT_ERROR, __func__);
        break;
      }
    }
  } else if (mOmxState == OMX_StateIdle) {
    RefPtr<OmxDataDecoder> self = this;
    mOmxLayer->SendCommand(OMX_CommandStateSet, OMX_StateExecuting, nullptr)
      ->Then(mOmxTaskQueue, __func__,
             [self] () {
               self->mOmxState = self->mOmxLayer->GetState();
               MOZ_ASSERT(self->mOmxState == OMX_StateExecuting);

               self->ResolveInitPromise(__func__);
             },
             [self] () {
               self->RejectInitPromise(DecoderFailureReason::INIT_ERROR, __func__);
             });
  } else if (mOmxState == OMX_StateExecuting) {
    // Config codec once it gets OMX_StateExecuting state.
    FillCodecConfigDataToOmx();
  } else {
    MOZ_ASSERT(0);
  }
}

void
OmxDataDecoder::ConfigAudioCodec()
{
  const AudioInfo* audioInfo = mTrackInfo->GetAsAudioInfo();
  OMX_ERRORTYPE err;

  // TODO: it needs to handle other formats like mp3, amr-nb...etc.
  if (audioInfo->mMimeType.EqualsLiteral("audio/mp4a-latm")) {
    OMX_AUDIO_PARAM_AACPROFILETYPE aac_profile;
    InitOmxParameter(&aac_profile);
    err = mOmxLayer->GetParameter(OMX_IndexParamAudioAac, &aac_profile, sizeof(aac_profile));
    CHECK_OMX_ERR(err);
    aac_profile.nSampleRate = audioInfo->mRate;
    aac_profile.nChannels = audioInfo->mChannels;
    aac_profile.eAACProfile = (OMX_AUDIO_AACPROFILETYPE)audioInfo->mProfile;
    err = mOmxLayer->SetParameter(OMX_IndexParamAudioAac, &aac_profile, sizeof(aac_profile));
    CHECK_OMX_ERR(err);
    LOG("Config OMX_IndexParamAudioAac, channel %d, sample rate %d, profile %d",
        audioInfo->mChannels, audioInfo->mRate, audioInfo->mProfile);
  }
}

void
OmxDataDecoder::FillCodecConfigDataToOmx()
{
  // Codec config data should be the first sample running on Omx TaskQueue.
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(!mMediaRawDatas.Length());
  MOZ_ASSERT(mOmxState == OMX_StateIdle || mOmxState == OMX_StateExecuting);


  RefPtr<BufferData> inbuf = FindAvailableBuffer(OMX_DirInput);
  if (mTrackInfo->IsAudio()) {
    AudioInfo* audio_info = mTrackInfo->GetAsAudioInfo();
    memcpy(inbuf->mBuffer->pBuffer,
           audio_info->mCodecSpecificConfig->Elements(),
           audio_info->mCodecSpecificConfig->Length());
    inbuf->mBuffer->nFilledLen = audio_info->mCodecSpecificConfig->Length();
    inbuf->mBuffer->nOffset = 0;
    inbuf->mBuffer->nFlags = (OMX_BUFFERFLAG_ENDOFFRAME | OMX_BUFFERFLAG_CODECCONFIG);
  } else {
    MOZ_ASSERT(0);
  }

  LOG("Feed codec configure data to OMX component");
  mOmxLayer->EmptyBuffer(inbuf)->Then(mOmxTaskQueue, __func__, this,
                                      &OmxDataDecoder::EmptyBufferDone,
                                      &OmxDataDecoder::EmptyBufferFailure);
}

bool
OmxDataDecoder::Event(OMX_EVENTTYPE aEvent, OMX_U32 aData1, OMX_U32 aData2)
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  if (mOmxLayer->Event(aEvent, aData1, aData2)) {
    return true;
  }

  switch (aEvent) {
    case OMX_EventPortSettingsChanged:
    {
      // According to spec: "To prevent the loss of any input data, the
      // component issuing the OMX_EventPortSettingsChanged event on its input
      // port should buffer all input port data that arrives between the
      // emission of the OMX_EventPortSettingsChanged event and the arrival of
      // the command to disable the input port."
      //
      // So client needs to disable port and reallocate buffers.
      MOZ_ASSERT(mPortSettingsChanged == -1);
      mPortSettingsChanged = aData1;
      LOG("Got OMX_EventPortSettingsChanged event");
      break;
    }
    default:
    {
      LOG("WARNING: got none handle event: %d, aData1: %d, aData2: %d",
          aEvent, aData1, aData2);
      return false;
    }
  }

  return true;
}

template<class T> void
OmxDataDecoder::InitOmxParameter(T* aParam)
{
  PodZero(aParam);
  aParam->nSize = sizeof(T);
  aParam->nVersion.s.nVersionMajor = 1;
}

bool
OmxDataDecoder::BuffersCanBeReleased(OMX_DIRTYPE aType)
{
  BUFFERLIST* buffers = GetBuffers(aType);
  uint32_t len = buffers->Length();
  for (uint32_t i = 0; i < len; i++) {
    BufferData::BufferStatus buf_status = buffers->ElementAt(i)->mStatus;
    if (buf_status == BufferData::BufferStatus::OMX_COMPONENT ||
        buf_status == BufferData::BufferStatus::OMX_CLIENT_OUTPUT) {
      return false;
    }
  }
  return true;
}

OMX_DIRTYPE
OmxDataDecoder::GetPortDirection(uint32_t aPortIndex)
{
  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOmxParameter(&def);
  def.nPortIndex = mPortSettingsChanged;

  OMX_ERRORTYPE err = mOmxLayer->GetParameter(OMX_IndexParamPortDefinition,
                                              &def,
                                              sizeof(def));
  if (err != OMX_ErrorNone) {
    return OMX_DirMax;
  }
  return def.eDir;
}

RefPtr<OmxPromiseLayer::OmxBufferPromise::AllPromiseType>
OmxDataDecoder::CollectBufferPromises(OMX_DIRTYPE aType)
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  nsTArray<RefPtr<OmxBufferPromise>> promises;
  OMX_DIRTYPE types[] = {OMX_DIRTYPE::OMX_DirInput, OMX_DIRTYPE::OMX_DirOutput};
  for (const auto type : types) {
    if ((aType == type) || (aType == OMX_DirMax)) {
      // find the buffer which has promise.
      BUFFERLIST* buffers = GetBuffers(type);

      for (uint32_t i = 0; i < buffers->Length(); i++) {
        BufferData* buf = buffers->ElementAt(i);
        if (!buf->mPromise.IsEmpty()) {
          // OmxBufferPromise is not exclusive, it can be multiple "Then"s, so it
          // is safe to call "Ensure" here.
          promises.AppendElement(buf->mPromise.Ensure(__func__));
        }
      }
    }
  }

  LOG("CollectBufferPromises: type %d, total %d promiese", aType, promises.Length());
  if (promises.Length()) {
    return OmxBufferPromise::All(mOmxTaskQueue, promises);
  }

  nsTArray<BufferData*> headers;
  return OmxBufferPromise::AllPromiseType::CreateAndResolve(headers, __func__);
}

void
OmxDataDecoder::PortSettingsChanged()
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  if (mPortSettingsChanged == -1 || mOmxState == OMX_STATETYPE::OMX_StateInvalid) {
    return;
  }

  // The PortSettingsChanged algorithm:
  //
  //   1. disable port.
  //   2. wait for port buffers return to client and then release these buffers.
  //   3. enable port.
  //   4. allocate port buffers.
  //

  // Disable port. Get port definition if the target port is enable.
  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOmxParameter(&def);
  def.nPortIndex = mPortSettingsChanged;

  OMX_ERRORTYPE err = mOmxLayer->GetParameter(OMX_IndexParamPortDefinition,
                                              &def,
                                              sizeof(def));
  CHECK_OMX_ERR(err);

  RefPtr<OmxDataDecoder> self = this;
  if (def.bEnabled) {
    // 1. disable port.
    LOG("PortSettingsChanged: disable port %d", def.nPortIndex);
    mOmxLayer->SendCommand(OMX_CommandPortDisable, mPortSettingsChanged, nullptr)
      ->Then(mOmxTaskQueue, __func__,
             [self, def] () -> RefPtr<OmxCommandPromise> {
               // 3. enable port.
               // Send enable port command.
               RefPtr<OmxCommandPromise> p =
                 self->mOmxLayer->SendCommand(OMX_CommandPortEnable,
                                              self->mPortSettingsChanged,
                                              nullptr);

               // 4. allocate port buffers.
               // Allocate new port buffers.
               nsresult rv = self->AllocateBuffers(def.eDir);
               if (NS_FAILED(rv)) {
                 self->NotifyError(OMX_ErrorUndefined, __func__);
               }

               return p;
             },
             [self] () {
               self->NotifyError(OMX_ErrorUndefined, __func__);
             })
      ->CompletionPromise()
      ->Then(mOmxTaskQueue, __func__,
             [self] () {
               LOG("PortSettingsChanged: port settings changed complete");
               // finish port setting changed.
               self->mPortSettingsChanged = -1;
               self->FillAndEmptyBuffers();
             },
             [self] () {
               self->NotifyError(OMX_ErrorUndefined, __func__);
             });

    // 2. wait for port buffers return to client and then release these buffers.
    //
    // Port buffers will be returned to client soon once OMX_CommandPortDisable
    // command is sent. Then releasing these buffers.
    CollectBufferPromises(def.eDir)
      ->Then(mOmxTaskQueue, __func__,
          [self, def] () {
            MOZ_ASSERT(self->BuffersCanBeReleased(def.eDir));
            nsresult rv = self->ReleaseBuffers(def.eDir);
            if (NS_FAILED(rv)) {
              MOZ_RELEASE_ASSERT(0);
              self->NotifyError(OMX_ErrorUndefined, __func__);
            }
          },
          [self] () {
            self->NotifyError(OMX_ErrorUndefined, __func__);
          });
  }
}

void
OmxDataDecoder::SendEosBuffer()
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  // There is no 'Drain' API in OpenMax, so it needs to wait for output sample
  // with EOS flag. However, MediaRawData doesn't provide EOS information,
  // so here it generates an empty BufferData with eos OMX_BUFFERFLAG_EOS in queue.
  // This behaviour should be compliant with spec, I think...
  RefPtr<MediaRawData> eos_data = new MediaRawData();
  mMediaRawDatas.AppendElement(eos_data);
  FillAndEmptyBuffers();
}

void
OmxDataDecoder::DoFlush()
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  // 1. Call OMX command OMX_CommandFlush in Omx TaskQueue.
  // 2. Remove all elements in mMediaRawDatas when flush is completed.
  RefPtr<OmxDataDecoder> self = this;
  mOmxLayer->SendCommand(OMX_CommandFlush, OMX_ALL, nullptr)
    ->Then(mOmxTaskQueue, __func__, this,
           &OmxDataDecoder::FlushComplete,
           &OmxDataDecoder::FlushFailure);
}

void
OmxDataDecoder::FlushComplete(OMX_COMMANDTYPE aCommandType)
{
  mMediaRawDatas.Clear();
  mFlushing = false;

  MonitorAutoLock lock(mMonitor);
  mMonitor.Notify();
  LOG("Flush complete");
}

void OmxDataDecoder::FlushFailure(OmxCommandFailureHolder aFailureHolder)
{
  NotifyError(OMX_ErrorUndefined, __func__);
  mFlushing = false;

  MonitorAutoLock lock(mMonitor);
  mMonitor.Notify();
}

}
