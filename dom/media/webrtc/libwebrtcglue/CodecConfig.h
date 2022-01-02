
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CODEC_CONFIG_H_
#define CODEC_CONFIG_H_

#include <string>
#include <vector>

#include "common/EncodingConstraints.h"

namespace mozilla {

/**
 * Minimalistic Audio Codec Config Params
 */
struct AudioCodecConfig {
  /*
   * The data-types for these properties mimic the
   * corresponding webrtc::CodecInst data-types.
   */
  int mType;
  std::string mName;
  int mFreq;
  int mChannels;

  bool mFECEnabled;
  bool mDtmfEnabled;
  uint32_t mFrameSizeMs;
  uint32_t mMaxFrameSizeMs;
  uint32_t mMinFrameSizeMs;

  // OPUS-specific
  bool mDTXEnabled;
  uint32_t mMaxAverageBitrate;
  int mMaxPlaybackRate;
  bool mCbrEnabled;

  AudioCodecConfig(int type, std::string name, int freq, int channels,
                   bool FECEnabled)
      : mType(type),
        mName(name),
        mFreq(freq),
        mChannels(channels),
        mFECEnabled(FECEnabled),
        mDtmfEnabled(false),
        mFrameSizeMs(0),
        mMaxFrameSizeMs(0),
        mMinFrameSizeMs(0),
        mDTXEnabled(false),
        mMaxAverageBitrate(0),
        mMaxPlaybackRate(0),
        mCbrEnabled(false) {}

  bool operator==(const AudioCodecConfig& aOther) const {
    return mType == aOther.mType && mName == aOther.mName &&
           mFreq == aOther.mFreq && mChannels == aOther.mChannels &&
           mFECEnabled == aOther.mFECEnabled &&
           mDtmfEnabled == aOther.mDtmfEnabled &&
           mFrameSizeMs == aOther.mFrameSizeMs &&
           mMaxFrameSizeMs == aOther.mMaxFrameSizeMs &&
           mMinFrameSizeMs == aOther.mMinFrameSizeMs &&
           mDTXEnabled == aOther.mDTXEnabled &&
           mMaxAverageBitrate == aOther.mMaxAverageBitrate &&
           mMaxPlaybackRate == aOther.mMaxPlaybackRate &&
           mCbrEnabled == aOther.mCbrEnabled;
  }
};

/*
 * Minimalistic video codec configuration
 * More to be added later depending on the use-case
 */

#define MAX_SPROP_LEN 128

// used for holding SDP negotiation results
struct VideoCodecConfigH264 {
  char sprop_parameter_sets[MAX_SPROP_LEN];
  int packetization_mode;
  int profile_level_id;
  int tias_bw;

  bool operator==(const VideoCodecConfigH264& aOther) const {
    return strncmp(sprop_parameter_sets, aOther.sprop_parameter_sets,
                   MAX_SPROP_LEN) == 0 &&
           packetization_mode == aOther.packetization_mode &&
           profile_level_id == aOther.profile_level_id &&
           tias_bw == aOther.tias_bw;
  }
};

// class so the std::strings can get freed more easily/reliably
class VideoCodecConfig {
 public:
  /*
   * The data-types for these properties mimic the
   * corresponding webrtc::VideoCodec data-types.
   */
  int mType;  // payload type
  std::string mName;

  std::vector<std::string> mAckFbTypes;
  std::vector<std::string> mNackFbTypes;
  std::vector<std::string> mCcmFbTypes;
  // Don't pass mOtherFbTypes from JsepVideoCodecDescription because we'd have
  // to drag SdpRtcpFbAttributeList::Feedback along too.
  bool mRembFbSet;
  bool mFECFbSet;
  bool mTransportCCFbSet;

  int mULPFECPayloadType;
  int mREDPayloadType;
  int mREDRTXPayloadType;
  int mRTXPayloadType;

