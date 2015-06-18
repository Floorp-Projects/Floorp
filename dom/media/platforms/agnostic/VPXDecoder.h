/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(VPXDecoder_h_)
#define VPXDecoder_h_

#include "PlatformDecoderModule.h"

#include <stdint.h>
#define VPX_DONT_DEFINE_STDINT_TYPES
#include "vpx/vp8dx.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_decoder.h"

namespace mozilla {

  using namespace layers;

class VPXDecoder : public MediaDataDecoder
{
public:
  VPXDecoder(const VideoInfo& aConfig,
             ImageContainer* aImageContainer,
             FlushableTaskQueue* aTaskQueue,
             MediaDataDecoderCallback* aCallback);

  ~VPXDecoder();

  nsresult Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;

  // Return true if mimetype is a VPX codec
  static bool IsVPX(const nsACString& aMimeType);

  enum Codec {
    VP8,
    VP9
  };

private:
  void DecodeFrame (MediaRawData* aSample);
  int DoDecodeFrame (MediaRawData* aSample);
  void DoDrain ();
  void OutputDelayedFrames ();

  nsRefPtr<ImageContainer> mImageContainer;
  RefPtr<FlushableTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;

  // VPx decoder state
  vpx_codec_ctx_t mVPX;
  vpx_codec_iter_t mIter;

  uint32_t mDisplayWidth;
  uint32_t mDisplayHeight;

  int mCodec;
};

} // namespace mozilla

#endif
