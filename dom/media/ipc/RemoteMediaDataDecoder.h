/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteMediaDataDecoder_h
#define include_dom_media_ipc_RemoteMediaDataDecoder_h
#include "PlatformDecoderModule.h"

#include "MediaData.h"

namespace mozilla {

class GpuDecoderModule;
class IRemoteDecoderChild;
class RemoteMediaDataDecoder;

DDLoggedTypeCustomNameAndBase(RemoteMediaDataDecoder,
                              RemoteMediaDataDecoder,
                              MediaDataDecoder);

// A MediaDataDecoder implementation that proxies through IPDL
// to a 'real' decoder in the GPU or RDD process.
// All requests get forwarded to a *DecoderChild instance that
// operates solely on the provided manager and abstract manager threads.
class RemoteMediaDataDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<RemoteMediaDataDecoder>
{
public:
  friend class GpuDecoderModule;

  // MediaDataDecoder
  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  nsCString GetDescriptionName() const override;
  ConversionRequired NeedsConversion() const override;

private:
  RemoteMediaDataDecoder(IRemoteDecoderChild* aChild,
                         nsIThread* aManagerThread,
                         AbstractThread* aAbstractManagerThread);
  ~RemoteMediaDataDecoder();

  // Only ever written to from the reader task queue (during the constructor and
  // destructor when we can guarantee no other threads are accessing it). Only
  // read from the manager thread.
  RefPtr<IRemoteDecoderChild> mChild;
  nsIThread* mManagerThread;
  AbstractThread* mAbstractManagerThread;
  // Only ever written/modified during decoder initialisation.
  // As such can be accessed from any threads after that.
  nsCString mDescription = NS_LITERAL_CSTRING("RemoteMediaDataDecoder");
  bool mIsHardwareAccelerated = false;
  nsCString mHardwareAcceleratedReason;
  ConversionRequired mConversion = ConversionRequired::kNeedNone;
};

} // namespace mozilla

#endif // include_dom_media_ipc_RemoteMediaDataDecoder_h
