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
#include "ByteWriter.h"
#include "MediaData.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Span.h"

mozilla::LazyLogModule gH265("H265");

#define LOG(msg, ...) MOZ_LOG(gH265, LogLevel::Debug, (msg, ##__VA_ARGS__))
#define LOGV(msg, ...) MOZ_LOG(gH265, LogLevel::Verbose, (msg, ##__VA_ARGS__))

namespace mozilla {

H265NALU::H265NALU(const uint8_t* aData, uint32_t aSize) : mNALU(aData, aSize) {
  // Per 7.3.1 NAL unit syntax
  BitReader reader(aData, aSize);
  Unused << reader.ReadBits(1);  // forbidden_zero_bit
  mNalUnitType = reader.ReadBits(6);
  mNuhLayerId = reader.ReadBits(6);
  mNuhTemporalIdPlus1 = reader.ReadBits(3);
  LOGV("Created H265NALU, type=%hhu, size=%u", mNalUnitType, aSize);
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
  hvcc.mGeneralTierFlag = reader.ReadBits(1);
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
  hvcc.mTemporalIdNested = reader.ReadBits(1);
  hvcc.mLengthSizeMinusOne = reader.ReadBits(2);
  const uint8_t numOfArrays = reader.ReadBits(8);
  for (uint8_t idx = 0; idx < numOfArrays; idx++) {
    Unused << reader.ReadBits(2);  // array_completeness + reserved
    const uint8_t nalUnitType = reader.ReadBits(6);
    const uint16_t numNalus = reader.ReadBits(16);
    LOGV("nalu-type=%u, nalu-num=%u", nalUnitType, numNalus);
    for (uint16_t nIdx = 0; nIdx < numNalus; nIdx++) {
      const uint16_t nalUnitLength = reader.ReadBits(16);
      uint32_t nalSize = nalUnitLength * 8;
      if (reader.BitsLeft() < nalUnitLength * 8) {
        return mozilla::Err(NS_ERROR_FAILURE);
      }
      const uint8_t* currentPtr =
          aExtraData->Elements() + reader.BitCount() / 8;
      H265NALU nalu(currentPtr, nalSize);
      // ReadBits can only read at most 32 bits at a time.
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

#undef LOG
#undef LOGV

}  // namespace mozilla
