/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxDataDecoder.h"

#include "OMX_Audio.h"
#include "OMX_Component.h"
#include "OMX_Types.h"

#include "OmxPlatformLayer.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/SizePrintfMacros.h"

#ifdef LOG
#undef LOG
#undef LOGL
#endif

#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("OmxDataDecoder(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

#define LOGL(arg, ...)                                                     \
  {                                                                        \
    void* p = self;                                              \
    MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug,                         \
            ("OmxDataDecoder(%p)::%s: " arg, p, __func__, ##__VA_ARGS__)); \
  }

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

// A helper class to retrieve AudioData or VideoData.
class MediaDataHelper {
protected:
  virtual ~MediaDataHelper() {}

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataHelper)

  MediaDataHelper(const TrackInfo* aTrackInfo,
                  layers::ImageContainer* aImageContainer,
                  OmxPromiseLayer* aOmxLayer);

  already_AddRefed<MediaData> GetMediaData(BufferData* aBufferData, bool& aPlatformDepenentData);

protected:
  already_AddRefed<AudioData> CreateAudioData(BufferData* aBufferData);

  already_AddRefed<VideoData> CreateYUV420VideoData(BufferData* aBufferData);

  const TrackInfo* mTrackInfo;

  OMX_PARAM_PORTDEFINITIONTYPE mOutputPortDef;

  // audio output
  MediaQueue<AudioData> mAudioQueue;

  AudioCompactor mAudioCompactor;

  // video output
  RefPtr<layers::ImageContainer> mImageContainer;
};

OmxDataDecoder::OmxDataDecoder(const TrackInfo& aTrackInfo,
                               layers::ImageContainer* aImageContainer)
  : mOmxTaskQueue(CreateMediaDecodeTaskQueue("OmxDataDecoder::mOmxTaskQueue"))
  , mImageContainer(aImageContainer)
  , mWatchManager(this, mOmxTaskQueue)
  , mOmxState(OMX_STATETYPE::OMX_StateInvalid, "OmxDataDecoder::mOmxState")
  , mTrackInfo(aTrackInfo.Clone())
  , mFlushing(false)
  , mShuttingDown(false)
  , mCheckingInputExhausted(false)
  , mPortSettingsChanged(-1, "OmxDataDecoder::mPortSettingsChanged")
{
  LOG("");
  mOmxLayer = new OmxPromiseLayer(mOmxTaskQueue, this, aImageContainer);
}

OmxDataDecoder::~OmxDataDecoder()
{
  LOG("");
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
  LOG("");
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  RefPtr<OmxDataDecoder> self = this;
  mOmxLayer->SendCommand(OMX_CommandFlush, OMX_ALL, nullptr)
    ->Then(mOmxTaskQueue, __func__,
        [self, this] () {
          mDrainPromise.ResolveIfExists(mDecodedData, __func__);
            mDecodedData.Clear();
        },
        [self, this] () {
          mDrainPromise.ResolveIfExists(mDecodedData, __func__);
          mDecodedData.Clear();
        });
}

RefPtr<MediaDataDecoder::InitPromise>
OmxDataDecoder::Init()
{
  LOG("");

  RefPtr<OmxDataDecoder> self = this;
  return InvokeAsync(mOmxTaskQueue, __func__, [self, this]() {
    InitializationTask();

    RefPtr<InitPromise> p = mInitPromise.Ensure(__func__);
    mOmxLayer->Init(mTrackInfo.get())
      ->Then(mOmxTaskQueue, __func__,
             [self, this]() {
               // Omx state should be OMX_StateIdle.
               mOmxState = mOmxLayer->GetState();
               MOZ_ASSERT(mOmxState != OMX_StateIdle);
             },
             [self, this]() {
               RejectInitPromise(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
             });
    return p;
  });
}

RefPtr<MediaDataDecoder::DecodePromise>
OmxDataDecoder::Decode(MediaRawData* aSample)
{
  LOG("sample %p", aSample);
  MOZ_ASSERT(mInitPromise.IsEmpty());

  RefPtr<OmxDataDecoder> self = this;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mOmxTaskQueue, __func__, [self, this, sample]() {
    RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);
    mMediaRawDatas.AppendElement(Move(sample));

    // Start to fill/empty buffers.
    if (mOmxState == OMX_StateIdle ||
        mOmxState == OMX_StateExecuting) {
      FillAndEmptyBuffers();
    }
    return p;
  });
}

