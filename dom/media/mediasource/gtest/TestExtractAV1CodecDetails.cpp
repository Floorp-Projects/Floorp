/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtest/gtest.h>
#include <stdint.h>

#include "AOMDecoder.h"
#include "VideoUtils.h"

namespace mozilla {
void PrintTo(const AOMDecoder::AV1SequenceInfo& aInfo, std::ostream* aStream) {
  nsAutoCString formatted = nsAutoCString();
  formatted.AppendPrintf(
      "av01.%01u.%02u%c.%02u.%01u.%01u%01u%01u.%02u.%02u.%02u.%01u (res: "
      "%ux%u) operating points: [",
      aInfo.mProfile, aInfo.mOperatingPoints[0].mLevel,
      aInfo.mOperatingPoints[0].mTier == 1 ? 'H' : 'M', aInfo.mBitDepth,
      aInfo.mMonochrome, aInfo.mSubsamplingX, aInfo.mSubsamplingY,
      static_cast<uint8_t>(aInfo.mChromaSamplePosition),
      static_cast<uint8_t>(aInfo.mColorSpace.mPrimaries),
      static_cast<uint8_t>(aInfo.mColorSpace.mTransfer),
      static_cast<uint8_t>(aInfo.mColorSpace.mMatrix),
      static_cast<uint8_t>(aInfo.mColorSpace.mRange), aInfo.mImage.Width(),
      aInfo.mImage.Height());
  size_t opCount = aInfo.mOperatingPoints.Length();
  for (size_t i = 0; i < opCount; i++) {
    const auto& op = aInfo.mOperatingPoints[i];
    formatted.AppendPrintf("{ layers: %x, level: %u, tier: %u }", op.mLayers,
                           op.mLevel, op.mTier);
    if (i != opCount - 1) {
      formatted.Append(", ");
    }
  }
  formatted.Append("]");
  *aStream << formatted;
}
}  // namespace mozilla

using namespace mozilla;

struct AV1TestData {
  const char* mCodecParameterString;
  const bool mExpectedValue;
  const char* mComment;
};

