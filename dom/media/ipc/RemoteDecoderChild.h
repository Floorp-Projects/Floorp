/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderChild_h
#define include_dom_media_ipc_RemoteDecoderChild_h
#include "mozilla/PRemoteDecoderChild.h"

#include "IRemoteDecoderChild.h"
#include "mozilla/ShmemPool.h"

namespace mozilla {

class RemoteDecoderManagerChild;
using mozilla::MediaDataDecoder;
using mozilla::ipc::IPCResult;

class RemoteDecoderChild : public PRemoteDecoderChild,
                           public IRemoteDecoderChild {
  friend class PRemoteDecoderChild;

 public:
  explicit RemoteDecoderChild(bool aRecreatedOnCrash = false);

  // PRemoteDecoderChild
  virtual IPCResult RecvOutput(const DecodedOutputIPDL& aDecodedData) = 0;
  IPCResult RecvInputExhausted();
  IPCResult RecvDrainComplete();
  IPCResult RecvError(const nsresult& aError);
  IPCResult RecvInitComplete(const TrackInfo::TrackType& trackType,
                             const nsCString& aDecoderDescription,
                             const bool& aHardware,
                             const nsCString& aHardwareReason,
                             const ConversionRequired& aConversion);
  IPCResult RecvInitFailed(const nsresult& aReason);
  IPCResult RecvFlushComplete();
  IPCResult RecvShutdownComplete();
  IPCResult RecvDoneWithInput(Shmem&& aInputShmem);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  // IRemoteDecoderChild
  RefPtr<MediaDataDecoder::InitPromise> Init() override;
  RefPtr<MediaDataDecoder::DecodePromise> Decode(
      MediaRawData* aSample) override;
  RefPtr<MediaDataDecoder::DecodePromise> Drain() override;
  RefPtr<MediaDataDecoder::FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  nsCString GetDescriptionName() const override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  MediaDataDecoder::ConversionRequired NeedsConversion() const override;
  void DestroyIPDL() override;

  // Called from IPDL when our actor has been destroyed
  void IPDLActorDestroyed();

  RemoteDecoderManagerChild* GetManager();

 protected:
  virtual ~RemoteDecoderChild();
  void AssertOnManagerThread() const;

  virtual void RecordShutdownTelemetry(bool aForAbnormalShutdown) {}

  RefPtr<RemoteDecoderChild> mIPDLSelfRef;
  bool mCanSend = false;
  MediaDataDecoder::DecodedData mDecodedData;

 private:
  RefPtr<nsIThread> mThread;

  MozPromiseHolder<MediaDataDecoder::InitPromise> mInitPromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDecodePromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDrainPromise;
  MozPromiseHolder<MediaDataDecoder::FlushPromise> mFlushPromise;
  MozPromiseHolder<ShutdownPromise> mShutdownPromise;

  TimeStamp mRemoteProcessCrashTime;

  nsCString mHardwareAcceleratedReason;
  nsCString mDescription;
  bool mInitialized = false;
  bool mIsHardwareAccelerated = false;
  // Set to true if the actor got destroyed and we haven't yet notified the
  // caller.
  bool mNeedNewDecoder = false;
  const bool mRecreatedOnCrash;
  MediaDataDecoder::ConversionRequired mConversion =
      MediaDataDecoder::ConversionRequired::kNeedNone;
  // Keep this instance alive during SendShutdown RecvShutdownComplete
  // handshake.
  RefPtr<RemoteDecoderChild> mShutdownSelfRef;
  ShmemPool mRawFramePool;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderChild_h
