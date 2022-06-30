/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineUtils.h"

#include "WMFUtils.h"

namespace mozilla {

#define ENUM_TO_STR(enumVal) \
  case enumVal:              \
    return #enumVal

#define ENUM_TO_STR2(guid, enumVal) \
  if (guid == enumVal) {            \
    return #enumVal;                \
  }

const char* MediaEventTypeToStr(MediaEventType aType) {
  switch (aType) {
    ENUM_TO_STR(MESourceUnknown);
    ENUM_TO_STR(MESourceStarted);
    ENUM_TO_STR(MEStreamStarted);
    ENUM_TO_STR(MESourceSeeked);
    ENUM_TO_STR(MEStreamSeeked);
    ENUM_TO_STR(MENewStream);
    ENUM_TO_STR(MEUpdatedStream);
    ENUM_TO_STR(MESourceStopped);
    ENUM_TO_STR(MEStreamStopped);
    ENUM_TO_STR(MESourcePaused);
    ENUM_TO_STR(MEStreamPaused);
    ENUM_TO_STR(MEEndOfPresentation);
    ENUM_TO_STR(MEEndOfStream);
    ENUM_TO_STR(MEMediaSample);
    ENUM_TO_STR(MEStreamTick);
    ENUM_TO_STR(MEStreamThinMode);
    ENUM_TO_STR(MEStreamFormatChanged);
    ENUM_TO_STR(MESourceRateChanged);
    ENUM_TO_STR(MEEndOfPresentationSegment);
    ENUM_TO_STR(MESourceCharacteristicsChanged);
    ENUM_TO_STR(MESourceRateChangeRequested);
    ENUM_TO_STR(MESourceMetadataChanged);
    ENUM_TO_STR(MESequencerSourceTopologyUpdated);
    default:
      return "Unknown MediaEventType";
  }
}

const char* GUIDToStr(GUID aGUID) {
  ENUM_TO_STR2(aGUID, MFAudioFormat_MP3)
  ENUM_TO_STR2(aGUID, MFAudioFormat_AAC)
  ENUM_TO_STR2(aGUID, MFAudioFormat_Vorbis)
  ENUM_TO_STR2(aGUID, MFAudioFormat_Opus)
  ENUM_TO_STR2(aGUID, MFVideoFormat_H264)
  ENUM_TO_STR2(aGUID, MFVideoFormat_VP80)
  ENUM_TO_STR2(aGUID, MFVideoFormat_VP90)
  ENUM_TO_STR2(aGUID, MFVideoFormat_AV1)
  ENUM_TO_STR2(aGUID, MFMediaType_Audio)
  return "Unknown GUID";
}

const char* MFVideoRotationFormatToStr(MFVideoRotationFormat aFormat) {
  switch (aFormat) {
    ENUM_TO_STR(MFVideoRotationFormat_0);
    ENUM_TO_STR(MFVideoRotationFormat_90);
    ENUM_TO_STR(MFVideoRotationFormat_180);
    ENUM_TO_STR(MFVideoRotationFormat_270);
    default:
      return "Unknown MFVideoRotationFormat";
  }
}

const char* MFVideoTransferFunctionToStr(MFVideoTransferFunction aFunc) {
  switch (aFunc) {
    ENUM_TO_STR(MFVideoTransFunc_Unknown);
    ENUM_TO_STR(MFVideoTransFunc_709);
    ENUM_TO_STR(MFVideoTransFunc_2020);
    ENUM_TO_STR(MFVideoTransFunc_sRGB);
    default:
      return "Unsupported MFVideoTransferFunction";
  }
}

const char* MFVideoPrimariesToStr(MFVideoPrimaries aPrimaries) {
  switch (aPrimaries) {
    ENUM_TO_STR(MFVideoPrimaries_Unknown);
    ENUM_TO_STR(MFVideoPrimaries_BT709);
    ENUM_TO_STR(MFVideoPrimaries_BT2020);
    default:
      return "Unsupported MFVideoPrimaries";
  }
}

#undef ENUM_TO_STR
#undef ENUM_TO_STR2

}  // namespace mozilla
