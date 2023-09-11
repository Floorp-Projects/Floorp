/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPCODECDESCRIPTION_H_
#define _JSEPCODECDESCRIPTION_H_

#include <cmath>
#include <string>
#include "sdp/SdpMediaSection.h"
#include "sdp/SdpHelper.h"
#include "nsCRT.h"
#include "nsString.h"
#include "mozilla/net/DataChannelProtocol.h"
#include "mozilla/Preferences.h"

namespace mozilla {

#define JSEP_CODEC_CLONE(T) \
  virtual JsepCodecDescription* Clone() const override { return new T(*this); }

// A single entry in our list of known codecs.
class JsepCodecDescription {
 public:
  JsepCodecDescription(const std::string& defaultPt, const std::string& name,
                       uint32_t clock, uint32_t channels, bool enabled)
      : mDefaultPt(defaultPt),
        mName(name),
        mClock(clock),
        mChannels(channels),
        mEnabled(enabled),
        mStronglyPreferred(false),
        mDirection(sdp::kSend) {}
  virtual ~JsepCodecDescription() {}

  virtual SdpMediaSection::MediaType Type() const = 0;

  virtual JsepCodecDescription* Clone() const = 0;

  bool GetPtAsInt(uint16_t* ptOutparam) const {
    return SdpHelper::GetPtAsInt(mDefaultPt, ptOutparam);
  }

  // The id used for codec stats, to uniquely identify this codec configuration
  // within a transport.
  const nsString& StatsId() const {
    if (!mStatsId) {
      mStatsId.emplace();
      mStatsId->AppendPrintf(
          "_%s_%s/%s_%u_%u_%s", mDefaultPt.c_str(),
          Type() == SdpMediaSection::kVideo ? "video" : "audio", mName.c_str(),
          mClock, mChannels, mSdpFmtpLine ? mSdpFmtpLine->c_str() : "nothing");
    }
    return *mStatsId;
  }

  virtual bool Matches(const std::string& fmt,
                       const SdpMediaSection& remoteMsection) const {
    // note: fmt here is remote fmt (to go with remoteMsection)
    if (Type() != remoteMsection.GetMediaType()) {
      return false;
    }

    const SdpRtpmapAttributeList::Rtpmap* entry(remoteMsection.FindRtpmap(fmt));

    if (entry) {
      if (!nsCRT::strcasecmp(mName.c_str(), entry->name.c_str()) &&
          (mClock == entry->clock) && (mChannels == entry->channels)) {
        return ParametersMatch(fmt, remoteMsection);
      }
    } else if (!fmt.compare("9") && mName == "G722") {
      return true;
    } else if (!fmt.compare("0") && mName == "PCMU") {
      return true;
    } else if (!fmt.compare("8") && mName == "PCMA") {
      return true;
    }
    return false;
  }

  virtual bool ParametersMatch(const std::string& fmt,
                               const SdpMediaSection& remoteMsection) const {
    return true;
  }

  Maybe<std::string> GetMatchingFormat(
      const SdpMediaSection& remoteMsection) const {
    for (const auto& fmt : remoteMsection.GetFormats()) {
      if (Matches(fmt, remoteMsection)) {
        return Some(fmt);
      }
    }
    return Nothing();
  }

  virtual bool Negotiate(const std::string& pt,
                         const SdpMediaSection& remoteMsection,
                         bool remoteIsOffer,
                         Maybe<const SdpMediaSection&> localMsection) {
    // Configuration might change. Invalidate the stats id.
    mStatsId = Nothing();
    if (mDirection == sdp::kSend || remoteIsOffer) {
      mDefaultPt = pt;
    }
    if (localMsection) {
      // Offer/answer is concluding. Update the sdpFmtpLine.
      MOZ_ASSERT(mDirection == sdp::kSend || mDirection == sdp::kRecv);
      const SdpMediaSection& msection =
          mDirection == sdp::kSend ? remoteMsection : *localMsection;
      UpdateSdpFmtpLine(ToMaybeRef(msection.FindFmtp(mDefaultPt)));
    }
    return true;
  }

  virtual void ApplyConfigToFmtp(
      UniquePtr<SdpFmtpAttributeList::Parameters>& aFmtp) const = 0;

  virtual void AddToMediaSection(SdpMediaSection& msection) const {
    if (mEnabled && msection.GetMediaType() == Type()) {
      if (mDirection == sdp::kRecv) {
        msection.AddCodec(mDefaultPt, mName, mClock, mChannels);
      }

      AddParametersToMSection(msection);
    }
  }

  virtual void AddParametersToMSection(SdpMediaSection& msection) const {}

  virtual void EnsureNoDuplicatePayloadTypes(std::set<std::string>& aUsedPts) {
    mEnabled = EnsurePayloadTypeNotDuplicate(aUsedPts, mDefaultPt);
  }

  bool EnsurePayloadTypeNotDuplicate(std::set<std::string>& aUsedPts,
                                     std::string& aPtToCheck) {
    if (!mEnabled) {
      return false;
    }

    if (!aUsedPts.count(aPtToCheck)) {
      aUsedPts.insert(aPtToCheck);
      return true;
    }

    // |codec| cannot use its current payload type. Try to find another.
    for (uint16_t freePt = 96; freePt <= 127; ++freePt) {
      // Not super efficient, but readability is probably more important.
      std::ostringstream os;
      os << freePt;
      std::string freePtAsString = os.str();

      if (!aUsedPts.count(freePtAsString)) {
        aUsedPts.insert(freePtAsString);
        aPtToCheck = freePtAsString;
        return true;
      }
    }

    return false;
  }

