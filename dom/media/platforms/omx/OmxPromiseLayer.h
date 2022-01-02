/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(OmxPromiseLayer_h_)
#  define OmxPromiseLayer_h_

#  include "mozilla/MozPromise.h"
#  include "mozilla/TaskQueue.h"

#  include "OMX_Core.h"
#  include "OMX_Types.h"

namespace mozilla {

namespace layers {
class ImageContainer;
}

class MediaData;
class MediaRawData;
class OmxDataDecoder;
class OmxPlatformLayer;
class TrackInfo;

/* This class acts as a middle layer between OmxDataDecoder and the underlying
 * OmxPlatformLayer.
 *
 * This class has two purposes:
 * 1. Using promise instead of OpenMax async callback function.
 *    For example, OmxCommandPromise is used for OpenMax IL SendCommand.
 * 2. Manage the buffer exchanged between client and component.
 *    Because omx buffer works crossing threads, so each omx buffer has its own
 *    promise, it is defined in BufferData.
 *
 * All of functions and members in this class should be run in the same
 * TaskQueue.
 */
class OmxPromiseLayer {
 protected:
  virtual ~OmxPromiseLayer() = default;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OmxPromiseLayer)

  OmxPromiseLayer(TaskQueue* aTaskQueue, OmxDataDecoder* aDataDecoder,
                  layers::ImageContainer* aImageContainer);

  class BufferData;

  typedef nsTArray<RefPtr<BufferData>> BUFFERLIST;

  class OmxBufferFailureHolder {
   public:
    OmxBufferFailureHolder(OMX_ERRORTYPE aError, BufferData* aBuffer)
        : mError(aError), mBuffer(aBuffer) {}

    OMX_ERRORTYPE mError;
    BufferData* mBuffer;
  };

  typedef MozPromise<BufferData*, OmxBufferFailureHolder,
                     /* IsExclusive = */ false>
      OmxBufferPromise;

  class OmxCommandFailureHolder {
   public:
    OmxCommandFailureHolder(OMX_ERRORTYPE aErrorType,
                            OMX_COMMANDTYPE aCommandType)
        : mErrorType(aErrorType), mCommandType(aCommandType) {}

    OMX_ERRORTYPE mErrorType;
    OMX_COMMANDTYPE mCommandType;
  };

  typedef MozPromise<OMX_COMMANDTYPE, OmxCommandFailureHolder,
                     /* IsExclusive = */ true>
      OmxCommandPromise;

  typedef MozPromise<uint32_t, bool, /* IsExclusive = */ true>
      OmxPortConfigPromise;

  // TODO: maybe a generic promise is good enough for this case?
  RefPtr<OmxCommandPromise> Init(const TrackInfo* aInfo);

  OMX_ERRORTYPE Config();

  RefPtr<OmxBufferPromise> FillBuffer(BufferData* aData);

  RefPtr<OmxBufferPromise> EmptyBuffer(BufferData* aData);

  RefPtr<OmxCommandPromise> SendCommand(OMX_COMMANDTYPE aCmd, OMX_U32 aParam1,
                                        OMX_PTR aCmdData);