RefPtr<MediaDataDecoder::FlushPromise>
OmxDataDecoder::Flush()
{
  LOG("");

  mFlushing = true;

  return InvokeAsync(mOmxTaskQueue, this, __func__, &OmxDataDecoder::DoFlush);
}

RefPtr<MediaDataDecoder::DecodePromise>
OmxDataDecoder::Drain()
{
  LOG("");

  RefPtr<OmxDataDecoder> self = this;
  return InvokeAsync(mOmxTaskQueue, __func__, [self, this]() {
    RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);
    SendEosBuffer();
    return p;
  });
}

RefPtr<ShutdownPromise>
OmxDataDecoder::Shutdown()
{
  LOG("");

  mShuttingDown = true;

  return InvokeAsync(mOmxTaskQueue, this, __func__,
                     &OmxDataDecoder::DoAsyncShutdown);
}

RefPtr<ShutdownPromise>
OmxDataDecoder::DoAsyncShutdown()
{
  LOG("");
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(!mFlushing);

  mWatchManager.Unwatch(mOmxState, &OmxDataDecoder::OmxStateRunner);
  mWatchManager.Unwatch(mPortSettingsChanged, &OmxDataDecoder::PortSettingsChanged);

  // Flush to all ports, so all buffers can be returned from component.
  RefPtr<OmxDataDecoder> self = this;
  mOmxLayer->SendCommand(OMX_CommandFlush, OMX_ALL, nullptr)
    ->Then(mOmxTaskQueue, __func__,
           [self] () -> RefPtr<OmxCommandPromise> {
             LOGL("DoAsyncShutdown: flush complete");
             return self->mOmxLayer->SendCommand(OMX_CommandStateSet, OMX_StateIdle, nullptr);
           },
           [self] (const OmxCommandFailureHolder& aError) {
             self->mOmxLayer->Shutdown();
             return OmxCommandPromise::CreateAndReject(aError, __func__);
           })
    ->Then(mOmxTaskQueue, __func__,
           [self] () -> RefPtr<OmxCommandPromise> {
             RefPtr<OmxCommandPromise> p =
               self->mOmxLayer->SendCommand(OMX_CommandStateSet, OMX_StateLoaded, nullptr);

             // According to spec 3.1.1.2.2.1:
             // OMX_StateLoaded needs to be sent before releasing buffers.
             // And state transition from OMX_StateIdle to OMX_StateLoaded
             // is completed when all of the buffers have been removed
             // from the component.
             // Here the buffer promises are not resolved due to displaying
             // in layer, it needs to wait before the layer returns the
             // buffers.
             LOGL("DoAsyncShutdown: releasing buffers...");
             self->ReleaseBuffers(OMX_DirInput);
             self->ReleaseBuffers(OMX_DirOutput);

             return p;
           },
           [self] (const OmxCommandFailureHolder& aError) {
             self->mOmxLayer->Shutdown();
             return OmxCommandPromise::CreateAndReject(aError, __func__);
           })
    ->Then(mOmxTaskQueue, __func__,
           [self] () {
             LOGL("DoAsyncShutdown: OMX_StateLoaded, it is safe to shutdown omx");
             self->mOmxLayer->Shutdown();
             self->mWatchManager.Shutdown();
             self->mOmxLayer = nullptr;
             self->mMediaDataHelper = nullptr;

             self->mShuttingDown = false;
             self->mOmxTaskQueue->BeginShutdown();
             self->mOmxTaskQueue->AwaitShutdownAndIdle();
             self->mShutdownPromise.Resolve(true, __func__);
           },
           [self] () {
             self->mOmxLayer->Shutdown();
             self->mWatchManager.Shutdown();
             self->mOmxLayer = nullptr;
             self->mMediaDataHelper = nullptr;

             self->mShuttingDown = false;
             self->mOmxTaskQueue->BeginShutdown();
             self->mOmxTaskQueue->AwaitShutdownAndIdle();
             self->mShutdownPromise.Resolve(true, __func__);
           });
  return mShutdownPromise.Ensure(__func__);
}