  // TODO Bug 1751671: Take a verbatim fmtp line (std::string or eq.) instead
  // of fmtp parameters that have to be (re-)serialized.
  void UpdateSdpFmtpLine(
      const Maybe<const SdpFmtpAttributeList::Parameters&> aParams) {
    mSdpFmtpLine = aParams.map([](const auto& aFmtp) {
      std::stringstream ss;
      aFmtp.Serialize(ss);
      return ss.str();
    });
  }

  std::string mDefaultPt;
  std::string mName;
  Maybe<std::string> mSdpFmtpLine;
  mutable Maybe<nsString> mStatsId;
  uint32_t mClock;
  uint32_t mChannels;
  bool mEnabled;
  bool mStronglyPreferred;
  sdp::Direction mDirection;
  // Will hold constraints from both fmtp and rid
  EncodingConstraints mConstraints;
};

class JsepAudioCodecDescription : public JsepCodecDescription {
 public:
  JsepAudioCodecDescription(const std::string& defaultPt,
                            const std::string& name, uint32_t clock,
                            uint32_t channels, bool enabled = true)
      : JsepCodecDescription(defaultPt, name, clock, channels, enabled),
        mMaxPlaybackRate(0),
        mForceMono(false),
        mFECEnabled(false),
        mDtmfEnabled(false),
        mMaxAverageBitrate(0),
        mDTXEnabled(false),
        mFrameSizeMs(0),
        mMinFrameSizeMs(0),
        mMaxFrameSizeMs(0),
        mCbrEnabled(false) {}

  static constexpr SdpMediaSection::MediaType type = SdpMediaSection::kAudio;

  SdpMediaSection::MediaType Type() const override { return type; }

  JSEP_CODEC_CLONE(JsepAudioCodecDescription)

  static UniquePtr<JsepAudioCodecDescription> CreateDefaultOpus() {
    // Per jmspeex on IRC:
    // For 32KHz sampling, 28 is ok, 32 is good, 40 should be really good
    // quality.  Note that 1-2Kbps will be wasted on a stereo Opus channel
    // with mono input compared to configuring it for mono.
    // If we reduce bitrate enough Opus will low-pass us; 16000 will kill a
    // 9KHz tone.  This should be adaptive when we're at the low-end of video
    // bandwidth (say <100Kbps), and if we're audio-only, down to 8 or
    // 12Kbps.
    return MakeUnique<JsepAudioCodecDescription>("109", "opus", 48000, 2);
  }

  static UniquePtr<JsepAudioCodecDescription> CreateDefaultG722() {
    return MakeUnique<JsepAudioCodecDescription>("9", "G722", 8000, 1);
  }

  static UniquePtr<JsepAudioCodecDescription> CreateDefaultPCMU() {
    return MakeUnique<JsepAudioCodecDescription>("0", "PCMU", 8000, 1);
  }

  static UniquePtr<JsepAudioCodecDescription> CreateDefaultPCMA() {
    return MakeUnique<JsepAudioCodecDescription>("8", "PCMA", 8000, 1);
  }

  static UniquePtr<JsepAudioCodecDescription> CreateDefaultTelephoneEvent() {
    return MakeUnique<JsepAudioCodecDescription>("101", "telephone-event", 8000,
                                                 1);
  }

  SdpFmtpAttributeList::OpusParameters GetOpusParameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    // Will contain defaults if nothing else
    SdpFmtpAttributeList::OpusParameters result;
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == SdpRtpmapAttributeList::kOpus) {
      result =
          static_cast<const SdpFmtpAttributeList::OpusParameters&>(*params);
    }

