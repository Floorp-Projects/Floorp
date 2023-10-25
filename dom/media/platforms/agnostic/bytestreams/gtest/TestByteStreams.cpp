/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "AnnexB.h"
#include "BufferReader.h"
#include "ByteWriter.h"
#include "H264.h"
#include "H265.h"
#include "mozilla/Types.h"

namespace mozilla {

// Create AVCC style extra data (the contents on an AVCC box). Note
// NALLengthSize will be 4 so AVCC samples need to set their data up
// accordingly.
static already_AddRefed<MediaByteBuffer> GetExtraData() {
  // Extra data with
  // - baseline profile(0x42 == 66).
  // - constraint flags 0 and 1 set(0xc0) -- normal for baseline profile.
  // - level 4.0 (0x28 == 40).
  // - 1280 * 720 resolution.
  return H264::CreateExtraData(0x42, 0xc0, 0x28, {1280, 720});
}

// Create an AVCC style sample with requested size in bytes. This sample is
// setup to contain a single NAL (in practice samples can contain many). The
// sample sets its NAL size to aSampleSize - 4 and stores that size in the first
// 4 bytes. Aside from the NAL size at the start, the data is uninitialized
// (beware)! aSampleSize is a uint32_t as samples larger than can be expressed
// by a uint32_t are not to spec.
static already_AddRefed<MediaRawData> GetAvccSample(uint32_t aSampleSize) {
  if (aSampleSize < 4) {
    // Stop tests asking for insane samples.
    EXPECT_FALSE(true) << "Samples should be requested with sane sizes";
  }
  nsTArray<uint8_t> sampleData;

  // Write the NAL size.
  ByteWriter<BigEndian> writer(sampleData);
  EXPECT_TRUE(writer.WriteU32(aSampleSize - 4));

  // Write the 'NAL'. Beware, this data is uninitialized.
  sampleData.AppendElements(static_cast<size_t>(aSampleSize) - 4);
  RefPtr<MediaRawData> rawData =
      new MediaRawData{sampleData.Elements(), sampleData.Length()};
  EXPECT_NE(rawData->Data(), nullptr);

  // Set extra data.
  rawData->mExtraData = GetExtraData();
  return rawData.forget();
}

static const uint8_t sHvccBytesBuffer[] = {
    1 /* version */,
    1 /* general_profile_space/general_tier_flag/general_profile_idc */,
    0x60 /* general_profile_compatibility_flags 1/4 */,
    0 /* general_profile_compatibility_flags 2/4 */,
    0 /* general_profile_compatibility_flags 3/4 */,
    0 /* general_profile_compatibility_flags 4/4 */,
    0x90 /* general_constraint_indicator_flags 1/6 */,
    0 /* general_constraint_indicator_flags 2/6 */,
    0 /* general_constraint_indicator_flags 3/6 */,
    0 /* general_constraint_indicator_flags 4/6 */,
    0 /* general_constraint_indicator_flags 5/6 */,
    0 /* general_constraint_indicator_flags 6/6 */,
    0x5A /* general_level_idc */,
    0 /* min_spatial_segmentation_idc 1/2 */,
    0 /* min_spatial_segmentation_idc 2/2 */,
    0 /* parallelismType */,
    1 /* chroma_format_idc */,
    0 /* bit_depth_luma_minus8 */,
    0 /* bit_depth_chroma_minus8 */,
    0 /* avgFrameRate 1/2 */,
    0 /* avgFrameRate 2/2 */,
    0x0F /* constantFrameRate/numTemporalLayers/temporalIdNested/lengthSizeMinusOne
          */
    ,
    2 /* numOfArrays */,
    /* SPS Array */
    0x21 /* NAL_unit_type (SPS) */,
    0 /* numNalus 1/2 */,
    1 /* numNalus 2/2 */,

    /* SPS */
    0 /* nalUnitLength 1/2 */,
    8 /* nalUnitLength 2/2 (header + rsbp) */,
    0x42 /* NALU header 1/2 */,
    0 /* NALU header 2/2 */,
    0 /* rbsp 1/6 */,
    0 /* rbsp 2/6 */,
    0 /* rbsp 3/6 */,
    0 /* rbsp 4/6 */,
    0 /* rbsp 5/6 */,
    0 /* rbsp 6/6 */,

    /* PPS Array */
    0x22 /* NAL_unit_type (PPS) */,
    0 /* numNalus 1/2 */,
    1 /* numNalus 2/2 */,

    /* PPS */
    0 /* nalUnitLength 1/2 */,
    3 /* nalUnitLength 2/2 (header + rsbp) */,
    0x44 /* NALU header 1/2 */,
    0 /* NALU header 2/2 */,
    0 /* rbsp */,
};

// Create a HVCC sample, which contain fake data, in given size.
static already_AddRefed<MediaRawData> GetHVCCSample(uint32_t aSampleSize) {
  if (aSampleSize < 4) {
    // Stop tests asking for insane samples.
    EXPECT_FALSE(true) << "Samples should be requested with sane sizes";
  }
  auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
  extradata->AppendElements(sHvccBytesBuffer, ArrayLength(sHvccBytesBuffer));

  // Write the NAL size.
  nsTArray<uint8_t> sampleData;
  ByteWriter<BigEndian> writer(sampleData);
  EXPECT_TRUE(writer.WriteU32(aSampleSize - 4));  // Assume it's a 4 bytes NALU

  // Fill fake empty data
  for (uint32_t idx = 0; idx < aSampleSize - 4; idx++) {
    sampleData.AppendElement(0);
  }
  RefPtr<MediaRawData> rawData =
      new MediaRawData{sampleData.Elements(), sampleData.Length()};
  EXPECT_NE(rawData->Data(), nullptr);
  EXPECT_EQ(rawData->Size(), aSampleSize);
  rawData->mExtraData = extradata;
  return rawData.forget();
}

// Create a HVCC sample by using given data in given size.
static already_AddRefed<MediaRawData> GetHVCCSample(
    const uint8_t* aData, const uint32_t aDataLength) {
  if (aDataLength < 4) {
    // Stop tests asking for insane samples.
    EXPECT_FALSE(true) << "Samples should be requested with sane sizes";
  }
  auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
  extradata->AppendElements(sHvccBytesBuffer, ArrayLength(sHvccBytesBuffer));

  // Write the NAL size.
  nsTArray<uint8_t> sampleData;
  ByteWriter<BigEndian> writer(sampleData);
  EXPECT_TRUE(writer.WriteU32(aDataLength));  // Assume it's a 4 bytes NALU
  sampleData.AppendElements(aData, aDataLength);

  RefPtr<MediaRawData> rawData =
      new MediaRawData{sampleData.Elements(), sampleData.Length()};
  EXPECT_NE(rawData->Data(), nullptr);
  EXPECT_EQ(rawData->Size(), aDataLength + 4);
  rawData->mExtraData = extradata;
  return rawData.forget();
}

// Test that conversion from AVCC to AnnexB works as expected.
TEST(AnnexB, AVCCToAnnexBConversion)
{
  RefPtr<MediaRawData> rawData{GetAvccSample(128)};

  {
    // Test conversion of data when not adding SPS works as expected.
    RefPtr<MediaRawData> rawDataClone = rawData->Clone();
    Result<Ok, nsresult> result =
        AnnexB::ConvertAVCCSampleToAnnexB(rawDataClone, /* aAddSps */ false);
    EXPECT_TRUE(result.isOk()) << "Conversion should succeed";
    EXPECT_EQ(rawDataClone->Size(), rawData->Size())
        << "AnnexB sample should be the same size as the AVCC sample -- the 4 "
           "byte NAL length data (AVCC) is replaced with 4 bytes of NAL "
           "separator (AnnexB)";
    EXPECT_TRUE(AnnexB::IsAnnexB(rawDataClone))
        << "The sample should be AnnexB following conversion";
  }

  {
    // Test that the SPS data is not added if the frame is not a keyframe.
    RefPtr<MediaRawData> rawDataClone = rawData->Clone();
    rawDataClone->mKeyframe =
        false;  // false is the default, but let's be sure.
    Result<Ok, nsresult> result =
        AnnexB::ConvertAVCCSampleToAnnexB(rawDataClone, /* aAddSps */ true);
    EXPECT_TRUE(result.isOk()) << "Conversion should succeed";
    EXPECT_EQ(rawDataClone->Size(), rawData->Size())
        << "AnnexB sample should be the same size as the AVCC sample -- the 4 "
           "byte NAL length data (AVCC) is replaced with 4 bytes of NAL "
           "separator (AnnexB) and SPS data is not added as the frame is not a "
           "keyframe";
    EXPECT_TRUE(AnnexB::IsAnnexB(rawDataClone))
        << "The sample should be AnnexB following conversion";
  }

  {
    // Test that the SPS data is added to keyframes.
    RefPtr<MediaRawData> rawDataClone = rawData->Clone();
    rawDataClone->mKeyframe = true;
    Result<Ok, nsresult> result =
        AnnexB::ConvertAVCCSampleToAnnexB(rawDataClone, /* aAddSps */ true);
    EXPECT_TRUE(result.isOk()) << "Conversion should succeed";
    EXPECT_GT(rawDataClone->Size(), rawData->Size())
        << "AnnexB sample should be larger than the AVCC sample because we've "
           "added SPS data";
    EXPECT_TRUE(AnnexB::IsAnnexB(rawDataClone))
        << "The sample should be AnnexB following conversion";
    // We could verify the SPS and PPS data we add, but we don't have great
    // tooling to do so. Consider doing so in future.
  }

  {
    // Test conversion involving subsample encryption doesn't overflow vlaues.
    const uint32_t sampleSize = UINT16_MAX * 2;
    RefPtr<MediaRawData> rawCryptoData{GetAvccSample(sampleSize)};
    // Need to be a keyframe to test prepending SPS + PPS to sample.
    rawCryptoData->mKeyframe = true;
    UniquePtr<MediaRawDataWriter> rawDataWriter = rawCryptoData->CreateWriter();

    rawDataWriter->mCrypto.mCryptoScheme = CryptoScheme::Cenc;

    // We want to check that the clear size doesn't overflow during conversion.
    // This size originates in a uint16_t, but since it can grow during AnnexB
    // we cover it here.
    const uint16_t clearSize = UINT16_MAX - 10;
    // Set a clear size very close to uint16_t max value.
    rawDataWriter->mCrypto.mPlainSizes.AppendElement(clearSize);
    rawDataWriter->mCrypto.mEncryptedSizes.AppendElement(sampleSize -
                                                         clearSize);

    RefPtr<MediaRawData> rawCryptoDataClone = rawCryptoData->Clone();
    Result<Ok, nsresult> result = AnnexB::ConvertAVCCSampleToAnnexB(
        rawCryptoDataClone, /* aAddSps */ true);
    EXPECT_TRUE(result.isOk()) << "Conversion should succeed";
    EXPECT_GT(rawCryptoDataClone->Size(), rawCryptoData->Size())
        << "AnnexB sample should be larger than the AVCC sample because we've "
           "added SPS data";
    EXPECT_GT(rawCryptoDataClone->mCrypto.mPlainSizes[0],
              rawCryptoData->mCrypto.mPlainSizes[0])
        << "Conversion should have increased clear data sizes without overflow";
    EXPECT_EQ(rawCryptoDataClone->mCrypto.mEncryptedSizes[0],
              rawCryptoData->mCrypto.mEncryptedSizes[0])
        << "Conversion should not affect encrypted sizes";
    EXPECT_TRUE(AnnexB::IsAnnexB(rawCryptoDataClone))
        << "The sample should be AnnexB following conversion";
  }
}

TEST(AnnexB, HVCCToAnnexBConversion)
{
  RefPtr<MediaRawData> rawData{GetHVCCSample(128)};
  {
    // Test conversion of data when not adding SPS works as expected.
    RefPtr<MediaRawData> rawDataClone = rawData->Clone();
    Result<Ok, nsresult> result =
        AnnexB::ConvertHVCCSampleToAnnexB(rawDataClone, /* aAddSps */ false);
    EXPECT_TRUE(result.isOk()) << "Conversion should succeed";
    EXPECT_EQ(rawDataClone->Size(), rawData->Size())
        << "AnnexB sample should be the same size as the HVCC sample -- the 4 "
           "byte NAL length data (HVCC) is replaced with 4 bytes of NAL "
           "separator (AnnexB)";
    EXPECT_TRUE(AnnexB::IsAnnexB(rawDataClone))
        << "The sample should be AnnexB following conversion";
  }
  {
    // Test that the SPS data is not added if the frame is not a keyframe.
    RefPtr<MediaRawData> rawDataClone = rawData->Clone();
    rawDataClone->mKeyframe =
        false;  // false is the default, but let's be sure.
    Result<Ok, nsresult> result =
        AnnexB::ConvertHVCCSampleToAnnexB(rawDataClone, /* aAddSps */ true);
    EXPECT_TRUE(result.isOk()) << "Conversion should succeed";
    EXPECT_EQ(rawDataClone->Size(), rawData->Size())
        << "AnnexB sample should be the same size as the HVCC sample -- the 4 "
           "byte NAL length data (HVCC) is replaced with 4 bytes of NAL "
           "separator (AnnexB) and SPS data is not added as the frame is not a "
           "keyframe";
    EXPECT_TRUE(AnnexB::IsAnnexB(rawDataClone))
        << "The sample should be AnnexB following conversion";
  }
  {
    // Test that the SPS data is added to keyframes.
    RefPtr<MediaRawData> rawDataClone = rawData->Clone();
    rawDataClone->mKeyframe = true;
    Result<Ok, nsresult> result =
        AnnexB::ConvertHVCCSampleToAnnexB(rawDataClone, /* aAddSps */ true);
    EXPECT_TRUE(result.isOk()) << "Conversion should succeed";
    EXPECT_GT(rawDataClone->Size(), rawData->Size())
        << "AnnexB sample should be larger than the HVCC sample because we've "
           "added SPS data";
    EXPECT_TRUE(AnnexB::IsAnnexB(rawDataClone))
        << "The sample should be AnnexB following conversion";
    // We could verify the SPS and PPS data we add, but we don't have great
    // tooling to do so. Consider doing so in future.
  }
  {
    // Test conversion involving subsample encryption doesn't overflow values.
    const uint32_t sampleSize = UINT16_MAX * 2;
    RefPtr<MediaRawData> rawCryptoData{GetHVCCSample(sampleSize)};
    // Need to be a keyframe to test prepending SPS + PPS to sample.
    rawCryptoData->mKeyframe = true;
    UniquePtr<MediaRawDataWriter> rawDataWriter = rawCryptoData->CreateWriter();

    rawDataWriter->mCrypto.mCryptoScheme = CryptoScheme::Cenc;

    // We want to check that the clear size doesn't overflow during conversion.
    // This size originates in a uint16_t, but since it can grow during AnnexB
    // we cover it here.
    const uint16_t clearSize = UINT16_MAX - 10;
    // Set a clear size very close to uint16_t max value.
    rawDataWriter->mCrypto.mPlainSizes.AppendElement(clearSize);
    rawDataWriter->mCrypto.mEncryptedSizes.AppendElement(sampleSize -
                                                         clearSize);

    RefPtr<MediaRawData> rawCryptoDataClone = rawCryptoData->Clone();
    Result<Ok, nsresult> result = AnnexB::ConvertHVCCSampleToAnnexB(
        rawCryptoDataClone, /* aAddSps */ true);
    EXPECT_TRUE(result.isOk()) << "Conversion should succeed";
    EXPECT_GT(rawCryptoDataClone->Size(), rawCryptoData->Size())
        << "AnnexB sample should be larger than the HVCC sample because we've "
           "added SPS data";
    EXPECT_GT(rawCryptoDataClone->mCrypto.mPlainSizes[0],
              rawCryptoData->mCrypto.mPlainSizes[0])
        << "Conversion should have increased clear data sizes without overflow";
    EXPECT_EQ(rawCryptoDataClone->mCrypto.mEncryptedSizes[0],
              rawCryptoData->mCrypto.mEncryptedSizes[0])
        << "Conversion should not affect encrypted sizes";
    EXPECT_TRUE(AnnexB::IsAnnexB(rawCryptoDataClone))
        << "The sample should be AnnexB following conversion";
  }
}

TEST(H264, AVCCParsingSuccess)
{
  auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
  uint8_t avccBytesBuffer[] = {
      1 /* version */,
      0x64 /* profile (High) */,
      0 /* profile compat (0) */,
      40 /* level (40) */,
      0xfc | 3 /* nal size - 1 */,
      0xe0 /* num SPS (0) */,
      0 /* num PPS (0) */
  };
  extradata->AppendElements(avccBytesBuffer, ArrayLength(avccBytesBuffer));
  auto rv = AVCCConfig::Parse(extradata);
  EXPECT_TRUE(rv.isOk());
  const auto avcc = rv.unwrap();
  EXPECT_EQ(avcc.mConfigurationVersion, 1);
  EXPECT_EQ(avcc.mAVCProfileIndication, 0x64);
  EXPECT_EQ(avcc.mProfileCompatibility, 0);
  EXPECT_EQ(avcc.mAVCLevelIndication, 40);
  EXPECT_EQ(avcc.NALUSize(), 4);
  EXPECT_EQ(avcc.mNumSPS, 0);
}

TEST(H264, AVCCParsingFailure)
{
  {
    // Incorrect version
    auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
    uint8_t avccBytesBuffer[] = {
        2 /* version */,
        0x64 /* profile (High) */,
        0 /* profile compat (0) */,
        40 /* level (40) */,
        0xfc | 3 /* nal size - 1 */,
        0xe0 /* num SPS (0) */,
        0 /* num PPS (0) */
    };
    extradata->AppendElements(avccBytesBuffer, ArrayLength(avccBytesBuffer));
    auto avcc = AVCCConfig::Parse(extradata);
    EXPECT_TRUE(avcc.isErr());
  }
  {
    // Insuffient data (lacking of PPS)
    auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
    uint8_t avccBytesBuffer[] = {
        1 /* version */,
        0x64 /* profile (High) */,
        0 /* profile compat (0) */,
        40 /* level (40) */,
        0xfc | 3 /* nal size - 1 */,
        0xe0 /* num SPS (0) */,
    };
    extradata->AppendElements(avccBytesBuffer, ArrayLength(avccBytesBuffer));
    auto avcc = AVCCConfig::Parse(extradata);
    EXPECT_TRUE(avcc.isErr());
  }
}

TEST(H265, HVCCParsingSuccess)
{
  {
    auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
    uint8_t hvccBytesBuffer[] = {
        1 /* version */,
        1 /* general_profile_space/general_tier_flag/general_profile_idc */,
        0x60 /* general_profile_compatibility_flags 1/4 */,
        0 /* general_profile_compatibility_flags 2/4 */,
        0 /* general_profile_compatibility_flags 3/4 */,
        0 /* general_profile_compatibility_flags 4/4 */,
        0x90 /* general_constraint_indicator_flags 1/6 */,
        0 /* general_constraint_indicator_flags 2/6 */,
        0 /* general_constraint_indicator_flags 3/6 */,
        0 /* general_constraint_indicator_flags 4/6 */,
        0 /* general_constraint_indicator_flags 5/6 */,
        0 /* general_constraint_indicator_flags 6/6 */,
        0x5A /* general_level_idc */,
        0 /* min_spatial_segmentation_idc 1/2 */,
        0 /* min_spatial_segmentation_idc 2/2 */,
        0 /* parallelismType */,
        1 /* chroma_format_idc */,
        0 /* bit_depth_luma_minus8 */,
        0 /* bit_depth_chroma_minus8 */,
        0 /* avgFrameRate 1/2 */,
        0 /* avgFrameRate 2/2 */,
        0x0F /* constantFrameRate/numTemporalLayers/temporalIdNested/lengthSizeMinusOne
              */
        ,
        0 /* numOfArrays */,
    };
    extradata->AppendElements(hvccBytesBuffer, ArrayLength(hvccBytesBuffer));
    auto rv = HVCCConfig::Parse(extradata);
    EXPECT_TRUE(rv.isOk());
    auto hvcc = rv.unwrap();
    EXPECT_EQ(hvcc.configurationVersion, 1);
    EXPECT_EQ(hvcc.general_profile_space, 0);
    EXPECT_EQ(hvcc.general_tier_flag, false);
    EXPECT_EQ(hvcc.general_profile_idc, 1);
    EXPECT_EQ(hvcc.general_profile_compatibility_flags, (uint32_t)0x60000000);
    EXPECT_EQ(hvcc.general_constraint_indicator_flags,
              (uint64_t)0x900000000000);
    EXPECT_EQ(hvcc.general_level_idc, 0x5A);
    EXPECT_EQ(hvcc.min_spatial_segmentation_idc, 0);
    EXPECT_EQ(hvcc.parallelismType, 0);
    EXPECT_EQ(hvcc.chroma_format_idc, 1);
    EXPECT_EQ(hvcc.bit_depth_luma_minus8, 0);
    EXPECT_EQ(hvcc.bit_depth_chroma_minus8, 0);
    EXPECT_EQ(hvcc.avgFrameRate, 0);
    EXPECT_EQ(hvcc.constantFrameRate, 0);
    EXPECT_EQ(hvcc.numTemporalLayers, 1);
    EXPECT_EQ(hvcc.temporalIdNested, true);
    EXPECT_EQ(hvcc.NALUSize(), 4);
    EXPECT_EQ(hvcc.mNALUs.Length(), uint32_t(0));
  }
  {
    // Multple NALUs
    auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
    uint8_t hvccBytesBuffer[] = {
        1 /* version */,
        1 /* general_profile_space/general_tier_flag/general_profile_idc */,
        0x60 /* general_profile_compatibility_flags 1/4 */,
        0 /* general_profile_compatibility_flags 2/4 */,
        0 /* general_profile_compatibility_flags 3/4 */,
        0 /* general_profile_compatibility_flags 4/4 */,
        0x90 /* general_constraint_indicator_flags 1/6 */,
        0 /* general_constraint_indicator_flags 2/6 */,
        0 /* general_constraint_indicator_flags 3/6 */,
        0 /* general_constraint_indicator_flags 4/6 */,
        0 /* general_constraint_indicator_flags 5/6 */,
        0 /* general_constraint_indicator_flags 6/6 */,
        0x5A /* general_level_idc */,
        0 /* min_spatial_segmentation_idc 1/2 */,
        0 /* min_spatial_segmentation_idc 2/2 */,
        0 /* parallelismType */,
        1 /* chroma_format_idc */,
        0 /* bit_depth_luma_minus8 */,
        0 /* bit_depth_chroma_minus8 */,
        0 /* avgFrameRate 1/2 */,
        0 /* avgFrameRate 2/2 */,
        0x0F /* constantFrameRate/numTemporalLayers/temporalIdNested/lengthSizeMinusOne
              */
        ,
        2 /* numOfArrays */,
        /* SPS Array */
        0x21 /* NAL_unit_type (SPS) */,
        0 /* numNalus 1/2 */,
        1 /* numNalus 2/2 */,

        /* SPS */
        0 /* nalUnitLength 1/2 */,
        8 /* nalUnitLength 2/2 (header + rsbp) */,
        0x42 /* NALU header 1/2 */,
        0 /* NALU header 2/2 */,
        0 /* rbsp 1/6 */,
        0 /* rbsp 2/6 */,
        0 /* rbsp 3/6 */,
        0 /* rbsp 4/6 */,
        0 /* rbsp 5/6 */,
        0 /* rbsp 6/6 */,

        /* PPS Array */
        0x22 /* NAL_unit_type (PPS) */,
        0 /* numNalus 1/2 */,
        2 /* numNalus 2/2 */,

        /* PPS 1 */
        0 /* nalUnitLength 1/2 */,
        3 /* nalUnitLength 2/2 (header + rsbp) */,
        0x44 /* NALU header 1/2 */,
        0 /* NALU header 2/2 */,
        0 /* rbsp */,

        /* PPS 2 */
        0 /* nalUnitLength 1/2 */,
        3 /* nalUnitLength 2/2 (header + rsbp) */,
        0x44 /* NALU header 1/2 */,
        0 /* NALU header 2/2 */,
        0 /* rbsp */,
    };
    extradata->AppendElements(hvccBytesBuffer, ArrayLength(hvccBytesBuffer));
    auto rv = HVCCConfig::Parse(extradata);
    EXPECT_TRUE(rv.isOk());
    auto hvcc = rv.unwrap();
    // Check NALU, it should contain 1 SPS and 2 PPS.
    EXPECT_EQ(hvcc.mNALUs.Length(), uint32_t(3));
    EXPECT_EQ(hvcc.mNALUs[0].mNalUnitType, H265NALU::NAL_TYPES::SPS_NUT);
    EXPECT_EQ(hvcc.mNALUs[0].mNuhLayerId, 0);
    EXPECT_EQ(hvcc.mNALUs[0].mNuhTemporalIdPlus1, 0);
    EXPECT_EQ(hvcc.mNALUs[0].IsSPS(), true);
    EXPECT_EQ(hvcc.mNALUs[0].mNALU.Length(), 8u);

    EXPECT_EQ(hvcc.mNALUs[1].mNalUnitType, H265NALU::NAL_TYPES::PPS_NUT);
    EXPECT_EQ(hvcc.mNALUs[1].mNuhLayerId, 0);
    EXPECT_EQ(hvcc.mNALUs[1].mNuhTemporalIdPlus1, 0);
    EXPECT_EQ(hvcc.mNALUs[1].IsSPS(), false);
    EXPECT_EQ(hvcc.mNALUs[1].mNALU.Length(), 3u);

    EXPECT_EQ(hvcc.mNALUs[2].mNalUnitType, H265NALU::NAL_TYPES::PPS_NUT);
    EXPECT_EQ(hvcc.mNALUs[2].mNuhLayerId, 0);
    EXPECT_EQ(hvcc.mNALUs[2].mNuhTemporalIdPlus1, 0);
    EXPECT_EQ(hvcc.mNALUs[2].IsSPS(), false);
    EXPECT_EQ(hvcc.mNALUs[2].mNALU.Length(), 3u);
  }
}

TEST(H265, HVCCParsingFailure)
{
  {
    // Incorrect version
    auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
    uint8_t hvccBytesBuffer[] = {
        2 /* version */,
        1 /* general_profile_space/general_tier_flag/general_profile_idc */,
        0x60 /* general_profile_compatibility_flags 1/4 */,
        0 /* general_profile_compatibility_flags 2/4 */,
        0 /* general_profile_compatibility_flags 3/4 */,
        0 /* general_profile_compatibility_flags 4/4 */,
        0x90 /* general_constraint_indicator_flags 1/6 */,
        0 /* general_constraint_indicator_flags 2/6 */,
        0 /* general_constraint_indicator_flags 3/6 */,
        0 /* general_constraint_indicator_flags 4/6 */,
        0 /* general_constraint_indicator_flags 5/6 */,
        0 /* general_constraint_indicator_flags 6/6 */,
        0x5A /* general_level_idc */,
        0 /* min_spatial_segmentation_idc 1/2 */,
        0 /* min_spatial_segmentation_idc 2/2 */,
        0 /* parallelismType */,
        1 /* chroma_format_idc */,
        0 /* bit_depth_luma_minus8 */,
        0 /* bit_depth_chroma_minus8 */,
        0 /* avgFrameRate 1/2 */,
        0 /* avgFrameRate 2/2 */,
        0x0F /* constantFrameRate/numTemporalLayers/temporalIdNested/lengthSizeMinusOne
              */
        ,
        0 /* numOfArrays */,
    };
    extradata->AppendElements(hvccBytesBuffer, ArrayLength(hvccBytesBuffer));
    auto avcc = HVCCConfig::Parse(extradata);
    EXPECT_TRUE(avcc.isErr());
  }
  {
    // Insuffient data
    auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
    uint8_t hvccBytesBuffer[] = {
        1 /* version */,
        1 /* general_profile_space/general_tier_flag/general_profile_idc */,
        0x60 /* general_profile_compatibility_flags 1/4 */,
        0 /* general_profile_compatibility_flags 2/4 */,
        0 /* general_profile_compatibility_flags 3/4 */,
        0 /* general_profile_compatibility_flags 4/4 */,
        0x90 /* general_constraint_indicator_flags 1/6 */,
        0 /* general_constraint_indicator_flags 2/6 */,
        0 /* general_constraint_indicator_flags 3/6 */,
        0 /* general_constraint_indicator_flags 4/6 */,
        0 /* general_constraint_indicator_flags 5/6 */,
        0 /* general_constraint_indicator_flags 6/6 */,
        0x5A /* general_level_idc */
    };
    extradata->AppendElements(hvccBytesBuffer, ArrayLength(hvccBytesBuffer));
    auto avcc = HVCCConfig::Parse(extradata);
    EXPECT_TRUE(avcc.isErr());
  }
}

TEST(H265, HVCCToAnnexB)
{
  auto extradata = MakeRefPtr<mozilla::MediaByteBuffer>();
  uint8_t hvccBytesBuffer[] = {
      1 /* version */,
      1 /* general_profile_space/general_tier_flag/general_profile_idc */,
      0x60 /* general_profile_compatibility_flags 1/4 */,
      0 /* general_profile_compatibility_flags 2/4 */,
      0 /* general_profile_compatibility_flags 3/4 */,
      0 /* general_profile_compatibility_flags 4/4 */,
      0x90 /* general_constraint_indicator_flags 1/6 */,
      0 /* general_constraint_indicator_flags 2/6 */,
      0 /* general_constraint_indicator_flags 3/6 */,
      0 /* general_constraint_indicator_flags 4/6 */,
      0 /* general_constraint_indicator_flags 5/6 */,
      0 /* general_constraint_indicator_flags 6/6 */,
      0x5A /* general_level_idc */,
      0 /* min_spatial_segmentation_idc 1/2 */,
      0 /* min_spatial_segmentation_idc 2/2 */,
      0 /* parallelismType */,
      1 /* chroma_format_idc */,
      0 /* bit_depth_luma_minus8 */,
      0 /* bit_depth_chroma_minus8 */,
      0 /* avgFrameRate 1/2 */,
      0 /* avgFrameRate 2/2 */,
      0x0F /* constantFrameRate/numTemporalLayers/temporalIdNested/lengthSizeMinusOne
            */
      ,
      2 /* numOfArrays */,
      /* SPS Array */
      0x21 /* NAL_unit_type (SPS) */,
      0 /* numNalus 1/2 */,
      1 /* numNalus 2/2 */,

      /* SPS */
      0 /* nalUnitLength 1/2 */,
      3 /* nalUnitLength 2/2 (header + rsbp) */,
      0x42 /* NALU header 1/2 */,
      0 /* NALU header 2/2 */,
      0 /* rbsp */,

      /* PPS Array */
      0x22 /* NAL_unit_type (PPS) */,
      0 /* numNalus 1/2 */,
      1 /* numNalus 2/2 */,

      /* PPS */
      0 /* nalUnitLength 1/2 */,
      3 /* nalUnitLength 2/2 (header + rsbp) */,
      0x44 /* NALU header 1/2 */,
      0 /* NALU header 2/2 */,
      0 /* rbsp */,
  };
  extradata->AppendElements(hvccBytesBuffer, ArrayLength(hvccBytesBuffer));

  // We convert hvcc extra-data to annexb format, then parse each nalu to see if
  // they are still correct or not.
  const size_t naluBytesSize = 3;  // NAL size is 3, see nalUnitLength above
  const size_t delimiterBytesSize = 4;  // 0x00000001
  const size_t naluPlusDelimiterBytesSize = naluBytesSize + delimiterBytesSize;
  RefPtr<mozilla::MediaByteBuffer> annexBExtraData =
      AnnexB::ConvertHVCCExtraDataToAnnexB(extradata);
  // 2 NALU, sps and pps
  EXPECT_EQ(annexBExtraData->Length(), naluPlusDelimiterBytesSize * 2);

  H265NALU sps(
      static_cast<uint8_t*>(annexBExtraData->Elements() + delimiterBytesSize),
      naluBytesSize);
  EXPECT_EQ(sps.mNalUnitType, H265NALU::NAL_TYPES::SPS_NUT);
  EXPECT_EQ(sps.mNuhLayerId, 0);
  EXPECT_EQ(sps.mNuhTemporalIdPlus1, 0);
  EXPECT_EQ(sps.IsSPS(), true);
  EXPECT_EQ(sps.mNALU.Length(), 3u);

  H265NALU pps(
      static_cast<uint8_t*>(annexBExtraData->Elements() +
                            naluPlusDelimiterBytesSize + delimiterBytesSize),
      naluBytesSize);
  EXPECT_EQ(pps.mNalUnitType, H265NALU::NAL_TYPES::PPS_NUT);
  EXPECT_EQ(pps.mNuhLayerId, 0);
  EXPECT_EQ(pps.mNuhTemporalIdPlus1, 0);
  EXPECT_EQ(pps.IsSPS(), false);
  EXPECT_EQ(pps.mNALU.Length(), 3u);
}

TEST(H265, AnnexBToHVCC)
{
  RefPtr<MediaRawData> rawData{GetHVCCSample(128)};
  RefPtr<MediaRawData> rawDataClone = rawData->Clone();
  Result<Ok, nsresult> result =
      AnnexB::ConvertHVCCSampleToAnnexB(rawDataClone, /* aAddSps */ false);
  EXPECT_TRUE(result.isOk()) << "HVCC to AnnexB Conversion should succeed";
  EXPECT_TRUE(AnnexB::IsAnnexB(rawDataClone))
      << "The sample should be AnnexB following conversion";

  auto rv = AnnexB::ConvertSampleToHVCC(rawDataClone);
  EXPECT_TRUE(rv.isOk()) << "AnnexB to HVCC Conversion should succeed";
  EXPECT_TRUE(AnnexB::IsHVCC(rawDataClone))
      << "The sample should be HVCC following conversion";
}

// This is SPS from 'hevc_white_frame.mp4'
static const uint8_t sSps[] = {
    0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x5d, 0xa0, 0x02, 0x00, 0x80,
    0x30, 0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0xc0, 0x5a, 0x02, 0x00,
    0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x3c, 0x10};

TEST(H265, ExtractHVCCExtraData)
{
  RefPtr<MediaRawData> rawData{GetHVCCSample(sSps, ArrayLength(sSps))};
  RefPtr<MediaByteBuffer> extradata = H265::ExtractHVCCExtraData(rawData);
  EXPECT_TRUE(extradata);
  auto rv = HVCCConfig::Parse(extradata);
  EXPECT_TRUE(rv.isOk());
  auto hvcc = rv.unwrap();
  EXPECT_EQ(hvcc.mNALUs.Length(), 1u);
  EXPECT_EQ(hvcc.mNALUs[0].mNalUnitType, H265NALU::NAL_TYPES::SPS_NUT);
  EXPECT_EQ(hvcc.mNALUs[0].mNuhLayerId, 0u);
  EXPECT_EQ(hvcc.mNALUs[0].mNuhTemporalIdPlus1, 1);
  EXPECT_EQ(hvcc.mNALUs[0].IsSPS(), true);
  EXPECT_EQ(hvcc.mNALUs[0].mNALU.Length(), 43u);
}

TEST(H265, DecodeSPSFromSPSNALU)
{
  H265NALU nalu{sSps, ArrayLength(sSps)};
  auto rv = H265::DecodeSPSFromSPSNALU(nalu);
  EXPECT_TRUE(rv.isOk());
  auto sps = rv.unwrap();
  // Examine the value by using HEVCESBrowser.
  EXPECT_EQ(sps.sps_video_parameter_set_id, 0u);
  EXPECT_EQ(sps.sps_max_sub_layers_minus1, 0u);
  EXPECT_EQ(sps.sps_temporal_id_nesting_flag, 1);
  EXPECT_EQ(sps.profile_tier_level.general_profile_space, 0u);
  EXPECT_EQ(sps.profile_tier_level.general_tier_flag, false);
  EXPECT_EQ(sps.profile_tier_level.general_profile_idc, 1u);
  EXPECT_EQ(sps.profile_tier_level.general_profile_compatibility_flags,
            0x60000000u);
  EXPECT_EQ(sps.profile_tier_level.general_progressive_source_flag, true);
  EXPECT_EQ(sps.profile_tier_level.general_interlaced_source_flag, false);
  EXPECT_EQ(sps.profile_tier_level.general_non_packed_constraint_flag, false);
  EXPECT_EQ(sps.profile_tier_level.general_frame_only_constraint_flag, true);
  EXPECT_EQ(sps.profile_tier_level.general_level_idc, 93u);
  EXPECT_EQ(sps.sps_seq_parameter_set_id, 0u);
  EXPECT_EQ(sps.chroma_format_idc, 1u);
  EXPECT_EQ(sps.separate_colour_plane_flag, false);
  EXPECT_EQ(sps.pic_width_in_luma_samples, 1024u);
  EXPECT_EQ(sps.pic_height_in_luma_samples, 768u);
  EXPECT_EQ(sps.conformance_window_flag, false);
  EXPECT_EQ(sps.bit_depth_luma_minus8, 0u);
  EXPECT_EQ(sps.bit_depth_chroma_minus8, 0u);
  EXPECT_EQ(sps.log2_max_pic_order_cnt_lsb_minus4, 4u);
  EXPECT_EQ(sps.sps_sub_layer_ordering_info_present_flag, true);
  EXPECT_EQ(sps.sps_max_dec_pic_buffering_minus1[0], 4u);
  EXPECT_EQ(sps.sps_max_num_reorder_pics[0], 2u);
  EXPECT_EQ(sps.sps_max_latency_increase_plus1[0], 5u);
  EXPECT_EQ(sps.log2_min_luma_coding_block_size_minus3, 0u);
  EXPECT_EQ(sps.log2_diff_max_min_luma_coding_block_size, 3u);
  EXPECT_EQ(sps.log2_min_luma_transform_block_size_minus2, 0u);
  EXPECT_EQ(sps.log2_diff_max_min_luma_transform_block_size, 3u);
  EXPECT_EQ(sps.max_transform_hierarchy_depth_inter, 0u);
  EXPECT_EQ(sps.max_transform_hierarchy_depth_inter, 0u);
  EXPECT_EQ(sps.pcm_enabled_flag, false);
  EXPECT_EQ(sps.num_short_term_ref_pic_sets, 0u);
  EXPECT_EQ(sps.sps_temporal_mvp_enabled_flag, true);
  EXPECT_EQ(sps.strong_intra_smoothing_enabled_flag, true);
  EXPECT_TRUE(sps.vui_parameters);
  EXPECT_EQ(sps.vui_parameters->video_full_range_flag, false);

  // Test public methods
  EXPECT_EQ(sps.BitDepthLuma(), 8u);
  EXPECT_EQ(sps.BitDepthChroma(), 8u);
  const auto imgSize = sps.GetImageSize();
  EXPECT_EQ(imgSize.Width(), 1024);
  EXPECT_EQ(imgSize.Height(), 768);
  const auto disSize = sps.GetDisplaySize();
  EXPECT_EQ(disSize, imgSize);
  EXPECT_EQ(sps.ColorDepth(), gfx::ColorDepth::COLOR_8);
  EXPECT_EQ(sps.ColorSpace(), gfx::YUVColorSpace::BT709);
  EXPECT_EQ(sps.IsFullColorRange(), false);
  EXPECT_EQ(sps.ColorPrimaries(), 2u);
  EXPECT_EQ(sps.TransferFunction(), 2u);
}

}  // namespace mozilla
