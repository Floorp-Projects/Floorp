/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGAUDIOENCODER_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGAUDIOENCODER_H_

#include "FFmpegDataEncoder.h"
#include "FFmpegLibWrapper.h"
#include "PlatformEncoderModule.h"
#include "TimedPacketizer.h"

// This must be the last header included
#include "FFmpegLibs.h"
#include "speex/speex_resampler.h"

namespace mozilla {

template <int V>
class FFmpegAudioEncoder : public MediaDataEncoder {};

template <>
class FFmpegAudioEncoder<LIBAV_VER> : public FFmpegDataEncoder<LIBAV_VER> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FFmpegAudioEncoder, final);

  FFmpegAudioEncoder(const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
                     const RefPtr<TaskQueue>& aTaskQueue,
                     const EncoderConfig& aConfig);

  nsCString GetDescriptionName() const override;

 protected:
  virtual ~FFmpegAudioEncoder() = default;
  // Methods only called on mTaskQueue.
  virtual nsresult InitSpecific() override;
#if LIBAVCODEC_VERSION_MAJOR >= 58
  Result<EncodedData, nsresult> EncodeOnePacket(Span<float> aSamples,
                                                media::TimeUnit aPts);
  Result<EncodedData, nsresult> EncodeInputWithModernAPIs(
      RefPtr<const MediaData> aSample) override;
  Result<MediaDataEncoder::EncodedData, nsresult> DrainWithModernAPIs()
      override;
#endif
  virtual RefPtr<MediaRawData> ToMediaRawData(AVPacket* aPacket) override;
  Result<already_AddRefed<MediaByteBuffer>, nsresult> GetExtraData(
      AVPacket* aPacket) override;
  // Most audio codecs (except PCM) require a very specific frame size.
  Maybe<TimedPacketizer<float, float>> mPacketizer;
  // A temporary buffer kept around for shuffling audio frames, resampling,
  // packetization, etc.
  nsTArray<float> mTempBuffer;
  // The pts of the first packet this encoder has seen, to be able to properly
  // mark encoder delay as such.
  media::TimeUnit mFirstPacketPts{media::TimeUnit::Invalid()};
  struct ResamplerDestroy {
    void operator()(SpeexResamplerState* aResampler);
  };
  // Rate at which this instance has been configured, which might be different
  // from the rate the underlying encoder is running at.
  int mInputSampleRate = 0;
  UniquePtr<SpeexResamplerState, ResamplerDestroy> mResampler;
  uint64_t mPacketsDelivered = 0;
  // Threshold under which a packet isn't returned to the encoder user,
  // because it is known to be silent and DTX is enabled.
  int mDtxThreshold = 0;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGAUDIOENCODER_H_