    return result;
  }

  SdpFmtpAttributeList::TelephoneEventParameters GetTelephoneEventParameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    // Will contain defaults if nothing else
    SdpFmtpAttributeList::TelephoneEventParameters result;
    auto* params = msection.FindFmtp(pt);

    if (params &&
        params->codec_type == SdpRtpmapAttributeList::kTelephoneEvent) {
      result =
          static_cast<const SdpFmtpAttributeList::TelephoneEventParameters&>(
              *params);
    }

    return result;
  }

  void AddParametersToMSection(SdpMediaSection& msection) const override {
    if (mDirection == sdp::kSend) {
      return;
    }

    if (mName == "opus") {
      UniquePtr<SdpFmtpAttributeList::Parameters> opusParams =
          MakeUnique<SdpFmtpAttributeList::OpusParameters>(
              GetOpusParameters(mDefaultPt, msection));

      ApplyConfigToFmtp(opusParams);

      msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, *opusParams));
    } else if (mName == "telephone-event") {
      // add the default dtmf tones
      SdpFmtpAttributeList::TelephoneEventParameters teParams(
          GetTelephoneEventParameters(mDefaultPt, msection));
      msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, teParams));
    }
  }

  bool Negotiate(const std::string& pt, const SdpMediaSection& remoteMsection,
                 bool remoteIsOffer,
                 Maybe<const SdpMediaSection&> localMsection) override {
    JsepCodecDescription::Negotiate(pt, remoteMsection, remoteIsOffer,
                                    localMsection);
    if (mName == "opus" && mDirection == sdp::kSend) {
      SdpFmtpAttributeList::OpusParameters opusParams(
          GetOpusParameters(mDefaultPt, remoteMsection));

      mMaxPlaybackRate = opusParams.maxplaybackrate;
      mForceMono = !opusParams.stereo;
      // draft-ietf-rtcweb-fec-03.txt section 4.2 says support for FEC
      // at the received side is declarative and can be negotiated
      // separately for either media direction.
      mFECEnabled = opusParams.useInBandFec;
      if ((opusParams.maxAverageBitrate >= 6000) &&
          (opusParams.maxAverageBitrate <= 510000)) {
        mMaxAverageBitrate = opusParams.maxAverageBitrate;
      }
      mDTXEnabled = opusParams.useDTX;
      if (remoteMsection.GetAttributeList().HasAttribute(
              SdpAttribute::kPtimeAttribute)) {
        mFrameSizeMs = remoteMsection.GetAttributeList().GetPtime();
      } else {
        mFrameSizeMs = opusParams.frameSizeMs;
      }
      mMinFrameSizeMs = opusParams.minFrameSizeMs;
      if (remoteMsection.GetAttributeList().HasAttribute(
              SdpAttribute::kMaxptimeAttribute)) {
        mMaxFrameSizeMs = remoteMsection.GetAttributeList().GetMaxptime();
      } else {
        mMaxFrameSizeMs = opusParams.maxFrameSizeMs;
      }
      mCbrEnabled = opusParams.useCbr;
    }

    return true;
  }

  void ApplyConfigToFmtp(
      UniquePtr<SdpFmtpAttributeList::Parameters>& aFmtp) const override {
    if (mName == "opus") {
      SdpFmtpAttributeList::OpusParameters opusParams;
      if (aFmtp) {
        MOZ_RELEASE_ASSERT(aFmtp->codec_type == SdpRtpmapAttributeList::kOpus);
        opusParams =
            static_cast<const SdpFmtpAttributeList::OpusParameters&>(*aFmtp);
        opusParams.useInBandFec = mFECEnabled ? 1 : 0;
      } else {
        // If we weren't passed a fmtp to use then show we can do in band FEC
        // for getCapabilities queries.
        opusParams.useInBandFec = 1;
      }
      if (mMaxPlaybackRate) {
        opusParams.maxplaybackrate = mMaxPlaybackRate;
      }
      opusParams.maxAverageBitrate = mMaxAverageBitrate;

      if (mChannels == 2 &&
          !Preferences::GetBool("media.peerconnection.sdp.disable_stereo_fmtp",
                                false) &&
          !mForceMono) {
        // We prefer to receive stereo, if available.
        opusParams.stereo = 1;
      }
      opusParams.useDTX = mDTXEnabled;
      opusParams.frameSizeMs = mFrameSizeMs;
      opusParams.minFrameSizeMs = mMinFrameSizeMs;
      opusParams.maxFrameSizeMs = mMaxFrameSizeMs;
      opusParams.useCbr = mCbrEnabled;
      aFmtp.reset(opusParams.Clone());
    }
  };

  uint32_t mMaxPlaybackRate;
  bool mForceMono;
  bool mFECEnabled;
  bool mDtmfEnabled;
  uint32_t mMaxAverageBitrate;
  bool mDTXEnabled;
  uint32_t mFrameSizeMs;
  uint32_t mMinFrameSizeMs;
  uint32_t mMaxFrameSizeMs;
  bool mCbrEnabled;
};

class JsepVideoCodecDescription : public JsepCodecDescription {
 public:
  JsepVideoCodecDescription(const std::string& defaultPt,
                            const std::string& name, uint32_t clock,
                            bool enabled = true)
      : JsepCodecDescription(defaultPt, name, clock, 0, enabled),
        mTmmbrEnabled(false),
        mRembEnabled(false),
        mFECEnabled(false),
        mTransportCCEnabled(false),
        mRtxEnabled(false),
        mProfileLevelId(0),
        mPacketizationMode(0) {
    // Add supported rtcp-fb types
    mNackFbTypes.push_back("");
    mNackFbTypes.push_back(SdpRtcpFbAttributeList::pli);
    mCcmFbTypes.push_back(SdpRtcpFbAttributeList::fir);
  }

  static constexpr SdpMediaSection::MediaType type = SdpMediaSection::kVideo;

  SdpMediaSection::MediaType Type() const override { return type; }

  static UniquePtr<JsepVideoCodecDescription> CreateDefaultVP8(bool aUseRtx) {
    auto codec = MakeUnique<JsepVideoCodecDescription>("120", "VP8", 90000);
    // Defaults for mandatory params
    codec->mConstraints.maxFs = 12288;  // Enough for 2048x1536
    codec->mConstraints.maxFps = Some(60);
    if (aUseRtx) {
      codec->EnableRtx("124");
    }
    return codec;
  }

  static UniquePtr<JsepVideoCodecDescription> CreateDefaultVP9(bool aUseRtx) {
    auto codec = MakeUnique<JsepVideoCodecDescription>("121", "VP9", 90000);
    // Defaults for mandatory params
    codec->mConstraints.maxFs = 12288;  // Enough for 2048x1536
    codec->mConstraints.maxFps = Some(60);
    if (aUseRtx) {
      codec->EnableRtx("125");
    }
    return codec;
  }

  static UniquePtr<JsepVideoCodecDescription> CreateDefaultH264_0(
      bool aUseRtx) {
    auto codec = MakeUnique<JsepVideoCodecDescription>("97", "H264", 90000);
    codec->mPacketizationMode = 0;
    // Defaults for mandatory params
    codec->mProfileLevelId = 0x42E01F;
    if (aUseRtx) {
      codec->EnableRtx("98");
    }
    return codec;
  }