TEST(ExtractAV1CodecDetails, TestInputData)
{
  AV1TestData tests[] = {
      // Format is:
      // av01.N.NN[MH].NN.B.BBN.NN.NN.NN.B
      // where
      //   N = decimal digit
      //   [] = single character
      //   B = binary digit
      // Field order:
      // <sample entry 4CC>.<profile>.<level><tier>.<bitDepth>
      // [.<monochrome>.<chromaSubsampling>
      // .<colorPrimaries>.<transferCharacteristics>.<matrixCoefficients>
      // .<videoFullRangeFlag>]

      // Format checks
      {"av01.0.10M.08", true, "Minimum length"},
      {"av1.0.10M.08", false, "Invalid 4CC"},
      {"av01..10M.08", false, "Blank field"},
      {"av01.-1.10M.08", false, "Negative field"},
      {"av01.0.10M.8", false, "Missing leading zeros"},

      // Field counts
      {"av01", false, "0 of 4 required fields"},
      {"av01.0", false, "1 of 4 required fields"},
      {"av01.0.10", false, "2 of 4 required fields"},
      {"av01.0.10M", false, "3 of 4 required fields"},
      {"av01.0.10M.08.0", false, "5 fields, AV1 requires 4 or 10"},
      {"av01.0.10M.08.0.110.01.01.01", false, "9 fields, AV1 requires 4 or 10"},
      {"av01.0.10M.08.0.110.01.01.01.0", true, "Maximum fields"},
      {"av01.0.10M.08.0.110.01.01.01.0.0", false, "Too many fields"},

      // "Comments" are allowed (unknown characters at the end of fields)
      {"av01.0.10M.08this is ignored", true, "Minimum length with comment"},
      {"av01.0.10Mbad comment", false, "Comment before required field"},
      {"av01.0.10M.08.0.110.01.01.01.0also ignored", true,
       "Maximum length with comment"},

      // Begin field checks

      // -- Profile --
      // Main Profile (0) tested above

      // High Profile requires 4:4:4 chroma subsampling without monochrome
      {"av01.1.10M.08", false, "High Profile (1) without parameters"},
      {"av01.1.10M.08.0.000.01.01.01.0", true, "High Profile (1)"},

      // Professional requires either of:
      // - 8bit or 10bit at 4:2:2
      // - 12bit at any subsampling
      {"av01.2.10M.10.0.100.01.01.01.0", true,
       "Professional Profile (2) 10-bit 4:2:2"},
      {"av01.2.10M.12.0.110.01.01.01.0", true,
       "Professional Profile (2) 12-bit 4:2:0"},

      {"av01.3.10M.12.0.000.01.01.01.0", false, "Invalid Profile 3"},

      // -- Level --
      {"av01.0.00M.08", true, "Level 0 (2.1)"},
      // Level 4.2 (10) tested above
      {"av01.0.14M.08", true, "Level 14 (5.2)"},
      {"av01.0.23M.08", true, "Level 23 (7.3)"},
      {"av01.0.24M.08", false, "Level 24 (Reserved)"},

      // -- Tier --
      // Main tier tested above
      {"av01.0.10H.08", true, "High tier"},

      // -- Bit depth --
      // 8-bit tested above with Main and High Profiles
      {"av01.0.10M.10", true, "Main 10-bit"},
      {"av01.1.10M.10.0.000.01.01.01.0", true, "High 10-bit"},
      {"av01.1.10M.12.0.000.01.01.01.0", false, "High 12-bit (Invalid)"},
      // Valid 12-bit tested for Professional Profile

      // -- Monochrome --
      // Monochrome off tested above
      {"av01.0.10M.08.1.110.01.01.01.0", true, "Main 8-bit monochrome"},
      {"av01.1.10M.10.1.000.01.01.01.0", false,
       "4:4:4 is incompatible with monochrome"},
      {"av01.2.10M.10.1.100.01.01.01.0", false,
       "4:2:0 is incompatible with monochrome"},
      {"av01.2.10M.12.1.110.01.01.01.0", true,
       "Professional 12-bit monochrome"},

      // -- Chroma subsampling --
      // Field is parsed by digits <x><y><position>
      // where positions are [unknown, vertical, colocated]
      {"av01.0.10M.08.0.112.01.01.01.0", true, "Chroma colocated"},
      // Main Profile, 4:2:0 tested above
      {"av01.0.10M.08.0.100.01.01.01.0", false,
       "4:2:2 not allowed on Main Profile"},
      // High Profile, 4:4:4 tested above
      {"av01.1.10M.08.0.110.01.01.01.0", false,
       "4:4:4 required on High Profile"},
      {"av01.2.10M.08.0.110.01.01.01.0", false,
       "4:2:0 not allowed on 8-bit Professional"},
      // Professional Profile, 8-bit 4:2:2 tested above
      // Professional Profile, 12-bit 4:2:0 tested above
      {"av01.2.10M.12.0.100.01.01.01.0", true, "12-bit 4:2:2"},
      {"av01.2.10M.12.0.000.01.01.01.0", true, "12-bit 4:4:4"},

      {"av01.2.10M.08.0.101.01.01.01.0", false, "Chroma position with 4:2:2"},
      {"av01.1.10M.08.0.001.01.01.01.0", false, "Chroma position with 4:4:4"},
      {"av01.0.10M.08.0.113.01.01.01.0", false, "Chroma position 3 (Reserved)"},

      // -- Color primaries --
      // 0, 3, [13-21], >23 are reserved
      // 1 (BT709) is tested above
      {"av01.0.10M.10.0.110.09.16.09.0", true,
       "Color space: BT2020/SMPTE2084/BT2020NCL"},
      {"av01.0.10M.10.0.110.00.16.09.0", false, "Primaries 0: Reserved"},
      {"av01.0.10M.10.0.110.03.16.09.0", false, "Primaries 3: Reserved"},
      {"av01.0.10M.10.0.110.13.16.09.0", false, "Primaries 13: Reserved"},
      {"av01.0.10M.10.0.110.21.16.09.0", false, "Primaries 21: Reserved"},
      {"av01.0.10M.10.0.110.22.16.09.0", true, "Primaries 22: EBU3213"},
      {"av01.0.10M.10.0.110.23.16.09.0", false, "Primaries 23: Reserved"},

      // -- Transfer characteristics --
      // 0, 3, >19 are all reserved
      // 1 (BT709) is tested above
      // 16 (SMPTE2084) is tested above
      {"av01.0.10M.10.0.110.09.14.09.0", true,
       "Color space: BT2020/BT2020 10-bit/BT2020NCL"},
      {"av01.0.10M.10.0.110.09.00.09.0", false, "Transfer 0: Reserved"},
      {"av01.0.10M.10.0.110.09.03.09.0", false, "Transfer 3: Reserved"},
      {"av01.0.10M.10.0.110.09.20.09.0", false, "Transfer 20: Reserved"},

      // -- Matrix coefficients --
      // 3, >15 are all reserved
      // 1 (BT709) is tested above
      // 9 (BT2020NCL) is tested above
      {"av01.1.10M.10.0.000.01.13.00.1", true, "4:4:4 10-bit sRGB"},
      {"av01.1.10M.10.0.000.01.13.00.0", false, "sRGB requires full range"},
      {"av01.2.10M.10.0.100.01.13.00.1", false,
       "Subsampling incompatible with sRGB"},
      {"av01.2.10M.12.0.000.01.13.00.1", true, "4:4:4 12-bit sRGB"},
      {"av01.2.10M.12.0.000.01.01.15.1", false, "Matrix 15: Reserved"},

      // -- Color range --
      // Full range and limited range tested above
      {"av01.0.10M.12.0.002.01.13.00.2", false, "Color range 2 invalid"},
  };

  for (const auto& data : tests) {
    auto info = AOMDecoder::CreateSequenceInfoFromCodecs(
        NS_ConvertUTF8toUTF16(data.mCodecParameterString));
    nsAutoCString desc = nsAutoCString(data.mCodecParameterString,
                                       strlen(data.mCodecParameterString));
    desc.AppendLiteral(" (");
    desc.Append(data.mComment, strlen(data.mComment));
    desc.AppendLiteral(")");
    EXPECT_EQ(info.isSome(), data.mExpectedValue) << desc;

    if (info.isSome()) {
      AOMDecoder::AV1SequenceInfo inputInfo = info.value();
      inputInfo.mImage = gfx::IntSize(1920, 1080);
      RefPtr<MediaByteBuffer> buffer = new MediaByteBuffer();
      bool wroteSequenceHeader;
      AOMDecoder::WriteAV1CBox(inputInfo, buffer, wroteSequenceHeader);
      EXPECT_EQ(wroteSequenceHeader, data.mExpectedValue) << desc;
      // Read equality test will fail also, don't clutter.
      if (!wroteSequenceHeader) {
        continue;
      }
      AOMDecoder::AV1SequenceInfo parsedInfo;
      bool readSequenceHeader;
      AOMDecoder::ReadAV1CBox(buffer, parsedInfo, readSequenceHeader);
      EXPECT_EQ(wroteSequenceHeader, readSequenceHeader) << desc;
      EXPECT_EQ(inputInfo, parsedInfo) << desc;
    }
  }
}