void
OmxDataDecoder::FillBufferDone(BufferData* aData)
{
  MOZ_ASSERT(!aData || aData->mStatus == BufferData::BufferStatus::OMX_CLIENT);

  // Don't output sample when flush or shutting down, especially for video
  // decoded frame. Because video decoded frame has a promise in BufferData
  // waiting for layer to resolve it via recycle callback on Gonk, if other
  // module doesn't send it to layer, it will cause a unresolved promise and
  // waiting for resolve infinitely.
  if (mFlushing || mShuttingDown) {
    LOG("mFlush or mShuttingDown, drop data");
    aData->mStatus = BufferData::BufferStatus::FREE;
    return;
  }

  if (aData->mBuffer->nFlags & OMX_BUFFERFLAG_EOS) {
    // Reach eos, it's an empty data so it doesn't need to output.
    EndOfStream();
    aData->mStatus = BufferData::BufferStatus::FREE;
  } else {
    Output(aData);
    FillAndEmptyBuffers();
  }
}

void
OmxDataDecoder::Output(BufferData* aData)
{
  if (!mMediaDataHelper) {
    mMediaDataHelper = new MediaDataHelper(mTrackInfo.get(), mImageContainer, mOmxLayer);
  }

  bool isPlatformData = false;
  RefPtr<MediaData> data = mMediaDataHelper->GetMediaData(aData, isPlatformData);
  if (!data) {
    aData->mStatus = BufferData::BufferStatus::FREE;
    return;
  }

  if (isPlatformData) {
    // If the MediaData is platform dependnet data, it's mostly a kind of
    // limited resource, for example, GraphicBuffer on Gonk. So we use promise
    // to notify when the resource is free.
    aData->mStatus = BufferData::BufferStatus::OMX_CLIENT_OUTPUT;

    MOZ_RELEASE_ASSERT(aData->mPromise.IsEmpty());
    RefPtr<OmxBufferPromise> p = aData->mPromise.Ensure(__func__);

    RefPtr<OmxDataDecoder> self = this;
    RefPtr<BufferData> buffer = aData;
    p->Then(mOmxTaskQueue, __func__,
        [self, buffer] () {
          MOZ_RELEASE_ASSERT(buffer->mStatus == BufferData::BufferStatus::OMX_CLIENT_OUTPUT);
          buffer->mStatus = BufferData::BufferStatus::FREE;
          self->FillAndEmptyBuffers();
        },
        [buffer] () {
          MOZ_RELEASE_ASSERT(buffer->mStatus == BufferData::BufferStatus::OMX_CLIENT_OUTPUT);
          buffer->mStatus = BufferData::BufferStatus::FREE;
        });
  } else {
    aData->mStatus = BufferData::BufferStatus::FREE;
  }

  mDecodedData.AppendElement(Move(data));
}

void
OmxDataDecoder::FillBufferFailure(OmxBufferFailureHolder aFailureHolder)
{
  NotifyError(aFailureHolder.mError, __func__);
}

void
OmxDataDecoder::EmptyBufferDone(BufferData* aData)
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(!aData || aData->mStatus == BufferData::BufferStatus::OMX_CLIENT);

  // Nothing to do when status of input buffer is OMX_CLIENT.
  aData->mStatus = BufferData::BufferStatus::FREE;
  FillAndEmptyBuffers();

  // There is no way to know if component gets enough raw samples to generate
  // output, especially for video decoding. So here it needs to request raw
  // samples aggressively.
  if (!mCheckingInputExhausted && !mMediaRawDatas.Length()) {
    mCheckingInputExhausted = true;

    RefPtr<OmxDataDecoder> self = this;
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("OmxDataDecoder::EmptyBufferDone", [self, this]() {
        mCheckingInputExhausted = false;

        if (mMediaRawDatas.Length()) {
          return;
        }

        mDecodePromise.ResolveIfExists(mDecodedData, __func__);
        mDecodedData.Clear();
      });

    mOmxTaskQueue->Dispatch(r.forget());
  }
}

void
OmxDataDecoder::EmptyBufferFailure(OmxBufferFailureHolder aFailureHolder)
{
  NotifyError(aFailureHolder.mError, __func__);
}

void
OmxDataDecoder::NotifyError(OMX_ERRORTYPE aOmxError, const char* aLine, const MediaResult& aError)
{
  LOG("NotifyError %d (%" PRIu32 ") at %s", static_cast<int>(aOmxError),
      static_cast<uint32_t>(aError.Code()), aLine);
  mDecodedData.Clear();
  mDecodePromise.RejectIfExists(aError, __func__);
  mDrainPromise.RejectIfExists(aError, __func__);
  mFlushPromise.RejectIfExists(aError, __func__);
}

