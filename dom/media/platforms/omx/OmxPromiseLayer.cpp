/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxPromiseLayer.h"
#include "OmxPlatformLayer.h"
#include "OmxDataDecoder.h"

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION < 21
#include "GonkOmxPlatformLayer.h"
#endif

extern mozilla::LogModule* GetPDMLog();

#ifdef LOG
#undef LOG
#endif

#define LOG(arg, ...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, ("OmxPromiseLayer:: " arg, ##__VA_ARGS__))

namespace mozilla {

OmxPromiseLayer::OmxPromiseLayer(TaskQueue* aTaskQueue, OmxDataDecoder* aDataDecoder)
  : mTaskQueue(aTaskQueue)
  , mFlushPortIndex(0)
{
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION < 21
  mPlatformLayer = new GonkOmxPlatformLayer(aDataDecoder, this, aTaskQueue);
#endif
  MOZ_ASSERT(!!mPlatformLayer);
}

RefPtr<OmxPromiseLayer::OmxCommandPromise>
OmxPromiseLayer::Init(TaskQueue* aTaskQueue, const TrackInfo* aInfo)
{
  mTaskQueue = aTaskQueue;
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  OMX_ERRORTYPE err = mPlatformLayer->InitOmxToStateLoaded(aInfo);
  if (err != OMX_ErrorNone) {
    OmxCommandFailureHolder failure(OMX_ErrorUndefined, OMX_CommandStateSet);
    return OmxCommandPromise::CreateAndReject(failure, __func__);
  }

  OMX_STATETYPE state = GetState();
  if (state ==  OMX_StateLoaded) {
    return OmxCommandPromise::CreateAndResolve(OMX_CommandStateSet, __func__);
  } if (state == OMX_StateIdle) {
    return SendCommand(OMX_CommandStateSet, OMX_StateIdle, nullptr);
  }

  OmxCommandFailureHolder failure(OMX_ErrorUndefined, OMX_CommandStateSet);
  return OmxCommandPromise::CreateAndReject(failure, __func__);
}

RefPtr<OmxPromiseLayer::OmxBufferPromise>
OmxPromiseLayer::FillBuffer(BufferData* aData)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  LOG("FillBuffer: buffer %p", aData->mBuffer);

  RefPtr<OmxBufferPromise> p = aData->mPromise.Ensure(__func__);

  OMX_ERRORTYPE err = mPlatformLayer->FillThisBuffer(aData);

  if (err != OMX_ErrorNone) {
    OmxBufferFailureHolder failure(err, aData);
    aData->mPromise.Reject(Move(failure), __func__);
  } else {
    aData->mStatus = BufferData::BufferStatus::OMX_COMPONENT;
    GetBufferHolders(OMX_DirOutput)->AppendElement(aData);
  }

  return p;
}

RefPtr<OmxPromiseLayer::OmxBufferPromise>
OmxPromiseLayer::EmptyBuffer(BufferData* aData)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  LOG("EmptyBuffer: buffer %p, size %d", aData->mBuffer, aData->mBuffer->nFilledLen);

  RefPtr<OmxBufferPromise> p = aData->mPromise.Ensure(__func__);

  OMX_ERRORTYPE err = mPlatformLayer->EmptyThisBuffer(aData);

  if (err != OMX_ErrorNone) {
    OmxBufferFailureHolder failure(err, aData);
    aData->mPromise.Reject(Move(failure), __func__);
  } else {
    if (aData->mRawData) {
      mRawDatas.AppendElement(Move(aData->mRawData));
    }
    aData->mStatus = BufferData::BufferStatus::OMX_COMPONENT;
    GetBufferHolders(OMX_DirInput)->AppendElement(aData);
  }

  return p;
}

OmxPromiseLayer::BUFFERLIST*
OmxPromiseLayer::GetBufferHolders(OMX_DIRTYPE aType)
{
  MOZ_ASSERT(aType == OMX_DirInput || aType == OMX_DirOutput);

  if (aType == OMX_DirInput) {
    return &mInbufferHolders;
  }

  return &mOutbufferHolders;
}

