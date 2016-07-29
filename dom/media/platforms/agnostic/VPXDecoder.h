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
  explicit VPXDecoder(const CreateDecoderParams& aParams);
  ~VPXDecoder();

  RefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;
  const char* GetDescriptionName() const override
  {
    return "libvpx video decoder";
  }

  enum Codec: uint8_t {
    VP8 = 1 << 0,
    VP9 = 1 << 1
  };

  // Return true if aMimeType is a one of the strings used by our demuxers to
  // identify VPX of the specified type. Does not parse general content type
  // strings, i.e. white space matters.
  static bool IsVPX(const nsACString& aMimeType, uint8_t aCodecMask=VP8|VP9);
  static bool IsVP8(const nsACString& aMimeType);
  static bool IsVP9(const nsACString& aMimeType);

private:
  void ProcessDecode(MediaRawData* aSample);
  int DoDecode(MediaRawData* aSample);
  void ProcessDrain();

  const RefPtr<ImageContainer> mImageContainer;
  const RefPtr<TaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
  Atomic<bool> mIsFlushing;

  // VPx decoder state
  vpx_codec_ctx_t mVPX;

  const VideoInfo& mInfo;

  const int mCodec;
};

} // namespace mozilla

#endif
