/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_
#define DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_

#include <stdint.h>

#include "mozilla/CheckedInt.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "nsTArray.h"

namespace mozilla {

class BitReader;
class MediaByteBuffer;
class MediaRawData;

// Most classes in this file are implemented according to the H265 spec
// (https://www.itu.int/rec/T-REC-H.265-202108-I/en), except the HVCCConfig,
// which is in the ISO/IEC 14496-15. To make it easier to read the
// implementation with the spec, the naming style in this file follows the spec
// instead of our usual style.

enum {
  kMaxLongTermRefPicSets = 32,   // See num_long_term_ref_pics_sps
  kMaxShortTermRefPicSets = 64,  // See num_short_term_ref_pic_sets
  kMaxSubLayers = 7,             // See [v/s]ps_max_sub_layers_minus1
};

// Spec 7.3.1 NAL unit syntax
class H265NALU final {
 public:
  H265NALU(const uint8_t* aData, uint32_t aByteSize);
  H265NALU() = default;

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
  bool IsVPS() const { return mNalUnitType == NAL_TYPES::VPS_NUT; }
  bool IsPPS() const { return mNalUnitType == NAL_TYPES::PPS_NUT; }
  bool IsSEI() const {
    return mNalUnitType == NAL_TYPES::PREFIX_SEI_NUT ||
           mNalUnitType == NAL_TYPES::SUFFIX_SEI_NUT;
  }

  uint8_t mNalUnitType;
  uint8_t mNuhLayerId;
  uint8_t mNuhTemporalIdPlus1;
  // This contain the full content of NALU, which can be used to decode rbsp.
  const Span<const uint8_t> mNALU;
};

// H265 spec, 7.3.3 Profile, tier and level syntax
struct H265ProfileTierLevel final {
  H265ProfileTierLevel() = default;

  enum H265ProfileIdc {
    kProfileIdcMain = 1,
    kProfileIdcMain10 = 2,
    kProfileIdcMainStill = 3,
    kProfileIdcRangeExtensions = 4,
    kProfileIdcHighThroughput = 5,
    kProfileIdcMultiviewMain = 6,
    kProfileIdcScalableMain = 7,
    kProfileIdc3dMain = 8,
    kProfileIdcScreenContentCoding = 9,
    kProfileIdcScalableRangeExtensions = 10,
    kProfileIdcHighThroughputScreenContentCoding = 11,
  };

  // From Table A.8 - General tier and level limits.
  uint32_t GetMaxLumaPs() const;

  // From A.4.2 - Profile-specific level limits for the video profiles.
  uint32_t GetDpbMaxPicBuf() const;

  // Syntax elements.
  uint8_t general_profile_space = {};
  bool general_tier_flag = {};
  uint8_t general_profile_idc = {};
  uint32_t general_profile_compatibility_flags = {};
  bool general_progressive_source_flag = {};
  bool general_interlaced_source_flag = {};
  bool general_non_packed_constraint_flag = {};
  bool general_frame_only_constraint_flag = {};
  uint8_t general_level_idc = {};
};

// H265 spec, 7.3.7 Short-term reference picture set syntax
struct H265StRefPicSet final {
  H265StRefPicSet() = default;

  // Syntax elements.
  uint32_t num_negative_pics = {};
  uint32_t num_positive_pics = {};

  // Calculated fields
  // From the H265 spec 7.4.8
  bool usedByCurrPicS0[kMaxShortTermRefPicSets] = {};  // (7-65)
  bool usedByCurrPicS1[kMaxShortTermRefPicSets] = {};  // (7-66)
  uint32_t deltaPocS0[kMaxShortTermRefPicSets] = {};   // (7-67) + (7-69)
  uint32_t deltaPocS1[kMaxShortTermRefPicSets] = {};   // (7-68) + (7-70)
  uint32_t numDeltaPocs = {};                          // (7-72)
};

// H265 spec, E.2.1 VUI parameters syntax
struct H265VUIParameters {
  H265VUIParameters() = default;

