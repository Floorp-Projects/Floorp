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
#include "mozilla/ArrayUtils.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Span.h"

mozilla::LazyLogModule gH265("H265");

#define LOG(msg, ...) MOZ_LOG(gH265, LogLevel::Debug, (msg, ##__VA_ARGS__))
#define LOGV(msg, ...) MOZ_LOG(gH265, LogLevel::Verbose, (msg, ##__VA_ARGS__))

#define READ_IN_RANGE_OR_RETURN(dest, val, min, max)            \
  do {                                                          \
    if ((min) <= (val) && (val) <= (max)) {                     \
      (dest) = (val);                                           \
    } else {                                                    \
      LOG(#dest " is not in the range of [" #min "," #max "]"); \
      return mozilla::Err(NS_ERROR_FAILURE);                    \
    }                                                           \
  } while (0)

#define IN_RANGE_OR_RETURN(val, min, max)                      \
  do {                                                         \
    if ((val) < (min) || (max) < (val)) {                      \
      LOG(#val " is not in the range of [" #min "," #max "]"); \
      return mozilla::Err(NS_ERROR_FAILURE);                   \
    }                                                          \
  } while (0)

#define NON_ZERO_OR_RETURN(dest, val)        \
  do {                                       \
    if ((val) != 0) {                        \
      (dest) = (val);                        \
    } else {                                 \
      LOG(#dest " should be non-zero");      \
      return mozilla::Err(NS_ERROR_FAILURE); \
    }                                        \
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
  if (!aSample || aSample->Size() < 3) {
    LOG("No sample or incorrect sample size");
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  // TODO : check video mime type to ensure the sample is for HEVC
  return HVCCConfig::Parse(aSample->mExtraData);
}

/* static */
Result<HVCCConfig, nsresult> HVCCConfig::Parse(
    const mozilla::MediaByteBuffer* aExtraData) {
  // From configurationVersion to numOfArrays, total 184 bits (23 bytes)
  if (!aExtraData || aExtraData->Length() < 23) {
    LOG("No extra-data or incorrect extra-data size");
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  const auto& byteBuffer = *aExtraData;
  if (byteBuffer[0] != 1) {
    LOG("Version should always be 1");
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  HVCCConfig hvcc;

  BitReader reader(aExtraData);
  hvcc.mConfigurationVersion = reader.ReadBits(8);
  hvcc.mGeneralProfileSpace = reader.ReadBits(2);
  hvcc.mGeneralTierFlag = reader.ReadBit();
  hvcc.mGeneralProfileIdc = reader.ReadBits(5);
  hvcc.mGeneralProfileCompatibilityFlags = reader.ReadU32();

  uint32_t flagHigh = reader.ReadU32();
  uint16_t flagLow = reader.ReadBits(16);
  hvcc.mGeneralConstraintIndicatorFlags =
      ((uint64_t)(flagHigh) << 16) | (uint64_t)(flagLow);

  hvcc.mGeneralLevelIdc = reader.ReadBits(8);
  Unused << reader.ReadBits(4);  // reserved
  hvcc.mMinSpatialSegmentationIdc = reader.ReadBits(12);
  Unused << reader.ReadBits(6);  // reserved
  hvcc.mParallelismType = reader.ReadBits(2);
  Unused << reader.ReadBits(6);  // reserved
  hvcc.mChromaFormatIdc = reader.ReadBits(2);
  Unused << reader.ReadBits(5);  // reserved
  hvcc.mBitDepthLumaMinus8 = reader.ReadBits(3);
  Unused << reader.ReadBits(5);  // reserved
  hvcc.mBitDepthChromaMinus8 = reader.ReadBits(3);
  hvcc.mAvgFrameRate = reader.ReadBits(16);
  hvcc.mConstantFrameRate = reader.ReadBits(2);
  hvcc.mNumTemporalLayers = reader.ReadBits(3);
  hvcc.mTemporalIdNested = reader.ReadBit();
  hvcc.mLengthSizeMinusOne = reader.ReadBits(2);
  const uint8_t numOfArrays = reader.ReadBits(8);
  for (uint8_t idx = 0; idx < numOfArrays; idx++) {
    Unused << reader.ReadBits(2);  // array_completeness + reserved
    const uint8_t nalUnitType = reader.ReadBits(6);
    const uint16_t numNalus = reader.ReadBits(16);
    LOGV("nalu-type=%u, nalu-num=%u", nalUnitType, numNalus);
    for (uint16_t nIdx = 0; nIdx < numNalus; nIdx++) {
      const uint16_t nalUnitLength = reader.ReadBits(16);
      if (reader.BitsLeft() < nalUnitLength * 8) {
        return mozilla::Err(NS_ERROR_FAILURE);
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
      MOZ_ASSERT(nalu.mNalUnitType == nalUnitType);
      hvcc.mNALUs.AppendElement(nalu);
    }
  }
  hvcc.mByteBuffer = aExtraData;
  return hvcc;
}

bool HVCCConfig::HasSPS() const {
  if (mNALUs.IsEmpty()) {
    return false;
  }
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
  READ_IN_RANGE_OR_RETURN(sps.sps_video_parameter_set_id, reader.ReadBits(4), 0,
                          15);
  READ_IN_RANGE_OR_RETURN(sps.sps_max_sub_layers_minus1, reader.ReadBits(3), 0,
                          6);
  sps.sps_temporal_id_nesting_flag = reader.ReadBit();
  ParseProfileTierLevel(reader, true /* aProfilePresentFlag, true per spec*/,
                        sps.sps_max_sub_layers_minus1, sps.profile_tier_level);
  READ_IN_RANGE_OR_RETURN(sps.sps_seq_parameter_set_id, reader.ReadUE(), 0, 15);
  READ_IN_RANGE_OR_RETURN(sps.chroma_format_idc, reader.ReadUE(), 0, 3);
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
  sps.conformance_window_flag = reader.ReadBit();
  if (sps.conformance_window_flag) {
    sps.conf_win_left_offset = reader.ReadUE();
    sps.conf_win_right_offset = reader.ReadUE();
    sps.conf_win_top_offset = reader.ReadUE();
    sps.conf_win_bottom_offset = reader.ReadUE();
  }
  READ_IN_RANGE_OR_RETURN(sps.bit_depth_luma_minus8, reader.ReadUE(), 0, 8);
  READ_IN_RANGE_OR_RETURN(sps.bit_depth_chroma_minus8, reader.ReadUE(), 0, 8);
  READ_IN_RANGE_OR_RETURN(sps.log2_max_pic_order_cnt_lsb_minus4,
                          reader.ReadUE(), 0, 12);
  sps.sps_sub_layer_ordering_info_present_flag = reader.ReadBit();
  for (auto i = sps.sps_sub_layer_ordering_info_present_flag
                    ? 0
                    : sps.sps_max_sub_layers_minus1;
       i <= sps.sps_max_sub_layers_minus1; i++) {
    sps.sps_max_dec_pic_buffering_minus1[i] = reader.ReadUE();
    sps.sps_max_num_reorder_pics[i] = reader.ReadUE();
    sps.sps_max_latency_increase_plus1[i] = reader.ReadUE();
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
    READ_IN_RANGE_OR_RETURN(sps.pcm_sample_bit_depth_luma_minus1,
                            reader.ReadBits(3), 0, sps.BitDepthLuma());
    READ_IN_RANGE_OR_RETURN(sps.pcm_sample_bit_depth_chroma_minus1,
                            reader.ReadBits(3), 0, sps.BitDepthChroma());
    READ_IN_RANGE_OR_RETURN(sps.log2_min_pcm_luma_coding_block_size_minus3,
                            reader.ReadUE(), 0, 2);
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

  READ_IN_RANGE_OR_RETURN(sps.num_short_term_ref_pic_sets, reader.ReadUE(), 0,
                          kMaxShortTermRefPicSets);
  for (auto i = 0; i < sps.num_short_term_ref_pic_sets; i++) {
    if (auto rv = ParseStRefPicSet(reader, i, sps); rv.isErr()) {
      LOG("Failed to parse short-term reference picture set.");
      return Err(NS_ERROR_FAILURE);
    }
  }
  if (auto long_term_ref_pics_present_flag = reader.ReadBit()) {
    uint32_t num_long_term_ref_pics_sps;
    READ_IN_RANGE_OR_RETURN(num_long_term_ref_pics_sps, reader.ReadUE(), 0,
                            kMaxLongTermRefPicSets);
    for (auto i = 0; i < num_long_term_ref_pics_sps; i++) {
      Unused << reader.ReadBits(sps.log2_max_pic_order_cnt_lsb_minus4 +
                                4);  // lt_ref_pic_poc_lsb_sps[i]
      Unused << reader.ReadBit();    // used_by_curr_pic_lt_sps_flag
    }
  }
  sps.sps_temporal_mvp_enabled_flag = reader.ReadBit();
  sps.strong_intra_smoothing_enabled_flag = reader.ReadBit();
  if (auto vui_parameters_present_flag = reader.ReadBit()) {
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
void H265::ParseProfileTierLevel(BitReader& aReader, bool aProfilePresentFlag,
                                 uint8_t aMaxNumSubLayersMinus1,
                                 H265ProfileTierLevel& aProfile) {
  // H265 spec, 7.3.3 Profile, tier and level syntax
  if (aProfilePresentFlag) {
    aProfile.general_profile_space = aReader.ReadBits(2);
    aProfile.general_tier_flag = aReader.ReadBit();
    aProfile.general_profile_idc = aReader.ReadBits(5);
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
      READ_IN_RANGE_OR_RETURN(delta_idx_minus1, aReader.ReadUE(), 0,
                              aStRpsIdx - 1);
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
    for (int64_t j = refSet.num_positive_pics - 1; j >= 0; j--) {
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
      int64_t d_poc = refSet.deltaPocS0[j] + deltaRps;
      if (d_poc < 0 && use_delta_flag[j]) {
        curStRefPicSet.deltaPocS0[i] = d_poc;
        curStRefPicSet.usedByCurrPicS0[i++] = used_by_curr_pic_flag[j];
      }
    }
    curStRefPicSet.num_negative_pics = i;
    // Calculate fields (7-62)
    i = 0;
    for (int64_t j = refSet.num_negative_pics - 1; j >= 0; j--) {
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
    for (auto i = 0; i < curStRefPicSet.num_negative_pics; i++) {
      uint32_t delta_poc_s0_minus1;
      READ_IN_RANGE_OR_RETURN(delta_poc_s0_minus1, aReader.ReadUE(), 0, 0x7FFF);
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
      int delta_poc_s1_minus1;
      READ_IN_RANGE_OR_RETURN(delta_poc_s1_minus1, aReader.ReadUE(), 0, 0x7FFF);
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
  const uint32_t spsMaxDecPicBufferingMinus1 =
      aSPS.sps_max_dec_pic_buffering_minus1[aSPS.sps_max_sub_layers_minus1];
  IN_RANGE_OR_RETURN(curStRefPicSet.num_negative_pics, 0,
                     spsMaxDecPicBufferingMinus1);
  CheckedUint32 maxPositivePics{spsMaxDecPicBufferingMinus1};
  maxPositivePics -= curStRefPicSet.num_negative_pics;
  IN_RANGE_OR_RETURN(curStRefPicSet.num_positive_pics, 0,
                     maxPositivePics.value());
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
      READ_IN_RANGE_OR_RETURN(cpb_cnt_minus1, aReader.ReadUE(), 0, 31);
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

#undef LOG
#undef LOGV

}  // namespace mozilla
