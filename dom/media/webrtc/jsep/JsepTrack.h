/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPTRACK_H_
#define _JSEPTRACK_H_

#include <functional>
#include <algorithm>
#include <string>
#include <map>
#include <set>
#include <vector>

#include <mozilla/UniquePtr.h>
#include "mozilla/Preferences.h"
#include "nsError.h"

#include "jsep/JsepTrackEncoding.h"
#include "jsep/SsrcGenerator.h"
#include "sdp/Sdp.h"
#include "sdp/SdpAttribute.h"
#include "sdp/SdpMediaSection.h"
#include "libwebrtcglue/RtpRtcpConfig.h"
namespace mozilla {

class JsepTrackNegotiatedDetails {
 public:
  JsepTrackNegotiatedDetails()
      : mTias(0), mRtpRtcpConf(webrtc::RtcpMode::kCompound) {}

  JsepTrackNegotiatedDetails(const JsepTrackNegotiatedDetails& orig)
      : mExtmap(orig.mExtmap),
        mUniquePayloadTypes(orig.mUniquePayloadTypes),
        mTias(orig.mTias),
        mRtpRtcpConf(orig.mRtpRtcpConf) {
    for (const auto& encoding : orig.mEncodings) {
      mEncodings.emplace_back(new JsepTrackEncoding(*encoding));
    }
  }

  size_t GetEncodingCount() const { return mEncodings.size(); }

  const JsepTrackEncoding& GetEncoding(size_t index) const {
    MOZ_RELEASE_ASSERT(index < mEncodings.size());
    return *mEncodings[index];
  }

