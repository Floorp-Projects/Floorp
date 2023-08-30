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

namespace mozilla {

/* static */ Result<HVCCConfig, nsresult> HVCCConfig::Parse(
    const mozilla::MediaRawData* aSample) {
  if (!aSample || aSample->Size() < 3) {
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
    return mozilla::Err(NS_ERROR_FAILURE);
  }
  const auto& byteBuffer = *aExtraData;
  if (byteBuffer[0] != 1) {
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

  return hvcc;
}

}  // namespace mozilla
