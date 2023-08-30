/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_
#define DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_

#include <stdint.h>

#include "mozilla/Result.h"

namespace mozilla {

class MediaByteBuffer;
class MediaRawData;

// ISO/IEC 14496-15 : hvcC. We only parse partial attributes, not all of them.
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

  // TODO : parse NALU in order to know if SPS exists

 private:
  HVCCConfig() = default;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_AGNOSTIC_BYTESTREAMS_H265_H_