  static UniquePtr<JsepVideoCodecDescription> CreateDefaultH264_1(
      bool aUseRtx) {
    auto codec = MakeUnique<JsepVideoCodecDescription>("126", "H264", 90000);
    codec->mPacketizationMode = 1;
    // Defaults for mandatory params
    codec->mProfileLevelId = 0x42E01F;
    if (aUseRtx) {
      codec->EnableRtx("127");
    }
    return codec;
  }

  static UniquePtr<JsepVideoCodecDescription> CreateDefaultUlpFec() {
    return MakeUnique<JsepVideoCodecDescription>(
        "123",     // payload type
        "ulpfec",  // codec name
        90000      // clock rate (match other video codecs)
    );
  }

  static UniquePtr<JsepVideoCodecDescription> CreateDefaultRed() {
    auto codec = MakeUnique<JsepVideoCodecDescription>(
        "122",  // payload type
        "red",  // codec name
        90000   // clock rate (match other video codecs)
    );
    codec->EnableRtx("119");
    return codec;
  }

  void ApplyConfigToFmtp(
      UniquePtr<SdpFmtpAttributeList::Parameters>& aFmtp) const override {
    if (mName == "H264") {
      SdpFmtpAttributeList::H264Parameters h264Params;
      if (aFmtp) {
        MOZ_RELEASE_ASSERT(aFmtp->codec_type == SdpRtpmapAttributeList::kH264);
        h264Params =
            static_cast<const SdpFmtpAttributeList::H264Parameters&>(*aFmtp);
      }

      if (mDirection == sdp::kSend) {
        if (!h264Params.level_asymmetry_allowed) {
          // First time the fmtp has been set; set just in case this is for a
          // sendonly m-line, since even though we aren't receiving the level
          // negotiation still needs to happen (sigh).
          h264Params.profile_level_id = mProfileLevelId;
        }
      } else {
        // Parameters that only apply to what we receive
        h264Params.max_mbps = mConstraints.maxMbps;
        h264Params.max_fs = mConstraints.maxFs;
        h264Params.max_cpb = mConstraints.maxCpb;
        h264Params.max_dpb = mConstraints.maxDpb;
        h264Params.max_br = mConstraints.maxBr;
        strncpy(h264Params.sprop_parameter_sets, mSpropParameterSets.c_str(),
                sizeof(h264Params.sprop_parameter_sets) - 1);
        h264Params.profile_level_id = mProfileLevelId;
      }

      // Parameters that apply to both the send and recv directions
      h264Params.packetization_mode = mPacketizationMode;
      // Hard-coded, may need to change someday?
      h264Params.level_asymmetry_allowed = true;

      // Parameters that apply to both the send and recv directions
      h264Params.packetization_mode = mPacketizationMode;
      // Hard-coded, may need to change someday?
      h264Params.level_asymmetry_allowed = true;
      aFmtp.reset(h264Params.Clone());
    } else if (mName == "VP8" || mName == "VP9") {
      SdpRtpmapAttributeList::CodecType type =
          mName == "VP8" ? SdpRtpmapAttributeList::CodecType::kVP8
                         : SdpRtpmapAttributeList::CodecType::kVP9;
      auto vp8Params = SdpFmtpAttributeList::VP8Parameters(type);

      if (aFmtp) {
        MOZ_RELEASE_ASSERT(aFmtp->codec_type == type);
        vp8Params =
            static_cast<const SdpFmtpAttributeList::VP8Parameters&>(*aFmtp);
      }
      // VP8 and VP9 share the same SDP parameters thus far
      vp8Params.max_fs = mConstraints.maxFs;
      if (mConstraints.maxFps.isSome()) {
        vp8Params.max_fr =
            static_cast<unsigned int>(std::round(*mConstraints.maxFps));
      } else {
        vp8Params.max_fr = 60;
      }
      aFmtp.reset(vp8Params.Clone());
    }
  }

  virtual void EnableTmmbr() {
    // EnableTmmbr can be called multiple times due to multiple calls to
    // PeerConnectionImpl::ConfigureJsepSessionCodecs
    if (!mTmmbrEnabled) {
      mTmmbrEnabled = true;
      mCcmFbTypes.push_back(SdpRtcpFbAttributeList::tmmbr);
    }
  }

  virtual void EnableRemb() {
    // EnableRemb can be called multiple times due to multiple calls to
    // PeerConnectionImpl::ConfigureJsepSessionCodecs
    if (!mRembEnabled) {
      mRembEnabled = true;
      mOtherFbTypes.push_back({"", SdpRtcpFbAttributeList::kRemb, "", ""});
    }
  }

  virtual void EnableFec(std::string redPayloadType,
                         std::string ulpfecPayloadType,
                         std::string redRtxPayloadType) {
    // Enabling FEC for video works a little differently than enabling
    // REMB or TMMBR.  Support for FEC is indicated by the presence of
    // particular codes (red and ulpfec) instead of using rtcpfb
    // attributes on a given codec.  There is no rtcpfb to push for FEC
    // as can be seen above when REMB or TMMBR are enabled.

    // Ensure we have valid payload types. This returns zero on failure, which
    // is a valid payload type.
    uint16_t redPt, ulpfecPt, redRtxPt;
    if (!SdpHelper::GetPtAsInt(redPayloadType, &redPt) ||
        !SdpHelper::GetPtAsInt(ulpfecPayloadType, &ulpfecPt) ||
        !SdpHelper::GetPtAsInt(redRtxPayloadType, &redRtxPt)) {
      return;
    }

    mFECEnabled = true;
    mREDPayloadType = redPayloadType;
    mULPFECPayloadType = ulpfecPayloadType;
    mREDRTXPayloadType = redRtxPayloadType;
  }

