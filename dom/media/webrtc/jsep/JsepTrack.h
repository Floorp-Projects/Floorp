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

  void TruncateEncodings(size_t aSize) {
    if (mEncodings.size() < aSize) {
      MOZ_ASSERT(false);
      return;
    }
    mEncodings.resize(aSize);
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

  void RecvTrackSetRemote(const Sdp& aSdp, const SdpMediaSection& aMsection);
  void RecvTrackSetLocal(const SdpMediaSection& aMsection);

  // This is called whenever a remote description is set; we do not wait for
  // offer/answer to complete, since there's nothing to actually negotiate here.
  void SendTrackSetRemote(SsrcGenerator& aSsrcGenerator,
                          const SdpMediaSection& aRemoteMsection);

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
      mRids = rhs.mRids;
      mSsrcs = rhs.mSsrcs;
      mSsrcToRtxSsrc = rhs.mSsrcToRtxSsrc;
      mActive = rhs.mActive;
      mRemoteSetSendBit = rhs.mRemoteSetSendBit;
      mReceptive = rhs.mReceptive;
      mMaxEncodings = rhs.mMaxEncodings;
      mInHaveRemote = rhs.mInHaveRemote;
      mRtxIsAllowed = rhs.mRtxIsAllowed;

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
        Preferences::GetBool("media.peerconnection.video.use_rtx", false) &&
        !mSsrcToRtxSsrc.empty()) {
      MOZ_ASSERT(mSsrcToRtxSsrc.size() == mSsrcs.size());
      for (const auto ssrc : mSsrcs) {
        auto it = mSsrcToRtxSsrc.find(ssrc);
        MOZ_ASSERT(it != mSsrcToRtxSsrc.end());
        result.push_back(it->second);
      }
    }
    return result;
  }

  virtual void EnsureSsrcs(SsrcGenerator& ssrcGenerator, size_t aNumber);

  bool GetActive() const { return mActive; }

  void SetActive(bool active) { mActive = active; }

  bool GetRemoteSetSendBit() const { return mRemoteSetSendBit; }
  bool GetReceptive() const { return mReceptive; }

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

  virtual nsresult Negotiate(const SdpMediaSection& answer,
                             const SdpMediaSection& remote,
                             const SdpMediaSection& local);
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

  void SetRids(const std::vector<std::string>& aRids);
  void ClearRids() { mRids.clear(); }
  const std::vector<std::string>& GetRids() const { return mRids; }

  void AddToMsection(const std::vector<std::string>& aRids,
                     sdp::Direction direction, SsrcGenerator& ssrcGenerator,
                     bool rtxEnabled, SdpMediaSection* msection);

  // See Bug 1642419, this can be removed when all sites are working with RTX.
  void SetRtxIsAllowed(bool aRtxIsAllowed) { mRtxIsAllowed = aRtxIsAllowed; }

  void SetMaxEncodings(size_t aMax);
  bool IsInHaveRemote() const { return mInHaveRemote; }

 private:
  std::vector<UniquePtr<JsepCodecDescription>> GetCodecClones() const;
  static void EnsureNoDuplicatePayloadTypes(
      std::vector<UniquePtr<JsepCodecDescription>>* codecs);
  static void GetPayloadTypes(
      const std::vector<UniquePtr<JsepCodecDescription>>& codecs,
      std::vector<uint16_t>* pts);
  void AddToMsection(const std::vector<UniquePtr<JsepCodecDescription>>& codecs,
                     SdpMediaSection* msection) const;
  void GetRids(const SdpMediaSection& msection, sdp::Direction direction,
               std::vector<SdpRidAttributeList::Rid>* rids) const;
  void CreateEncodings(
      const SdpMediaSection& remote,
      const std::vector<UniquePtr<JsepCodecDescription>>& negotiatedCodecs,
      JsepTrackNegotiatedDetails* details);

  virtual std::vector<UniquePtr<JsepCodecDescription>> NegotiateCodecs(
      const SdpMediaSection& remote, bool remoteIsOffer,
      Maybe<const SdpMediaSection&> local);

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
  // List of rids. May be initially populated from JS, or from a remote SDP.
  // Can be updated by remote SDP. If no negotiation has taken place at all,
  // this will be empty. If negotiation has taken place, but no simulcast
  // attr was negotiated, this will contain the empty string as a single
  // element. If a simulcast attribute was negotiated, this will contain the
  // negotiated rids.
  std::vector<std::string> mRids;
  UniquePtr<JsepTrackNegotiatedDetails> mNegotiatedDetails;
  std::vector<uint32_t> mSsrcs;
  std::map<uint32_t, uint32_t> mSsrcToRtxSsrc;
  bool mActive;
  bool mRemoteSetSendBit;
  // This is used to drive RTCRtpTransceiver.[[Receptive]]. Basically, this
  // denotes whether we are prepared to receive RTP. When we apply a local
  // description with the recv bit set, this is set to true, even if we have
  // not seen the remote description yet. If we apply either a local or remote
  // description without the recv bit set (from our perspective), this is set
  // to false.
  bool mReceptive = false;
  size_t mMaxEncodings = 3;
  bool mInHaveRemote = false;

  // See Bug 1642419, this can be removed when all sites are working with RTX.
  bool mRtxIsAllowed = true;
};

}  // namespace mozilla

#endif
