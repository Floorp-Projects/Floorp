/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(VPXDecoder_h_)
#  define VPXDecoder_h_

#  include <stdint.h>

#  include "PlatformDecoderModule.h"
#  include "mozilla/Span.h"
#  include "mozilla/gfx/Types.h"
#  include "vpx/vp8dx.h"
#  include "vpx/vpx_codec.h"
#  include "vpx/vpx_decoder.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(VPXDecoder, MediaDataDecoder);

class VPXDecoder : public MediaDataDecoder,
                   public DecoderDoctorLifeLogger<VPXDecoder> {
 public:
  explicit VPXDecoder(const CreateDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override {
    return "libvpx video decoder"_ns;
  }

  enum Codec : uint8_t {
    VP8 = 1 << 0,
    VP9 = 1 << 1,
    Unknown = 1 << 7,
  };

  // Return true if aMimeType is a one of the strings used by our demuxers to
  // identify VPX of the specified type. Does not parse general content type
  // strings, i.e. white space matters.
  static bool IsVPX(const nsACString& aMimeType,
                    uint8_t aCodecMask = VP8 | VP9);
  static bool IsVP8(const nsACString& aMimeType);
  static bool IsVP9(const nsACString& aMimeType);

  // Return true if a sample is a keyframe for the specified codec.
  static bool IsKeyframe(Span<const uint8_t> aBuffer, Codec aCodec);

  // Return the frame dimensions for a sample for the specified codec.
  static gfx::IntSize GetFrameSize(Span<const uint8_t> aBuffer, Codec aCodec);
  // Return the display dimensions for a sample for the specified codec.
  static gfx::IntSize GetDisplaySize(Span<const uint8_t> aBuffer, Codec aCodec);

  // Return the VP9 profile as per https://www.webmproject.org/vp9/profiles/
  // Return negative value if error.
  static int GetVP9Profile(Span<const uint8_t> aBuffer);

  struct VPXStreamInfo {
    gfx::IntSize mImage;
    gfx::IntSize mDisplay;
    bool mKeyFrame = false;

    uint8_t mProfile = 0;
    uint8_t mBitDepth = 8;
    /*
    0 CS_UNKNOWN Unknown (in this case the color space must be signaled outside
      the VP9 bitstream).
    1 CS_BT_601 Rec. ITU-R BT.601-7
    2 CS_BT_709 Rec. ITU-R BT.709-6
    3 CS_SMPTE_170 SMPTE-170
    4 CS_SMPTE_240 SMPTE-240
    5 CS_BT_2020 Rec. ITU-R BT.2020-2
    6 CS_RESERVED Reserved
    7 CS_RGB sRGB (IEC 61966-2-1)
    */
    int mColorSpace = 1;  // CS_BT_601

    gfx::YUVColorSpace ColorSpace() const {
      switch (mColorSpace) {
        case 1:
        case 3:
        case 4:
          return gfx::YUVColorSpace::BT601;
        case 2:
          return gfx::YUVColorSpace::BT709;
        case 5:
          return gfx::YUVColorSpace::BT2020;
        default:
          return gfx::YUVColorSpace::Default;
      }
    }

    // Ref: ISO/IEC 23091-2:2019
    enum class ColorPrimaries {
      BT_709_6 = 1,
      Unspecified = 2,
      BT_470_6_M = 4,
      BT_470_7_BG = 5,
      BT_601_7 = 6,
      SMPTE_ST_240 = 7,
      Film = 8,
      BT_2020_Nonconstant_Luminance = 9,
      SMPTE_ST_428_1 = 10,
      SMPTE_RP_431_2 = 11,
      SMPTE_EG_432_1 = 12,
      EBU_Tech_3213_E = 22,
    };

    // Ref: ISO/IEC 23091-2:2019
    enum class TransferCharacteristics {
      BT_709_6 = 1,
      Unspecified = 2,
      BT_470_6_M = 4,
      BT_470_7_BG = 5,
      BT_601_7 = 6,
      SMPTE_ST_240 = 7,
      Linear = 8,
      Logrithmic = 9,
      Logrithmic_Sqrt = 10,
      IEC_61966_2_4 = 11,
      BT_1361_0 = 12,
      IEC_61966_2_1 = 13,
      BT_2020_10bit = 14,
      BT_2020_12bit = 15,
      SMPTE_ST_2084 = 16,
      SMPTE_ST_428_1 = 17,
      BT_2100_HLG = 18,
    };

    enum class MatrixCoefficients {
      Identity = 0,
      BT_709_6 = 1,
      Unspecified = 2,
      FCC = 4,
      BT_470_7_BG = 5,
      BT_601_7 = 6,
      SMPTE_ST_240 = 7,
      YCgCo = 8,
      BT_2020_Nonconstant_Luminance = 9,
      BT_2020_Constant_Luminance = 10,
      SMPTE_ST_2085 = 11,
      Chromacity_Constant_Luminance = 12,
      Chromacity_Nonconstant_Luminance = 13,
      BT_2100_ICC = 14,
    };

    /*
    mFullRange == false then:
      For BitDepth equals 8:
        Y is between 16 and 235 inclusive.
        U and V are between 16 and 240 inclusive.
      For BitDepth equals 10:
        Y is between 64 and 940 inclusive.
        U and V are between 64 and 960 inclusive.
      For BitDepth equals 12:
        Y is between 256 and 3760.
        U and V are between 256 and 3840 inclusive.
    mFullRange == true then:
      No restriction on Y, U, V values.
    */
    bool mFullRange = false;

    gfx::ColorRange ColorRange() const {
      return mFullRange ? gfx::ColorRange::FULL : gfx::ColorRange::LIMITED;
    }

    /*
      Sub-sampling, used only for non sRGB colorspace.
      subsampling_x subsampling_y Description
          0             0         YUV 4:4:4
          0             1         YUV 4:4:0
          1             0         YUV 4:2:2
          1             1         YUV 4:2:0
    */
    bool mSubSampling_x = true;
    bool mSubSampling_y = true;

    bool IsCompatible(const VPXStreamInfo& aOther) const {
      return mImage == aOther.mImage && mProfile == aOther.mProfile &&
             mBitDepth == aOther.mBitDepth &&
             mSubSampling_x == aOther.mSubSampling_x &&
             mSubSampling_y == aOther.mSubSampling_y &&
             mColorSpace == aOther.mColorSpace &&
             mFullRange == aOther.mFullRange;
    }
  };

  static bool GetStreamInfo(Span<const uint8_t> aBuffer, VPXStreamInfo& aInfo,
                            Codec aCodec);

  static void GetVPCCBox(MediaByteBuffer* aDestBox, const VPXStreamInfo& aInfo);

 private:
  ~VPXDecoder();
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);
  MediaResult DecodeAlpha(vpx_image_t** aImgAlpha, const MediaRawData* aSample);

  const RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::KnowsCompositor> mImageAllocator;
  const RefPtr<TaskQueue> mTaskQueue;

  // VPx decoder state
  vpx_codec_ctx_t mVPX;

  // VPx alpha decoder state
  vpx_codec_ctx_t mVPXAlpha;

  const VideoInfo mInfo;

  const Codec mCodec;
  const bool mLowLatency;
};

}  // namespace mozilla

#endif