  // Syntax elements.
  uint32_t sar_width = {};
  uint32_t sar_height = {};
  bool video_full_range_flag = {};
  Maybe<uint8_t> colour_primaries;
  Maybe<uint8_t> transfer_characteristics;
  Maybe<uint8_t> matrix_coeffs;
};

// H265 spec, 7.3.2.2 Sequence parameter set RBSP syntax
struct H265SPS final {
  H265SPS() = default;

  bool operator==(const H265SPS& aOther) const;
  bool operator!=(const H265SPS& aOther) const;

  // Syntax elements.
  uint8_t sps_video_parameter_set_id = {};
  uint8_t sps_max_sub_layers_minus1 = {};
  bool sps_temporal_id_nesting_flag = {};
  H265ProfileTierLevel profile_tier_level = {};
  uint32_t sps_seq_parameter_set_id = {};
  uint32_t chroma_format_idc = {};
  bool separate_colour_plane_flag = {};
  uint32_t pic_width_in_luma_samples = {};
  uint32_t pic_height_in_luma_samples = {};

  bool conformance_window_flag = {};
  uint32_t conf_win_left_offset = {};
  uint32_t conf_win_right_offset = {};
  uint32_t conf_win_top_offset = {};
  uint32_t conf_win_bottom_offset = {};

  uint32_t bit_depth_luma_minus8 = {};
  uint32_t bit_depth_chroma_minus8 = {};
  uint32_t log2_max_pic_order_cnt_lsb_minus4 = {};
  bool sps_sub_layer_ordering_info_present_flag = {};
  uint32_t sps_max_dec_pic_buffering_minus1[kMaxSubLayers] = {};
  uint32_t sps_max_num_reorder_pics[kMaxSubLayers] = {};
  uint32_t sps_max_latency_increase_plus1[kMaxSubLayers] = {};
  uint32_t log2_min_luma_coding_block_size_minus3 = {};
  uint32_t log2_diff_max_min_luma_coding_block_size = {};
  uint32_t log2_min_luma_transform_block_size_minus2 = {};
  uint32_t log2_diff_max_min_luma_transform_block_size = {};
  uint32_t max_transform_hierarchy_depth_inter = {};
  uint32_t max_transform_hierarchy_depth_intra = {};

  bool pcm_enabled_flag = {};
  uint8_t pcm_sample_bit_depth_luma_minus1 = {};
  uint8_t pcm_sample_bit_depth_chroma_minus1 = {};
  uint32_t log2_min_pcm_luma_coding_block_size_minus3 = {};
  uint32_t log2_diff_max_min_pcm_luma_coding_block_size = {};
  bool pcm_loop_filter_disabled_flag = {};

  uint32_t num_short_term_ref_pic_sets = {};
  H265StRefPicSet st_ref_pic_set[kMaxShortTermRefPicSets] = {};

  bool sps_temporal_mvp_enabled_flag = {};
  bool strong_intra_smoothing_enabled_flag = {};
  Maybe<H265VUIParameters> vui_parameters;

  // Calculated fields
  uint32_t subWidthC = {};       // From Table 6-1.
  uint32_t subHeightC = {};      // From Table 6-1.
  CheckedUint32 mDisplayWidth;   // Per (E-68) + (E-69)
  CheckedUint32 mDisplayHeight;  // Per (E-70) + (E-71)
  uint32_t maxDpbSize = {};

  // Often used information
  uint32_t BitDepthLuma() const { return bit_depth_luma_minus8 + 8; }
  uint32_t BitDepthChroma() const { return bit_depth_chroma_minus8 + 8; }
  gfx::IntSize GetImageSize() const;
  gfx::IntSize GetDisplaySize() const;
  gfx::ColorDepth ColorDepth() const;
  gfx::YUVColorSpace ColorSpace() const;
  bool IsFullColorRange() const;
  uint8_t ColorPrimaries() const;
  uint8_t TransferFunction() const;
};

// ISO/IEC 14496-15 : hvcC.
struct HVCCConfig final {
 public:
  static Result<HVCCConfig, nsresult> Parse(
      const mozilla::MediaRawData* aSample);
  static Result<HVCCConfig, nsresult> Parse(
      const mozilla::MediaByteBuffer* aExtraData);

