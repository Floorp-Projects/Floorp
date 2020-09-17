/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderChild_h
#define include_dom_media_ipc_RemoteDecoderChild_h
#include "mozilla/PRemoteDecoderChild.h"

#include <functional>
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

  void ActorDestroy(ActorDestroyReason aWhy) override;

  // IRemoteDecoderChild
  RefPtr<MediaDataDecoder::InitPromise> Init() override;
  RefPtr<MediaDataDecoder::DecodePromise> Decode(
      const nsTArray<RefPtr<MediaRawData>>& aSamples) override;
  RefPtr<MediaDataDecoder::DecodePromise> Drain() override;
  RefPtr<MediaDataDecoder::FlushPromise> Flush() override;
  RefPtr<mozilla::ShutdownPromise> Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  nsCString GetDescriptionName() const override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  MediaDataDecoder::ConversionRequired NeedsConversion() const override;
  void DestroyIPDL() override;

  // Called from IPDL when our actor has been destroyed
  void IPDLActorDestroyed();

  RemoteDecoderManagerChild* GetManager();

 protected:
  virtual ~RemoteDecoderChild() = default;
  void AssertOnManagerThread() const;

  virtual MediaResult ProcessOutput(DecodedOutputIPDL&& aDecodedData) = 0;
  virtual void RecordShutdownTelemetry(bool aForAbnormalShutdown) {}

  RefPtr<RemoteDecoderChild> mIPDLSelfRef;
  MediaDataDecoder::DecodedData mDecodedData;

 private:
  const nsCOMPtr<nsISerialEventTarget> mThread;

  MozPromiseHolder<MediaDataDecoder::InitPromise> mInitPromise;
  MozPromiseRequestHolder<PRemoteDecoderChild::InitPromise> mInitPromiseRequest;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDecodePromise;
  MozPromiseHolder<MediaDataDecoder::DecodePromise> mDrainPromise;
  MozPromiseHolder<MediaDataDecoder::FlushPromise> mFlushPromise;
  MozPromiseHolder<mozilla::ShutdownPromise> mShutdownPromise;

  void HandleRejectionError(
      const ipc::ResponseRejectReason& aReason,
      std::function<void(const MediaResult&)>&& aCallback);
  TimeStamp mRemoteProcessCrashTime;

  nsCString mHardwareAcceleratedReason;
  nsCString mDescription;
  bool mIsHardwareAccelerated = false;
  const bool mRecreatedOnCrash;
  MediaDataDecoder::ConversionRequired mConversion =
      MediaDataDecoder::ConversionRequired::kNeedNone;
  ShmemPool mRawFramePool;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderChild_h
