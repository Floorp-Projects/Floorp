/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DecoderTypes_h
#define mozilla_dom_DecoderTypes_h

#include "mozilla/Maybe.h"
#include "mozilla/dom/EncodedVideoChunk.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "mozilla/dom/VideoDecoderBinding.h"
#include "mozilla/dom/VideoFrame.h"
#include "nsStringFwd.h"
#include "nsTLiteralString.h"

namespace mozilla {

class TrackInfo;
class MediaByteBuffer;

namespace dom {

struct VideoColorSpaceInternal {
  explicit VideoColorSpaceInternal(const VideoColorSpaceInit& aColorSpaceInit);
  VideoColorSpaceInternal() = default;
  VideoColorSpaceInit ToColorSpaceInit() const;

  Maybe<bool> mFullRange;
  Maybe<VideoMatrixCoefficients> mMatrix;
  Maybe<VideoColorPrimaries> mPrimaries;
  Maybe<VideoTransferCharacteristics> mTransfer;
};

class VideoDecoderConfigInternal {
 public:
  static UniquePtr<VideoDecoderConfigInternal> Create(
      const VideoDecoderConfig& aConfig);
  ~VideoDecoderConfigInternal() = default;

  nsString mCodec;
  Maybe<uint32_t> mCodedHeight;
  Maybe<uint32_t> mCodedWidth;
  Maybe<VideoColorSpaceInternal> mColorSpace;
  Maybe<RefPtr<MediaByteBuffer>> mDescription;
  Maybe<uint32_t> mDisplayAspectHeight;
  Maybe<uint32_t> mDisplayAspectWidth;
  HardwareAcceleration mHardwareAcceleration;
  Maybe<bool> mOptimizeForLatency;

 private:
  VideoDecoderConfigInternal(const nsAString& aCodec,
                             Maybe<uint32_t>&& aCodedHeight,
                             Maybe<uint32_t>&& aCodedWidth,
                             Maybe<VideoColorSpaceInternal>&& aColorSpace,
                             Maybe<RefPtr<MediaByteBuffer>>&& aDescription,
                             Maybe<uint32_t>&& aDisplayAspectHeight,
                             Maybe<uint32_t>&& aDisplayAspectWidth,
                             const HardwareAcceleration& aHardwareAcceleration,
                             Maybe<bool>&& aOptimizeForLatency);
};

class VideoDecoderTraits {
 public:
  static constexpr nsLiteralCString Name = "VideoDecoder"_ns;
  using ConfigType = VideoDecoderConfig;
  using ConfigTypeInternal = VideoDecoderConfigInternal;
  using InputType = EncodedVideoChunk;
  using InputTypeInternal = EncodedVideoChunkData;
  using OutputType = VideoFrame;
  using OutputCallbackType = VideoFrameOutputCallback;

  static bool IsSupported(const ConfigTypeInternal& aConfig);
  static Result<UniquePtr<TrackInfo>, nsresult> CreateTrackInfo(
      const ConfigTypeInternal& aConfig);
  static bool Validate(const ConfigType& aConfig);
  static UniquePtr<ConfigTypeInternal> CreateConfigInternal(
      const ConfigType& aConfig);
  static bool IsKeyChunk(const InputType& aInput);
  static UniquePtr<InputTypeInternal> CreateInputInternal(
      const InputType& aInput);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DecoderTypes_h
