/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsep/JsepTrack.h"
#include "jsep/JsepCodecDescription.h"
#include "jsep/JsepTrackEncoding.h"

#include <algorithm>

namespace mozilla {
void JsepTrack::GetNegotiatedPayloadTypes(
    std::vector<uint16_t>* payloadTypes) const {
  if (!mNegotiatedDetails) {
    return;
  }

  for (const auto& encoding : mNegotiatedDetails->mEncodings) {
    GetPayloadTypes(encoding->GetCodecs(), payloadTypes);
  }

  // Prune out dupes
  std::sort(payloadTypes->begin(), payloadTypes->end());
  auto newEnd = std::unique(payloadTypes->begin(), payloadTypes->end());
  payloadTypes->erase(newEnd, payloadTypes->end());
}

/* static */
void JsepTrack::GetPayloadTypes(
    const std::vector<UniquePtr<JsepCodecDescription>>& codecs,
    std::vector<uint16_t>* payloadTypes) {
  for (const auto& codec : codecs) {
    uint16_t pt;
    if (!codec->GetPtAsInt(&pt)) {
      MOZ_ASSERT(false);
      continue;
    }
    payloadTypes->push_back(pt);
  }
}

void JsepTrack::EnsureNoDuplicatePayloadTypes(
    std::vector<UniquePtr<JsepCodecDescription>>* codecs) {
  std::set<std::string> uniquePayloadTypes;
  for (auto& codec : *codecs) {
    codec->EnsureNoDuplicatePayloadTypes(uniquePayloadTypes);
  }
}

void JsepTrack::EnsureSsrcs(SsrcGenerator& ssrcGenerator, size_t aNumber) {
  while (mSsrcs.size() < aNumber) {
    uint32_t ssrc, rtxSsrc;
    if (!ssrcGenerator.GenerateSsrc(&ssrc) ||
        !ssrcGenerator.GenerateSsrc(&rtxSsrc)) {
      return;
    }
    mSsrcs.push_back(ssrc);
    mSsrcToRtxSsrc[ssrc] = rtxSsrc;
    MOZ_ASSERT(mSsrcs.size() == mSsrcToRtxSsrc.size());
  }
}

void JsepTrack::PopulateCodecs(
    const std::vector<UniquePtr<JsepCodecDescription>>& prototype) {
  mPrototypeCodecs.clear();
  for (const auto& prototypeCodec : prototype) {
    if (prototypeCodec->Type() == mType) {
      mPrototypeCodecs.emplace_back(prototypeCodec->Clone());
      mPrototypeCodecs.back()->mDirection = mDirection;
    }
  }

  EnsureNoDuplicatePayloadTypes(&mPrototypeCodecs);
}

void JsepTrack::AddToOffer(SsrcGenerator& ssrcGenerator,
                           SdpMediaSection* offer) {
  AddToMsection(mPrototypeCodecs, offer);

  if (mDirection == sdp::kSend) {
    std::vector<std::string> rids;
    if (offer->IsSending()) {
      rids = mRids;
    }

    AddToMsection(rids, sdp::kSend, ssrcGenerator,
                  IsRtxEnabled(mPrototypeCodecs), offer);
  }
}

void JsepTrack::AddToAnswer(const SdpMediaSection& offer,
                            SsrcGenerator& ssrcGenerator,
                            SdpMediaSection* answer) {
  // We do not modify mPrototypeCodecs here, since we're only creating an
  // answer. Once offer/answer concludes, we will update mPrototypeCodecs.
  std::vector<UniquePtr<JsepCodecDescription>> codecs =
      NegotiateCodecs(offer, true, Nothing());
  if (codecs.empty()) {
    return;
  }

  AddToMsection(codecs, answer);

  if (mDirection == sdp::kSend) {
    AddToMsection(mRids, sdp::kSend, ssrcGenerator, IsRtxEnabled(codecs),
                  answer);
  }
}

void JsepTrack::SetRids(const std::vector<std::string>& aRids) {
  MOZ_ASSERT(!aRids.empty());
  if (!mRids.empty()) {
    return;
  }
  mRids = aRids;
}

void JsepTrack::SetMaxEncodings(size_t aMax) {
  mMaxEncodings = aMax;
  if (mRids.size() > mMaxEncodings) {
    mRids.resize(mMaxEncodings);
  }
}

void JsepTrack::RecvTrackSetRemote(const Sdp& aSdp,
                                   const SdpMediaSection& aMsection) {
  mInHaveRemote = true;
  MOZ_ASSERT(mDirection == sdp::kRecv);
  MOZ_ASSERT(aMsection.GetMediaType() !=
             SdpMediaSection::MediaType::kApplication);
  std::string error;
  SdpHelper helper(&error);

  mRemoteSetSendBit = aMsection.IsSending();
  if (!mRemoteSetSendBit) {
    mReceptive = false;
  }

  if (aMsection.IsSending()) {
    (void)helper.GetIdsFromMsid(aSdp, aMsection, &mStreamIds);
  } else {
    mStreamIds.clear();
  }

  // We do this whether or not the track is active
  SetCNAME(helper.GetCNAME(aMsection));
  mSsrcs.clear();
  if (aMsection.GetAttributeList().HasAttribute(SdpAttribute::kSsrcAttribute)) {
    for (const auto& ssrcAttr : aMsection.GetAttributeList().GetSsrc().mSsrcs) {
      mSsrcs.push_back(ssrcAttr.ssrc);
    }
  }

  // Use FID ssrc-group to associate rtx ssrcs with "regular" ssrcs. Despite
  // not being part of RFC 4588, this is how rtx is negotiated by libwebrtc
  // and jitsi.
  mSsrcToRtxSsrc.clear();
  if (aMsection.GetAttributeList().HasAttribute(
          SdpAttribute::kSsrcGroupAttribute)) {
    for (const auto& group :
         aMsection.GetAttributeList().GetSsrcGroup().mSsrcGroups) {
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

void JsepTrack::RecvTrackSetLocal(const SdpMediaSection& aMsection) {
  MOZ_ASSERT(mDirection == sdp::kRecv);

  // TODO: Should more stuff live in here? Anything that needs to happen when we
  // decide we're ready to receive packets should probably go in here.
  mReceptive = aMsection.IsReceiving();
}

void JsepTrack::SendTrackSetRemote(SsrcGenerator& aSsrcGenerator,
                                   const SdpMediaSection& aRemoteMsection) {
  mInHaveRemote = true;
  if (mType == SdpMediaSection::kApplication) {
    return;
  }

  std::vector<SdpRidAttributeList::Rid> rids;

  // TODO: Current language in webrtc-pc is completely broken, and so I will
  // not be quoting it here.
  if ((mType == SdpMediaSection::kVideo) &&
      aRemoteMsection.GetAttributeList().HasAttribute(
          SdpAttribute::kSimulcastAttribute)) {
    // Note: webrtc-pc does not appear to support the full IETF simulcast
    // spec. In particular, the IETF simulcast spec supports requesting
    // multiple different sets of encodings. For example, "a=simulcast:send
    // 1,2;3,4;5,6" means that there are three simulcast streams, the first of
    // which can use either rid 1 or 2 (but not both), the second of which can
    // use rid 3 or 4 (but not both), and the third of which can use rid 5 or
    // 6 (but not both). webrtc-pc does not support this either/or stuff for
    // rid; each simulcast stream gets exactly one rid.
    // Also, webrtc-pc does not support the '~' pause syntax at all
    // See https://github.com/w3c/webrtc-pc/issues/2769
    GetRids(aRemoteMsection, sdp::kRecv, &rids);
  }

  if (mRids.empty()) {
    // Initial configuration
    for (const auto& ridAttr : rids) {
      // TODO: Spec might change, making a length > 16 invalid SDP.
      std::string dummy;
      if (SdpRidAttributeList::CheckRidValidity(ridAttr.id, &dummy) &&
          ridAttr.id.size() <= SdpRidAttributeList::kMaxRidLength) {
        mRids.push_back(ridAttr.id);
      }
    }
    if (mRids.size() > mMaxEncodings) {
      mRids.resize(mMaxEncodings);
    }
  } else {
    // JSEP is allowed to remove or reorder rids. RTCRtpSender won't pay
    // attention to reordering.
    std::vector<std::string> newRids;
    for (const auto& ridAttr : rids) {
      for (const auto& oldRid : mRids) {
        if (oldRid == ridAttr.id) {
          newRids.push_back(oldRid);
          break;
        }
      }
    }
    mRids = std::move(newRids);
  }

  if (mRids.empty()) {
    mRids.push_back("");
  }

  UpdateSsrcs(aSsrcGenerator, mRids.size());
}

void JsepTrack::AddToMsection(
    const std::vector<UniquePtr<JsepCodecDescription>>& codecs,
    SdpMediaSection* msection) const {
  MOZ_ASSERT(msection->GetMediaType() == mType);
  MOZ_ASSERT(!codecs.empty());

  for (const auto& codec : codecs) {
    codec->AddToMediaSection(*msection);
  }

  if ((mDirection == sdp::kSend) && (mType != SdpMediaSection::kApplication) &&
      msection->IsSending()) {
    if (mStreamIds.empty()) {
      msection->AddMsid("-", mTrackId);
    } else {
      for (const std::string& streamId : mStreamIds) {
        msection->AddMsid(streamId, mTrackId);
      }
    }
  }
}

void JsepTrack::UpdateSsrcs(SsrcGenerator& ssrcGenerator, size_t encodings) {
  MOZ_ASSERT(mDirection == sdp::kSend);
  MOZ_ASSERT(mType != SdpMediaSection::kApplication);
  size_t numSsrcs = std::max<size_t>(encodings, 1U);

  EnsureSsrcs(ssrcGenerator, numSsrcs);
  PruneSsrcs(numSsrcs);
  if (mNegotiatedDetails && mNegotiatedDetails->GetEncodingCount() > numSsrcs) {
    mNegotiatedDetails->TruncateEncodings(numSsrcs);
  }

  MOZ_ASSERT(!mSsrcs.empty());
}

void JsepTrack::PruneSsrcs(size_t aNumSsrcs) {
  mSsrcs.resize(aNumSsrcs);

  // We might have duplicate entries in mSsrcs, so we need to resize first and
  // then remove ummatched rtx ssrcs.
  auto itor = mSsrcToRtxSsrc.begin();
  while (itor != mSsrcToRtxSsrc.end()) {
    if (std::find(mSsrcs.begin(), mSsrcs.end(), itor->first) == mSsrcs.end()) {
      itor = mSsrcToRtxSsrc.erase(itor);
    } else {
      ++itor;
    }
  }
}

bool JsepTrack::IsRtxEnabled(
    const std::vector<UniquePtr<JsepCodecDescription>>& codecs) const {
  for (const auto& codec : codecs) {
    if (codec->Type() == SdpMediaSection::kVideo &&
        static_cast<const JsepVideoCodecDescription*>(codec.get())
            ->mRtxEnabled) {
      return true;
    }
  }

  return false;
}

void JsepTrack::AddToMsection(const std::vector<std::string>& aRids,
                              sdp::Direction direction,
                              SsrcGenerator& ssrcGenerator, bool rtxEnabled,
                              SdpMediaSection* msection) {
  if (aRids.size() > 1) {
    UniquePtr<SdpSimulcastAttribute> simulcast(new SdpSimulcastAttribute);
    UniquePtr<SdpRidAttributeList> ridAttrs(new SdpRidAttributeList);
    for (const std::string& rid : aRids) {
      SdpRidAttributeList::Rid ridAttr;
      ridAttr.id = rid;
      ridAttr.direction = direction;
      ridAttrs->mRids.push_back(ridAttr);

      SdpSimulcastAttribute::Version version;
      version.choices.push_back(SdpSimulcastAttribute::Encoding(rid, false));
      if (direction == sdp::kSend) {
        simulcast->sendVersions.push_back(version);
      } else {
        simulcast->recvVersions.push_back(version);
      }
    }

    msection->GetAttributeList().SetAttribute(simulcast.release());
    msection->GetAttributeList().SetAttribute(ridAttrs.release());
  }

  bool requireRtxSsrcs = rtxEnabled && msection->IsSending();

  if (mType != SdpMediaSection::kApplication && mDirection == sdp::kSend) {
    UpdateSsrcs(ssrcGenerator, aRids.size());

    if (requireRtxSsrcs) {
      MOZ_ASSERT(mSsrcs.size() == mSsrcToRtxSsrc.size());
      std::vector<uint32_t> allSsrcs;
      UniquePtr<SdpSsrcGroupAttributeList> group(new SdpSsrcGroupAttributeList);
      for (const auto& ssrc : mSsrcs) {
        const auto rtxSsrc = mSsrcToRtxSsrc[ssrc];
        allSsrcs.push_back(ssrc);
        allSsrcs.push_back(rtxSsrc);
        group->PushEntry(SdpSsrcGroupAttributeList::kFid, {ssrc, rtxSsrc});
      }
      msection->SetSsrcs(allSsrcs, mCNAME);
      msection->GetAttributeList().SetAttribute(group.release());
    } else {
      msection->SetSsrcs(mSsrcs, mCNAME);
    }
  }
}

void JsepTrack::GetRids(const SdpMediaSection& msection,
                        sdp::Direction direction,
                        std::vector<SdpRidAttributeList::Rid>* rids) const {
  rids->clear();
  if (!msection.GetAttributeList().HasAttribute(
          SdpAttribute::kSimulcastAttribute)) {
    return;
  }

  const SdpSimulcastAttribute& simulcast(
      msection.GetAttributeList().GetSimulcast());

  const SdpSimulcastAttribute::Versions* versions = nullptr;
  switch (direction) {
    case sdp::kSend:
      versions = &simulcast.sendVersions;
      break;
    case sdp::kRecv:
      versions = &simulcast.recvVersions;
      break;
  }

  if (!versions->IsSet()) {
    return;
  }

  // RFC 8853 does not seem to forbid duplicate rids in a simulcast attribute.
  // So, while this is obviously silly, we should be prepared for it and
  // ignore those duplicate rids.
  std::set<std::string> uniqueRids;
  for (const SdpSimulcastAttribute::Version& version : *versions) {
    if (!version.choices.empty() && !uniqueRids.count(version.choices[0].rid)) {
      // We validate that rids are present (and sane) elsewhere.
      rids->push_back(*msection.FindRid(version.choices[0].rid));
      uniqueRids.insert(version.choices[0].rid);
    }
  }
}

void JsepTrack::CreateEncodings(
    const SdpMediaSection& remote,
    const std::vector<UniquePtr<JsepCodecDescription>>& negotiatedCodecs,
    JsepTrackNegotiatedDetails* negotiatedDetails) {
  negotiatedDetails->mTias = remote.GetBandwidth("TIAS");

  webrtc::RtcpMode rtcpMode = webrtc::RtcpMode::kCompound;
  // rtcp-rsize (video only)
  if (remote.GetMediaType() == SdpMediaSection::kVideo &&
      remote.GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpRsizeAttribute)) {
    rtcpMode = webrtc::RtcpMode::kReducedSize;
  }
  negotiatedDetails->mRtpRtcpConf = RtpRtcpConfig(rtcpMode);

  // TODO add support for b=AS if TIAS is not set (bug 976521)

  if (mRids.empty()) {
    mRids.push_back("");
  }

  size_t numEncodings = mRids.size();

  // Drop SSRCs if fewer RIDs were offered than we have encodings
  if (mSsrcs.size() > numEncodings) {
    PruneSsrcs(numEncodings);
  }

  // For each stream make sure we have an encoding, and configure
  // that encoding appropriately.
  for (size_t i = 0; i < numEncodings; ++i) {
    UniquePtr<JsepTrackEncoding> encoding(new JsepTrackEncoding);
    if (mRids.size() > i) {
      encoding->mRid = mRids[i];
    }
    for (const auto& codec : negotiatedCodecs) {
      encoding->AddCodec(*codec);
    }
    negotiatedDetails->mEncodings.push_back(std::move(encoding));
  }
}

std::vector<UniquePtr<JsepCodecDescription>> JsepTrack::GetCodecClones() const {
  std::vector<UniquePtr<JsepCodecDescription>> clones;
  for (const auto& codec : mPrototypeCodecs) {
    clones.emplace_back(codec->Clone());
  }
  return clones;
}

static bool CompareCodec(const UniquePtr<JsepCodecDescription>& lhs,
                         const UniquePtr<JsepCodecDescription>& rhs) {
  return lhs->mStronglyPreferred && !rhs->mStronglyPreferred;
}

void JsepTrack::MaybeStoreCodecToLog(const std::string& codec,
                                     SdpMediaSection::MediaType type) {
  // We are logging ulpfec and red elsewhere and will not log rtx.
  if (!nsCRT::strcasecmp(codec.c_str(), "ulpfec") ||
      !nsCRT::strcasecmp(codec.c_str(), "red") ||
      !nsCRT::strcasecmp(codec.c_str(), "rtx")) {
    return;
  }

  if (type == SdpMediaSection::kVideo) {
    if (nsCRT::strcasestr(codec.c_str(), "fec") && mFecCodec.empty()) {
      mFecCodec = codec;
    } else if (!nsCRT::strcasestr(codec.c_str(), "fec") &&
               mVideoPreferredCodec.empty()) {
      mVideoPreferredCodec = codec;
    }
  } else if (type == SdpMediaSection::kAudio && mAudioPreferredCodec.empty()) {
    mAudioPreferredCodec = codec;
  }
}

std::vector<UniquePtr<JsepCodecDescription>> JsepTrack::NegotiateCodecs(
    const SdpMediaSection& remote, bool remoteIsOffer,
    Maybe<const SdpMediaSection&> local) {
  std::vector<UniquePtr<JsepCodecDescription>> negotiatedCodecs;
  std::vector<UniquePtr<JsepCodecDescription>> newPrototypeCodecs;
  bool onlyRedUlpFec = true;

  // Outer loop establishes the remote side's preference
  for (const std::string& fmt : remote.GetFormats()) {
    // Decide if we want to store this codec for logging.
    const auto* entry = remote.FindRtpmap(fmt);
    if (entry) {
      MaybeStoreCodecToLog(entry->name, remote.GetMediaType());
    }

    for (auto& codec : mPrototypeCodecs) {
      if (!codec || !codec->mEnabled || !codec->Matches(fmt, remote)) {
        continue;
      }

      // First codec of ours that matches. See if we can negotiate it.
      UniquePtr<JsepCodecDescription> clone(codec->Clone());
      if (clone->Negotiate(fmt, remote, remoteIsOffer, local)) {
        // If negotiation succeeded, remember the payload type the other side
        // used for reoffers.
        codec->mDefaultPt = fmt;

        // Remember whether we negotiated rtx and the associated pt for later.
        if (codec->Type() == SdpMediaSection::kVideo) {
          JsepVideoCodecDescription* videoCodec =
              static_cast<JsepVideoCodecDescription*>(codec.get());
          JsepVideoCodecDescription* cloneVideoCodec =
              static_cast<JsepVideoCodecDescription*>(clone.get());
          bool useRtx =
              mRtxIsAllowed &&
              Preferences::GetBool("media.peerconnection.video.use_rtx", false);
          videoCodec->mRtxEnabled = useRtx && cloneVideoCodec->mRtxEnabled;
          videoCodec->mRtxPayloadType = cloneVideoCodec->mRtxPayloadType;
        }

        if (codec->mName != "red" && codec->mName != "ulpfec") {
          onlyRedUlpFec = false;
        }

        // Moves the codec out of mPrototypeCodecs, leaving an empty
        // UniquePtr, so we don't use it again. Also causes successfully
        // negotiated codecs to be placed up front in the future.
        newPrototypeCodecs.emplace_back(std::move(codec));
        negotiatedCodecs.emplace_back(std::move(clone));
        break;
      }
    }
  }

  if (onlyRedUlpFec) {
    // We don't have any codecs we can actually use. Clearing so we don't
    // attempt to create a connection signaling only RED and/or ULPFEC.
    negotiatedCodecs.clear();
  }

  // newPrototypeCodecs contains just the negotiated stuff so far. Add the rest.
  for (auto& codec : mPrototypeCodecs) {
    if (codec) {
      newPrototypeCodecs.emplace_back(std::move(codec));
    }
  }

  // Negotiated stuff is up front, so it will take precedence when ensuring
  // there are no duplicate payload types.
  EnsureNoDuplicatePayloadTypes(&newPrototypeCodecs);
  std::swap(newPrototypeCodecs, mPrototypeCodecs);

  // Find the (potential) red codec and ulpfec codec or telephone-event
  JsepVideoCodecDescription* red = nullptr;
  JsepVideoCodecDescription* ulpfec = nullptr;
  JsepAudioCodecDescription* dtmf = nullptr;
  // We can safely cast here since JsepTrack has a MediaType and only codecs
  // that match that MediaType (kAudio or kVideo) are added.
  for (auto& codec : negotiatedCodecs) {
    if (codec->mName == "red") {
      red = static_cast<JsepVideoCodecDescription*>(codec.get());
    } else if (codec->mName == "ulpfec") {
      ulpfec = static_cast<JsepVideoCodecDescription*>(codec.get());
    } else if (codec->mName == "telephone-event") {
      dtmf = static_cast<JsepAudioCodecDescription*>(codec.get());
    }
  }
  // Video FEC is indicated by the existence of the red and ulpfec
  // codecs and not an attribute on the particular video codec (like in
  // a rtcpfb attr). If we see both red and ulpfec codecs, we enable FEC
  // on all the other codecs.
  if (red && ulpfec) {
    for (auto& codec : negotiatedCodecs) {
      if (codec->mName != "red" && codec->mName != "ulpfec") {
        JsepVideoCodecDescription* videoCodec =
            static_cast<JsepVideoCodecDescription*>(codec.get());
        videoCodec->EnableFec(red->mDefaultPt, ulpfec->mDefaultPt,
                              red->mRtxPayloadType);
      }
    }
  }

  // Dtmf support is indicated by the existence of the telephone-event
  // codec, and not an attribute on the particular audio codec (like in a
  // rtcpfb attr). If we see the telephone-event codec, we enabled dtmf
  // support on all the other audio codecs.
  if (dtmf) {
    for (auto& codec : negotiatedCodecs) {
      JsepAudioCodecDescription* audioCodec =
          static_cast<JsepAudioCodecDescription*>(codec.get());
      audioCodec->mDtmfEnabled = true;
    }
  }

  // Make sure strongly preferred codecs are up front, overriding the remote
  // side's preference.
  std::stable_sort(negotiatedCodecs.begin(), negotiatedCodecs.end(),
                   CompareCodec);

  if (!red) {
    // No red, remove ulpfec
    negotiatedCodecs.erase(
        std::remove_if(negotiatedCodecs.begin(), negotiatedCodecs.end(),
                       [ulpfec](const UniquePtr<JsepCodecDescription>& codec) {
                         return codec.get() == ulpfec;
                       }),
        negotiatedCodecs.end());
    // Make sure there's no dangling ptr here
    ulpfec = nullptr;
  }

  return negotiatedCodecs;
}

nsresult JsepTrack::Negotiate(const SdpMediaSection& answer,
                              const SdpMediaSection& remote,
                              const SdpMediaSection& local) {
  std::vector<UniquePtr<JsepCodecDescription>> negotiatedCodecs =
      NegotiateCodecs(remote, &answer != &remote, SomeRef(local));

  if (negotiatedCodecs.empty()) {
    return NS_ERROR_FAILURE;
  }

  UniquePtr<JsepTrackNegotiatedDetails> negotiatedDetails =
      MakeUnique<JsepTrackNegotiatedDetails>();

  CreateEncodings(remote, negotiatedCodecs, negotiatedDetails.get());

  if (answer.GetAttributeList().HasAttribute(SdpAttribute::kExtmapAttribute)) {
    for (auto& extmapAttr : answer.GetAttributeList().GetExtmap().mExtmaps) {
      SdpDirectionAttribute::Direction direction = extmapAttr.direction;
      if (&remote == &answer) {
        // Answer is remote, we need to flip this.
        direction = reverse(direction);
      }

      if (direction & mDirection) {
        negotiatedDetails->mExtmap[extmapAttr.extensionname] = extmapAttr;
      }
    }
  }

  mInHaveRemote = false;
  mNegotiatedDetails = std::move(negotiatedDetails);
  return NS_OK;
}

// When doing bundle, if all else fails we can try to figure out which m-line a
// given RTP packet belongs to by looking at the payload type field. This only
// works, however, if that payload type appeared in only one m-section.
// We figure that out here.
/* static */
void JsepTrack::SetUniquePayloadTypes(std::vector<JsepTrack*>& tracks) {
  // Maps to track details if no other track contains the payload type,
  // otherwise maps to nullptr.
  std::map<uint16_t, JsepTrackNegotiatedDetails*> payloadTypeToDetailsMap;

  for (JsepTrack* track : tracks) {
    if (track->GetMediaType() == SdpMediaSection::kApplication) {
      continue;
    }

    auto* details = track->GetNegotiatedDetails();
    if (!details) {
      // Can happen if negotiation fails on a track
      continue;
    }

    std::vector<uint16_t> payloadTypesForTrack;
    track->GetNegotiatedPayloadTypes(&payloadTypesForTrack);

    for (uint16_t pt : payloadTypesForTrack) {
      if (payloadTypeToDetailsMap.count(pt)) {
        // Found in more than one track, not unique
        payloadTypeToDetailsMap[pt] = nullptr;
      } else {
        payloadTypeToDetailsMap[pt] = details;
      }
    }
  }

  for (auto ptAndDetails : payloadTypeToDetailsMap) {
    uint16_t uniquePt = ptAndDetails.first;
    MOZ_ASSERT(uniquePt <= UINT8_MAX);
    auto trackDetails = ptAndDetails.second;

    if (trackDetails) {
      trackDetails->mUniquePayloadTypes.push_back(
          static_cast<uint8_t>(uniquePt));
    }
  }
}

}  // namespace mozilla