already_AddRefed<MediaRawData>
OmxPromiseLayer::FindAndRemoveRawData(OMX_TICKS aTimecode)
{
  for (auto raw : mRawDatas) {
    if (raw->mTimecode == aTimecode) {
      mRawDatas.RemoveElement(raw);
      return raw.forget();
    }
  }
  return nullptr;
}

already_AddRefed<BufferData>
OmxPromiseLayer::FindAndRemoveBufferHolder(OMX_DIRTYPE aType,
                                           BufferData::BufferID aId)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  RefPtr<BufferData> holder;
  BUFFERLIST* holders = GetBufferHolders(aType);

  for (uint32_t i = 0; i < holders->Length(); i++) {
    if (holders->ElementAt(i)->ID() == aId) {
      holder = holders->ElementAt(i);
      holders->RemoveElementAt(i);
      return holder.forget();
    }
  }

  return nullptr;
}

already_AddRefed<BufferData>
OmxPromiseLayer::FindBufferById(OMX_DIRTYPE aType, BufferData::BufferID aId)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  RefPtr<BufferData> holder;
  BUFFERLIST* holders = GetBufferHolders(aType);

  for (uint32_t i = 0; i < holders->Length(); i++) {
    if (holders->ElementAt(i)->ID() == aId) {
      holder = holders->ElementAt(i);
      return holder.forget();
    }
  }

  return nullptr;
}

void
OmxPromiseLayer::EmptyFillBufferDone(OMX_DIRTYPE aType, BufferData* aData)
{
  MOZ_ASSERT(!!aData);
  LOG("EmptyFillBufferDone: type %d, buffer %p", aType, aData->mBuffer);
  if (aData) {
    if (aType == OMX_DirOutput) {
      aData->mRawData = nullptr;
      aData->mRawData = FindAndRemoveRawData(aData->mBuffer->nTimeStamp);
    }
    aData->mStatus = BufferData::BufferStatus::OMX_CLIENT;
    aData->mPromise.Resolve(aData, __func__);
  }
}

void
OmxPromiseLayer::EmptyFillBufferDone(OMX_DIRTYPE aType, BufferData::BufferID aID)
{
  RefPtr<BufferData> holder = FindAndRemoveBufferHolder(aType, aID);
  MOZ_ASSERT(!!holder);
  LOG("EmptyFillBufferDone: type %d, buffer %p", aType, holder->mBuffer);
  if (holder) {
    if (aType == OMX_DirOutput) {
      holder->mRawData = nullptr;
      holder->mRawData = FindAndRemoveRawData(holder->mBuffer->nTimeStamp);
    }
    holder->mStatus = BufferData::BufferStatus::OMX_CLIENT;
    holder->mPromise.Resolve(holder, __func__);
  }
}

RefPtr<OmxPromiseLayer::OmxCommandPromise>
OmxPromiseLayer::SendCommand(OMX_COMMANDTYPE aCmd, OMX_U32 aParam1, OMX_PTR aCmdData)
{
  // No need to issue flush because of buffers are in client already.
  //
  // Some components fail to respond flush event when all of buffers are in
  // client.
  if (aCmd == OMX_CommandFlush) {
    bool needFlush = false;
    if ((aParam1 & OMX_DirInput && mInbufferHolders.Length()) ||
        (aParam1 & OMX_DirOutput && mOutbufferHolders.Length())) {
      needFlush = true;
    }
    if (!needFlush) {
      LOG("SendCommand: buffers are in client already, no need to flush");
      mRawDatas.Clear();
      return OmxCommandPromise::CreateAndResolve(OMX_CommandFlush, __func__);
    }
  }

  OMX_ERRORTYPE err = mPlatformLayer->SendCommand(aCmd, aParam1, aCmdData);
  if (err != OMX_ErrorNone) {
    OmxCommandFailureHolder failure(OMX_ErrorNotReady, aCmd);
    return OmxCommandPromise::CreateAndReject(failure, __func__);
  }

  RefPtr<OmxCommandPromise> p;
  if (aCmd == OMX_CommandStateSet) {
    p = mCommandStatePromise.Ensure(__func__);
  } else if (aCmd == OMX_CommandFlush) {
    p = mFlushPromise.Ensure(__func__);
    mFlushPortIndex = aParam1;
    // Clear all buffered raw data.
    mRawDatas.Clear();
  } else if (aCmd == OMX_CommandPortEnable) {
    p = mPortEnablePromise.Ensure(__func__);
  } else if (aCmd == OMX_CommandPortDisable) {
    p = mPortDisablePromise.Ensure(__func__);
  } else {
    LOG("SendCommand: error unsupport command");
    MOZ_ASSERT(0);
  }

  return p;
}

