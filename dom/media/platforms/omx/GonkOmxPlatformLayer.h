/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkOmxPlatformLayer_h_)
#define GonkOmxPlatformLayer_h_

#pragma GCC visibility push(default)

#include "OmxPlatformLayer.h"
#include "OMX_Component.h"
#include <utils/RefBase.h>
#include <media/stagefright/OMXClient.h>

namespace android {
class MemoryDealer;
class IMemory;
}

namespace mozilla {

class GonkOmxObserver;

/*
 * Due to Android's omx node could live in local process (client) or remote
 * process (mediaserver).
 *
 * When it is in local process, the IOMX::buffer_id is OMX_BUFFERHEADERTYPE
 * pointer actually, it is safe to use it directly.
 *
 * When it is in remote process, the OMX_BUFFERHEADERTYPE pointer is 'IN' the
 * remote process. It can't be used in local process, so here it allocates a
 * local OMX_BUFFERHEADERTYPE.
 */
class GonkBufferData : public OmxPromiseLayer::BufferData {
protected:
  virtual ~GonkBufferData() {}

public:
  // aMemory is an IPC based memory which will be used as the pBuffer in
  // mLocalBuffer.
  GonkBufferData(android::IOMX::buffer_id aId, bool aLiveInLocal, android::IMemory* aMemory);

  BufferID ID() override
  {
    return mId;
  }

  bool IsLocalBuffer()
  {
    return !!mLocalBuffer.get();
  }

  // Android OMX uses this id to pass the buffer between OMX component and
  // client.
  android::IOMX::buffer_id mId;

  // mLocalBuffer are used only when the omx node is in mediaserver.
  // Due to IPC problem, the mId is the OMX_BUFFERHEADERTYPE address in mediaserver.
  // It can't mapping to client process, so we need a local OMX_BUFFERHEADERTYPE
  // here.
  nsAutoPtr<OMX_BUFFERHEADERTYPE> mLocalBuffer;
};

class GonkOmxPlatformLayer : public OmxPlatformLayer {
public:
  GonkOmxPlatformLayer(OmxDataDecoder* aDataDecoder,
                       OmxPromiseLayer* aPromiseLayer,
                       TaskQueue* aTaskQueue);

  nsresult AllocateOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBufferList) override;

  nsresult ReleaseOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBufferList) override;

  OMX_ERRORTYPE GetState(OMX_STATETYPE* aType) override;

  OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE aParamIndex,
                             OMX_PTR aComponentParameterStructure,
                             OMX_U32 aComponentParameterSize) override;

  OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nIndex,
                             OMX_PTR aComponentParameterStructure,
                             OMX_U32 aComponentParameterSize) override;

  OMX_ERRORTYPE InitOmxToStateLoaded(const TrackInfo* aInfo) override;

  OMX_ERRORTYPE EmptyThisBuffer(BufferData* aData) override;

  OMX_ERRORTYPE FillThisBuffer(BufferData* aData) override;

  OMX_ERRORTYPE SendCommand(OMX_COMMANDTYPE aCmd,
                            OMX_U32 aParam1,
                            OMX_PTR aCmdData) override;

  nsresult Shutdown() override;

  // TODO:
  // There is another InitOmxParameter in OmxDataDecoder. They need to combinate
  // to one function.
  template<class T> void InitOmxParameter(T* aParam);

protected:
  bool LoadComponent(const char* aName);

  friend class GonkOmxObserver;

  RefPtr<TaskQueue> mTaskQueue;

  // OMX_DirInput is 0, OMX_DirOutput is 1.
  android::sp<android::MemoryDealer> mMemoryDealer[2];

  android::sp<GonkOmxObserver> mOmxObserver;

  android::sp<android::IOMX> mOmx;

  android::IOMX::node_id mNode;

  android::OMXClient mOmxClient;

  uint32_t mQuirks;

  bool mUsingHardwareCodec;
};

}

#pragma GCC visibility pop

#endif // GonkOmxPlatformLayer_h_
