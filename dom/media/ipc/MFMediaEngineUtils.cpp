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

const char* MediaEngineEventToStr(MF_MEDIA_ENGINE_EVENT aEvent) {
  switch (aEvent) {
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_LOADSTART);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_PROGRESS);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_SUSPEND);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_ABORT);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_ERROR);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_EMPTIED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_STALLED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_PLAY);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_PAUSE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_LOADEDDATA);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_WAITING);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_PLAYING);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_CANPLAY);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_SEEKING);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_SEEKED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_TIMEUPDATE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_ENDED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_RATECHANGE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_FORMATCHANGE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_BALANCECHANGE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_OPMINFO);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_RESOURCELOST);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE);
    default:
      return "Unknown MF_MEDIA_ENGINE_EVENT";
  }
}

const char* MFMediaEngineErrorToStr(MFMediaEngineError aError) {
  switch (aError) {
    ENUM_TO_STR(MF_MEDIA_ENGINE_ERR_NOERROR);
    ENUM_TO_STR(MF_MEDIA_ENGINE_ERR_ABORTED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_ERR_NETWORK);
    ENUM_TO_STR(MF_MEDIA_ENGINE_ERR_DECODE);
    ENUM_TO_STR(MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED);
    ENUM_TO_STR(MF_MEDIA_ENGINE_ERR_ENCRYPTED);
    default:
      return "Unknown MFMediaEngineError";
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