  uint32_t mTias;
  EncodingConstraints mEncodingConstraints;
  struct Encoding {
    std::string rid;
    EncodingConstraints constraints;
    bool operator==(const Encoding& aOther) const {
      return rid == aOther.rid && constraints == aOther.constraints;
    }
  };
  std::vector<Encoding> mEncodings;
  std::string mSpropParameterSets;
  uint8_t mProfile;
  uint8_t mConstraints;
  uint8_t mLevel;
  uint8_t mPacketizationMode;
  // TODO: add external negotiated SPS/PPS

  bool operator==(const VideoCodecConfig& aRhs) const {
    return mType == aRhs.mType && mName == aRhs.mName &&
           mAckFbTypes == aRhs.mAckFbTypes &&
           mNackFbTypes == aRhs.mNackFbTypes &&
           mCcmFbTypes == aRhs.mCcmFbTypes && mRembFbSet == aRhs.mRembFbSet &&
           mFECFbSet == aRhs.mFECFbSet &&
           mTransportCCFbSet == aRhs.mTransportCCFbSet &&
           mULPFECPayloadType == aRhs.mULPFECPayloadType &&
           mREDPayloadType == aRhs.mREDPayloadType &&
           mREDRTXPayloadType == aRhs.mREDRTXPayloadType &&
           mRTXPayloadType == aRhs.mRTXPayloadType && mTias == aRhs.mTias &&
           mEncodingConstraints == aRhs.mEncodingConstraints &&
           mEncodings == aRhs.mEncodings &&
           mSpropParameterSets == aRhs.mSpropParameterSets &&
           mProfile == aRhs.mProfile && mConstraints == aRhs.mConstraints &&
           mLevel == aRhs.mLevel &&
           mPacketizationMode == aRhs.mPacketizationMode;
  }

  VideoCodecConfig(int type, std::string name,
                   const EncodingConstraints& constraints,
                   const struct VideoCodecConfigH264* h264 = nullptr)
      : mType(type),
        mName(name),
        mRembFbSet(false),
        mFECFbSet(false),
        mTransportCCFbSet(false),
        mULPFECPayloadType(-1),
        mREDPayloadType(-1),
        mREDRTXPayloadType(-1),
        mRTXPayloadType(-1),
        mTias(0),
        mEncodingConstraints(constraints),
        mProfile(0x42),
        mConstraints(0xE0),
        mLevel(0x0C),
        mPacketizationMode(1) {
    if (h264) {
      mProfile = (h264->profile_level_id & 0x00FF0000) >> 16;
      mConstraints = (h264->profile_level_id & 0x0000FF00) >> 8;
      mLevel = (h264->profile_level_id & 0x000000FF);
      mPacketizationMode = h264->packetization_mode;
      mSpropParameterSets = h264->sprop_parameter_sets;
    }
  }

  bool ResolutionEquals(const VideoCodecConfig& aConfig) const {
    if (mEncodings.size() != aConfig.mEncodings.size()) {
      return false;
    }
    for (size_t i = 0; i < mEncodings.size(); ++i) {
      if (!mEncodings[i].constraints.ResolutionEquals(
              aConfig.mEncodings[i].constraints)) {
        return false;
      }
    }
    return true;
  }

  // Nothing seems to use this right now. Do we intend to support this
  // someday?
  bool RtcpFbAckIsSet(const std::string& type) const {
    for (auto i = mAckFbTypes.begin(); i != mAckFbTypes.end(); ++i) {
      if (*i == type) {
        return true;
      }
    }
    return false;
  }

  bool RtcpFbNackIsSet(const std::string& type) const {
    for (auto i = mNackFbTypes.begin(); i != mNackFbTypes.end(); ++i) {
      if (*i == type) {
        return true;
      }
    }
    return false;
  }

  bool RtcpFbCcmIsSet(const std::string& type) const {
    for (auto i = mCcmFbTypes.begin(); i != mCcmFbTypes.end(); ++i) {
      if (*i == type) {
        return true;
      }
    }
    return false;
  }

  bool RtcpFbRembIsSet() const { return mRembFbSet; }

  bool RtcpFbFECIsSet() const { return mFECFbSet; }

  bool RtcpFbTransportCCIsSet() const { return mTransportCCFbSet; }

  bool RtxPayloadTypeIsSet() const { return mRTXPayloadType != -1; }
};
}  // namespace mozilla
#endif