void
OmxDataDecoder::FillAndEmptyBuffers()
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(mOmxState == OMX_StateExecuting);

  // During the port setting changed, it is forbidden to do any buffer operation.
  if (mPortSettingsChanged != -1 || mShuttingDown || mFlushing) {
    return;
  }

  // Trigger input port.
  while (!!mMediaRawDatas.Length()) {
    // input buffer must be used by component if there is data available.
    RefPtr<BufferData> inbuf = FindAvailableBuffer(OMX_DirInput);
    if (!inbuf) {
      LOG("no input buffer!");
      break;
    }

    RefPtr<MediaRawData> data = mMediaRawDatas[0];
    // Buffer size should large enough for raw data.
    MOZ_RELEASE_ASSERT(inbuf->mBuffer->nAllocLen >= data->Size());

    memcpy(inbuf->mBuffer->pBuffer, data->Data(), data->Size());
    inbuf->mBuffer->nFilledLen = data->Size();
    inbuf->mBuffer->nOffset = 0;
    inbuf->mBuffer->nFlags = inbuf->mBuffer->nAllocLen > data->Size() ?
                             OMX_BUFFERFLAG_ENDOFFRAME : 0;
    inbuf->mBuffer->nTimeStamp = data->mTime.ToMicroseconds();
    if (data->Size()) {
      inbuf->mRawData = mMediaRawDatas[0];
    } else {
       LOG("send EOS buffer");
      inbuf->mBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
    }

    LOG("feed sample %p to omx component, len %ld, flag %lX", data.get(),
        inbuf->mBuffer->nFilledLen, inbuf->mBuffer->nFlags);
    mOmxLayer->EmptyBuffer(inbuf)->Then(mOmxTaskQueue, __func__, this,
                                        &OmxDataDecoder::EmptyBufferDone,
                                        &OmxDataDecoder::EmptyBufferFailure);
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
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  LOG("called from %s", aMethodName);
  mInitPromise.ResolveIfExists(mTrackInfo->GetType(), aMethodName);
}

void
OmxDataDecoder::RejectInitPromise(MediaResult aError, const char* aMethodName)
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  mInitPromise.RejectIfExists(aError, aMethodName);
}

