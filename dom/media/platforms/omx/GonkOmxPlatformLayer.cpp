/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkOmxPlatformLayer.h"

#include <binder/MemoryDealer.h>
#include <cutils/properties.h>
#include <media/IOMX.h>
#include <media/stagefright/MediaCodecList.h>
#include <utils/List.h>

#include "mozilla/Monitor.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/GrallocTextureClient.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"

#include "ImageContainer.h"
#include "MediaInfo.h"
#include "OmxDataDecoder.h"


#ifdef LOG
#undef LOG
#endif

#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("GonkOmxPlatformLayer(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

#define CHECK_ERR(err)                    \
  if (err != OK)                       {  \
    LOG("error %d at %s", err, __func__); \
    return NS_ERROR_FAILURE;              \
  }                                       \

// Android proprietary value.
#define ANDROID_OMX_VIDEO_CodingVP8 (static_cast<OMX_VIDEO_CODINGTYPE>(9))

using namespace android;

namespace mozilla {

// In Gonk, the software component name has prefix "OMX.google". It needs to
// have a way to use hardware codec first.
bool IsSoftwareCodec(const char* aComponentName)
{
  nsAutoCString str(aComponentName);
  return (str.Find(NS_LITERAL_CSTRING("OMX.google.")) == -1 ? false : true);
}

bool IsInEmulator()
{
  char propQemu[PROPERTY_VALUE_MAX];
  property_get("ro.kernel.qemu", propQemu, "");
  return !strncmp(propQemu, "1", 1);
}

class GonkOmxObserver : public BnOMXObserver {
public:
  void onMessage(const omx_message& aMsg)
  {
    switch (aMsg.type) {
      case omx_message::EVENT:
      {
        sp<GonkOmxObserver> self = this;
        nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self, aMsg] () {
          if (self->mClient && self->mClient->Event(aMsg.u.event_data.event,
                                                    aMsg.u.event_data.data1,
                                                    aMsg.u.event_data.data2))
          {
            return;
          }
        });
        mTaskQueue->Dispatch(r.forget());
        break;
      }
      case omx_message::EMPTY_BUFFER_DONE:
      {
        sp<GonkOmxObserver> self = this;
        nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self, aMsg] () {
          if (!self->mPromiseLayer) {
            return;
          }
          BufferData::BufferID id = (BufferData::BufferID)aMsg.u.buffer_data.buffer;
          self->mPromiseLayer->EmptyFillBufferDone(OMX_DirInput, id);
        });
        mTaskQueue->Dispatch(r.forget());
        break;
      }
      case omx_message::FILL_BUFFER_DONE:
      {
        sp<GonkOmxObserver> self = this;
        nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([self, aMsg] () {
          if (!self->mPromiseLayer) {
            return;
          }

          // TODO: these codes look a little ugly, it'd be better to improve them.
          RefPtr<BufferData> buf;
          BufferData::BufferID id = (BufferData::BufferID)aMsg.u.extended_buffer_data.buffer;
          buf = self->mPromiseLayer->FindAndRemoveBufferHolder(OMX_DirOutput, id);
          MOZ_RELEASE_ASSERT(buf);
          GonkBufferData* gonkBuffer = static_cast<GonkBufferData*>(buf.get());

          // Copy the critical information to local buffer.
          if (gonkBuffer->IsLocalBuffer()) {
            gonkBuffer->mBuffer->nOffset = aMsg.u.extended_buffer_data.range_offset;
            gonkBuffer->mBuffer->nFilledLen = aMsg.u.extended_buffer_data.range_length;
            gonkBuffer->mBuffer->nFlags = aMsg.u.extended_buffer_data.flags;
            gonkBuffer->mBuffer->nTimeStamp = aMsg.u.extended_buffer_data.timestamp;
          }
          self->mPromiseLayer->EmptyFillBufferDone(OMX_DirOutput, buf);
        });
        mTaskQueue->Dispatch(r.forget());
        break;
      }
      default:
      {
        LOG("Unhandle event %d", aMsg.type);
      }
    }
  }

  void Shutdown()
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    mPromiseLayer = nullptr;
    mClient = nullptr;
  }

  GonkOmxObserver(TaskQueue* aTaskQueue, OmxPromiseLayer* aPromiseLayer, OmxDataDecoder* aDataDecoder)
    : mTaskQueue(aTaskQueue)
    , mPromiseLayer(aPromiseLayer)
    , mClient(aDataDecoder)
  {}