  uint8_t NALUSize() const { return lengthSizeMinusOne + 1; }
  uint32_t NumSPS() const;
  bool HasSPS() const;

  uint8_t configurationVersion;
  uint8_t general_profile_space;
  bool general_tier_flag;
  uint8_t general_profile_idc;
  uint32_t general_profile_compatibility_flags;
  uint64_t general_constraint_indicator_flags;
  uint8_t general_level_idc;
  uint16_t min_spatial_segmentation_idc;
  uint8_t parallelismType;
  uint8_t chroma_format_idc;
  uint8_t bit_depth_luma_minus8;
  uint8_t bit_depth_chroma_minus8;
  uint16_t avgFrameRate;
  uint8_t constantFrameRate;
  uint8_t numTemporalLayers;
  bool temporalIdNested;
  uint8_t lengthSizeMinusOne;

  nsTArray<H265NALU> mNALUs;

  // Keep the orginal buffer alive in order to let H265NALU always access to
  // valid data if there is any NALU.
  RefPtr<const MediaByteBuffer> mByteBuffer;

 private:
  HVCCConfig() = default;
};

class H265 final {
 public:
  static Result<H265SPS, nsresult> DecodeSPSFromHVCCExtraData(
      const mozilla::MediaByteBuffer* aExtraData);
  static Result<H265SPS, nsresult> DecodeSPSFromSPSNALU(
      const H265NALU& aSPSNALU);

  // Extract SPS and PPS NALs from aSample by looking into each NALs.
  static already_AddRefed<mozilla::MediaByteBuffer> ExtractHVCCExtraData(
      const mozilla::MediaRawData* aSample);

  // Return true if both extradata are equal.
  static bool CompareExtraData(const mozilla::MediaByteBuffer* aExtraData1,
                               const mozilla::MediaByteBuffer* aExtraData2);

 private:
  // Return RAW BYTE SEQUENCE PAYLOAD (rbsp) from NAL content.
  static already_AddRefed<mozilla::MediaByteBuffer> DecodeNALUnit(
      const Span<const uint8_t>& aNALU);

  //  Parse the profile level based on the H265 spec, 7.3.3. MUST use a bit
  //  reader which starts from the position of the first bit of the data.
  static Result<Ok, nsresult> ParseProfileTierLevel(
      BitReader& aReader, bool aProfilePresentFlag,
      uint8_t aMaxNumSubLayersMinus1, H265ProfileTierLevel& aProfile);

  //  Parse the short-term reference picture set based on the H265 spec, 7.3.7.
  //  MUST use a bit reader which starts from the position of the first bit of
  //  the data.
  static Result<Ok, nsresult> ParseStRefPicSet(BitReader& aReader,
                                               uint32_t aStRpsIdx,
                                               H265SPS& aSPS);

  //  Parse the VUI parameters based on the H265 spec, E.2.1. MUST use a bit
  //  reader which starts from the position of the first bit of the data.
  static Result<Ok, nsresult> ParseVuiParameters(BitReader& aReader,
                                                 H265SPS& aSPS);

  // Parse and ignore the structure. MUST use a bitreader which starts from the
  // position of the first bit of the data.
  static Result<Ok, nsresult> ParseAndIgnoreScalingListData(BitReader& aReader);
  static Result<Ok, nsresult> ParseAndIgnoreHrdParameters(
      BitReader& aReader, bool aCommonInfPresentFlag,
      int aMaxNumSubLayersMinus1);
  static Result<Ok, nsresult> ParseAndIgnoreSubLayerHrdParameters(
      BitReader& aReader, int aCpbCnt, bool aSubPicHrdParamsPresentFlag);
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_