  nsresult AllocateOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBuffers);

  nsresult ReleaseOmxBuffer(OMX_DIRTYPE aType, BUFFERLIST* aBuffers);

  OMX_STATETYPE GetState();

  OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE aParamIndex,
                             OMX_PTR aComponentParameterStructure,
                             OMX_U32 aComponentParameterSize);

  OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nIndex,
                             OMX_PTR aComponentParameterStructure,
                             OMX_U32 aComponentParameterSize);

  OMX_U32 InputPortIndex();

  OMX_U32 OutputPortIndex();

  nsresult Shutdown();

  // BufferData maintains the status of OMX buffer (OMX_BUFFERHEADERTYPE).
  // mStatus tracks the buffer owner.
  // And a promise because OMX buffer working among different threads.
  class BufferData {
   protected:
    virtual ~BufferData() = default;

   public:
    explicit BufferData(OMX_BUFFERHEADERTYPE* aBuffer)
        : mEos(false), mStatus(BufferStatus::FREE), mBuffer(aBuffer) {}

    typedef void* BufferID;

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BufferData)

    // In most cases, the ID of this buffer is the pointer address of mBuffer.
    // However, on some platforms it may be another value.
    virtual BufferID ID() { return mBuffer; }

    // Return the platform dependent MediaData().
    // For example, it returns the MediaData with Gralloc texture.
    // If it returns nullptr, then caller uses the normal way to
    // create MediaData().
    virtual already_AddRefed<MediaData> GetPlatformMediaData() {
      return nullptr;
    }

    // The buffer could be used by several objects. And only one object owns the
    // buffer the same time.
    //   FREE:
    //     nobody uses it.
    //
    //   OMX_COMPONENT:
    //     buffer is used by OMX component (OmxPlatformLayer).
    //
    //   OMX_CLIENT:
    //     buffer is used by client which is wait for audio/video playing
    //     (OmxDataDecoder)
    //
    //   OMX_CLIENT_OUTPUT:
    //     used by client to output decoded data (for example, Gecko layer in
    //     this case)
    //
    // For output port buffer, the status transition is:
    // FREE -> OMX_COMPONENT -> OMX_CLIENT -> OMX_CLIENT_OUTPUT -> FREE
    //
    // For input port buffer, the status transition is:
    // FREE -> OMX_COMPONENT -> OMX_CLIENT -> FREE
    //
    enum BufferStatus {
      FREE,
      OMX_COMPONENT,
      OMX_CLIENT,
      OMX_CLIENT_OUTPUT,
      INVALID
    };

    bool mEos;

    // The raw keeps in OmxPromiseLayer after EmptyBuffer and then passing to
    // output decoded buffer in EmptyFillBufferDone. It is used to keep the
    // records of the original data from demuxer, like duration, stream
    // offset...etc.
    RefPtr<MediaRawData> mRawData;

    // Because OMX buffer works across threads, so it uses a promise
    // for each buffer when the buffer is used by Omx component.
    MozPromiseHolder<OmxBufferPromise> mPromise;
    BufferStatus mStatus;
    OMX_BUFFERHEADERTYPE* mBuffer;
  };

  void EmptyFillBufferDone(OMX_DIRTYPE aType, BufferData::BufferID aID);

  void EmptyFillBufferDone(OMX_DIRTYPE aType, BufferData* aData);

  already_AddRefed<BufferData> FindBufferById(OMX_DIRTYPE aType,
                                              BufferData::BufferID aId);

  already_AddRefed<BufferData> FindAndRemoveBufferHolder(
      OMX_DIRTYPE aType, BufferData::BufferID aId);

  // Return true if event is handled.
  bool Event(OMX_EVENTTYPE aEvent, OMX_U32 aData1, OMX_U32 aData2);

 protected:
  struct FlushCommand {
    OMX_DIRTYPE type;
    OMX_PTR cmd;
  };

  BUFFERLIST* GetBufferHolders(OMX_DIRTYPE aType);

  already_AddRefed<MediaRawData> FindAndRemoveRawData(OMX_TICKS aTimecode);

  RefPtr<TaskQueue> mTaskQueue;

  MozPromiseHolder<OmxCommandPromise> mCommandStatePromise;

  MozPromiseHolder<OmxCommandPromise> mPortDisablePromise;

  MozPromiseHolder<OmxCommandPromise> mPortEnablePromise;

  MozPromiseHolder<OmxCommandPromise> mFlushPromise;

  nsTArray<FlushCommand> mFlushCommands;

  UniquePtr<OmxPlatformLayer> mPlatformLayer;

 private:
  // Elements are added to holders when FillBuffer() or FillBuffer(). And
  // removing element when the promise is resolved. Buffers in these lists
  // should NOT be used by other component; for example, output it to audio
  // output. These lists should be empty when engine is about to shutdown.
  //
  // Note:
  //      There bufferlist should not be used by other class directly.
  BUFFERLIST mInbufferHolders;

  BUFFERLIST mOutbufferHolders;

  nsTArray<RefPtr<MediaRawData>> mRawDatas;
};

}  // namespace mozilla

#endif /* OmxPromiseLayer_h_ */