protected:
  RefPtr<TaskQueue> mTaskQueue;
  // TODO:
  //   we should combine both event handlers into one. And we should provide
  //   an unified way for event handling in OmxPlatformLayer class.
  RefPtr<OmxPromiseLayer> mPromiseLayer;
  RefPtr<OmxDataDecoder> mClient;
};

// This class allocates Gralloc buffer and manages TextureClient's recycle.
class GonkTextureClientRecycleHandler : public layers::ITextureClientRecycleAllocator
{
  typedef MozPromise<layers::TextureClient*, nsresult, /* IsExclusive = */ true> TextureClientRecyclePromise;

public:
  GonkTextureClientRecycleHandler(OMX_VIDEO_PORTDEFINITIONTYPE& aDef)
    : ITextureClientRecycleAllocator()
    , mMonitor("GonkTextureClientRecycleHandler")
  {
    // Allocate Gralloc texture memory.
    layers::GrallocTextureData* textureData =
      layers::GrallocTextureData::Create(gfx::IntSize(aDef.nFrameWidth, aDef.nFrameHeight),
                                         aDef.eColorFormat,
                                         gfx::BackendType::NONE,
                                         GraphicBuffer::USAGE_HW_TEXTURE | GraphicBuffer::USAGE_SW_READ_OFTEN,
                                         layers::ImageBridgeChild::GetSingleton());

    mGraphBuffer = textureData->GetGraphicBuffer();
    MOZ_ASSERT(mGraphBuffer.get());

    mTextureClient =
      layers::TextureClient::CreateWithData(textureData,
                                            layers::TextureFlags::DEALLOCATE_CLIENT | layers::TextureFlags::RECYCLE,
                                            layers::ImageBridgeChild::GetSingleton());
    MOZ_ASSERT(mTextureClient);

    mPromise.SetMonitor(&mMonitor);
  }

  RefPtr<TextureClientRecyclePromise> WaitforRecycle()
  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(!!mGraphBuffer.get());

    mTextureClient->SetRecycleAllocator(this);
    return mPromise.Ensure(__func__);
  }

  // DO NOT use smart pointer to receive TextureClient; otherwise it will
  // distrupt the reference count.
  layers::TextureClient* GetTextureClient()
  {
    return mTextureClient;
  }

  GraphicBuffer* GetGraphicBuffer()
  {
    MonitorAutoLock lock(mMonitor);
    return mGraphBuffer.get();
  }

  // This function is called from layers thread.
  void RecycleTextureClient(layers::TextureClient* aClient) override
  {
    MOZ_ASSERT(mTextureClient == aClient);

    // Clearing the recycle allocator drops a reference, so make sure we stay alive
    // for the duration of this function.
    RefPtr<GonkTextureClientRecycleHandler> kungFuDeathGrip(this);
    aClient->SetRecycleAllocator(nullptr);

    {
      MonitorAutoLock lock(mMonitor);
      mPromise.ResolveIfExists(mTextureClient, __func__);
    }
  }

  void Shutdown()
  {
    MonitorAutoLock lock(mMonitor);

    mPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);

    // DO NOT clear TextureClient here.
    // The ref count could be 1 and RecycleCallback will be called if we clear
    // the ref count here. That breaks the whole mechanism. (RecycleCallback
    // should be called from layers)
    mGraphBuffer = nullptr;
  }

