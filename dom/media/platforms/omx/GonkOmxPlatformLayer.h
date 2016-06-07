/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GonkOmxPlatformLayer_h_)
#define GonkOmxPlatformLayer_h_

#pragma GCC visibility push(default)

#include <bitset>

#include <utils/RefBase.h>
#include <media/stagefright/OMXClient.h>
#include "nsAutoPtr.h"

#include "OMX_Component.h"

#include "OmxPlatformLayer.h"

class nsACString;

namespace android {
class IMemory;
class MemoryDealer;
}

namespace mozilla {

class GonkOmxObserver;
class GonkOmxPlatformLayer;
class GonkTextureClientRecycleHandler;

/*
 * Due to Android's omx node could live in local process (client) or remote
 * process (mediaserver). And there are 3 kinds of buffer in Android OMX.
 *
 * 1.
 * When buffer is in local process, the IOMX::buffer_id is OMX_BUFFERHEADERTYPE
 * pointer actually, it is safe to use it directly.
 *
 * 2.
 * When buffer is in remote process, the OMX_BUFFERHEADERTYPE pointer is 'IN' the
 * remote process. It can't be used in local process, so here it allocates a
 * local OMX_BUFFERHEADERTYPE. The raw/decoded data is in the android shared
 * memory, IMemory.
 *
 * 3.
 * When buffer is in remote process for the display output port. It uses
 * GraphicBuffer to accelerate the decoding and display.
 *
 */
class GonkBufferData : public OmxPromiseLayer::BufferData {
protected:
  virtual ~GonkBufferData() {}

public:
  GonkBufferData(bool aLiveInLocal,
                 GonkOmxPlatformLayer* aLayer);

  BufferID ID() override
  {
    return mId;
  }

  already_AddRefed<MediaData> GetPlatformMediaData() override;

  bool IsLocalBuffer()
  {
    return !!mMirrorBuffer.get();
  }

  void ReleaseBuffer();

  nsresult SetBufferId(android::IOMX::buffer_id aId)
  {
    mId = aId;
    return NS_OK;
  }

  // The mBuffer is in local process. And aId is actually the OMX_BUFFERHEADERTYPE
  // pointer. It doesn't need a mirror buffer.
  nsresult InitLocalBuffer(android::IOMX::buffer_id aId);

  // aMemory is an IPC based memory which will be used as the pBuffer in
  // mBuffer. And the mBuffer will be the mirror OMX_BUFFERHEADERTYPE
  // of the one in the remote process.
  nsresult InitSharedMemory(android::IMemory* aMemory);

  // GraphicBuffer is for video decoding acceleration on output port.
  // Then mBuffer is the mirror OMX_BUFFERHEADERTYPE of the one in the remote
  // process.
  nsresult InitGraphicBuffer(OMX_VIDEO_PORTDEFINITIONTYPE& aDef);

  // Android OMX uses this id to pass the buffer between OMX component and
  // client.
  android::IOMX::buffer_id mId;

  // mMirrorBuffer are used only when the omx node is in mediaserver.
  // Due to IPC problem, the mId is the OMX_BUFFERHEADERTYPE address in mediaserver.
  // It can't mapping to client process, so we need a local OMX_BUFFERHEADERTYPE
  // here to mirror the remote OMX_BUFFERHEADERTYPE in mediaserver.
  nsAutoPtr<OMX_BUFFERHEADERTYPE> mMirrorBuffer;

  // It creates GraphicBuffer and manages TextureClient.
  RefPtr<GonkTextureClientRecycleHandler> mTextureClientRecycleHandler;

  GonkOmxPlatformLayer* mGonkPlatformLayer;
};

class GonkOmxPlatformLayer : public OmxPlatformLayer {
public:
  enum {
    kRequiresAllocateBufferOnInputPorts = 0,
    kRequiresAllocateBufferOnOutputPorts,
    QUIRKS,
  };
  typedef std::bitset<QUIRKS> Quirks;

  struct ComponentInfo {
    const char* mName;
    Quirks mQuirks;
  };

  GonkOmxPlatformLayer(OmxDataDecoder* aDataDecoder,
                       OmxPromiseLayer* aPromiseLayer,
                       TaskQueue* aTaskQueue,
                       layers::ImageContainer* aImageContainer);

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

  static bool FindComponents(const nsACString& aMimeType,
                             nsTArray<ComponentInfo>* aComponents = nullptr);

  // Android/QCOM decoder uses its own OMX_VIDEO_CodingVP8 definition in
  // frameworks/native/media/include/openmax/OMX_Video.h, not the one defined
  // in OpenMAX v1.1.2 OMX_VideoExt.h
  OMX_VIDEO_CODINGTYPE CompressionFormat() override;

protected:
  friend GonkBufferData;

  layers::ImageContainer* GetImageContainer();

  const TrackInfo* GetTrackInfo();

  TaskQueue* GetTaskQueue()
  {
    return mTaskQueue;
  }

  nsresult EnableOmxGraphicBufferPort(OMX_PARAM_PORTDEFINITIONTYPE& aDef);

  bool LoadComponent(const ComponentInfo& aComponent);

  friend class GonkOmxObserver;

  RefPtr<TaskQueue> mTaskQueue;

  RefPtr<layers::ImageContainer> mImageContainer;

  // OMX_DirInput is 0, OMX_DirOutput is 1.
  android::sp<android::MemoryDealer> mMemoryDealer[2];

  android::sp<GonkOmxObserver> mOmxObserver;

  android::sp<android::IOMX> mOmx;

  android::IOMX::node_id mNode;

  android::OMXClient mOmxClient;

  Quirks mQuirks;
};

}

#pragma GCC visibility pop

#endif // GonkOmxPlatformLayer_h_
