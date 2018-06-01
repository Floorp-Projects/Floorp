/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxPromiseLayer.h"

#include "ImageContainer.h"

#include "OmxDataDecoder.h"
#include "OmxPlatformLayer.h"


#ifdef LOG
#undef LOG
#endif

#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("OmxPromiseLayer(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

OmxPromiseLayer::OmxPromiseLayer(TaskQueue* aTaskQueue,
                                 OmxDataDecoder* aDataDecoder,
                                 layers::ImageContainer* aImageContainer)
  : mTaskQueue(aTaskQueue)
{
  mPlatformLayer = OmxPlatformLayer::Create(aDataDecoder,
                                            this,
                                            aTaskQueue,
                                            aImageContainer);
  MOZ_ASSERT(!!mPlatformLayer);
}

RefPtr<OmxPromiseLayer::OmxCommandPromise>
OmxPromiseLayer::Init(const TrackInfo* aInfo)
{
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

OMX_ERRORTYPE
OmxPromiseLayer::Config()
{
  MOZ_ASSERT(GetState() == OMX_StateLoaded);

  return mPlatformLayer->Config();
}

RefPtr<OmxPromiseLayer::OmxBufferPromise>
OmxPromiseLayer::FillBuffer(BufferData* aData)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  LOG("buffer %p", aData->mBuffer);

  RefPtr<OmxBufferPromise> p = aData->mPromise.Ensure(__func__);

  OMX_ERRORTYPE err = mPlatformLayer->FillThisBuffer(aData);

  if (err != OMX_ErrorNone) {
    OmxBufferFailureHolder failure(err, aData);
    aData->mPromise.Reject(std::move(failure), __func__);
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
  LOG("buffer %p, size %lu", aData->mBuffer, aData->mBuffer->nFilledLen);

  RefPtr<OmxBufferPromise> p = aData->mPromise.Ensure(__func__);

  OMX_ERRORTYPE err = mPlatformLayer->EmptyThisBuffer(aData);

  if (err != OMX_ErrorNone) {
    OmxBufferFailureHolder failure(err, aData);
    aData->mPromise.Reject(std::move(failure), __func__);
  } else {
    if (aData->mRawData) {
      mRawDatas.AppendElement(std::move(aData->mRawData));
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
    if (raw->mTime.ToMicroseconds() == aTimecode) {
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
  if (aData) {
    LOG("type %d, buffer %p", aType, aData->mBuffer);
    if (aType == OMX_DirOutput) {
      aData->mRawData = nullptr;
      aData->mRawData = FindAndRemoveRawData(aData->mBuffer->nTimeStamp);
    }
    aData->mStatus = BufferData::BufferStatus::OMX_CLIENT;
    aData->mPromise.Resolve(aData, __func__);
  } else {
    LOG("type %d, no buffer", aType);
  }
}

void
OmxPromiseLayer::EmptyFillBufferDone(OMX_DIRTYPE aType, BufferData::BufferID aID)
{
  RefPtr<BufferData> holder = FindAndRemoveBufferHolder(aType, aID);
  EmptyFillBufferDone(aType, holder);
}

RefPtr<OmxPromiseLayer::OmxCommandPromise>
OmxPromiseLayer::SendCommand(OMX_COMMANDTYPE aCmd, OMX_U32 aParam1, OMX_PTR aCmdData)
{
  if (aCmd == OMX_CommandFlush) {
    // It doesn't support another flush commands before previous one is completed.
    MOZ_RELEASE_ASSERT(!mFlushCommands.Length());

    // Some coomponents don't send event with OMX_ALL, they send flush complete
    // event with input port and another event for output port.
    // In prupose of better compatibility, we interpret the OMX_ALL to OMX_DirInput
    // and OMX_DirOutput flush separately.
    OMX_DIRTYPE types[] = {OMX_DIRTYPE::OMX_DirInput, OMX_DIRTYPE::OMX_DirOutput};
    for(const auto type : types) {
      if ((aParam1 == type) || (aParam1 == OMX_ALL)) {
        mFlushCommands.AppendElement(FlushCommand({type, aCmdData}));
      }

      if (type == OMX_DirInput) {
        // Clear all buffered raw data.
        mRawDatas.Clear();
      }
    }

    // Don't overlay more than one flush command, some components can't overlay flush commands.
    // So here we send another flush after receiving the previous flush completed event.
    if (mFlushCommands.Length()) {
      OMX_ERRORTYPE err =
        mPlatformLayer->SendCommand(OMX_CommandFlush,
                                    mFlushCommands.ElementAt(0).type,
                                    mFlushCommands.ElementAt(0).cmd);
      if (err != OMX_ErrorNone) {
        OmxCommandFailureHolder failure(OMX_ErrorNotReady, OMX_CommandFlush);
        return OmxCommandPromise::CreateAndReject(failure, __func__);
      }
    } else {
      LOG("OMX_CommandFlush parameter error");
      OmxCommandFailureHolder failure(OMX_ErrorNotReady, OMX_CommandFlush);
      return OmxCommandPromise::CreateAndReject(failure, __func__);
    }
  } else {
    OMX_ERRORTYPE err = mPlatformLayer->SendCommand(aCmd, aParam1, aCmdData);
    if (err != OMX_ErrorNone) {
      OmxCommandFailureHolder failure(OMX_ErrorNotReady, aCmd);
      return OmxCommandPromise::CreateAndReject(failure, __func__);
    }
  }

  RefPtr<OmxCommandPromise> p;
  if (aCmd == OMX_CommandStateSet) {
    p = mCommandStatePromise.Ensure(__func__);
  } else if (aCmd == OMX_CommandFlush) {
    p = mFlushPromise.Ensure(__func__);
  } else if (aCmd == OMX_CommandPortEnable) {
    p = mPortEnablePromise.Ensure(__func__);
  } else if (aCmd == OMX_CommandPortDisable) {
    p = mPortDisablePromise.Ensure(__func__);
  } else {
    LOG("error unsupport command");
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
      } else if (cmd == OMX_CommandFlush) {
        MOZ_RELEASE_ASSERT(mFlushCommands.ElementAt(0).type == aData2);
        LOG("OMX_CommandFlush completed port type %lu", aData2);
        mFlushCommands.RemoveElementAt(0);

        // Sending next flush command.
        if (mFlushCommands.Length()) {
          OMX_ERRORTYPE err =
            mPlatformLayer->SendCommand(OMX_CommandFlush,
                                        mFlushCommands.ElementAt(0).type,
                                        mFlushCommands.ElementAt(0).cmd);
          if (err != OMX_ErrorNone) {
            OmxCommandFailureHolder failure(OMX_ErrorNotReady, OMX_CommandFlush);
            mFlushPromise.Reject(failure, __func__);
          }
        } else {
          mFlushPromise.Resolve(OMX_CommandFlush, __func__);
        }
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
      } else if (cmd == OMX_CommandFlush) {
        OmxCommandFailureHolder failure(OMX_ErrorUndefined, OMX_CommandFlush);
        mFlushPromise.Reject(failure, __func__);
      } else if (cmd == OMX_CommandPortDisable) {
        OmxCommandFailureHolder failure(OMX_ErrorUndefined, OMX_CommandPortDisable);
        mPortDisablePromise.Reject(failure, __func__);
      } else if (cmd == OMX_CommandPortEnable) {
        OmxCommandFailureHolder failure(OMX_ErrorUndefined, OMX_CommandPortEnable);
        mPortEnablePromise.Reject(failure, __func__);
      } else {
        return false;
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

OMX_U32
OmxPromiseLayer::InputPortIndex()
{
  return mPlatformLayer->InputPortIndex();
}

OMX_U32
OmxPromiseLayer::OutputPortIndex()
{
  return mPlatformLayer->OutputPortIndex();
}

nsresult
OmxPromiseLayer::Shutdown()
{
  LOG("");
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  MOZ_ASSERT(!GetBufferHolders(OMX_DirInput)->Length());
  MOZ_ASSERT(!GetBufferHolders(OMX_DirOutput)->Length());
  return mPlatformLayer->Shutdown();
}

}