  virtual void EnableTransportCC() {
    if (!mTransportCCEnabled) {
      mTransportCCEnabled = true;
      mOtherFbTypes.push_back(
          {"", SdpRtcpFbAttributeList::kTransportCC, "", ""});
    }
  }

  void EnableRtx(const std::string& rtxPayloadType) {
    mRtxEnabled = true;
    mRtxPayloadType = rtxPayloadType;
  }

  void AddParametersToMSection(SdpMediaSection& msection) const override {
    AddFmtpsToMSection(msection);
    AddRtcpFbsToMSection(msection);
  }

  void AddFmtpsToMSection(SdpMediaSection& msection) const {
    if (mName == "H264") {
      UniquePtr<SdpFmtpAttributeList::Parameters> h264Params =
          MakeUnique<SdpFmtpAttributeList::H264Parameters>(
              GetH264Parameters(mDefaultPt, msection));

      ApplyConfigToFmtp(h264Params);

      msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, *h264Params));
    } else if (mName == "VP8" || mName == "VP9") {
      if (mDirection == sdp::kRecv) {
        // VP8 and VP9 share the same SDP parameters thus far
        UniquePtr<SdpFmtpAttributeList::Parameters> vp8Params =
            MakeUnique<SdpFmtpAttributeList::VP8Parameters>(
                GetVP8Parameters(mDefaultPt, msection));
        ApplyConfigToFmtp(vp8Params);
        msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mDefaultPt, *vp8Params));
      }
    }

    if (mRtxEnabled && mDirection == sdp::kRecv) {
      SdpFmtpAttributeList::RtxParameters params(
          GetRtxParameters(mDefaultPt, msection));
      uint16_t apt;
      if (SdpHelper::GetPtAsInt(mDefaultPt, &apt)) {
        if (apt <= 127) {
          msection.AddCodec(mRtxPayloadType, "rtx", mClock, mChannels);

          params.apt = apt;
          msection.SetFmtp(SdpFmtpAttributeList::Fmtp(mRtxPayloadType, params));
        }
      }
    }
  }

  void AddRtcpFbsToMSection(SdpMediaSection& msection) const {
    SdpRtcpFbAttributeList rtcpfbs(msection.GetRtcpFbs());
    for (const auto& rtcpfb : rtcpfbs.mFeedbacks) {
      if (rtcpfb.pt == mDefaultPt) {
        // Already set by the codec for the other direction.
        return;
      }
    }

    for (const std::string& type : mAckFbTypes) {
      rtcpfbs.PushEntry(mDefaultPt, SdpRtcpFbAttributeList::kAck, type);
    }
    for (const std::string& type : mNackFbTypes) {
      rtcpfbs.PushEntry(mDefaultPt, SdpRtcpFbAttributeList::kNack, type);
    }
    for (const std::string& type : mCcmFbTypes) {
      rtcpfbs.PushEntry(mDefaultPt, SdpRtcpFbAttributeList::kCcm, type);
    }
    for (const auto& fb : mOtherFbTypes) {
      rtcpfbs.PushEntry(mDefaultPt, fb.type, fb.parameter, fb.extra);
    }

    msection.SetRtcpFbs(rtcpfbs);
  }

  SdpFmtpAttributeList::H264Parameters GetH264Parameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    // Will contain defaults if nothing else
    SdpFmtpAttributeList::H264Parameters result;
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == SdpRtpmapAttributeList::kH264) {
      result =
          static_cast<const SdpFmtpAttributeList::H264Parameters&>(*params);
    }

    return result;
  }

  SdpFmtpAttributeList::RedParameters GetRedParameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    SdpFmtpAttributeList::RedParameters result;
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == SdpRtpmapAttributeList::kRed) {
      result = static_cast<const SdpFmtpAttributeList::RedParameters&>(*params);
    }

    return result;
  }

  SdpFmtpAttributeList::RtxParameters GetRtxParameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    SdpFmtpAttributeList::RtxParameters result;
    const auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == SdpRtpmapAttributeList::kRtx) {
      result = static_cast<const SdpFmtpAttributeList::RtxParameters&>(*params);
    }

    return result;
  }

  Maybe<std::string> GetRtxPtByApt(const std::string& apt,
                                   const SdpMediaSection& msection) const {
    Maybe<std::string> result;
    uint16_t aptAsInt;
    if (!SdpHelper::GetPtAsInt(apt, &aptAsInt)) {
      return result;
    }

    const SdpAttributeList& attrs = msection.GetAttributeList();
    if (attrs.HasAttribute(SdpAttribute::kFmtpAttribute)) {
      for (const auto& fmtpAttr : attrs.GetFmtp().mFmtps) {
        if (fmtpAttr.parameters) {
          auto* params = fmtpAttr.parameters.get();
          if (params && params->codec_type == SdpRtpmapAttributeList::kRtx) {
            const SdpFmtpAttributeList::RtxParameters* rtxParams =
                static_cast<const SdpFmtpAttributeList::RtxParameters*>(params);
            if (rtxParams->apt == aptAsInt) {
              result = Some(fmtpAttr.format);
              break;
            }
          }
        }
      }
    }
    return result;
  }

  SdpFmtpAttributeList::VP8Parameters GetVP8Parameters(
      const std::string& pt, const SdpMediaSection& msection) const {
    SdpRtpmapAttributeList::CodecType expectedType(
        mName == "VP8" ? SdpRtpmapAttributeList::kVP8
                       : SdpRtpmapAttributeList::kVP9);

    // Will contain defaults if nothing else
    SdpFmtpAttributeList::VP8Parameters result(expectedType);
    auto* params = msection.FindFmtp(pt);

    if (params && params->codec_type == expectedType) {
      result = static_cast<const SdpFmtpAttributeList::VP8Parameters&>(*params);
    }

    return result;
  }

  void NegotiateRtcpFb(const SdpMediaSection& remoteMsection,
                       SdpRtcpFbAttributeList::Type type,
                       std::vector<std::string>* supportedTypes) {
    Maybe<std::string> remoteFmt = GetMatchingFormat(remoteMsection);
    if (!remoteFmt) {
      return;
    }
    std::vector<std::string> temp;
    for (auto& subType : *supportedTypes) {
      if (remoteMsection.HasRtcpFb(*remoteFmt, type, subType)) {
        temp.push_back(subType);
      }
    }
    *supportedTypes = temp;
  }

  void NegotiateRtcpFb(
      const SdpMediaSection& remoteMsection,
      std::vector<SdpRtcpFbAttributeList::Feedback>* supportedFbs) {
    Maybe<std::string> remoteFmt = GetMatchingFormat(remoteMsection);
    if (!remoteFmt) {
      return;
    }
    std::vector<SdpRtcpFbAttributeList::Feedback> temp;
    for (auto& fb : *supportedFbs) {
      if (remoteMsection.HasRtcpFb(*remoteFmt, fb.type, fb.parameter)) {
        temp.push_back(fb);
      }
    }
    *supportedFbs = temp;
  }

  void NegotiateRtcpFb(const SdpMediaSection& remote) {
    // Removes rtcp-fb types that the other side doesn't support
    NegotiateRtcpFb(remote, SdpRtcpFbAttributeList::kAck, &mAckFbTypes);
    NegotiateRtcpFb(remote, SdpRtcpFbAttributeList::kNack, &mNackFbTypes);
    NegotiateRtcpFb(remote, SdpRtcpFbAttributeList::kCcm, &mCcmFbTypes);
    NegotiateRtcpFb(remote, &mOtherFbTypes);
  }

  virtual bool Negotiate(const std::string& pt,
                         const SdpMediaSection& remoteMsection,
                         bool remoteIsOffer,
                         Maybe<const SdpMediaSection&> localMsection) override {
    JsepCodecDescription::Negotiate(pt, remoteMsection, remoteIsOffer,
                                    localMsection);
    if (mName == "H264") {
      SdpFmtpAttributeList::H264Parameters h264Params(
          GetH264Parameters(mDefaultPt, remoteMsection));

      // Level is negotiated symmetrically if level asymmetry is disallowed
      if (!h264Params.level_asymmetry_allowed) {
        SetSaneH264Level(std::min(GetSaneH264Level(h264Params.profile_level_id),
                                  GetSaneH264Level(mProfileLevelId)),
                         &mProfileLevelId);
      }

      if (mDirection == sdp::kSend) {
        // Remote values of these apply only to the send codec.
        mConstraints.maxFs = h264Params.max_fs;
        mConstraints.maxMbps = h264Params.max_mbps;
        mConstraints.maxCpb = h264Params.max_cpb;
        mConstraints.maxDpb = h264Params.max_dpb;
        mConstraints.maxBr = h264Params.max_br;
        mSpropParameterSets = h264Params.sprop_parameter_sets;
        // Only do this if we didn't symmetrically negotiate above
        if (h264Params.level_asymmetry_allowed) {
          SetSaneH264Level(GetSaneH264Level(h264Params.profile_level_id),
                           &mProfileLevelId);
        }
      } else {
        // TODO(bug 1143709): max-recv-level support
      }
    } else if (mName == "VP8" || mName == "VP9") {
      if (mDirection == sdp::kSend) {
        SdpFmtpAttributeList::VP8Parameters vp8Params(
            GetVP8Parameters(mDefaultPt, remoteMsection));

        mConstraints.maxFs = vp8Params.max_fs;
        // Right now, we treat max-fr=0 (or the absence of max-fr) as no limit.
        // We will eventually want to stop doing this (bug 1762600).
        if (vp8Params.max_fr) {
          mConstraints.maxFps = Some(vp8Params.max_fr);
        }
      }
    }

    if (mRtxEnabled && (mDirection == sdp::kSend || remoteIsOffer)) {
      Maybe<std::string> rtxPt = GetRtxPtByApt(mDefaultPt, remoteMsection);
      if (rtxPt.isSome()) {
        EnableRtx(*rtxPt);
      } else {
        mRtxEnabled = false;
        mRtxPayloadType = "";
      }
    }

    NegotiateRtcpFb(remoteMsection);
    return true;
  }

  // Maps the not-so-sane encoding of H264 level into something that is
  // ordered in the way one would expect
  // 1b is 0xAB, everything else is the level left-shifted one half-byte
  // (eg; 1.0 is 0xA0, 1.1 is 0xB0, 3.1 is 0x1F0)
  static uint32_t GetSaneH264Level(uint32_t profileLevelId) {
    uint32_t profileIdc = (profileLevelId >> 16);

    if (profileIdc == 0x42 || profileIdc == 0x4D || profileIdc == 0x58) {
      if ((profileLevelId & 0x10FF) == 0x100B) {
        // Level 1b
        return 0xAB;
      }
    }

    uint32_t level = profileLevelId & 0xFF;

    if (level == 0x09) {
      // Another way to encode level 1b
      return 0xAB;
    }

    return level << 4;
  }

  static void SetSaneH264Level(uint32_t level, uint32_t* profileLevelId) {
    uint32_t profileIdc = (*profileLevelId >> 16);
    uint32_t levelMask = 0xFF;

    if (profileIdc == 0x42 || profileIdc == 0x4d || profileIdc == 0x58) {
      levelMask = 0x10FF;
      if (level == 0xAB) {
        // Level 1b
        level = 0x100B;
      } else {
        // Not 1b, just shift
        level = level >> 4;
      }
    } else if (level == 0xAB) {
      // Another way to encode 1b
      level = 0x09;
    } else {
      // Not 1b, just shift
      level = level >> 4;
    }

    *profileLevelId = (*profileLevelId & ~levelMask) | level;
  }

  enum Subprofile {
    kH264ConstrainedBaseline,
    kH264Baseline,
    kH264Main,
    kH264Extended,
    kH264High,
    kH264High10,
    kH264High42,
    kH264High44,
    kH264High10I,
    kH264High42I,
    kH264High44I,
    kH264CALVC44,
    kH264UnknownSubprofile
  };

  static Subprofile GetSubprofile(uint32_t profileLevelId) {
    // Based on Table 5 from RFC 6184:
    //        Profile     profile_idc        profile-iop
    //                    (hexadecimal)      (binary)

    //        CB          42 (B)             x1xx0000
    //           same as: 4D (M)             1xxx0000
    //           same as: 58 (E)             11xx0000
    //        B           42 (B)             x0xx0000
    //           same as: 58 (E)             10xx0000
    //        M           4D (M)             0x0x0000
    //        E           58                 00xx0000
    //        H           64                 00000000
    //        H10         6E                 00000000
    //        H42         7A                 00000000
    //        H44         F4                 00000000
    //        H10I        6E                 00010000
    //        H42I        7A                 00010000
    //        H44I        F4                 00010000
    //        C44I        2C                 00010000

    if ((profileLevelId & 0xFF4F00) == 0x424000) {
      // 01001111 (mask, 0x4F)
      // x1xx0000 (from table)
      // 01000000 (expected value, 0x40)
      return kH264ConstrainedBaseline;
    }

    if ((profileLevelId & 0xFF8F00) == 0x4D8000) {
      // 10001111 (mask, 0x8F)
      // 1xxx0000 (from table)
      // 10000000 (expected value, 0x80)
      return kH264ConstrainedBaseline;
    }

    if ((profileLevelId & 0xFFCF00) == 0x58C000) {
      // 11001111 (mask, 0xCF)
      // 11xx0000 (from table)
      // 11000000 (expected value, 0xC0)
      return kH264ConstrainedBaseline;
    }

    if ((profileLevelId & 0xFF4F00) == 0x420000) {
      // 01001111 (mask, 0x4F)
      // x0xx0000 (from table)
      // 00000000 (expected value)
      return kH264Baseline;
    }

    if ((profileLevelId & 0xFFCF00) == 0x588000) {
      // 11001111 (mask, 0xCF)
      // 10xx0000 (from table)
      // 10000000 (expected value, 0x80)
      return kH264Baseline;
    }

    if ((profileLevelId & 0xFFAF00) == 0x4D0000) {
      // 10101111 (mask, 0xAF)
      // 0x0x0000 (from table)
      // 00000000 (expected value)
      return kH264Main;
    }

    if ((profileLevelId & 0xFF0000) == 0x580000) {
      // 11001111 (mask, 0xCF)
      // 00xx0000 (from table)
      // 00000000 (expected value)
      return kH264Extended;
    }

    if ((profileLevelId & 0xFFFF00) == 0x640000) {
      return kH264High;
    }

    if ((profileLevelId & 0xFFFF00) == 0x6E0000) {
      return kH264High10;
    }

    if ((profileLevelId & 0xFFFF00) == 0x7A0000) {
      return kH264High42;
    }

    if ((profileLevelId & 0xFFFF00) == 0xF40000) {
      return kH264High44;
    }

    if ((profileLevelId & 0xFFFF00) == 0x6E1000) {
      return kH264High10I;
    }

    if ((profileLevelId & 0xFFFF00) == 0x7A1000) {
      return kH264High42I;
    }

    if ((profileLevelId & 0xFFFF00) == 0xF41000) {
      return kH264High44I;
    }

    if ((profileLevelId & 0xFFFF00) == 0x2C1000) {
      return kH264CALVC44;
    }

    return kH264UnknownSubprofile;
  }

  virtual bool ParametersMatch(
      const std::string& fmt,
      const SdpMediaSection& remoteMsection) const override {
    if (mName == "H264") {
      SdpFmtpAttributeList::H264Parameters h264Params(
          GetH264Parameters(fmt, remoteMsection));

      if (h264Params.packetization_mode != mPacketizationMode) {
        return false;
      }

      if (GetSubprofile(h264Params.profile_level_id) !=
          GetSubprofile(mProfileLevelId)) {
        return false;
      }
    }

    return true;
  }

  virtual bool RtcpFbRembIsSet() const {
    for (const auto& fb : mOtherFbTypes) {
      if (fb.type == SdpRtcpFbAttributeList::kRemb) {
        return true;
      }
    }
    return false;
  }

  virtual bool RtcpFbTransportCCIsSet() const {
    for (const auto& fb : mOtherFbTypes) {
      if (fb.type == SdpRtcpFbAttributeList::kTransportCC) {
        return true;
      }
    }
    return false;
  }

  void EnsureNoDuplicatePayloadTypes(std::set<std::string>& aUsedPts) override {
    JsepCodecDescription::EnsureNoDuplicatePayloadTypes(aUsedPts);
    if (mFECEnabled) {
      mFECEnabled = EnsurePayloadTypeNotDuplicate(aUsedPts, mREDPayloadType) &&
                    EnsurePayloadTypeNotDuplicate(aUsedPts, mULPFECPayloadType);
    }
    if (mRtxEnabled) {
      mRtxEnabled = EnsurePayloadTypeNotDuplicate(aUsedPts, mRtxPayloadType);
    }
  }

  JSEP_CODEC_CLONE(JsepVideoCodecDescription)

  std::vector<std::string> mAckFbTypes;
  std::vector<std::string> mNackFbTypes;
  std::vector<std::string> mCcmFbTypes;
  std::vector<SdpRtcpFbAttributeList::Feedback> mOtherFbTypes;
  bool mTmmbrEnabled;
  bool mRembEnabled;
  bool mFECEnabled;
  bool mTransportCCEnabled;
  bool mRtxEnabled;
  std::string mREDPayloadType;
  std::string mREDRTXPayloadType;
  std::string mULPFECPayloadType;
  std::string mRtxPayloadType;

  // H264-specific stuff
  uint32_t mProfileLevelId;
  uint32_t mPacketizationMode;
  std::string mSpropParameterSets;
};