void
OmxDataDecoder::OmxStateRunner()
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  LOG("OMX state: %s", StateTypeToStr(mOmxState));

  // TODO: maybe it'd be better to use promise CompletionPromise() to replace
  //       this state machine.
  if (mOmxState == OMX_StateLoaded) {
    ConfigCodec();

    // Send OpenMax state command to OMX_StateIdle.
    RefPtr<OmxDataDecoder> self = this;
    mOmxLayer->SendCommand(OMX_CommandStateSet, OMX_StateIdle, nullptr)
      ->Then(mOmxTaskQueue, __func__,
             [self] () {
               // Current state should be OMX_StateIdle.
               self->mOmxState = self->mOmxLayer->GetState();
               MOZ_ASSERT(self->mOmxState == OMX_StateIdle);
             },
             [self] () {
               self->RejectInitPromise(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
             });

    // Allocate input and output buffers.
    OMX_DIRTYPE types[] = {OMX_DIRTYPE::OMX_DirInput, OMX_DIRTYPE::OMX_DirOutput};
    for(const auto id : types) {
      if (NS_FAILED(AllocateBuffers(id))) {
        LOG("Failed to allocate buffer on port %d", id);
        RejectInitPromise(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
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
               self->RejectInitPromise(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
             });
  } else if (mOmxState == OMX_StateExecuting) {
    // Configure codec once it gets OMX_StateExecuting state.
    FillCodecConfigDataToOmx();
  } else {
    MOZ_ASSERT(0);
  }
}

void
OmxDataDecoder::ConfigCodec()
{
  OMX_ERRORTYPE err = mOmxLayer->Config();
  CHECK_OMX_ERR(err);
}

void
OmxDataDecoder::FillCodecConfigDataToOmx()
{
  // Codec configure data should be the first sample running on Omx TaskQueue.
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(!mMediaRawDatas.Length());
  MOZ_ASSERT(mOmxState == OMX_StateIdle || mOmxState == OMX_StateExecuting);


  RefPtr<BufferData> inbuf = FindAvailableBuffer(OMX_DirInput);
  RefPtr<MediaByteBuffer> csc;
  if (mTrackInfo->IsAudio()) {
    csc = mTrackInfo->GetAsAudioInfo()->mCodecSpecificConfig;
  } else if (mTrackInfo->IsVideo()) {
    csc = mTrackInfo->GetAsVideoInfo()->mExtraData;
  }

  MOZ_RELEASE_ASSERT(csc);

  // Some codecs like h264, its codec specific data is at the first packet, not in container.
  if (csc->Length()) {
    memcpy(inbuf->mBuffer->pBuffer,
           csc->Elements(),
           csc->Length());
    inbuf->mBuffer->nFilledLen = csc->Length();
    inbuf->mBuffer->nOffset = 0;
    inbuf->mBuffer->nFlags = (OMX_BUFFERFLAG_ENDOFFRAME | OMX_BUFFERFLAG_CODECCONFIG);

    LOG("Feed codec configure data to OMX component");
    mOmxLayer->EmptyBuffer(inbuf)->Then(mOmxTaskQueue, __func__, this,
                                        &OmxDataDecoder::EmptyBufferDone,
                                        &OmxDataDecoder::EmptyBufferFailure);
  }
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
      // Don't always disable port. See bug 1235340.
      if (aData2 == 0 ||
          aData2 == OMX_IndexParamPortDefinition) {
        // According to spec: "To prevent the loss of any input data, the
        // component issuing the OMX_EventPortSettingsChanged event on its input
        // port should buffer all input port data that arrives between the
        // emission of the OMX_EventPortSettingsChanged event and the arrival of
        // the command to disable the input port."
        //
        // So client needs to disable port and reallocate buffers.
        MOZ_ASSERT(mPortSettingsChanged == -1);
        mPortSettingsChanged = aData1;
      }
      LOG("Got OMX_EventPortSettingsChanged event");
      break;
    }
    default:
    {
      // Got error during decoding, send msg to MFR skipping to next key frame.
      if (aEvent == OMX_EventError && mOmxState == OMX_StateExecuting) {
        NotifyError((OMX_ERRORTYPE)aData1, __func__,
                    MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__));
        return true;
      }
      LOG("WARNING: got none handle event: %d, aData1: %ld, aData2: %ld",
          aEvent, aData1, aData2);
      return false;
    }
  }

  return true;
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

  LOG("CollectBufferPromises: type %d, total %" PRIuSIZE " promiese", aType, promises.Length());
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
    LOG("PortSettingsChanged: disable port %lu", def.nPortIndex);
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
             [self] (const OmxCommandFailureHolder& aError) {
               self->NotifyError(OMX_ErrorUndefined, __func__);
               return OmxCommandPromise::CreateAndReject(aError, __func__);
             })
      ->Then(mOmxTaskQueue, __func__,
             [self] () {
               LOGL("PortSettingsChanged: port settings changed complete");
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

RefPtr<MediaDataDecoder::FlushPromise>
OmxDataDecoder::DoFlush()
{
  MOZ_ASSERT(mOmxTaskQueue->IsCurrentThreadIn());

  mDecodedData.Clear();
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  mDrainPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  RefPtr<FlushPromise> p = mFlushPromise.Ensure(__func__);

  // 1. Call OMX command OMX_CommandFlush in Omx TaskQueue.
  // 2. Remove all elements in mMediaRawDatas when flush is completed.
  mOmxLayer->SendCommand(OMX_CommandFlush, OMX_ALL, nullptr)
    ->Then(mOmxTaskQueue, __func__, this,
           &OmxDataDecoder::FlushComplete,
           &OmxDataDecoder::FlushFailure);

  return p;
}

void
OmxDataDecoder::FlushComplete(OMX_COMMANDTYPE aCommandType)
{
  mMediaRawDatas.Clear();
  mFlushing = false;

  LOG("Flush complete");
  mFlushPromise.ResolveIfExists(true, __func__);
}

void OmxDataDecoder::FlushFailure(OmxCommandFailureHolder aFailureHolder)
{
  mFlushing = false;
  mFlushPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
}

MediaDataHelper::MediaDataHelper(const TrackInfo* aTrackInfo,
                                 layers::ImageContainer* aImageContainer,
                                 OmxPromiseLayer* aOmxLayer)
  : mTrackInfo(aTrackInfo)
  , mAudioCompactor(mAudioQueue)
  , mImageContainer(aImageContainer)
{
  InitOmxParameter(&mOutputPortDef);
  mOutputPortDef.nPortIndex = aOmxLayer->OutputPortIndex();
  aOmxLayer->GetParameter(OMX_IndexParamPortDefinition, &mOutputPortDef, sizeof(mOutputPortDef));
}

already_AddRefed<MediaData>
MediaDataHelper::GetMediaData(BufferData* aBufferData, bool& aPlatformDepenentData)
{
  aPlatformDepenentData = false;
  RefPtr<MediaData> data;

  if (mTrackInfo->IsAudio()) {
    if (!aBufferData->mBuffer->nFilledLen) {
      return nullptr;
    }
    data = CreateAudioData(aBufferData);
  } else if (mTrackInfo->IsVideo()) {
    data = aBufferData->GetPlatformMediaData();
    if (data) {
      aPlatformDepenentData = true;
    } else {
      if (!aBufferData->mBuffer->nFilledLen) {
        return nullptr;
      }
      // Get YUV VideoData, it uses more CPU, in most cases, on software codec.
      data = CreateYUV420VideoData(aBufferData);
    }

    // Update video time code, duration... from the raw data.
    VideoData* video(data->As<VideoData>());
    if (aBufferData->mRawData) {
      video->mTime = aBufferData->mRawData->mTime;
      video->mTimecode = aBufferData->mRawData->mTimecode;
      video->mOffset = aBufferData->mRawData->mOffset;
      video->mDuration = aBufferData->mRawData->mDuration;
      video->mKeyframe = aBufferData->mRawData->mKeyframe;
    }
  }

  return data.forget();
}

already_AddRefed<AudioData>
MediaDataHelper::CreateAudioData(BufferData* aBufferData)
{
  RefPtr<AudioData> audio;
  OMX_BUFFERHEADERTYPE* buf = aBufferData->mBuffer;
  const AudioInfo* info = mTrackInfo->GetAsAudioInfo();
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
    audio = mAudioQueue.PopFront();
  }

  return audio.forget();
}

already_AddRefed<VideoData>
MediaDataHelper::CreateYUV420VideoData(BufferData* aBufferData)
{
  uint8_t *yuv420p_buffer = (uint8_t *)aBufferData->mBuffer->pBuffer;
  int32_t stride = mOutputPortDef.format.video.nStride;
  int32_t slice_height = mOutputPortDef.format.video.nSliceHeight;
  int32_t width = mTrackInfo->GetAsVideoInfo()->mImage.width;
  int32_t height = mTrackInfo->GetAsVideoInfo()->mImage.height;

  // TODO: convert other formats to YUV420.
  if (mOutputPortDef.format.video.eColorFormat != OMX_COLOR_FormatYUV420Planar) {
    return nullptr;
  }

  size_t yuv420p_y_size = stride * slice_height;
  size_t yuv420p_u_size = ((stride + 1) / 2) * ((slice_height + 1) / 2);
  uint8_t *yuv420p_y = yuv420p_buffer;
  uint8_t *yuv420p_u = yuv420p_y + yuv420p_y_size;
  uint8_t *yuv420p_v = yuv420p_u + yuv420p_u_size;

  VideoData::YCbCrBuffer b;
  b.mPlanes[0].mData = yuv420p_y;
  b.mPlanes[0].mWidth = width;
  b.mPlanes[0].mHeight = height;
  b.mPlanes[0].mStride = stride;
  b.mPlanes[0].mOffset = 0;
  b.mPlanes[0].mSkip = 0;

  b.mPlanes[1].mData = yuv420p_u;
  b.mPlanes[1].mWidth = (width + 1) / 2;
  b.mPlanes[1].mHeight = (height + 1) / 2;
  b.mPlanes[1].mStride = (stride + 1) / 2;
  b.mPlanes[1].mOffset = 0;
  b.mPlanes[1].mSkip = 0;

  b.mPlanes[2].mData = yuv420p_v;
  b.mPlanes[2].mWidth =(width + 1) / 2;
  b.mPlanes[2].mHeight = (height + 1) / 2;
  b.mPlanes[2].mStride = (stride + 1) / 2;
  b.mPlanes[2].mOffset = 0;
  b.mPlanes[2].mSkip = 0;

  VideoInfo info(*mTrackInfo->GetAsVideoInfo());
  RefPtr<VideoData> data =
    VideoData::CreateAndCopyData(info,
                                 mImageContainer,
                                 0, // Filled later by caller.
                                 media::TimeUnit::Zero(), // Filled later by caller.
                                 media::TimeUnit::FromMicroseconds(1), // We don't know the duration.
                                 b,
                                 0, // Filled later by caller.
                                 media::TimeUnit::FromMicroseconds(-1),
                                 info.ImageRect());

  LOG("YUV420 VideoData: disp width %d, height %d, pic width %d, height %d, time %lld",
      info.mDisplay.width, info.mDisplay.height, info.mImage.width,
      info.mImage.height, aBufferData->mBuffer->nTimeStamp);

  return data.forget();
}

}
