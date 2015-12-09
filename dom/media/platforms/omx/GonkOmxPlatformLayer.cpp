/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxDataDecoder.h"
#include "OmxPromiseLayer.h"
#include "GonkOmxPlatformLayer.h"
#include "MediaInfo.h"
#include <binder/MemoryDealer.h>
#include <media/IOMX.h>
#include <utils/List.h>
#include <media/stagefright/OMXCodec.h>

extern mozilla::LogModule* GetPDMLog();

#ifdef LOG
#undef LOG
#endif

#define LOG(arg, ...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, ("GonkOmxPlatformLayer:: " arg, ##__VA_ARGS__))

using namespace android;

namespace mozilla {

extern void GetPortIndex(nsTArray<uint32_t>& aPortIndex);

bool IsSoftwareCodec(const char* aComponentName) {
  nsAutoCString str(aComponentName);
  return (str.Find(NS_LITERAL_CSTRING("OMX.google.")) == -1 ? false : true);
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
  //   we should combination both event handlers into one. And we should provide
  //   an unified way for event handling in OmxPlatforLayer class.
  RefPtr<OmxPromiseLayer> mPromiseLayer;
  RefPtr<OmxDataDecoder> mClient;
};

GonkBufferData::GonkBufferData(android::IOMX::buffer_id aId, bool aLiveInLocal, android::IMemory* aMemory)
  : BufferData((OMX_BUFFERHEADERTYPE*)aId)
  , mId(aId)
{
  if (!aLiveInLocal) {
    mLocalBuffer = new OMX_BUFFERHEADERTYPE;
    PodZero(mLocalBuffer.get());
    // aMemory is a IPC memory, it is safe to use it here.
    mLocalBuffer->pBuffer = (OMX_U8*)aMemory->pointer();
    mBuffer = mLocalBuffer.get();
  }
}

GonkOmxPlatformLayer::GonkOmxPlatformLayer(OmxDataDecoder* aDataDecoder,
                                           OmxPromiseLayer* aPromiseLayer,
                                           TaskQueue* aTaskQueue)
  : mTaskQueue(aTaskQueue)
  , mNode(0)
  , mQuirks(0)
  , mUsingHardwareCodec(false)
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
  GetPortIndex(portindex);
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

  size_t t = def.nBufferCountActual * def.nBufferSize;
  LOG("Buffer count %d, buffer size %d", def.nBufferCountActual, def.nBufferSize);

  bool liveinlocal = mOmx->livesLocally(mNode, getpid());

  // MemoryDealer is a IPC buffer allocator in Gonk because IOMX is actually
  // lives in mediaserver.
  mMemoryDealer[aType] = new MemoryDealer(t, "Gecko-OMX");
  for (OMX_U32 i = 0; i < def.nBufferCountActual; ++i) {
    sp<IMemory> mem = mMemoryDealer[aType]->allocate(def.nBufferSize);
    MOZ_ASSERT(mem.get());

    IOMX::buffer_id bufferID;
    status_t st;

    if ((mQuirks & OMXCodec::kRequiresAllocateBufferOnInputPorts && aType == OMX_DirInput) ||
        (mQuirks & OMXCodec::kRequiresAllocateBufferOnOutputPorts && aType == OMX_DirOutput)) {
      st = mOmx->allocateBufferWithBackup(mNode, aType, mem, &bufferID);
    } else {
      st = mOmx->useBuffer(mNode, aType, mem, &bufferID);
    }

    if (st != OK) {
      return NS_ERROR_FAILURE;
    }

    aBufferList->AppendElement(new GonkBufferData(bufferID, liveinlocal, mem.get()));
  }

  return NS_OK;
}

nsresult
GonkOmxPlatformLayer::ReleaseOmxBuffer(OMX_DIRTYPE aType,
                                       BUFFERLIST* aBufferList)
{
  status_t st;
  for (uint32_t i = 0; i < aBufferList->Length(); i++) {
    IOMX::buffer_id id = (OMX_BUFFERHEADERTYPE*) aBufferList->ElementAt(i)->ID();
    st = mOmx->freeBuffer(mNode, aType, id);
    if (st != OK) {
      return NS_ERROR_FAILURE;
    }
  }
  aBufferList->Clear();
  mMemoryDealer[aType].clear();

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
  status_t err = mOmxClient.connect();
  if (err != OK) {
      return OMX_ErrorUndefined;
  }
  mOmx = mOmxClient.interface();
  if (!mOmx.get()) {
    return OMX_ErrorUndefined;
  }

  // In Gonk, the software compoment name has prefix "OMX.google". It needs to
  // have a way to use hardware codec first.
  android::Vector<OMXCodec::CodecNameAndQuirks> matchingCodecs;
  const char* swcomponent = nullptr;
  OMXCodec::findMatchingCodecs(aInfo->mMimeType.Data(),
                               0,
                               nullptr,
                               0,
                               &matchingCodecs);
  for (uint32_t i = 0; i < matchingCodecs.size(); i++) {
    const char* componentName = matchingCodecs.itemAt(i).mName.string();
    if (IsSoftwareCodec(componentName)) {
      swcomponent = componentName;
    } else {
      // Try to use hardware codec first.
      if (LoadComponent(componentName)) {
        mUsingHardwareCodec = true;
        return OMX_ErrorNone;
      }
    }
  }

  // TODO: in android ICS, the software codec is allocated in mediaserver by
  //       default, it may be necessay to allocate it in local process.
  //
  // fallback to sw codec
  if (LoadComponent(swcomponent)) {
    return OMX_ErrorNone;
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
  return (OMX_ERRORTYPE)mOmx->fillBuffer(mNode, (IOMX::buffer_id)aData->mBuffer);
}

OMX_ERRORTYPE
GonkOmxPlatformLayer::SendCommand(OMX_COMMANDTYPE aCmd,
                                  OMX_U32 aParam1,
                                  OMX_PTR aCmdData)
{
  return  (OMX_ERRORTYPE)mOmx->sendCommand(mNode, aCmd, aParam1);
}

bool
GonkOmxPlatformLayer::LoadComponent(const char* aName)
{
  status_t err = mOmx->allocateNode(aName, mOmxObserver, &mNode);
  if (err == OK) {
    OMXCodec::findCodecQuirks(aName, &mQuirks);
    LOG("Load OpenMax component %s, quirks %x, live locally %d",
        aName, mQuirks, mOmx->livesLocally(mNode, getpid()));
    return true;
  }
  return false;
}

template<class T> void
GonkOmxPlatformLayer::InitOmxParameter(T* aParam)
{
  PodZero(aParam);
  aParam->nSize = sizeof(T);
  aParam->nVersion.s.nVersionMajor = 1;
}

} // mozilla