class JsepApplicationCodecDescription : public JsepCodecDescription {
  // This is the new draft-21 implementation
 public:
  JsepApplicationCodecDescription(const std::string& name, uint16_t channels,
                                  uint16_t localPort,
                                  uint32_t localMaxMessageSize,
                                  bool enabled = true)
      : JsepCodecDescription("", name, 0, channels, enabled),
        mLocalPort(localPort),
        mLocalMaxMessageSize(localMaxMessageSize),
        mRemotePort(0),
        mRemoteMaxMessageSize(0),
        mRemoteMMSSet(false) {}

  static constexpr SdpMediaSection::MediaType type =
      SdpMediaSection::kApplication;

  SdpMediaSection::MediaType Type() const override { return type; }

  JSEP_CODEC_CLONE(JsepApplicationCodecDescription)

  static UniquePtr<JsepApplicationCodecDescription> CreateDefault() {
    return MakeUnique<JsepApplicationCodecDescription>(
        "webrtc-datachannel", WEBRTC_DATACHANNEL_STREAMS_DEFAULT,
        WEBRTC_DATACHANNEL_PORT_DEFAULT,
        WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_LOCAL);
  }

  // Override, uses sctpport or sctpmap instead of rtpmap
  virtual bool Matches(const std::string& fmt,
                       const SdpMediaSection& remoteMsection) const override {
    if (type != remoteMsection.GetMediaType()) {
      return false;
    }

    int sctp_port = remoteMsection.GetSctpPort();
    bool fmt_matches =
        nsCRT::strcasecmp(mName.c_str(),
                          remoteMsection.GetFormats()[0].c_str()) == 0;
    if (sctp_port && fmt_matches) {
      // New sctp draft 21 format
      return true;
    }

    const SdpSctpmapAttributeList::Sctpmap* sctp_map(
        remoteMsection.GetSctpmap());
    if (sctp_map) {
      // Old sctp draft 05 format
      return nsCRT::strcasecmp(mName.c_str(), sctp_map->name.c_str()) == 0;
    }

    return false;
  }

