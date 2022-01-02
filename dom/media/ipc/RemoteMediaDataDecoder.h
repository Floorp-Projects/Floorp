/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteMediaDataDecoder_h
#define include_dom_media_ipc_RemoteMediaDataDecoder_h
#include "PlatformDecoderModule.h"

#include "MediaData.h"

namespace mozilla {

class RemoteDecoderChild;
class RemoteDecoderManagerChild;
class RemoteMediaDataDecoder;

DDLoggedTypeCustomNameAndBase(RemoteMediaDataDecoder, RemoteMediaDataDecoder,
                              MediaDataDecoder);

// A MediaDataDecoder implementation that proxies through IPDL
// to a 'real' decoder in the GPU or RDD process.
// All requests get forwarded to a *DecoderChild instance that
// operates solely on the provided manager and abstract manager threads.
class RemoteMediaDataDecoder
    : public MediaDataDecoder,
      public DecoderDoctorLifeLogger<RemoteMediaDataDecoder> {
 public:
  explicit RemoteMediaDataDecoder(RemoteDecoderChild* aChild);

  // MediaDataDecoder
  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  bool CanDecodeBatch() const override { return true; }
  RefPtr<DecodePromise> DecodeBatch(
      nsTArray<RefPtr<MediaRawData>>&& aSamples) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  nsCString GetDescriptionName() const override;
  ConversionRequired NeedsConversion() const override;

 private:
  ~RemoteMediaDataDecoder();

  // Only ever written to from the reader task queue (during the constructor and
  // destructor when we can guarantee no other threads are accessing it). Only
  // read from the manager thread.
  RefPtr<RemoteDecoderChild> mChild;
  // Only ever written/modified during decoder initialisation.
  // As such can be accessed from any threads after that.
  nsCString mDescription = "RemoteMediaDataDecoder"_ns;
  bool mIsHardwareAccelerated = false;
  nsCString mHardwareAcceleratedReason;
  ConversionRequired mConversion = ConversionRequired::kNeedNone;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteMediaDataDecoder_h
