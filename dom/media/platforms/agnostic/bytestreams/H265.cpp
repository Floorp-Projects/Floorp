/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "H265.h"
#include <stdint.h>

#include <cmath>
#include <limits>

#include "AnnexB.h"
#include "BitReader.h"
#include "BitWriter.h"
#include "BufferReader.h"
#include "ByteStreamsUtils.h"
#include "ByteWriter.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Span.h"

mozilla::LazyLogModule gH265("H265");

#define LOG(msg, ...) MOZ_LOG(gH265, LogLevel::Debug, (msg, ##__VA_ARGS__))
#define LOGV(msg, ...) MOZ_LOG(gH265, LogLevel::Verbose, (msg, ##__VA_ARGS__))

#define TRUE_OR_RETURN(condition)            \
  do {                                       \
    if (!(condition)) {                      \
      LOG(#condition " should be true!");    \
      return mozilla::Err(NS_ERROR_FAILURE); \
    }                                        \
  } while (0)

#define IN_RANGE_OR_RETURN(val, min, max)                      \
  do {                                                         \
    int64_t temp = AssertedCast<int64_t>(val);                 \
    if ((temp) < (min) || (max) < (temp)) {                    \
      LOG(#val " is not in the range of [" #min "," #max "]"); \
      return mozilla::Err(NS_ERROR_FAILURE);                   \
    }                                                          \
  } while (0)

#define NON_ZERO_OR_RETURN(dest, val)          \
  do {                                         \
    int64_t temp = AssertedCast<int64_t>(val); \
    if ((temp) != 0) {                         \
      (dest) = (temp);                         \
    } else {                                   \
      LOG(#dest " should be non-zero");        \
      return mozilla::Err(NS_ERROR_FAILURE);   \
    }                                          \
  } while (0)

namespace mozilla {

H265NALU::H265NALU(const uint8_t* aData, uint32_t aByteSize)
    : mNALU(aData, aByteSize) {
  // Per 7.3.1 NAL unit syntax
  BitReader reader(aData, aByteSize * 8);
  Unused << reader.ReadBit();  // forbidden_zero_bit
  mNalUnitType = reader.ReadBits(6);
  mNuhLayerId = reader.ReadBits(6);
  mNuhTemporalIdPlus1 = reader.ReadBits(3);
  LOGV("Created H265NALU, type=%hhu, size=%u", mNalUnitType, aByteSize);
}

/* static */ Result<HVCCConfig, nsresult> HVCCConfig::Parse(
    const mozilla::MediaRawData* aSample) {
  if (!aSample) {
    LOG("No sample");
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  if (aSample->Size() < 3) {
    LOG("Incorrect sample size %zu", aSample->Size());
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  if (aSample->mTrackInfo &&
      !aSample->mTrackInfo->mMimeType.EqualsLiteral("video/hevc")) {
    LOG("Only allow 'video/hevc' (mimeType=%s)",
        aSample->mTrackInfo->mMimeType.get());
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  return HVCCConfig::Parse(aSample->mExtraData);
}

/* static */
Result<HVCCConfig, nsresult> HVCCConfig::Parse(
    const mozilla::MediaByteBuffer* aExtraData) {
  // From configurationVersion to numOfArrays, total 184 bits (23 bytes)
  if (!aExtraData) {
    LOG("No extra-data");
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  if (aExtraData->Length() < 23) {
    LOG("Incorrect extra-data size %zu", aExtraData->Length());
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  const auto& byteBuffer = *aExtraData;
  if (byteBuffer[0] != 1) {
    LOG("Version should always be 1");
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  HVCCConfig hvcc;

  BitReader reader(aExtraData);
  hvcc.configurationVersion = reader.ReadBits(8);
  hvcc.general_profile_space = reader.ReadBits(2);
  hvcc.general_tier_flag = reader.ReadBit();
  hvcc.general_profile_idc = reader.ReadBits(5);
  hvcc.general_profile_compatibility_flags = reader.ReadU32();

  uint32_t flagHigh = reader.ReadU32();
  uint16_t flagLow = reader.ReadBits(16);
  hvcc.general_constraint_indicator_flags =
      ((uint64_t)(flagHigh) << 16) | (uint64_t)(flagLow);

  hvcc.general_level_idc = reader.ReadBits(8);
  Unused << reader.ReadBits(4);  // reserved
  hvcc.min_spatial_segmentation_idc = reader.ReadBits(12);
  Unused << reader.ReadBits(6);  // reserved
  hvcc.parallelismType = reader.ReadBits(2);
  Unused << reader.ReadBits(6);  // reserved
  hvcc.chroma_format_idc = reader.ReadBits(2);
  Unused << reader.ReadBits(5);  // reserved
  hvcc.bit_depth_luma_minus8 = reader.ReadBits(3);
  Unused << reader.ReadBits(5);  // reserved
  hvcc.bit_depth_chroma_minus8 = reader.ReadBits(3);
  hvcc.avgFrameRate = reader.ReadBits(16);
  hvcc.constantFrameRate = reader.ReadBits(2);
  hvcc.numTemporalLayers = reader.ReadBits(3);
  hvcc.temporalIdNested = reader.ReadBit();
  hvcc.lengthSizeMinusOne = reader.ReadBits(2);
  const uint8_t numOfArrays = reader.ReadBits(8);
  for (uint8_t idx = 0; idx < numOfArrays; idx++) {
    Unused << reader.ReadBits(2);  // array_completeness + reserved
    const uint8_t nalUnitType = reader.ReadBits(6);
    const uint16_t numNalus = reader.ReadBits(16);
    LOGV("nalu-type=%u, nalu-num=%u", nalUnitType, numNalus);
    for (uint16_t nIdx = 0; nIdx < numNalus; nIdx++) {
      const uint16_t nalUnitLength = reader.ReadBits(16);
      if (reader.BitsLeft() < nalUnitLength * 8) {
        LOG("Aborting parsing, NALU size (%u bits) is larger than remaining "
            "(%zu bits)!",
            nalUnitLength * 8u, reader.BitsLeft());
        // We return what we've parsed so far and ignore the rest.
        hvcc.mByteBuffer = aExtraData;
        return hvcc;
      }
      const uint8_t* currentPtr =
          aExtraData->Elements() + reader.BitCount() / 8;
      H265NALU nalu(currentPtr, nalUnitLength);
      // ReadBits can only read at most 32 bits at a time.
      uint32_t nalSize = nalUnitLength * 8;
      while (nalSize > 0) {
        uint32_t readBits = nalSize > 32 ? 32 : nalSize;
        reader.ReadBits(readBits);
        nalSize -= readBits;
      }
      // Per ISO_IEC-14496-15-2022, 8.3.2.1.3 Semantics, NALU should only be
      // SPS/PPS/VPS or SEI, ignore all the other types of NALU.
      if (nalu.IsSPS() || nalu.IsPPS() || nalu.IsVPS() || nalu.IsSEI()) {
        hvcc.mNALUs.AppendElement(nalu);
      } else {
        LOG("Ignore NALU (%u) which is not SPS/PPS/VPS or SEI",
            nalu.mNalUnitType);
      }
    }
  }
  hvcc.mByteBuffer = aExtraData;
  return hvcc;
}

uint32_t HVCCConfig::NumSPS() const {
  uint32_t spsCounter = 0;
  for (const auto& nalu : mNALUs) {
    if (nalu.IsSPS()) {
      spsCounter++;
    }
  }
  return spsCounter;
}

bool HVCCConfig::HasSPS() const {
  bool hasSPS = false;
  for (const auto& nalu : mNALUs) {
    if (nalu.IsSPS()) {
      hasSPS = true;
      break;
    }
  }
  return hasSPS;
}

/* static */
Result<H265SPS, nsresult> H265::DecodeSPSFromSPSNALU(const H265NALU& aSPSNALU) {
  MOZ_ASSERT(aSPSNALU.IsSPS());
  RefPtr<MediaByteBuffer> rbsp = H265::DecodeNALUnit(aSPSNALU.mNALU);
  if (!rbsp) {
    LOG("Failed to decode NALU");
    return Err(NS_ERROR_FAILURE);
  }

  // H265 spec, 7.3.2.2.1 seq_parameter_set_rbsp
  H265SPS sps;
  BitReader reader(rbsp);
  sps.sps_video_parameter_set_id = reader.ReadBits(4);
  IN_RANGE_OR_RETURN(sps.sps_video_parameter_set_id, 0, 15);
  sps.sps_max_sub_layers_minus1 = reader.ReadBits(3);
  IN_RANGE_OR_RETURN(sps.sps_max_sub_layers_minus1, 0, 6);
  sps.sps_temporal_id_nesting_flag = reader.ReadBit();

  if (auto rv = ParseProfileTierLevel(
          reader, true /* aProfilePresentFlag, true per spec*/,
          sps.sps_max_sub_layers_minus1, sps.profile_tier_level);
      rv.isErr()) {
    LOG("Failed to parse the profile tier level.");
    return Err(NS_ERROR_FAILURE);
  }

  sps.sps_seq_parameter_set_id = reader.ReadUE();
  IN_RANGE_OR_RETURN(sps.sps_seq_parameter_set_id, 0, 15);
  sps.chroma_format_idc = reader.ReadUE();
  IN_RANGE_OR_RETURN(sps.chroma_format_idc, 0, 3);

  if (sps.chroma_format_idc == 3) {
    sps.separate_colour_plane_flag = reader.ReadBit();
  }

  // From Table 6-1.
  if (sps.chroma_format_idc == 1) {
    sps.subWidthC = sps.subHeightC = 2;
  } else if (sps.chroma_format_idc == 2) {
    sps.subWidthC = 2;
    sps.subHeightC = 1;
  } else {
    sps.subWidthC = sps.subHeightC = 1;
  }

  NON_ZERO_OR_RETURN(sps.pic_width_in_luma_samples, reader.ReadUE());
  NON_ZERO_OR_RETURN(sps.pic_height_in_luma_samples, reader.ReadUE());
  {
    // (A-2) Calculate maxDpbSize
    const auto maxLumaPs = sps.profile_tier_level.GetMaxLumaPs();
    CheckedUint32 picSize = sps.pic_height_in_luma_samples;
    picSize *= sps.pic_width_in_luma_samples;
    if (!picSize.isValid()) {
      LOG("Invalid picture size");
      return Err(NS_ERROR_FAILURE);
    }
    const auto picSizeInSamplesY = picSize.value();
    const auto maxDpbPicBuf = sps.profile_tier_level.GetDpbMaxPicBuf();
    if (picSizeInSamplesY <= (maxLumaPs >> 2)) {
      sps.maxDpbSize = std::min(4 * maxDpbPicBuf, 16u);
    } else if (picSizeInSamplesY <= (maxLumaPs >> 1)) {
      sps.maxDpbSize = std::min(2 * maxDpbPicBuf, 16u);
    } else if (picSizeInSamplesY <= ((3 * maxLumaPs) >> 2)) {
      sps.maxDpbSize = std::min((4 * maxDpbPicBuf) / 3, 16u);
    } else {
      sps.maxDpbSize = maxDpbPicBuf;
    }
  }

  sps.conformance_window_flag = reader.ReadBit();
  if (sps.conformance_window_flag) {
    sps.conf_win_left_offset = reader.ReadUE();
    sps.conf_win_right_offset = reader.ReadUE();
    sps.conf_win_top_offset = reader.ReadUE();
    sps.conf_win_bottom_offset = reader.ReadUE();
  }
  sps.bit_depth_luma_minus8 = reader.ReadUE();
  IN_RANGE_OR_RETURN(sps.bit_depth_luma_minus8, 0, 8);
  sps.bit_depth_chroma_minus8 = reader.ReadUE();
  IN_RANGE_OR_RETURN(sps.bit_depth_chroma_minus8, 0, 8);
  sps.log2_max_pic_order_cnt_lsb_minus4 = reader.ReadUE();
  IN_RANGE_OR_RETURN(sps.log2_max_pic_order_cnt_lsb_minus4, 0, 12);
  sps.sps_sub_layer_ordering_info_present_flag = reader.ReadBit();
  for (auto i = sps.sps_sub_layer_ordering_info_present_flag
                    ? 0
                    : sps.sps_max_sub_layers_minus1;
       i <= sps.sps_max_sub_layers_minus1; i++) {
    sps.sps_max_dec_pic_buffering_minus1[i] = reader.ReadUE();
    IN_RANGE_OR_RETURN(sps.sps_max_dec_pic_buffering_minus1[i], 0,
                       sps.maxDpbSize - 1);
    sps.sps_max_num_reorder_pics[i] = reader.ReadUE();
    IN_RANGE_OR_RETURN(sps.sps_max_num_reorder_pics[i], 0,
                       sps.sps_max_dec_pic_buffering_minus1[i]);
    // 7.4.3.2.1, see sps_max_dec_pic_buffering_minus1 and
    // sps_max_num_reorder_pics, "When i is greater than 0, ....".
    if (i > 0) {
      TRUE_OR_RETURN(sps.sps_max_dec_pic_buffering_minus1[i] >=
                     sps.sps_max_dec_pic_buffering_minus1[i - 1]);
      TRUE_OR_RETURN(sps.sps_max_num_reorder_pics[i] >=
                     sps.sps_max_num_reorder_pics[i - 1]);
    }
    sps.sps_max_latency_increase_plus1[i] = reader.ReadUE();
    IN_RANGE_OR_RETURN(sps.sps_max_latency_increase_plus1[i], 0, 0xFFFFFFFE);
  }
  sps.log2_min_luma_coding_block_size_minus3 = reader.ReadUE();
  sps.log2_diff_max_min_luma_coding_block_size = reader.ReadUE();
  sps.log2_min_luma_transform_block_size_minus2 = reader.ReadUE();
  sps.log2_diff_max_min_luma_transform_block_size = reader.ReadUE();
  sps.max_transform_hierarchy_depth_inter = reader.ReadUE();
  sps.max_transform_hierarchy_depth_intra = reader.ReadUE();
  const auto scaling_list_enabled_flag = reader.ReadBit();
  if (scaling_list_enabled_flag) {
    Unused << reader.ReadBit();  // sps_scaling_list_data_present_flag
    if (auto rv = ParseAndIgnoreScalingListData(reader); rv.isErr()) {
      LOG("Failed to parse scaling list data.");
      return Err(NS_ERROR_FAILURE);
    }
  }

  // amp_enabled_flag and sample_adaptive_offset_enabled_flag
  Unused << reader.ReadBits(2);

  sps.pcm_enabled_flag = reader.ReadBit();
  if (sps.pcm_enabled_flag) {
    sps.pcm_sample_bit_depth_luma_minus1 = reader.ReadBits(3);
    IN_RANGE_OR_RETURN(sps.pcm_sample_bit_depth_luma_minus1, 0,
                       sps.BitDepthLuma());
    sps.pcm_sample_bit_depth_chroma_minus1 = reader.ReadBits(3);
    IN_RANGE_OR_RETURN(sps.pcm_sample_bit_depth_chroma_minus1, 0,
                       sps.BitDepthChroma());
    sps.log2_min_pcm_luma_coding_block_size_minus3 = reader.ReadUE();
    IN_RANGE_OR_RETURN(sps.log2_min_pcm_luma_coding_block_size_minus3, 0, 2);
    uint32_t log2MinIpcmCbSizeY{sps.log2_min_pcm_luma_coding_block_size_minus3 +
                                3};
    sps.log2_diff_max_min_pcm_luma_coding_block_size = reader.ReadUE();
    {
      // Validate value
      CheckedUint32 log2MaxIpcmCbSizeY{
          sps.log2_diff_max_min_pcm_luma_coding_block_size};
      log2MaxIpcmCbSizeY += log2MinIpcmCbSizeY;
      CheckedUint32 minCbLog2SizeY{sps.log2_min_luma_coding_block_size_minus3};
      minCbLog2SizeY += 3;  // (7-10)
      CheckedUint32 ctbLog2SizeY{minCbLog2SizeY};
      ctbLog2SizeY += sps.log2_diff_max_min_luma_coding_block_size;  // (7-11)
      IN_RANGE_OR_RETURN(log2MaxIpcmCbSizeY.value(), 0,
                         std::min(ctbLog2SizeY.value(), uint32_t(5)));
    }
    sps.pcm_loop_filter_disabled_flag = reader.ReadBit();
  }

  sps.num_short_term_ref_pic_sets = reader.ReadUE();
  IN_RANGE_OR_RETURN(sps.num_short_term_ref_pic_sets, 0,
                     kMaxShortTermRefPicSets);
  for (auto i = 0; i < sps.num_short_term_ref_pic_sets; i++) {
    if (auto rv = ParseStRefPicSet(reader, i, sps); rv.isErr()) {
      LOG("Failed to parse short-term reference picture set.");
      return Err(NS_ERROR_FAILURE);
    }
  }
  const auto long_term_ref_pics_present_flag = reader.ReadBit();
  if (long_term_ref_pics_present_flag) {
    uint32_t num_long_term_ref_pics_sps;
    num_long_term_ref_pics_sps = reader.ReadUE();
    IN_RANGE_OR_RETURN(num_long_term_ref_pics_sps, 0, kMaxLongTermRefPicSets);
    for (auto i = 0; i < num_long_term_ref_pics_sps; i++) {
      Unused << reader.ReadBits(sps.log2_max_pic_order_cnt_lsb_minus4 +
                                4);  // lt_ref_pic_poc_lsb_sps[i]
      Unused << reader.ReadBit();    // used_by_curr_pic_lt_sps_flag
    }
  }
  sps.sps_temporal_mvp_enabled_flag = reader.ReadBit();
  sps.strong_intra_smoothing_enabled_flag = reader.ReadBit();
  const auto vui_parameters_present_flag = reader.ReadBit();
  if (vui_parameters_present_flag) {
    if (auto rv = ParseVuiParameters(reader, sps); rv.isErr()) {
      LOG("Failed to parse VUI parameter.");
      return Err(NS_ERROR_FAILURE);
    }
  }

  // The rest is extension data we don't care about, so no need to parse them.
  return sps;
}

/* static */
Result<H265SPS, nsresult> H265::DecodeSPSFromHVCCExtraData(
    const mozilla::MediaByteBuffer* aExtraData) {
  auto rv = HVCCConfig::Parse(aExtraData);
  if (rv.isErr()) {
    LOG("Only support HVCC extra-data");
    return Err(NS_ERROR_FAILURE);
  }
  const auto& hvcc = rv.unwrap();
  const H265NALU* spsNALU = nullptr;
  for (const auto& nalu : hvcc.mNALUs) {
    if (nalu.IsSPS()) {
      spsNALU = &nalu;
      break;
    }
  }
  if (!spsNALU) {
    LOG("No sps found");
    return Err(NS_ERROR_FAILURE);
  }
  return DecodeSPSFromSPSNALU(*spsNALU);
}

/* static */
Result<Ok, nsresult> H265::ParseProfileTierLevel(
    BitReader& aReader, bool aProfilePresentFlag,
    uint8_t aMaxNumSubLayersMinus1, H265ProfileTierLevel& aProfile) {
  // H265 spec, 7.3.3 Profile, tier and level syntax
  if (aProfilePresentFlag) {
    aProfile.general_profile_space = aReader.ReadBits(2);
    aProfile.general_tier_flag = aReader.ReadBit();
    aProfile.general_profile_idc = aReader.ReadBits(5);
    IN_RANGE_OR_RETURN(aProfile.general_profile_idc, 0, 11);
    aProfile.general_profile_compatibility_flags = aReader.ReadU32();
    aProfile.general_progressive_source_flag = aReader.ReadBit();
    aProfile.general_interlaced_source_flag = aReader.ReadBit();
    aProfile.general_non_packed_constraint_flag = aReader.ReadBit();
    aProfile.general_frame_only_constraint_flag = aReader.ReadBit();
    // ignored attributes, in total general_reserved_zero_43bits
    Unused << aReader.ReadBits(32);
    Unused << aReader.ReadBits(11);
    // general_inbld_flag or general_reserved_zero_bit
    Unused << aReader.ReadBit();
  }
  aProfile.general_level_idc = aReader.ReadBits(8);

  // Following are all ignored attributes.
  bool sub_layer_profile_present_flag[8];
  bool sub_layer_level_present_flag[8];
  for (auto i = 0; i < aMaxNumSubLayersMinus1; i++) {
    sub_layer_profile_present_flag[i] = aReader.ReadBit();
    sub_layer_level_present_flag[i] = aReader.ReadBit();
  }
  if (aMaxNumSubLayersMinus1 > 0) {
    for (auto i = aMaxNumSubLayersMinus1; i < 8; i++) {
      // reserved_zero_2bits
      Unused << aReader.ReadBits(2);
    }
  }
  for (auto i = 0; i < aMaxNumSubLayersMinus1; i++) {
    if (sub_layer_profile_present_flag[i]) {
      // sub_layer_profile_space, sub_layer_tier_flag, sub_layer_profile_idc
      Unused << aReader.ReadBits(8);
      // sub_layer_profile_compatibility_flag
      Unused << aReader.ReadBits(32);
      // sub_layer_progressive_source_flag, sub_layer_interlaced_source_flag,
      // sub_layer_non_packed_constraint_flag,
      // sub_layer_frame_only_constraint_flag
      Unused << aReader.ReadBits(4);
      // ignored attributes, in total general_reserved_zero_43bits
      Unused << aReader.ReadBits(32);
      Unused << aReader.ReadBits(11);
      // sub_layer_inbld_flag or reserved_zero_bit
      Unused << aReader.ReadBit();
    }
    if (sub_layer_level_present_flag[i]) {
      Unused << aReader.ReadBits(8);  // sub_layer_level_idc
    }
  }
  return Ok();
}

uint32_t H265ProfileTierLevel::GetMaxLumaPs() const {
  // From Table A.8 - General tier and level limits.
  // "general_level_idc and sub_layer_level_idc[ i ] shall be set equal to a
  // value of 30 times the level number specified in Table A.8".
  if (general_level_idc <= 30) {  // level 1
    return 36864;
  }
  if (general_level_idc <= 60) {  // level 2
    return 122880;
  }
  if (general_level_idc <= 63) {  // level 2.1
    return 245760;
  }
  if (general_level_idc <= 90) {  // level 3
    return 552960;
  }
  if (general_level_idc <= 93) {  // level 3.1
    return 983040;
  }
  if (general_level_idc <= 123) {  // level 4, 4.1
    return 2228224;
  }
  if (general_level_idc <= 156) {  // level 5, 5.1, 5.2
    return 8912896;
  }
  // level 6, 6.1, 6.2 - beyond that there's no actual limit.
  return 35651584;
}

uint32_t H265ProfileTierLevel::GetDpbMaxPicBuf() const {
  // From A.4.2 - Profile-specific level limits for the video profiles.
  // "maxDpbPicBuf is equal to 6 for all profiles where the value of
  // sps_curr_pic_ref_enabled_flag is required to be equal to 0 and 7 for all
  // profiles where the value of sps_curr_pic_ref_enabled_flag is not required
  // to be equal to 0." From A.3 Profile, the flag in the main, main still,
  // range extensions and high throughput is required to be zero.
  return (general_profile_idc >= H265ProfileIdc::kProfileIdcMain &&
          general_profile_idc <= H265ProfileIdc::kProfileIdcHighThroughput)
             ? 6
             : 7;
}

/* static */
Result<Ok, nsresult> H265::ParseAndIgnoreScalingListData(BitReader& aReader) {
  // H265 spec, 7.3.4 Scaling list data syntax
  for (auto sizeIdx = 0; sizeIdx < 4; sizeIdx++) {
    for (auto matrixIdx = 0; matrixIdx < 6;
         matrixIdx += (sizeIdx == 3) ? 3 : 1) {
      const auto scaling_list_pred_mode_flag = aReader.ReadBit();
      if (!scaling_list_pred_mode_flag) {
        Unused << aReader.ReadUE();  // scaling_list_pred_matrix_id_delta
      } else {
        int32_t coefNum = std::min(64, (1 << (4 + (sizeIdx << 1))));
        if (sizeIdx > 1) {
          Unused << aReader.ReadSE();  // scaling_list_dc_coef_minus8
        }
        for (auto i = 0; i < coefNum; i++) {
          Unused << aReader.ReadSE();  // scaling_list_delta_coef
        }
      }
    }
  }
  return Ok();
}

/* static */
Result<Ok, nsresult> H265::ParseStRefPicSet(BitReader& aReader,
                                            uint32_t aStRpsIdx, H265SPS& aSPS) {
  // H265 Spec, 7.3.7 Short-term reference picture set syntax
  MOZ_ASSERT(aStRpsIdx < kMaxShortTermRefPicSets);
  bool inter_ref_pic_set_prediction_flag = false;
  H265StRefPicSet& curStRefPicSet = aSPS.st_ref_pic_set[aStRpsIdx];
  if (aStRpsIdx != 0) {
    inter_ref_pic_set_prediction_flag = aReader.ReadBit();
  }
  if (inter_ref_pic_set_prediction_flag) {
    int delta_idx_minus1 = 0;
    if (aStRpsIdx == aSPS.num_short_term_ref_pic_sets) {
      delta_idx_minus1 = aReader.ReadUE();
      IN_RANGE_OR_RETURN(delta_idx_minus1, 0, aStRpsIdx - 1);
    }
    const uint32_t RefRpsIdx = aStRpsIdx - (delta_idx_minus1 + 1);  // (7-59)
    const bool delta_rps_sign = aReader.ReadBit();
    const uint32_t abs_delta_rps_minus1 = aReader.ReadUE();
    IN_RANGE_OR_RETURN(abs_delta_rps_minus1, 0, 0x7FFF);
    const int32_t deltaRps =
        (1 - 2 * delta_rps_sign) *
        AssertedCast<int32_t>(abs_delta_rps_minus1 + 1);  // (7-60)

    bool used_by_curr_pic_flag[kMaxShortTermRefPicSets] = {};
    bool use_delta_flag[kMaxShortTermRefPicSets] = {};
    // 7.4.8 - use_delta_flag defaults to 1 if not present.
    std::fill_n(use_delta_flag, kMaxShortTermRefPicSets, true);
    const H265StRefPicSet& refSet = aSPS.st_ref_pic_set[RefRpsIdx];
    for (auto j = 0; j <= refSet.numDeltaPocs; j++) {
      used_by_curr_pic_flag[j] = aReader.ReadBit();
      if (!used_by_curr_pic_flag[j]) {
        use_delta_flag[j] = aReader.ReadBit();
      }
    }
    // Calculate fields (7-61)
    uint32_t i = 0;
    for (int64_t j = static_cast<int64_t>(refSet.num_positive_pics) - 1; j >= 0;
         j--) {
      MOZ_DIAGNOSTIC_ASSERT(j < kMaxShortTermRefPicSets);
      int64_t d_poc = refSet.deltaPocS1[j] + deltaRps;
      if (d_poc < 0 && use_delta_flag[refSet.num_negative_pics + j]) {
        curStRefPicSet.deltaPocS0[i] = d_poc;
        curStRefPicSet.usedByCurrPicS0[i++] =
            used_by_curr_pic_flag[refSet.num_negative_pics + j];
      }
    }
    if (deltaRps < 0 && use_delta_flag[refSet.numDeltaPocs]) {
      curStRefPicSet.deltaPocS0[i] = deltaRps;
      curStRefPicSet.usedByCurrPicS0[i++] =
          used_by_curr_pic_flag[refSet.numDeltaPocs];
    }
    for (auto j = 0; j < refSet.num_negative_pics; j++) {
      MOZ_DIAGNOSTIC_ASSERT(j < kMaxShortTermRefPicSets);
      int64_t d_poc = refSet.deltaPocS0[j] + deltaRps;
      if (d_poc < 0 && use_delta_flag[j]) {
        curStRefPicSet.deltaPocS0[i] = d_poc;
        curStRefPicSet.usedByCurrPicS0[i++] = used_by_curr_pic_flag[j];
      }
    }
    curStRefPicSet.num_negative_pics = i;
    // Calculate fields (7-62)
    i = 0;
    for (int64_t j = static_cast<int64_t>(refSet.num_negative_pics) - 1; j >= 0;
         j--) {
      MOZ_DIAGNOSTIC_ASSERT(j < kMaxShortTermRefPicSets);
      int64_t d_poc = refSet.deltaPocS0[j] + deltaRps;
      if (d_poc > 0 && use_delta_flag[j]) {
        curStRefPicSet.deltaPocS1[i] = d_poc;
        curStRefPicSet.usedByCurrPicS1[i++] = used_by_curr_pic_flag[j];
      }
    }
    if (deltaRps > 0 && use_delta_flag[refSet.numDeltaPocs]) {
      curStRefPicSet.deltaPocS1[i] = deltaRps;
      curStRefPicSet.usedByCurrPicS1[i++] =
          used_by_curr_pic_flag[refSet.numDeltaPocs];
    }
    for (auto j = 0; j < refSet.num_positive_pics; j++) {
      MOZ_DIAGNOSTIC_ASSERT(j < kMaxShortTermRefPicSets);
      int64_t d_poc = refSet.deltaPocS1[j] + deltaRps;
      if (d_poc > 0 && use_delta_flag[refSet.num_negative_pics + j]) {
        curStRefPicSet.deltaPocS1[i] = d_poc;
        curStRefPicSet.usedByCurrPicS1[i++] =
            used_by_curr_pic_flag[refSet.num_negative_pics + j];
      }
    }
    curStRefPicSet.num_positive_pics = i;
  } else {
    curStRefPicSet.num_negative_pics = aReader.ReadUE();
    curStRefPicSet.num_positive_pics = aReader.ReadUE();
    const uint32_t spsMaxDecPicBufferingMinus1 =
        aSPS.sps_max_dec_pic_buffering_minus1[aSPS.sps_max_sub_layers_minus1];
    IN_RANGE_OR_RETURN(curStRefPicSet.num_negative_pics, 0,
                       spsMaxDecPicBufferingMinus1);
    CheckedUint32 maxPositivePics{spsMaxDecPicBufferingMinus1};
    maxPositivePics -= curStRefPicSet.num_negative_pics;
    IN_RANGE_OR_RETURN(curStRefPicSet.num_positive_pics, 0,
                       maxPositivePics.value());
    for (auto i = 0; i < curStRefPicSet.num_negative_pics; i++) {
      const uint32_t delta_poc_s0_minus1 = aReader.ReadUE();
      IN_RANGE_OR_RETURN(delta_poc_s0_minus1, 0, 0x7FFF);
      if (i == 0) {
        // (7-67)
        curStRefPicSet.deltaPocS0[i] = -(delta_poc_s0_minus1 + 1);
      } else {
        // (7-69)
        curStRefPicSet.deltaPocS0[i] =
            curStRefPicSet.deltaPocS0[i - 1] - (delta_poc_s0_minus1 + 1);
      }
      curStRefPicSet.usedByCurrPicS0[i] = aReader.ReadBit();
    }
    for (auto i = 0; i < curStRefPicSet.num_positive_pics; i++) {
      const int delta_poc_s1_minus1 = aReader.ReadUE();
      IN_RANGE_OR_RETURN(delta_poc_s1_minus1, 0, 0x7FFF);
      if (i == 0) {
        // (7-68)
        curStRefPicSet.deltaPocS1[i] = delta_poc_s1_minus1 + 1;
      } else {
        // (7-70)
        curStRefPicSet.deltaPocS1[i] =
            curStRefPicSet.deltaPocS1[i - 1] + delta_poc_s1_minus1 + 1;
      }
      curStRefPicSet.usedByCurrPicS1[i] = aReader.ReadBit();
    }
  }
  // (7-71)
  curStRefPicSet.numDeltaPocs =
      curStRefPicSet.num_negative_pics + curStRefPicSet.num_positive_pics;
  return Ok();
}

/* static */
Result<Ok, nsresult> H265::ParseVuiParameters(BitReader& aReader,
                                              H265SPS& aSPS) {
  // VUI parameters: Table E.1 "Interpretation of sample aspect ratio indicator"
  static constexpr int kTableSarWidth[] = {0,  1,  12, 10, 16,  40, 24, 20, 32,
                                           80, 18, 15, 64, 160, 4,  3,  2};
  static constexpr int kTableSarHeight[] = {0,  1,  11, 11, 11, 33, 11, 11, 11,
                                            33, 11, 11, 33, 99, 3,  2,  1};
  static_assert(std::size(kTableSarWidth) == std::size(kTableSarHeight),
                "sar tables must have the same size");
  aSPS.vui_parameters = Some(H265VUIParameters());
  H265VUIParameters* vui = aSPS.vui_parameters.ptr();

  const auto aspect_ratio_info_present_flag = aReader.ReadBit();
  if (aspect_ratio_info_present_flag) {
    const auto aspect_ratio_idc = aReader.ReadBits(8);
    constexpr int kExtendedSar = 255;
    if (aspect_ratio_idc == kExtendedSar) {
      vui->sar_width = aReader.ReadBits(16);
      vui->sar_height = aReader.ReadBits(16);
    } else {
      const auto max_aspect_ratio_idc = std::size(kTableSarWidth) - 1;
      IN_RANGE_OR_RETURN(aspect_ratio_idc, 0, max_aspect_ratio_idc);
      vui->sar_width = kTableSarWidth[aspect_ratio_idc];
      vui->sar_height = kTableSarHeight[aspect_ratio_idc];
    }
  }

  const auto overscan_info_present_flag = aReader.ReadBit();
  if (overscan_info_present_flag) {
    Unused << aReader.ReadBit();  // overscan_appropriate_flag
  }

  const auto video_signal_type_present_flag = aReader.ReadBit();
  if (video_signal_type_present_flag) {
    Unused << aReader.ReadBits(3);  // video_format
    vui->video_full_range_flag = aReader.ReadBit();
    const auto colour_description_present_flag = aReader.ReadBit();
    if (colour_description_present_flag) {
      vui->colour_primaries.emplace(aReader.ReadBits(8));
      vui->transfer_characteristics.emplace(aReader.ReadBits(8));
      vui->matrix_coeffs.emplace(aReader.ReadBits(8));
    }
  }

  const auto chroma_loc_info_present_flag = aReader.ReadBit();
  if (chroma_loc_info_present_flag) {
    Unused << aReader.ReadUE();  // chroma_sample_loc_type_top_field
    Unused << aReader.ReadUE();  // chroma_sample_loc_type_bottom_field
  }

  // Ignore neutral_chroma_indication_flag, field_seq_flag and
  // frame_field_info_present_flag.
  Unused << aReader.ReadBits(3);

  const auto default_display_window_flag = aReader.ReadBit();
  if (default_display_window_flag) {
    uint32_t def_disp_win_left_offset = aReader.ReadUE();
    uint32_t def_disp_win_right_offset = aReader.ReadUE();
    uint32_t def_disp_win_top_offset = aReader.ReadUE();
    uint32_t def_disp_win_bottom_offset = aReader.ReadUE();
    // (E-68) + (E-69)
    aSPS.mDisplayWidth = aSPS.subWidthC;
    aSPS.mDisplayWidth *=
        (aSPS.conf_win_left_offset + def_disp_win_left_offset);
    aSPS.mDisplayWidth *=
        (aSPS.conf_win_right_offset + def_disp_win_right_offset);
    if (!aSPS.mDisplayWidth.isValid()) {
      LOG("mDisplayWidth overflow!");
      return Err(NS_ERROR_FAILURE);
    }
    IN_RANGE_OR_RETURN(aSPS.mDisplayWidth.value(), 0,
                       aSPS.pic_width_in_luma_samples);

    // (E-70) + (E-71)
    aSPS.mDisplayHeight = aSPS.subHeightC;
    aSPS.mDisplayHeight *= (aSPS.conf_win_top_offset + def_disp_win_top_offset);
    aSPS.mDisplayHeight *=
        (aSPS.conf_win_bottom_offset + def_disp_win_bottom_offset);
    if (!aSPS.mDisplayHeight.isValid()) {
      LOG("mDisplayHeight overflow!");
      return Err(NS_ERROR_FAILURE);
    }
    IN_RANGE_OR_RETURN(aSPS.mDisplayHeight.value(), 0,
                       aSPS.pic_height_in_luma_samples);
  }

  const auto vui_timing_info_present_flag = aReader.ReadBit();
  if (vui_timing_info_present_flag) {
    Unused << aReader.ReadU32();  // vui_num_units_in_tick
    Unused << aReader.ReadU32();  // vui_time_scale
    const auto vui_poc_proportional_to_timing_flag = aReader.ReadBit();
    if (vui_poc_proportional_to_timing_flag) {
      Unused << aReader.ReadUE();  // vui_num_ticks_poc_diff_one_minus1
    }
    const auto vui_hrd_parameters_present_flag = aReader.ReadBit();
    if (vui_hrd_parameters_present_flag) {
      if (auto rv = ParseAndIgnoreHrdParameters(aReader, true,
                                                aSPS.sps_max_sub_layers_minus1);
          rv.isErr()) {
        LOG("Failed to parse Hrd parameters");
        return rv;
      }
    }
  }

  const auto bitstream_restriction_flag = aReader.ReadBit();
  if (bitstream_restriction_flag) {
    // Skip tiles_fixed_structure_flag, motion_vectors_over_pic_boundaries_flag
    // and restricted_ref_pic_lists_flag.
    Unused << aReader.ReadBits(3);
    Unused << aReader.ReadUE();  // min_spatial_segmentation_idc
    Unused << aReader.ReadUE();  // max_bytes_per_pic_denom
    Unused << aReader.ReadUE();  // max_bits_per_min_cu_denom
    Unused << aReader.ReadUE();  // log2_max_mv_length_horizontal
    Unused << aReader.ReadUE();  // log2_max_mv_length_vertical
  }
  return Ok();
}

/* static */
Result<Ok, nsresult> H265::ParseAndIgnoreHrdParameters(
    BitReader& aReader, bool aCommonInfPresentFlag,
    int aMaxNumSubLayersMinus1) {
  // H265 Spec, E.2.2 HRD parameters syntax
  bool nal_hrd_parameters_present_flag = false;
  bool vcl_hrd_parameters_present_flag = false;
  bool sub_pic_hrd_params_present_flag = false;
  if (aCommonInfPresentFlag) {
    nal_hrd_parameters_present_flag = aReader.ReadBit();
    vcl_hrd_parameters_present_flag = aReader.ReadBit();
    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
      sub_pic_hrd_params_present_flag = aReader.ReadBit();
      if (sub_pic_hrd_params_present_flag) {
        Unused << aReader.ReadBits(8);  // tick_divisor_minus2
        // du_cpb_removal_delay_increment_length_minus1
        Unused << aReader.ReadBits(5);
        // sub_pic_cpb_params_in_pic_timing_sei_flag
        Unused << aReader.ReadBits(1);
        Unused << aReader.ReadBits(5);  // dpb_output_delay_du_length_minus1
      }

      Unused << aReader.ReadBits(4);  // bit_rate_scale
      Unused << aReader.ReadBits(4);  // cpb_size_scale
      if (sub_pic_hrd_params_present_flag) {
        Unused << aReader.ReadBits(4);  // cpb_size_du_scale
      }
      Unused << aReader.ReadBits(5);  // initial_cpb_removal_delay_length_minus1
      Unused << aReader.ReadBits(5);  // au_cpb_removal_delay_length_minus1
      Unused << aReader.ReadBits(5);  // dpb_output_delay_length_minus1
    }
  }
  for (int i = 0; i <= aMaxNumSubLayersMinus1; i++) {
    bool fixed_pic_rate_within_cvs_flag = false;
    if (auto fixed_pic_rate_general_flag = aReader.ReadBit();
        !fixed_pic_rate_general_flag) {
      fixed_pic_rate_within_cvs_flag = aReader.ReadBit();
    }
    bool low_delay_hrd_flag = false;
    if (fixed_pic_rate_within_cvs_flag) {
      Unused << aReader.ReadUE();  // elemental_duration_in_tc_minus1
    } else {
      low_delay_hrd_flag = aReader.ReadBit();
    }
    int cpb_cnt_minus1 = 0;
    if (!low_delay_hrd_flag) {
      cpb_cnt_minus1 = aReader.ReadUE();
      IN_RANGE_OR_RETURN(cpb_cnt_minus1, 0, 31);
    }
    if (nal_hrd_parameters_present_flag) {
      if (auto rv = ParseAndIgnoreSubLayerHrdParameters(
              aReader, cpb_cnt_minus1 + 1, sub_pic_hrd_params_present_flag);
          rv.isErr()) {
        LOG("Failed to parse nal Hrd parameters");
        return rv;
      };
    }
    if (vcl_hrd_parameters_present_flag) {
      if (auto rv = ParseAndIgnoreSubLayerHrdParameters(
              aReader, cpb_cnt_minus1 + 1, sub_pic_hrd_params_present_flag);
          rv.isErr()) {
        LOG("Failed to parse vcl Hrd parameters");
        return rv;
      }
    }
  }
  return Ok();
}

/* static */
Result<Ok, nsresult> H265::ParseAndIgnoreSubLayerHrdParameters(
    BitReader& aReader, int aCpbCnt, bool aSubPicHrdParamsPresentFlag) {
  // H265 Spec, E.2.3 Sub-layer HRD parameters syntax
  for (auto i = 0; i < aCpbCnt; i++) {
    Unused << aReader.ReadUE();  // bit_rate_value_minus1
    Unused << aReader.ReadUE();  // cpb_size_value_minus1
    if (aSubPicHrdParamsPresentFlag) {
      Unused << aReader.ReadUE();  // cpb_size_du_value_minus1
      Unused << aReader.ReadUE();  // bit_rate_du_value_minus1
    }
    Unused << aReader.ReadBit();  // cbr_flag
  }
  return Ok();
}

bool H265SPS::operator==(const H265SPS& aOther) const {
  return memcmp(this, &aOther, sizeof(H265SPS)) == 0;
}

bool H265SPS::operator!=(const H265SPS& aOther) const {
  return !(operator==(aOther));
}

gfx::IntSize H265SPS::GetImageSize() const {
  return gfx::IntSize(pic_width_in_luma_samples, pic_height_in_luma_samples);
}

gfx::IntSize H265SPS::GetDisplaySize() const {
  if (mDisplayWidth.value() == 0 || mDisplayHeight.value() == 0) {
    return GetImageSize();
  }
  return gfx::IntSize(mDisplayWidth.value(), mDisplayHeight.value());
}

gfx::ColorDepth H265SPS::ColorDepth() const {
  if (bit_depth_luma_minus8 != 0 && bit_depth_luma_minus8 != 2 &&
      bit_depth_luma_minus8 != 4) {
    // We don't know what that is, just assume 8 bits to prevent decoding
    // regressions if we ever encounter those.
    return gfx::ColorDepth::COLOR_8;
  }
  return gfx::ColorDepthForBitDepth(BitDepthLuma());
}

// PrimaryID, TransferID and MatrixID are defined in ByteStreamsUtils.h
static PrimaryID GetPrimaryID(const Maybe<uint8_t>& aPrimary) {
  if (!aPrimary || *aPrimary < 1 || *aPrimary > 22 || *aPrimary == 3) {
    return PrimaryID::INVALID;
  }
  if (*aPrimary > 12 && *aPrimary < 22) {
    return PrimaryID::INVALID;
  }
  return static_cast<PrimaryID>(*aPrimary);
}

static TransferID GetTransferID(const Maybe<uint8_t>& aTransfer) {
  if (!aTransfer || *aTransfer < 1 || *aTransfer > 18 || *aTransfer == 3) {
    return TransferID::INVALID;
  }
  return static_cast<TransferID>(*aTransfer);
}

static MatrixID GetMatrixID(const Maybe<uint8_t>& aMatrix) {
  if (!aMatrix || *aMatrix > 11 || *aMatrix == 3) {
    return MatrixID::INVALID;
  }
  return static_cast<MatrixID>(*aMatrix);
}

gfx::YUVColorSpace H265SPS::ColorSpace() const {
  // Bitfield, note that guesses with higher values take precedence over
  // guesses with lower values.
  enum Guess {
    GUESS_BT601 = 1 << 0,
    GUESS_BT709 = 1 << 1,
    GUESS_BT2020 = 1 << 2,
  };

  uint32_t guess = 0;
  if (vui_parameters) {
    switch (GetPrimaryID(vui_parameters->colour_primaries)) {
      case PrimaryID::BT709:
        guess |= GUESS_BT709;
        break;
      case PrimaryID::BT470M:
      case PrimaryID::BT470BG:
      case PrimaryID::SMPTE170M:
      case PrimaryID::SMPTE240M:
        guess |= GUESS_BT601;
        break;
      case PrimaryID::BT2020:
        guess |= GUESS_BT2020;
        break;
      case PrimaryID::FILM:
      case PrimaryID::SMPTEST428_1:
      case PrimaryID::SMPTEST431_2:
      case PrimaryID::SMPTEST432_1:
      case PrimaryID::EBU_3213_E:
      case PrimaryID::INVALID:
      case PrimaryID::UNSPECIFIED:
        break;
    }

    switch (GetTransferID(vui_parameters->transfer_characteristics)) {
      case TransferID::BT709:
        guess |= GUESS_BT709;
        break;
      case TransferID::GAMMA22:
      case TransferID::GAMMA28:
      case TransferID::SMPTE170M:
      case TransferID::SMPTE240M:
        guess |= GUESS_BT601;
        break;
      case TransferID::BT2020_10:
      case TransferID::BT2020_12:
        guess |= GUESS_BT2020;
        break;
      case TransferID::LINEAR:
      case TransferID::LOG:
      case TransferID::LOG_SQRT:
      case TransferID::IEC61966_2_4:
      case TransferID::BT1361_ECG:
      case TransferID::IEC61966_2_1:
      case TransferID::SMPTEST2084:
      case TransferID::SMPTEST428_1:
      case TransferID::ARIB_STD_B67:
      case TransferID::INVALID:
      case TransferID::UNSPECIFIED:
        break;
    }

    switch (GetMatrixID(vui_parameters->matrix_coeffs)) {
      case MatrixID::BT709:
        guess |= GUESS_BT709;
        break;
      case MatrixID::BT470BG:
      case MatrixID::SMPTE170M:
      case MatrixID::SMPTE240M:
        guess |= GUESS_BT601;
        break;
      case MatrixID::BT2020_NCL:
      case MatrixID::BT2020_CL:
        guess |= GUESS_BT2020;
        break;
      case MatrixID::RGB:
      case MatrixID::FCC:
      case MatrixID::YCOCG:
      case MatrixID::YDZDX:
      case MatrixID::INVALID:
      case MatrixID::UNSPECIFIED:
        break;
    }
  }

  // Removes lowest bit until only a single bit remains.
  while (guess & (guess - 1)) {
    guess &= guess - 1;
  }
  if (!guess) {
    // A better default to BT601 which should die a slow death.
    guess = GUESS_BT709;
  }

  switch (guess) {
    case GUESS_BT601:
      return gfx::YUVColorSpace::BT601;
    case GUESS_BT709:
      return gfx::YUVColorSpace::BT709;
    default:
      MOZ_DIAGNOSTIC_ASSERT(guess == GUESS_BT2020);
      return gfx::YUVColorSpace::BT2020;
  }
}

bool H265SPS::IsFullColorRange() const {
  return vui_parameters ? vui_parameters->video_full_range_flag : false;
}

uint8_t H265SPS::ColorPrimaries() const {
  // Per H265 spec E.3.1, "When the colour_primaries syntax element is not
  // present, the value of colour_primaries is inferred to be equal to 2 (the
  // chromaticity is unspecified or is determined by the application).".
  if (!vui_parameters || !vui_parameters->colour_primaries) {
    return 2;
  }
  return vui_parameters->colour_primaries.value();
}

uint8_t H265SPS::TransferFunction() const {
  // Per H265 spec E.3.1, "When the transfer_characteristics syntax element is
  // not present, the value of transfer_characteristics is inferred to be equal
  // to 2 (the transfer characteristics are unspecified or are determined by the
  // application)."
  if (!vui_parameters || !vui_parameters->transfer_characteristics) {
    return 2;
  }
  return vui_parameters->transfer_characteristics.value();
}

/* static */
already_AddRefed<mozilla::MediaByteBuffer> H265::DecodeNALUnit(
    const Span<const uint8_t>& aNALU) {
  RefPtr<mozilla::MediaByteBuffer> rbsp = new mozilla::MediaByteBuffer;
  BufferReader reader(aNALU.Elements(), aNALU.Length());
  auto header = reader.ReadU16();
  if (header.isErr()) {
    return nullptr;
  }
  uint32_t lastbytes = 0xffff;
  while (reader.Remaining()) {
    auto res = reader.ReadU8();
    if (res.isErr()) {
      return nullptr;
    }
    uint8_t byte = res.unwrap();
    if ((lastbytes & 0xffff) == 0 && byte == 0x03) {
      // reset last two bytes, to detect the 0x000003 sequence again.
      lastbytes = 0xffff;
    } else {
      rbsp->AppendElement(byte);
    }
    lastbytes = (lastbytes << 8) | byte;
  }
  return rbsp.forget();
}

/* static */
already_AddRefed<mozilla::MediaByteBuffer> H265::ExtractHVCCExtraData(
    const mozilla::MediaRawData* aSample) {
  size_t sampleSize = aSample->Size();
  if (aSample->mCrypto.IsEncrypted()) {
    // The content is encrypted, we can only parse the non-encrypted data.
    MOZ_ASSERT(aSample->mCrypto.mPlainSizes.Length() > 0);
    if (aSample->mCrypto.mPlainSizes.Length() == 0 ||
        aSample->mCrypto.mPlainSizes[0] > sampleSize) {
      LOG("Invalid crypto content");
      return nullptr;
    }
    sampleSize = aSample->mCrypto.mPlainSizes[0];
  }

  auto hvcc = HVCCConfig::Parse(aSample);
  if (hvcc.isErr()) {
    LOG("Only support extracting extradata from HVCC");
    return nullptr;
  }
  const auto nalLenSize = hvcc.unwrap().NALUSize();
  BufferReader reader(aSample->Data(), sampleSize);

  nsTArray<Maybe<H265SPS>> spsRefTable;
  nsTArray<H265NALU> spsNALUs;
  // If we encounter SPS with the same id but different content, we will stop
  // attempting to detect duplicates.
  bool checkDuplicate = true;
  const H265SPS* firstSPS = nullptr;

  RefPtr<mozilla::MediaByteBuffer> extradata = new mozilla::MediaByteBuffer;
  while (reader.Remaining() > nalLenSize) {
    // ISO/IEC 14496-15, 4.2.3.2 Syntax. (NALUSample) Reading the size of NALU.
    uint32_t nalLen = 0;
    switch (nalLenSize) {
      case 1:
        Unused << reader.ReadU8().map(
            [&](uint8_t x) mutable { return nalLen = x; });
        break;
      case 2:
        Unused << reader.ReadU16().map(
            [&](uint16_t x) mutable { return nalLen = x; });
        break;
      case 3:
        Unused << reader.ReadU24().map(
            [&](uint32_t x) mutable { return nalLen = x; });
        break;
      default:
        MOZ_DIAGNOSTIC_ASSERT(nalLenSize == 4);
        Unused << reader.ReadU32().map(
            [&](uint32_t x) mutable { return nalLen = x; });
        break;
    }
    const uint8_t* p = reader.Read(nalLen);
    if (!p) {
      // The read failed, but we may already have some SPS data so break out of
      // reading and process what we have, if any.
      break;
    }
    const H265NALU nalu(p, nalLen);
    LOGV("Found NALU, type=%u", nalu.mNalUnitType);
    if (nalu.IsSPS()) {
      auto rv = H265::DecodeSPSFromSPSNALU(nalu);
      if (rv.isErr()) {
        // Invalid SPS, ignore.
        LOG("Ignore invalid SPS");
        continue;
      }
      const H265SPS sps = rv.unwrap();
      const uint8_t spsId = sps.sps_seq_parameter_set_id;  // 0~15
      if (spsId >= spsRefTable.Length()) {
        if (!spsRefTable.SetLength(spsId + 1, fallible)) {
          NS_WARNING("OOM while expanding spsRefTable!");
          return nullptr;
        }
      }
      if (checkDuplicate && spsRefTable[spsId] &&
          *(spsRefTable[spsId]) == sps) {
        // Duplicate ignore.
        continue;
      }
      if (spsRefTable[spsId]) {
        // We already have detected a SPS with this Id. Just to be safe we
        // disable SPS duplicate detection.
        checkDuplicate = false;
      } else {
        spsRefTable[spsId] = Some(sps);
        spsNALUs.AppendElement(nalu);
        if (!firstSPS) {
          firstSPS = spsRefTable[spsId].ptr();
        }
      }
    }
  }

  LOGV("Found %zu SPS NALU", spsNALUs.Length());
  if (!spsNALUs.IsEmpty()) {
    MOZ_ASSERT(firstSPS);
    BitWriter writer(extradata);

    // ISO/IEC 14496-15, HEVCDecoderConfigurationRecord. But we only append SPS.
    writer.WriteBits(1, 8);  // version
    const auto& profile = firstSPS->profile_tier_level;
    writer.WriteBits(profile.general_profile_space, 2);
    writer.WriteBits(profile.general_tier_flag, 1);
    writer.WriteBits(profile.general_profile_idc, 5);
    writer.WriteU32(profile.general_profile_compatibility_flags);

    // general_constraint_indicator_flags
    writer.WriteBit(profile.general_progressive_source_flag);
    writer.WriteBit(profile.general_interlaced_source_flag);
    writer.WriteBit(profile.general_non_packed_constraint_flag);
    writer.WriteBit(profile.general_frame_only_constraint_flag);
    writer.WriteBits(0, 44); /* ignored 44 bits */

    writer.WriteU8(profile.general_level_idc);
    writer.WriteBits(0, 4);   // reserved
    writer.WriteBits(0, 12);  // min_spatial_segmentation_idc
    writer.WriteBits(0, 6);   // reserved
    writer.WriteBits(0, 2);   // parallelismType
    writer.WriteBits(0, 6);   // reserved
    writer.WriteBits(firstSPS->chroma_format_idc, 2);
    writer.WriteBits(0, 5);  // reserved
    writer.WriteBits(firstSPS->bit_depth_luma_minus8, 3);
    writer.WriteBits(0, 5);  // reserved
    writer.WriteBits(firstSPS->bit_depth_chroma_minus8, 3);
    // avgFrameRate + constantFrameRate + numTemporalLayers + temporalIdNested
    writer.WriteBits(0, 22);
    writer.WriteBits(nalLenSize - 1, 2);  // lengthSizeMinusOne
    writer.WriteU8(1);                    // numOfArrays, only SPS
    for (auto j = 0; j < 1; j++) {
      writer.WriteBits(0, 2);                   // array_completeness + reserved
      writer.WriteBits(H265NALU::SPS_NUT, 6);   // NAL_unit_type
      writer.WriteBits(spsNALUs.Length(), 16);  // numNalus
      for (auto i = 0; i < spsNALUs.Length(); i++) {
        writer.WriteBits(spsNALUs[i].mNALU.Length(),
                         16);  // nalUnitLength
        MOZ_ASSERT(writer.BitCount() % 8 == 0);
        extradata->AppendElements(spsNALUs[i].mNALU.Elements(),
                                  spsNALUs[i].mNALU.Length());
        writer.AdvanceBytes(spsNALUs[i].mNALU.Length());
      }
    }
  }

  return extradata.forget();
}

class SPSIterator final {
 public:
  explicit SPSIterator(const HVCCConfig& aConfig) : mConfig(aConfig) {}

  SPSIterator& operator++() {
    size_t idx = 0;
    for (idx = mNextIdx; idx < mConfig.mNALUs.Length(); idx++) {
      if (mConfig.mNALUs[idx].IsSPS()) {
        mSPS = &mConfig.mNALUs[idx];
        break;
      }
    }
    mNextIdx = idx + 1;
    return *this;
  }

  explicit operator bool() const { return mNextIdx < mConfig.mNALUs.Length(); }

  const H265NALU* operator*() const { return mSPS ? mSPS : nullptr; }

 private:
  size_t mNextIdx = 0;
  const H265NALU* mSPS = nullptr;
  const HVCCConfig& mConfig;
};

/* static */
bool AreTwoSPSIdentical(const H265NALU& aLhs, const H265NALU& aRhs) {
  MOZ_ASSERT(aLhs.IsSPS() && aRhs.IsSPS());
  auto rv1 = H265::DecodeSPSFromSPSNALU(aLhs);
  auto rv2 = H265::DecodeSPSFromSPSNALU(aRhs);
  if (rv1.isErr() || rv2.isErr()) {
    return false;
  }
  return rv1.unwrap() == rv2.unwrap();
}

/* static */
bool H265::CompareExtraData(const mozilla::MediaByteBuffer* aExtraData1,
                            const mozilla::MediaByteBuffer* aExtraData2) {
  if (aExtraData1 == aExtraData2) {
    return true;
  }

  auto config1 = HVCCConfig::Parse(aExtraData1);
  auto config2 = HVCCConfig::Parse(aExtraData2);
  if (config1.isErr() || config2.isErr()) {
    return false;
  }

  uint8_t numSPS = config1.unwrap().NumSPS();
  if (numSPS == 0 || numSPS != config2.unwrap().NumSPS()) {
    return false;
  }

  // We only compare if the SPS are the same as the various HEVC decoders can
  // deal with in-band change of PPS.
  SPSIterator it1(config1.unwrap());
  SPSIterator it2(config2.unwrap());
  while (it1 && it2) {
    const H265NALU* nalu1 = *it1;
    const H265NALU* nalu2 = *it2;
    if (!nalu1 || !nalu2) {
      return false;
    }
    if (!AreTwoSPSIdentical(*nalu1, *nalu2)) {
      return false;
    }
    ++it1;
    ++it2;
  }
  return true;
}

#undef LOG
#undef LOGV

}  // namespace mozilla
