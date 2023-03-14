/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderChild_h
#define include_dom_media_ipc_RemoteDecoderChild_h

#include <functional>

#include "mozilla/PRemoteDecoderChild.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/ShmemRecycleAllocator.h"

namespace mozilla {

class RemoteDecoderManagerChild;
using mozilla::MediaDataDecoder;
using mozilla::ipc::IPCResult;

class RemoteDecoderChild : public ShmemRecycleAllocator<RemoteDecoderChild>,
                           public PRemoteDecoderChild {
  friend class PRemoteDecoderChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderChild);

  explicit RemoteDecoderChild(RemoteDecodeIn aLocation);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  // This interface closely mirrors the MediaDataDecoder plus a bit
  // (DestroyIPDL) to allow proxying to a remote decoder in RemoteDecoderModule.
  RefPtr<MediaDataDecoder::InitPromise> Init();
  RefPtr<MediaDataDecoder::DecodePromise> Decode(
      const nsTArray<RefPtr<MediaRawData>>& aSamples);
  RefPtr<MediaDataDecoder::DecodePromise> Drain();
  RefPtr<MediaDataDecoder::FlushPromise> Flush();
  RefPtr<mozilla::ShutdownPromise> Shutdown();
  bool IsHardwareAccelerated(nsACString& aFailureReason) const;
  nsCString GetDescriptionName() const;
  nsCString GetProcessName() const;
  nsCString GetCodecName() const;
  void SetSeekThreshold(const media::TimeUnit& aTime);
  MediaDataDecoder::ConversionRequired NeedsConversion() const;
  void DestroyIPDL();

  // Called from IPDL when our actor has been destroyed
  void IPDLActorDestroyed();

  RemoteDecoderManagerChild* GetManager();

 protected:
  virtual ~RemoteDecoderChild();
  void AssertOnManagerThread() const;

  virtual MediaResult ProcessOutput(DecodedOutputIPDL&& aDecodedData) = 0;
  virtual void RecordShutdownTelemetry(bool aForAbnormalShutdown) {}

  RefPtr<RemoteDecoderChild> mIPDLSelfRef;
  MediaDataDecoder::DecodedData mDecodedData;
  const RemoteDecodeIn mLocation;

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

  nsCString mHardwareAcceleratedReason;
  nsCString mDescription;
  nsCString mProcessName;
  nsCString mCodecName;
  bool mIsHardwareAccelerated = false;
  bool mRemoteDecoderCrashed = false;
  MediaDataDecoder::ConversionRequired mConversion =
      MediaDataDecoder::ConversionRequired::kNeedNone;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderChild_h
