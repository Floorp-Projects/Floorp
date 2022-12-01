/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderParent_h
#define include_dom_media_ipc_RemoteDecoderParent_h

#include "mozilla/PRemoteDecoderParent.h"
#include "mozilla/ShmemRecycleAllocator.h"

namespace mozilla {

class RemoteDecoderManagerParent;
using mozilla::ipc::IPCResult;

class RemoteDecoderParent : public ShmemRecycleAllocator<RemoteDecoderParent>,
                            public PRemoteDecoderParent {
  friend class PRemoteDecoderParent;

 public:
  // We refcount this class since the task queue can have runnables
  // that reference us.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderParent)

  RemoteDecoderParent(RemoteDecoderManagerParent* aParent,
                      const CreateDecoderParams::OptionSet& aOptions,
                      nsISerialEventTarget* aManagerThread,
                      TaskQueue* aDecodeTaskQueue,
                      const Maybe<uint64_t>& aMediaEngineId,
                      Maybe<TrackingId> aTrackingId);

  void Destroy();

  // PRemoteDecoderParent
  virtual IPCResult RecvConstruct(ConstructResolver&& aResolver) = 0;
  IPCResult RecvInit(InitResolver&& aResolver);
  IPCResult RecvDecode(ArrayOfRemoteMediaRawData* aData,
                       DecodeResolver&& aResolver);
  IPCResult RecvFlush(FlushResolver&& aResolver);
  IPCResult RecvDrain(DrainResolver&& aResolver);
  IPCResult RecvShutdown(ShutdownResolver&& aResolver);
  IPCResult RecvSetSeekThreshold(const media::TimeUnit& aTime);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 protected:
  virtual ~RemoteDecoderParent();

  bool OnManagerThread();

  virtual MediaResult ProcessDecodedData(MediaDataDecoder::DecodedData&& aData,
                                         DecodedOutputIPDL& aDecodedData) = 0;

  const RefPtr<RemoteDecoderManagerParent> mParent;
  const CreateDecoderParams::OptionSet mOptions;
  const RefPtr<TaskQueue> mDecodeTaskQueue;
  RefPtr<MediaDataDecoder> mDecoder;
  const Maybe<TrackingId> mTrackingId;

  // Only be used on Windows when the media engine playback is enabled.
  const Maybe<uint64_t> mMediaEngineId;

 private:
  void DecodeNextSample(const RefPtr<ArrayOfRemoteMediaRawData>& aData,
                        size_t aIndex, MediaDataDecoder::DecodedData&& aOutput,
                        DecodeResolver&& aResolver);
  RefPtr<RemoteDecoderParent> mIPDLSelfRef;
  const RefPtr<nsISerialEventTarget> mManagerThread;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderParent_h
