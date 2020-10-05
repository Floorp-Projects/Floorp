/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "AnnexB.h"
#include "ByteWriter.h"
#include "H264.h"

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

// Test that conversion from AVCC to AnnexB works as expected.
TEST(AnnexB, AnnexBConversion)
{
  RefPtr<MediaRawData> rawData{GetAvccSample(128)};

  {
    // Test conversion of data when not adding SPS works as expected.
    RefPtr<MediaRawData> rawDataClone = rawData->Clone();
    Result<Ok, nsresult> result =
        AnnexB::ConvertSampleToAnnexB(rawDataClone, /* aAddSps */ false);
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
        AnnexB::ConvertSampleToAnnexB(rawDataClone, /* aAddSps */ true);
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
        AnnexB::ConvertSampleToAnnexB(rawDataClone, /* aAddSps */ true);
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
    Result<Ok, nsresult> result =
        AnnexB::ConvertSampleToAnnexB(rawCryptoDataClone, /* aAddSps */ true);
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

}  // namespace mozilla