TEST(ExtractAV1CodecDetails, TestParsingOutput)
{
  auto info = AOMDecoder::CreateSequenceInfoFromCodecs(
      nsString(u"av01.0.14M.08.0.112.01.01.01.0"));
  EXPECT_TRUE(info.isSome());

  if (info.isSome()) {
    EXPECT_EQ(info->mProfile, 0u);
    EXPECT_EQ(info->mOperatingPoints.Length(), 1u);
    EXPECT_EQ(info->mOperatingPoints[0].mLayers, 0u);
    EXPECT_EQ(info->mOperatingPoints[0].mLevel, 14u);
    EXPECT_EQ(info->mOperatingPoints[0].mTier, 0u);
    EXPECT_EQ(info->mBitDepth, 8u);
    EXPECT_EQ(info->mMonochrome, false);
    EXPECT_EQ(info->mSubsamplingX, true);
    EXPECT_EQ(info->mSubsamplingY, true);
    EXPECT_EQ(info->mChromaSamplePosition,
              AOMDecoder::ChromaSamplePosition::Colocated);
    EXPECT_EQ(info->mColorSpace.mPrimaries, gfx::CICP::CP_BT709);
    EXPECT_EQ(info->mColorSpace.mTransfer, gfx::CICP::TC_BT709);
    EXPECT_EQ(info->mColorSpace.mMatrix, gfx::CICP::MC_BT709);
    EXPECT_EQ(info->mColorSpace.mRange, gfx::ColorRange::LIMITED);
  }

  info = AOMDecoder::CreateSequenceInfoFromCodecs(
      nsString(u"av01.1.11H.10.0.000.07.07.07.1"));
  EXPECT_TRUE(info.isSome());

  if (info.isSome()) {
    EXPECT_EQ(info->mProfile, 1u);
    EXPECT_EQ(info->mOperatingPoints.Length(), 1u);
    EXPECT_EQ(info->mOperatingPoints[0].mLayers, 0u);
    EXPECT_EQ(info->mOperatingPoints[0].mLevel, 11u);
    EXPECT_EQ(info->mOperatingPoints[0].mTier, 1u);
    EXPECT_EQ(info->mBitDepth, 10u);
    EXPECT_EQ(info->mMonochrome, false);
    EXPECT_EQ(info->mSubsamplingX, false);
    EXPECT_EQ(info->mSubsamplingY, false);
    EXPECT_EQ(info->mChromaSamplePosition,
              AOMDecoder::ChromaSamplePosition::Unknown);
    EXPECT_EQ(info->mColorSpace.mPrimaries, gfx::CICP::CP_SMPTE240);
    EXPECT_EQ(info->mColorSpace.mTransfer, gfx::CICP::TC_SMPTE240);
    EXPECT_EQ(info->mColorSpace.mMatrix, gfx::CICP::MC_SMPTE240);
    EXPECT_EQ(info->mColorSpace.mRange, gfx::ColorRange::FULL);
  }

  info = AOMDecoder::CreateSequenceInfoFromCodecs(
      nsString(u"av01.2.22H.12.1.110.10.08.04.1"));
  EXPECT_TRUE(info.isSome());

  if (info.isSome()) {
    EXPECT_EQ(info->mProfile, 2u);
    EXPECT_EQ(info->mOperatingPoints.Length(), 1u);
    EXPECT_EQ(info->mOperatingPoints[0].mLayers, 0u);
    EXPECT_EQ(info->mOperatingPoints[0].mLevel, 22u);
    EXPECT_EQ(info->mOperatingPoints[0].mTier, 1u);
    EXPECT_EQ(info->mBitDepth, 12u);
    EXPECT_EQ(info->mMonochrome, true);
    EXPECT_EQ(info->mSubsamplingX, true);
    EXPECT_EQ(info->mSubsamplingY, true);
    EXPECT_EQ(info->mChromaSamplePosition,
              AOMDecoder::ChromaSamplePosition::Unknown);
    EXPECT_EQ(info->mColorSpace.mPrimaries, gfx::CICP::CP_XYZ);
    EXPECT_EQ(info->mColorSpace.mTransfer, gfx::CICP::TC_LINEAR);
    EXPECT_EQ(info->mColorSpace.mMatrix, gfx::CICP::MC_FCC);
    EXPECT_EQ(info->mColorSpace.mRange, gfx::ColorRange::FULL);
  }
}