  const SdpExtmapAttributeList::Extmap* GetExt(
      const std::string& ext_name) const {
    auto it = mExtmap.find(ext_name);
    if (it != mExtmap.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void ForEachRTPHeaderExtension(
      const std::function<void(const SdpExtmapAttributeList::Extmap& extmap)>&
          fn) const {
    for (auto entry : mExtmap) {
      fn(entry.second);
    }
  }

  std::vector<uint8_t> GetUniquePayloadTypes() const {
    return mUniquePayloadTypes;
  }

  uint32_t GetTias() const { return mTias; }

  RtpRtcpConfig GetRtpRtcpConfig() const { return mRtpRtcpConf; }

 private:
  friend class JsepTrack;

  std::map<std::string, SdpExtmapAttributeList::Extmap> mExtmap;
  std::vector<uint8_t> mUniquePayloadTypes;
  std::vector<UniquePtr<JsepTrackEncoding>> mEncodings;
  uint32_t mTias;  // bits per second
  RtpRtcpConfig mRtpRtcpConf;
};

class JsepTrack {
 public:
  JsepTrack(mozilla::SdpMediaSection::MediaType type, sdp::Direction direction)
      : mType(type),
        mDirection(direction),
        mActive(false),
        mRemoteSetSendBit(false) {}

  virtual ~JsepTrack() {}

  void UpdateStreamIds(const std::vector<std::string>& streamIds) {
    mStreamIds = streamIds;
  }

  void SetTrackId(const std::string& aTrackId) { mTrackId = aTrackId; }

  void ClearStreamIds() { mStreamIds.clear(); }

  void UpdateRecvTrack(const Sdp& sdp, const SdpMediaSection& msection) {
    MOZ_ASSERT(mDirection == sdp::kRecv);
    MOZ_ASSERT(msection.GetMediaType() !=
               SdpMediaSection::MediaType::kApplication);
    std::string error;
    SdpHelper helper(&error);

    mRemoteSetSendBit = msection.IsSending();

    if (msection.IsSending()) {
      (void)helper.GetIdsFromMsid(sdp, msection, &mStreamIds);
    } else {
      mStreamIds.clear();
    }

    // We do this whether or not the track is active
    SetCNAME(helper.GetCNAME(msection));
    mSsrcs.clear();
    if (msection.GetAttributeList().HasAttribute(
            SdpAttribute::kSsrcAttribute)) {
      for (auto& ssrcAttr : msection.GetAttributeList().GetSsrc().mSsrcs) {
        mSsrcs.push_back(ssrcAttr.ssrc);
      }
    }

    // Use FID ssrc-group to associate rtx ssrcs with "regular" ssrcs. Despite
    // not being part of RFC 4588, this is how rtx is negotiated by libwebrtc
    // and jitsi.
    mSsrcToRtxSsrc.clear();
    if (msection.GetAttributeList().HasAttribute(
            SdpAttribute::kSsrcGroupAttribute)) {
      for (const auto& group :
           msection.GetAttributeList().GetSsrcGroup().mSsrcGroups) {
        if (group.semantics == SdpSsrcGroupAttributeList::kFid &&
            group.ssrcs.size() == 2) {
          // Ensure we have a "regular" ssrc for each rtx ssrc.
          if (std::find(mSsrcs.begin(), mSsrcs.end(), group.ssrcs[0]) !=
              mSsrcs.end()) {
            mSsrcToRtxSsrc[group.ssrcs[0]] = group.ssrcs[1];

            // Remove rtx ssrcs from mSsrcs
            auto res = std::remove_if(
                mSsrcs.begin(), mSsrcs.end(),
                [group](uint32_t ssrc) { return ssrc == group.ssrcs[1]; });
            mSsrcs.erase(res, mSsrcs.end());
          }
        }
      }
    }
  }

  JsepTrack(const JsepTrack& orig) { *this = orig; }

  JsepTrack(JsepTrack&& orig) = default;
  JsepTrack& operator=(JsepTrack&& rhs) = default;

  JsepTrack& operator=(const JsepTrack& rhs) {
    if (this != &rhs) {
      mType = rhs.mType;
      mStreamIds = rhs.mStreamIds;
      mTrackId = rhs.mTrackId;
      mCNAME = rhs.mCNAME;
      mDirection = rhs.mDirection;
      mJsEncodeConstraints = rhs.mJsEncodeConstraints;
      mSsrcs = rhs.mSsrcs;
      mSsrcToRtxSsrc = rhs.mSsrcToRtxSsrc;
      mActive = rhs.mActive;
      mRemoteSetSendBit = rhs.mRemoteSetSendBit;

      mPrototypeCodecs.clear();
      for (const auto& codec : rhs.mPrototypeCodecs) {
        mPrototypeCodecs.emplace_back(codec->Clone());
      }
      if (rhs.mNegotiatedDetails) {
        mNegotiatedDetails.reset(
            new JsepTrackNegotiatedDetails(*rhs.mNegotiatedDetails));
      }
    }
    return *this;
  }

  virtual mozilla::SdpMediaSection::MediaType GetMediaType() const {
    return mType;
  }

  virtual const std::vector<std::string>& GetStreamIds() const {
    return mStreamIds;
  }

  virtual const std::string& GetCNAME() const { return mCNAME; }

  virtual void SetCNAME(const std::string& cname) { mCNAME = cname; }

  virtual sdp::Direction GetDirection() const { return mDirection; }

  virtual const std::vector<uint32_t>& GetSsrcs() const { return mSsrcs; }

  virtual std::vector<uint32_t> GetRtxSsrcs() const {
    std::vector<uint32_t> result;
    if (mRtxIsAllowed &&
        Preferences::GetBool("media.peerconnection.video.use_rtx", false)) {
      std::for_each(
          mSsrcToRtxSsrc.begin(), mSsrcToRtxSsrc.end(),
          [&result](const auto& pair) { result.push_back(pair.second); });
    }
    return result;
  }

  virtual void EnsureSsrcs(SsrcGenerator& ssrcGenerator, size_t aNumber);

  bool GetActive() const { return mActive; }

  void SetActive(bool active) { mActive = active; }

  bool GetRemoteSetSendBit() const { return mRemoteSetSendBit; }

  virtual void PopulateCodecs(
      const std::vector<UniquePtr<JsepCodecDescription>>& prototype);

  template <class UnaryFunction>
  void ForEachCodec(UnaryFunction func) {
    std::for_each(mPrototypeCodecs.begin(), mPrototypeCodecs.end(), func);
  }

  template <class BinaryPredicate>
  void SortCodecs(BinaryPredicate sorter) {
    std::stable_sort(mPrototypeCodecs.begin(), mPrototypeCodecs.end(), sorter);
  }

  // These two are non-const because this is where ssrcs are chosen.
  virtual void AddToOffer(SsrcGenerator& ssrcGenerator, SdpMediaSection* offer);
  virtual void AddToAnswer(const SdpMediaSection& offer,
                           SsrcGenerator& ssrcGenerator,
                           SdpMediaSection* answer);

  virtual void Negotiate(const SdpMediaSection& answer,
                         const SdpMediaSection& remote);
  static void SetUniquePayloadTypes(std::vector<JsepTrack*>& tracks);
  virtual void GetNegotiatedPayloadTypes(
      std::vector<uint16_t>* payloadTypes) const;

  // This will be set when negotiation is carried out.
  virtual const JsepTrackNegotiatedDetails* GetNegotiatedDetails() const {
    if (mNegotiatedDetails) {
      return mNegotiatedDetails.get();
    }
    return nullptr;
  }

  virtual JsepTrackNegotiatedDetails* GetNegotiatedDetails() {
    if (mNegotiatedDetails) {
      return mNegotiatedDetails.get();
    }
    return nullptr;
  }

  virtual void ClearNegotiatedDetails() { mNegotiatedDetails.reset(); }

  struct JsConstraints {
    std::string rid;
    bool paused = false;
    EncodingConstraints constraints;
    bool operator==(const JsConstraints& other) const {
      return rid == other.rid && paused == other.paused &&
             constraints == other.constraints;
    }
  };

  // Returns true if the constraints changed.
  bool SetJsConstraints(const std::vector<JsConstraints>& constraintsList);

  void GetJsConstraints(std::vector<JsConstraints>* outConstraintsList) const {
    MOZ_ASSERT(outConstraintsList);
    *outConstraintsList = mJsEncodeConstraints;
  }

  void AddToMsection(const std::vector<JsConstraints>& constraintsList,
                     sdp::Direction direction, SsrcGenerator& ssrcGenerator,
                     bool rtxEnabled, SdpMediaSection* msection);

  // See Bug 1642419, this can be removed when all sites are working with RTX.
  void SetRtxIsAllowed(bool aRtxIsAllowed) { mRtxIsAllowed = aRtxIsAllowed; }

 private:
  std::vector<UniquePtr<JsepCodecDescription>> GetCodecClones() const;
  static void EnsureNoDuplicatePayloadTypes(
      std::vector<UniquePtr<JsepCodecDescription>>* codecs);
  static void GetPayloadTypes(
      const std::vector<UniquePtr<JsepCodecDescription>>& codecs,
      std::vector<uint16_t>* pts);
  void AddToMsection(const std::vector<UniquePtr<JsepCodecDescription>>& codecs,
                     SdpMediaSection* msection);
  void GetRids(
      const SdpMediaSection& msection, sdp::Direction direction,
      std::vector<std::pair<SdpRidAttributeList::Rid, bool>>* rids) const;
  void CreateEncodings(
      const SdpMediaSection& remote,
      const std::vector<UniquePtr<JsepCodecDescription>>& negotiatedCodecs,
      JsepTrackNegotiatedDetails* details);

  virtual std::vector<UniquePtr<JsepCodecDescription>> NegotiateCodecs(
      const SdpMediaSection& remote, bool isOffer);

  JsConstraints* FindConstraints(
      const std::string& rid,
      std::vector<JsConstraints>& constraintsList) const;
  void NegotiateRids(
      const std::vector<std::pair<SdpRidAttributeList::Rid, bool>>& rids,
      std::vector<JsConstraints>* constraints) const;
  void UpdateSsrcs(SsrcGenerator& ssrcGenerator, size_t encodings);
  void PruneSsrcs(size_t aNumSsrcs);
  bool IsRtxEnabled(
      const std::vector<UniquePtr<JsepCodecDescription>>& codecs) const;

  mozilla::SdpMediaSection::MediaType mType;
  // These are the ids that everyone outside of JsepSession care about
  std::vector<std::string> mStreamIds;
  std::string mTrackId;
  std::string mCNAME;
  sdp::Direction mDirection;
  std::vector<UniquePtr<JsepCodecDescription>> mPrototypeCodecs;
  // Holds encoding params/constraints from JS. Simulcast happens when there are
  // multiple of these. If there are none, we assume unconstrained unicast with
  // no rid.
  std::vector<JsConstraints> mJsEncodeConstraints;
  UniquePtr<JsepTrackNegotiatedDetails> mNegotiatedDetails;
  std::vector<uint32_t> mSsrcs;
  std::map<uint32_t, uint32_t> mSsrcToRtxSsrc;
  bool mActive;
  bool mRemoteSetSendBit;

  // See Bug 1642419, this can be removed when all sites are working with RTX.
  bool mRtxIsAllowed = true;
};

}  // namespace mozilla

#endif
