/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_IRemoteDecoderChild_h
#define include_dom_media_ipc_IRemoteDecoderChild_h

#include "PlatformDecoderModule.h"
#include "mozilla/TaskQueue.h"

namespace mozilla {

// This interface mirrors the MediaDataDecoder plus a bit (DestroyIPDL)
// to allow proxying to a remote decoder in RemoteDecoderModule or
// GpuDecoderModule. RemoteAudioDecoderChild, RemoteVideoDecoderChild,
// and VideoDecoderChild (for GPU) implement this interface.
class IRemoteDecoderChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IRemoteDecoderChild);

  virtual RefPtr<MediaDataDecoder::InitPromise> Init() = 0;
  virtual RefPtr<MediaDataDecoder::DecodePromise> Decode(
      MediaRawData* aSample) = 0;
  virtual RefPtr<MediaDataDecoder::DecodePromise> Drain() = 0;
  virtual RefPtr<MediaDataDecoder::FlushPromise> Flush() = 0;
  virtual RefPtr<ShutdownPromise> Shutdown() = 0;
  virtual bool IsHardwareAccelerated(nsACString& aFailureReason) const {
    return false;
  }
  virtual nsCString GetDescriptionName() const = 0;
  virtual void SetSeekThreshold(const media::TimeUnit& aTime) {}
  virtual MediaDataDecoder::ConversionRequired NeedsConversion() const {
    return MediaDataDecoder::ConversionRequired::kNeedNone;
  }

  virtual void DestroyIPDL() = 0;

 protected:
  virtual ~IRemoteDecoderChild() {}
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_IRemoteDecoderChild_h
