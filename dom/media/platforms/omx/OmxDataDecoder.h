/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(OmxDataDecoder_h_)
#define OmxDataDecoder_h_

#include "mozilla/Monitor.h"

#include "AudioCompactor.h"
#include "ImageContainer.h"
#include "MediaInfo.h"
#include "PlatformDecoderModule.h"

#include "OMX_Component.h"

#include "OmxPromiseLayer.h"

namespace mozilla {

class MediaDataHelper;

typedef OmxPromiseLayer::OmxCommandPromise OmxCommandPromise;
typedef OmxPromiseLayer::OmxBufferPromise OmxBufferPromise;
typedef OmxPromiseLayer::OmxBufferFailureHolder OmxBufferFailureHolder;
typedef OmxPromiseLayer::OmxCommandFailureHolder OmxCommandFailureHolder;
typedef OmxPromiseLayer::BufferData BufferData;
typedef OmxPromiseLayer::BUFFERLIST BUFFERLIST;

/* OmxDataDecoder is the major class which performs followings:
 *   1. Translate PDM function into OMX commands.
 *   2. Keeping the buffers between client and component.
 *   3. Manage the OMX state.
 *
 * From the definition in OpenMax spec. "2.2.1", there are 3 major roles in
 * OpenMax IL.
 *
 * IL client:
 *   "The IL client may be a layer below the GUI application, such as GStreamer,
 *   or may be several layers below the GUI layer."
 *
 *   OmxDataDecoder acts as the IL client.
 *
 * OpenMAX IL component:
 *   "A component that is intended to wrap functionality that is required in the
 *   target system."
 *
 *   OmxPromiseLayer acts as the OpenMAX IL component.
 *
 * OpenMAX IL core:
 *   "Platform-specific code that has the functionality necessary to locate and
 *   then load an OpenMAX IL component into main memory."
 *
 *   OmxPlatformLayer acts as the OpenMAX IL core.
 */
class OmxDataDecoder : public MediaDataDecoder {
protected:
  virtual ~OmxDataDecoder();

public:
  OmxDataDecoder(const TrackInfo& aTrackInfo,
                 MediaDataDecoderCallback* aCallback,
                 layers::ImageContainer* aImageContainer);

  RefPtr<InitPromise> Init() override;

  nsresult Input(MediaRawData* aSample) override;

  nsresult Flush() override;

  nsresult Drain() override;

  nsresult Shutdown() override;

  const char* GetDescriptionName() const override
  {
    return "omx decoder";
  }

  // Return true if event is handled.
  bool Event(OMX_EVENTTYPE aEvent, OMX_U32 aData1, OMX_U32 aData2);

protected:
  void InitializationTask();

  void ResolveInitPromise(const char* aMethodName);

  void RejectInitPromise(DecoderFailureReason aReason, const char* aMethodName);

  void OmxStateRunner();

  void FillAndEmptyBuffers();

  void FillBufferDone(BufferData* aData);

  void FillBufferFailure(OmxBufferFailureHolder aFailureHolder);

  void EmptyBufferDone(BufferData* aData);

  void EmptyBufferFailure(OmxBufferFailureHolder aFailureHolder);

  void NotifyError(OMX_ERRORTYPE aOmxError,
                   const char* aLine,
                   MediaDataDecoderError aError = MediaDataDecoderError::FATAL_ERROR);

  // Configure audio/video codec.
  // Some codec may just ignore this and rely on codec specific data in
  // FillCodecConfigDataToOmx().
  void ConfigCodec();

  // Sending codec specific data to OMX component. OMX component could send a
  // OMX_EventPortSettingsChanged back to client. And then client needs to
  // disable port and reallocate buffer.
  void FillCodecConfigDataToOmx();

  void SendEosBuffer();

  void EndOfStream();

  // It could be called after codec specific data is sent and component found
  // the port format is changed due to different codec specific.
  void PortSettingsChanged();

  void Output(BufferData* aData);

  // Buffer can be released if its status is not OMX_COMPONENT or
  // OMX_CLIENT_OUTPUT.
  bool BuffersCanBeReleased(OMX_DIRTYPE aType);

  OMX_DIRTYPE GetPortDirection(uint32_t aPortIndex);

  void DoAsyncShutdown();

  void DoFlush();

  void FlushComplete(OMX_COMMANDTYPE aCommandType);

  void FlushFailure(OmxCommandFailureHolder aFailureHolder);

  BUFFERLIST* GetBuffers(OMX_DIRTYPE aType);

  nsresult AllocateBuffers(OMX_DIRTYPE aType);

  nsresult ReleaseBuffers(OMX_DIRTYPE aType);

  BufferData* FindAvailableBuffer(OMX_DIRTYPE aType);

  // aType could be OMX_DirMax for all types.
  RefPtr<OmxPromiseLayer::OmxBufferPromise::AllPromiseType>
  CollectBufferPromises(OMX_DIRTYPE aType);

  Monitor mMonitor;

  // The Omx TaskQueue.
  RefPtr<TaskQueue> mOmxTaskQueue;

  RefPtr<TaskQueue> mReaderTaskQueue;

  RefPtr<layers::ImageContainer> mImageContainer;

  WatchManager<OmxDataDecoder> mWatchManager;

  // It is accessed in omx TaskQueue.
  Watchable<OMX_STATETYPE> mOmxState;

  RefPtr<OmxPromiseLayer> mOmxLayer;

  UniquePtr<TrackInfo> mTrackInfo;

  // It is accessed in both omx and reader TaskQueue.
  Atomic<bool> mFlushing;

  // It is accessed in Omx/reader TaskQeueu.
  Atomic<bool> mShuttingDown;

  // It is accessed in Omx TaskQeueu.
  bool mCheckingInputExhausted;

  // It is accessed in reader TaskQueue.
  MozPromiseHolder<InitPromise> mInitPromise;

  // It is written in Omx TaskQeueu. Read in Omx TaskQueue.
  // It value means the port index which port settings is changed.
  // -1 means no port setting changed.
  //
  // Note: when port setting changed, there should be no buffer operations
  //       via EmptyBuffer or FillBuffer.
  Watchable<int32_t> mPortSettingsChanged;

  // It is access in Omx TaskQueue.
  nsTArray<RefPtr<MediaRawData>> mMediaRawDatas;

  BUFFERLIST mInPortBuffers;

  BUFFERLIST mOutPortBuffers;

  RefPtr<MediaDataHelper> mMediaDataHelper;

  MediaDataDecoderCallback* mCallback;
};

template<class T>
void InitOmxParameter(T* aParam)
{
  PodZero(aParam);
  aParam->nSize = sizeof(T);
  aParam->nVersion.s.nVersionMajor = 1;
}

}

#endif /* OmxDataDecoder_h_ */