  virtual void AddToMediaSection(SdpMediaSection& msection) const override {
    if (mEnabled && msection.GetMediaType() == type) {
      if (mDirection == sdp::kRecv) {
        msection.AddDataChannel(mName, mLocalPort, mChannels,
                                mLocalMaxMessageSize);
      }

      AddParametersToMSection(msection);
    }
  }

  bool Negotiate(const std::string& pt, const SdpMediaSection& remoteMsection,
                 bool remoteIsOffer,
                 Maybe<const SdpMediaSection&> localMsection) override {
    JsepCodecDescription::Negotiate(pt, remoteMsection, remoteIsOffer,
                                    localMsection);

    uint32_t message_size;
    mRemoteMMSSet = remoteMsection.GetMaxMessageSize(&message_size);
    if (mRemoteMMSSet) {
      mRemoteMaxMessageSize = message_size;
    } else {
      mRemoteMaxMessageSize =
          WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE_DEFAULT;
    }

    int sctp_port = remoteMsection.GetSctpPort();
    if (sctp_port) {
      mRemotePort = sctp_port;
      return true;
    }

    const SdpSctpmapAttributeList::Sctpmap* sctp_map(
        remoteMsection.GetSctpmap());
    if (sctp_map) {
      mRemotePort = std::stoi(sctp_map->pt);
      return true;
    }

    return false;
  }

  // We only support one datachannel per m-section
  void EnsureNoDuplicatePayloadTypes(std::set<std::string>& aUsedPts) override {
  }

  void ApplyConfigToFmtp(
      UniquePtr<SdpFmtpAttributeList::Parameters>& aFmtp) const override{};

  uint16_t mLocalPort;
  uint32_t mLocalMaxMessageSize;
  uint16_t mRemotePort;
  uint32_t mRemoteMaxMessageSize;
  bool mRemoteMMSSet;
};

}  // namespace mozilla

#endif