private:
  // Because TextureClient calls RecycleCallbackl when ref count is 1, so we
  // should hold only one reference here and use raw pointer when out of this
  // class.
  RefPtr<layers::TextureClient> mTextureClient;

  // It is protected by mMonitor.
  sp<android::GraphicBuffer> mGraphBuffer;

  // It is protected by mMonitor.
  MozPromiseHolder<TextureClientRecyclePromise> mPromise;

  Monitor mMonitor;
};

GonkBufferData::GonkBufferData(bool aLiveInLocal,
                               GonkOmxPlatformLayer* aGonkPlatformLayer)
  : BufferData(nullptr)
  , mId(0)
  , mGonkPlatformLayer(aGonkPlatformLayer)
{
  if (!aLiveInLocal) {
    mMirrorBuffer = new OMX_BUFFERHEADERTYPE;
    PodZero(mMirrorBuffer.get());
    mBuffer = mMirrorBuffer.get();
  }
}

void
GonkBufferData::ReleaseBuffer()
{
  if (mTextureClientRecycleHandler) {
    mTextureClientRecycleHandler->Shutdown();
    mTextureClientRecycleHandler = nullptr;
  }
}

nsresult
GonkBufferData::InitSharedMemory(android::IMemory* aMemory)
{
  MOZ_RELEASE_ASSERT(mMirrorBuffer.get());

  // aMemory is a IPC memory, it is safe to use it here.
  mBuffer->pBuffer = (OMX_U8*)aMemory->pointer();
  mBuffer->nAllocLen = aMemory->size();
  return NS_OK;
}

nsresult
GonkBufferData::InitLocalBuffer(IOMX::buffer_id aId)
{
  MOZ_RELEASE_ASSERT(!mMirrorBuffer.get());

  mBuffer = (OMX_BUFFERHEADERTYPE*)aId;
  return NS_OK;
}