bool
OmxPromiseLayer::Event(OMX_EVENTTYPE aEvent, OMX_U32 aData1, OMX_U32 aData2)
{
  OMX_COMMANDTYPE cmd = (OMX_COMMANDTYPE) aData1;
  switch (aEvent) {
    case OMX_EventCmdComplete:
    {
      if (cmd == OMX_CommandStateSet) {
        mCommandStatePromise.Resolve(OMX_CommandStateSet, __func__);
      } else if (cmd == OMX_CommandFlush && mFlushPortIndex == aData2) {
        mFlushPromise.Resolve(OMX_CommandFlush, __func__);
      } else if (cmd == OMX_CommandPortDisable) {
        mPortDisablePromise.Resolve(OMX_CommandPortDisable, __func__);
      } else if (cmd == OMX_CommandPortEnable) {
        mPortEnablePromise.Resolve(OMX_CommandPortEnable, __func__);
      }
      break;
    }
    case OMX_EventError:
    {
      if (cmd == OMX_CommandStateSet) {
        OmxCommandFailureHolder failure(OMX_ErrorUndefined, OMX_CommandStateSet);
        mCommandStatePromise.Reject(failure, __func__);
      } else if (cmd == OMX_CommandFlush && mFlushPortIndex == aData2) {
        OmxCommandFailureHolder failure(OMX_ErrorUndefined, OMX_CommandFlush);
        mFlushPromise.Reject(failure, __func__);
      } else if (cmd == OMX_CommandPortDisable) {
        OmxCommandFailureHolder failure(OMX_ErrorUndefined, OMX_CommandPortDisable);
        mPortDisablePromise.Reject(failure, __func__);
      } else if (cmd == OMX_CommandPortEnable) {
        OmxCommandFailureHolder failure(OMX_ErrorUndefined, OMX_CommandPortEnable);
        mPortEnablePromise.Reject(failure, __func__);
      }
      break;
    }
    default:
    {
      return false;
    }
  }
  return true;
}

nsresult
OmxPromiseLayer::AllocateOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBuffers)
{
  return mPlatformLayer->AllocateOmxBuffer(aType, aBuffers);
}

nsresult
OmxPromiseLayer::ReleaseOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBuffers)
{
  return mPlatformLayer->ReleaseOmxBuffer(aType, aBuffers);
}

OMX_STATETYPE
OmxPromiseLayer::GetState()
{
  OMX_STATETYPE state;
  OMX_ERRORTYPE err = mPlatformLayer->GetState(&state);
  return err == OMX_ErrorNone ? state : OMX_StateInvalid;
}

OMX_ERRORTYPE
OmxPromiseLayer::GetParameter(OMX_INDEXTYPE aParamIndex,
                              OMX_PTR aComponentParameterStructure,
                              OMX_U32 aComponentParameterSize)
{
  return mPlatformLayer->GetParameter(aParamIndex,
                                      aComponentParameterStructure,
                                      aComponentParameterSize);
}

OMX_ERRORTYPE
OmxPromiseLayer::SetParameter(OMX_INDEXTYPE aParamIndex,
                              OMX_PTR aComponentParameterStructure,
                              OMX_U32 aComponentParameterSize)
{
  return mPlatformLayer->SetParameter(aParamIndex,
                                      aComponentParameterStructure,
                                      aComponentParameterSize);
}

nsresult
OmxPromiseLayer::Shutdown()
{
  LOG("Shutdown");
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(!GetBufferHolders(OMX_DirInput)->Length());
  MOZ_ASSERT(!GetBufferHolders(OMX_DirOutput)->Length());
  return mPlatformLayer->Shutdown();
}

}
