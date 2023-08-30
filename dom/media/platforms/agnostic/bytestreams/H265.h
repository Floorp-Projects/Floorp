/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_
#define DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_

#include <stdint.h>

#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "nsTArray.h"

namespace mozilla {

class BitReader;
class MediaByteBuffer;
class MediaRawData;

// H265 spec
// https://www.itu.int/rec/T-REC-H.265-202108-I/en

// Spec 7.3.1 NAL unit syntax
class H265NALU final {
 public:
  H265NALU(const uint8_t* aData, uint32_t aSize);

  // Table 7-1
  enum NAL_TYPES {
    TRAIL_N = 0,
    TRAIL_R = 1,
    TSA_N = 2,
    TSA_R = 3,
    STSA_N = 4,
    STSA_R = 5,
    RADL_N = 6,
    RADL_R = 7,
    RASL_N = 8,
    RASL_R = 9,
    RSV_VCL_N10 = 10,
    RSV_VCL_R11 = 11,
    RSV_VCL_N12 = 12,
    RSV_VCL_R13 = 13,
    RSV_VCL_N14 = 14,
    RSV_VCL_R15 = 15,
    BLA_W_LP = 16,
    BLA_W_RADL = 17,
    BLA_N_LP = 18,
    IDR_W_RADL = 19,
    IDR_N_LP = 20,
    CRA_NUT = 21,
    RSV_IRAP_VCL22 = 22,
    RSV_IRAP_VCL23 = 23,
    RSV_VCL24 = 24,
    RSV_VCL25 = 25,
    RSV_VCL26 = 26,
    RSV_VCL27 = 27,
    RSV_VCL28 = 28,
    RSV_VCL29 = 29,
    RSV_VCL30 = 30,
    RSV_VCL31 = 31,
    VPS_NUT = 32,
    SPS_NUT = 33,
    PPS_NUT = 34,
    AUD_NUT = 35,
    EOS_NUT = 36,
    EOB_NUT = 37,
    FD_NUT = 38,
    PREFIX_SEI_NUT = 39,
    SUFFIX_SEI_NUT = 40,
    RSV_NVCL41 = 41,
    RSV_NVCL42 = 42,
    RSV_NVCL43 = 43,
    RSV_NVCL44 = 44,
    RSV_NVCL45 = 45,
    RSV_NVCL46 = 46,
    RSV_NVCL47 = 47,
    UNSPEC48 = 48,
    UNSPEC49 = 49,
    UNSPEC50 = 50,
    UNSPEC51 = 51,
    UNSPEC52 = 52,
    UNSPEC53 = 53,
    UNSPEC54 = 54,
    UNSPEC55 = 55,
    UNSPEC56 = 56,
    UNSPEC57 = 57,
    UNSPEC58 = 58,
    UNSPEC59 = 59,
    UNSPEC60 = 60,
    UNSPEC61 = 61,
    UNSPEC62 = 62,
    UNSPEC63 = 63,
  };

  bool IsIframe() const {
    return mNalUnitType == NAL_TYPES::IDR_W_RADL ||
           mNalUnitType == NAL_TYPES::IDR_N_LP;
  }

  bool IsSPS() const { return mNalUnitType == NAL_TYPES::SPS_NUT; }

  uint8_t mNalUnitType;
  uint8_t mNuhLayerId;
  uint8_t mNuhTemporalIdPlus1;
  // This contain the full content of NALU, which can be used to decode rbsp.
  const Span<const uint8_t> mNALU;
};

// ISO/IEC 14496-15 : hvcC.
struct HVCCConfig final {
 public:
  static Result<HVCCConfig, nsresult> Parse(
      const mozilla::MediaRawData* aSample);
  static Result<HVCCConfig, nsresult> Parse(
      const mozilla::MediaByteBuffer* aExtraData);

  uint8_t NALUSize() const { return mLengthSizeMinusOne + 1; }

  uint8_t mConfigurationVersion;
  uint8_t mGeneralProfileSpace;
  bool mGeneralTierFlag;
  uint8_t mGeneralProfileIdc;
  uint32_t mGeneralProfileCompatibilityFlags;
  uint64_t mGeneralConstraintIndicatorFlags;
  uint8_t mGeneralLevelIdc;
  uint16_t mMinSpatialSegmentationIdc;
  uint8_t mParallelismType;
  uint8_t mChromaFormatIdc;
  uint8_t mBitDepthLumaMinus8;
  uint8_t mBitDepthChromaMinus8;
  uint16_t mAvgFrameRate;
  uint8_t mConstantFrameRate;
  uint8_t mNumTemporalLayers;
  bool mTemporalIdNested;
  uint8_t mLengthSizeMinusOne;

  nsTArray<H265NALU> mNALUs;

  // Keep the orginal buffer alive in order to let H265NALU always access to
  // valid data if there is any NALU.
  RefPtr<const MediaByteBuffer> mByteBuffer;

 private:
  HVCCConfig() = default;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_