nsresult
GonkBufferData::InitGraphicBuffer(OMX_VIDEO_PORTDEFINITIONTYPE& aDef)
{
  mTextureClientRecycleHandler = new GonkTextureClientRecycleHandler(aDef);

  if (!mTextureClientRecycleHandler->GetGraphicBuffer()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

already_AddRefed<MediaData>
GonkBufferData::GetPlatformMediaData()
{
  if (mGonkPlatformLayer->GetTrackInfo()->GetAsAudioInfo()) {
    // This is audio decoding.
    return nullptr;
  }

  if (!mTextureClientRecycleHandler) {
    // There is no GraphicBuffer, it should fallback to normal YUV420 VideoData.
    return nullptr;
  }

  VideoInfo info(*mGonkPlatformLayer->GetTrackInfo()->GetAsVideoInfo());
  RefPtr<VideoData> data =
    VideoData::CreateAndCopyIntoTextureClient(info,
                                              0,
                                              mBuffer->nTimeStamp,
                                              1,
                                              mTextureClientRecycleHandler->GetTextureClient(),
                                              false,
                                              0,
                                              info.ImageRect());
  LOG("%p, disp width %d, height %d, pic width %d, height %d, time %ld",
      this, info.mDisplay.width, info.mDisplay.height,
      info.mImage.width, info.mImage.height, mBuffer->nTimeStamp);

  // Get TextureClient Promise here to wait for resolved.
  RefPtr<GonkBufferData> self(this);
  mTextureClientRecycleHandler->WaitforRecycle()
    ->Then(mGonkPlatformLayer->GetTaskQueue(), __func__,
           [self] () {
             // Waiting for texture to be freed.
             if (self->mTextureClientRecycleHandler) {
               self->mTextureClientRecycleHandler->GetTextureClient()->WaitForBufferOwnership();
             }
             self->mPromise.ResolveIfExists(self, __func__);
           },
           [self] () {
             OmxBufferFailureHolder failure(OMX_ErrorUndefined, self);
             self->mPromise.RejectIfExists(failure, __func__);
           });

  return data.forget();
}

GonkOmxPlatformLayer::GonkOmxPlatformLayer(OmxDataDecoder* aDataDecoder,
                                           OmxPromiseLayer* aPromiseLayer,
                                           TaskQueue* aTaskQueue,
                                           layers::ImageContainer* aImageContainer)
  : mTaskQueue(aTaskQueue)
  , mImageContainer(aImageContainer)
  , mNode(0)
{
  mOmxObserver = new GonkOmxObserver(mTaskQueue, aPromiseLayer, aDataDecoder);
}

nsresult
GonkOmxPlatformLayer::AllocateOmxBuffer(OMX_DIRTYPE aType,
                                        BUFFERLIST* aBufferList)
{
  MOZ_ASSERT(!mMemoryDealer[aType].get());

  // Get port definition.
  OMX_PARAM_PORTDEFINITIONTYPE def;
  nsTArray<uint32_t> portindex;
  GetPortIndices(portindex);
  for (auto idx : portindex) {
    InitOmxParameter(&def);
    def.nPortIndex = idx;

    OMX_ERRORTYPE err = GetParameter(OMX_IndexParamPortDefinition,
                                     &def,
                                     sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
    if (err != OMX_ErrorNone) {
      return NS_ERROR_FAILURE;
    } else if (def.eDir == aType) {
      LOG("Get OMX_IndexParamPortDefinition: port: %d, type: %d", def.nPortIndex, def.eDir);
      break;
    }
  }

  size_t t = 0;

  // Configure video output GraphicBuffer for video decoding acceleration.
  bool useGralloc = false;
  if (aType == OMX_DirOutput && mQuirks.test(kRequiresAllocateBufferOnOutputPorts) &&
      (def.eDomain == OMX_PortDomainVideo)) {
    if (NS_FAILED(EnableOmxGraphicBufferPort(def))) {
      return NS_ERROR_FAILURE;
    }

    LOG("Enable OMX GraphicBuffer port, number %d, width %d, height %d", def.nBufferCountActual,
        def.format.video.nFrameWidth, def.format.video.nFrameHeight);

    useGralloc = true;

    t = 1024; // MemoryDealer doesn't like 0, it's just for MemoryDealer happy.
  } else {
    t = def.nBufferCountActual * def.nBufferSize;
    LOG("Buffer count %d, buffer size %d", def.nBufferCountActual, def.nBufferSize);
  }

  bool liveinlocal = mOmx->livesLocally(mNode, getpid());

  // MemoryDealer is a IPC buffer allocator in Gonk because IOMX is actually
  // lives in mediaserver.
  mMemoryDealer[aType] = new MemoryDealer(t, "Gecko-OMX");
  for (OMX_U32 i = 0; i < def.nBufferCountActual; ++i) {
    RefPtr<GonkBufferData> buffer;
    IOMX::buffer_id bufferID;
    status_t st;
    nsresult rv;

    buffer = new GonkBufferData(liveinlocal, this);
    if (useGralloc) {
      // Buffer is lived remotely. Use GraphicBuffer for decoded video frame display.
      rv = buffer->InitGraphicBuffer(def.format.video);
      NS_ENSURE_SUCCESS(rv, rv);
      st = mOmx->useGraphicBuffer(mNode,
                                  def.nPortIndex,
                                  buffer->mTextureClientRecycleHandler->GetGraphicBuffer(),
                                  &bufferID);
      CHECK_ERR(st);
    } else {
      sp<IMemory> mem = mMemoryDealer[aType]->allocate(def.nBufferSize);
      MOZ_ASSERT(mem.get());

      if ((mQuirks.test(kRequiresAllocateBufferOnInputPorts) && aType == OMX_DirInput) ||
          (mQuirks.test(kRequiresAllocateBufferOnOutputPorts) && aType == OMX_DirOutput)) {
        // Buffer is lived remotely. We allocate a local OMX_BUFFERHEADERTYPE
        // as the mirror of the remote OMX_BUFFERHEADERTYPE.
        st = mOmx->allocateBufferWithBackup(mNode, aType, mem, &bufferID);
        CHECK_ERR(st);
        rv = buffer->InitSharedMemory(mem.get());
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // Buffer is lived locally, bufferID is the actually OMX_BUFFERHEADERTYPE
        // pointer.
        st = mOmx->useBuffer(mNode, aType, mem, &bufferID);
        CHECK_ERR(st);
        rv = buffer->InitLocalBuffer(bufferID);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    rv = buffer->SetBufferId(bufferID);
    NS_ENSURE_SUCCESS(rv, rv);

    aBufferList->AppendElement(buffer);
  }

  return NS_OK;
}

nsresult
GonkOmxPlatformLayer::ReleaseOmxBuffer(OMX_DIRTYPE aType,
                                       BUFFERLIST* aBufferList)
{
  status_t st;
  uint32_t len = aBufferList->Length();
  for (uint32_t i = 0; i < len; i++) {
    GonkBufferData* buffer = static_cast<GonkBufferData*>(aBufferList->ElementAt(i).get());
    IOMX::buffer_id id = (OMX_BUFFERHEADERTYPE*) buffer->ID();
    st = mOmx->freeBuffer(mNode, aType, id);
    if (st != OK) {
      return NS_ERROR_FAILURE;
    }
    buffer->ReleaseBuffer();
  }
  aBufferList->Clear();
  mMemoryDealer[aType].clear();

  return NS_OK;
}

nsresult
GonkOmxPlatformLayer::EnableOmxGraphicBufferPort(OMX_PARAM_PORTDEFINITIONTYPE& aDef)
{
  status_t st;

  st = mOmx->enableGraphicBuffers(mNode, aDef.nPortIndex, OMX_TRUE);
  CHECK_ERR(st);

  return NS_OK;
}

OMX_ERRORTYPE
GonkOmxPlatformLayer::GetState(OMX_STATETYPE* aType)
{
  return (OMX_ERRORTYPE)mOmx->getState(mNode, aType);
}

OMX_ERRORTYPE
GonkOmxPlatformLayer::GetParameter(OMX_INDEXTYPE aParamIndex,
                                   OMX_PTR aComponentParameterStructure,
                                   OMX_U32 aComponentParameterSize)
{
  return (OMX_ERRORTYPE)mOmx->getParameter(mNode,
                                           aParamIndex,
                                           aComponentParameterStructure,
                                           aComponentParameterSize);
}

OMX_ERRORTYPE
GonkOmxPlatformLayer::SetParameter(OMX_INDEXTYPE aParamIndex,
                                   OMX_PTR aComponentParameterStructure,
                                   OMX_U32 aComponentParameterSize)
{
  return (OMX_ERRORTYPE)mOmx->setParameter(mNode,
                                           aParamIndex,
                                           aComponentParameterStructure,
                                           aComponentParameterSize);
}

nsresult
GonkOmxPlatformLayer::Shutdown()
{
  mOmx->freeNode(mNode);
  mOmxObserver->Shutdown();
  mOmxObserver = nullptr;
  mOmxClient.disconnect();

  return NS_OK;
}

OMX_ERRORTYPE
GonkOmxPlatformLayer::InitOmxToStateLoaded(const TrackInfo* aInfo)
{
  mInfo = aInfo;
  status_t err = mOmxClient.connect();
  if (err != OK) {
      return OMX_ErrorUndefined;
  }
  mOmx = mOmxClient.interface();
  if (!mOmx.get()) {
    return OMX_ErrorUndefined;
  }

  LOG("find componenet for mime type %s", mInfo->mMimeType.Data());

  nsTArray<ComponentInfo> components;
  if (FindComponents(mInfo->mMimeType, &components)) {
    for (auto comp : components) {
      if (LoadComponent(comp)) {
        return OMX_ErrorNone;
      }
    }
  }

  LOG("no component is loaded");
  return OMX_ErrorUndefined;
}

OMX_ERRORTYPE
GonkOmxPlatformLayer::EmptyThisBuffer(BufferData* aData)
{
  return (OMX_ERRORTYPE)mOmx->emptyBuffer(mNode,
                                          (IOMX::buffer_id)aData->ID(),
                                          aData->mBuffer->nOffset,
                                          aData->mBuffer->nFilledLen,
                                          aData->mBuffer->nFlags,
                                          aData->mBuffer->nTimeStamp);
}

OMX_ERRORTYPE
GonkOmxPlatformLayer::FillThisBuffer(BufferData* aData)
{
  return (OMX_ERRORTYPE)mOmx->fillBuffer(mNode, (IOMX::buffer_id)aData->ID());
}

OMX_ERRORTYPE
GonkOmxPlatformLayer::SendCommand(OMX_COMMANDTYPE aCmd,
                                  OMX_U32 aParam1,
                                  OMX_PTR aCmdData)
{
  return  (OMX_ERRORTYPE)mOmx->sendCommand(mNode, aCmd, aParam1);
}

bool
GonkOmxPlatformLayer::LoadComponent(const ComponentInfo& aComponent)
{
  status_t err = mOmx->allocateNode(aComponent.mName, mOmxObserver, &mNode);
  if (err == OK) {
    mQuirks = aComponent.mQuirks;
    LOG("Load OpenMax component %s, alloc input %d, alloc output %d, live locally %d",
        aComponent.mName, mQuirks.test(kRequiresAllocateBufferOnInputPorts),
        mQuirks.test(kRequiresAllocateBufferOnOutputPorts),
        mOmx->livesLocally(mNode, getpid()));
    return true;
  }
  return false;
}

layers::ImageContainer*
GonkOmxPlatformLayer::GetImageContainer()
{
  return mImageContainer;
}

const TrackInfo*
GonkOmxPlatformLayer::GetTrackInfo()
{
  return mInfo;
}

bool
GonkOmxPlatformLayer::FindComponents(const nsACString& aMimeType,
                                     nsTArray<ComponentInfo>* aComponents)
{
  static const MediaCodecList* codecs = MediaCodecList::getInstance();

  bool useHardwareCodecOnly = false;

  // H264 and H263 has different profiles, software codec doesn't support high profile.
  // So we use hardware codec only.
  if (!IsInEmulator() &&
      (aMimeType.EqualsLiteral("video/avc") ||
       aMimeType.EqualsLiteral("video/mp4") ||
       aMimeType.EqualsLiteral("video/mp4v-es") ||
       aMimeType.EqualsLiteral("video/3gp"))) {
    useHardwareCodecOnly = true;
  }

  const char* mime = aMimeType.Data();
  // Translate VP8 MIME type to Android format.
  if (aMimeType.EqualsLiteral("video/webm; codecs=vp8")) {
    mime = "video/x-vnd.on2.vp8";
  }

  size_t start = 0;
  bool found = false;
  while (true) {
    ssize_t index = codecs->findCodecByType(mime, false /* encoder */, start);
    if (index < 0) {
      break;
    }
    start = index + 1;

    const char* name = codecs->getCodecName(index);
    if (IsSoftwareCodec(name) && useHardwareCodecOnly) {
      continue;
    }

    found = true;

    if (!aComponents) {
      continue;
    }
    ComponentInfo* comp = aComponents->AppendElement();
    comp->mName = name;
    if (codecs->codecHasQuirk(index, "requires-allocate-on-input-ports")) {
      comp->mQuirks.set(kRequiresAllocateBufferOnInputPorts);
    }
    if (codecs->codecHasQuirk(index, "requires-allocate-on-output-ports")) {
      comp->mQuirks.set(kRequiresAllocateBufferOnOutputPorts);
    }
  }

  return found;
}

OMX_VIDEO_CODINGTYPE
GonkOmxPlatformLayer::CompressionFormat()
{
  MOZ_ASSERT(mInfo);

  return mInfo->mMimeType.EqualsLiteral("video/webm; codecs=vp8") ?
    ANDROID_OMX_VIDEO_CodingVP8 : OmxPlatformLayer::CompressionFormat();
}

} // mozilla
