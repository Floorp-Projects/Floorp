/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPSESSIONIMPL_H_
#define _JSEPSESSIONIMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "jsep/JsepCodecDescription.h"
#include "jsep/JsepSession.h"
#include "jsep/JsepTrack.h"
#include "jsep/JsepTransceiver.h"
#include "jsep/SsrcGenerator.h"
#include "sdp/HybridSdpParser.h"
#include "sdp/SdpHelper.h"

namespace mozilla {

// JsepSessionImpl members that have default copy c'tors, to simplify the
// implementation of the copy c'tor for JsepSessionImpl
class JsepSessionCopyableStuff {
 protected:
  struct JsepDtlsFingerprint {
    std::string mAlgorithm;
    std::vector<uint8_t> mValue;
  };

  Maybe<bool> mIsPendingOfferer;
  Maybe<bool> mIsCurrentOfferer;
  bool mIceControlling = false;
  std::string mIceUfrag;
  std::string mIcePwd;
  std::string mOldIceUfrag;
  std::string mOldIcePwd;
  bool mRemoteIsIceLite = false;
  std::vector<std::string> mIceOptions;
  JsepBundlePolicy mBundlePolicy = kBundleBalanced;
  std::vector<JsepDtlsFingerprint> mDtlsFingerprints;
  uint64_t mSessionId = 0;
  uint64_t mSessionVersion = 0;
  size_t mMidCounter = 0;
  std::set<std::string> mUsedMids;
  size_t mTransportIdCounter = 0;
  std::vector<JsepExtmapMediaType> mRtpExtensions;
  std::set<uint16_t> mExtmapEntriesEverUsed;
  std::string mDefaultRemoteStreamId;
  std::string mCNAME;
  // Used to prevent duplicate local SSRCs. Not used to prevent local/remote or
  // remote-only duplication, which will be important for EKT but not now.
  std::set<uint32_t> mSsrcs;
  std::string mLastError;
  std::vector<std::pair<size_t, std::string>> mLastSdpParsingErrors;
  bool mEncodeTrackId = true;
  SsrcGenerator mSsrcGenerator;
};

class JsepSessionImpl : public JsepSession, public JsepSessionCopyableStuff {
 public:
  JsepSessionImpl(const std::string& name, UniquePtr<JsepUuidGenerator> uuidgen)
      : JsepSession(name),
        mUuidGen(std::move(uuidgen)),
        mSdpHelper(&mLastError),
        mParser(new HybridSdpParser()) {}

  JsepSessionImpl(const JsepSessionImpl& aOrig);

  JsepSession* Clone() const override { return new JsepSessionImpl(*this); }

  // Implement JsepSession methods.
  virtual nsresult Init() override;

  nsresult SetBundlePolicy(JsepBundlePolicy policy) override;

  virtual bool RemoteIsIceLite() const override { return mRemoteIsIceLite; }

  virtual std::vector<std::string> GetIceOptions() const override {
    return mIceOptions;
  }

  virtual nsresult AddDtlsFingerprint(
      const std::string& algorithm, const std::vector<uint8_t>& value) override;

  nsresult AddRtpExtension(JsepMediaType mediaType,
                           const std::string& extensionName,
                           SdpDirectionAttribute::Direction direction);
  virtual nsresult AddAudioRtpExtension(
      const std::string& extensionName,
      SdpDirectionAttribute::Direction direction =
          SdpDirectionAttribute::Direction::kSendrecv) override;

  virtual nsresult AddVideoRtpExtension(
      const std::string& extensionName,
      SdpDirectionAttribute::Direction direction =
          SdpDirectionAttribute::Direction::kSendrecv) override;

  virtual nsresult AddAudioVideoRtpExtension(
      const std::string& extensionName,
      SdpDirectionAttribute::Direction direction =
          SdpDirectionAttribute::Direction::kSendrecv) override;

  virtual std::vector<UniquePtr<JsepCodecDescription>>& Codecs() override {
    return mSupportedCodecs;
  }

  virtual Result CreateOffer(const JsepOfferOptions& options,
                             std::string* offer) override;

  virtual Result CreateAnswer(const JsepAnswerOptions& options,
                              std::string* answer) override;

  virtual std::string GetLocalDescription(
      JsepDescriptionPendingOrCurrent type) const override;

  virtual std::string GetRemoteDescription(
      JsepDescriptionPendingOrCurrent type) const override;

  virtual Result SetLocalDescription(JsepSdpType type,
                                     const std::string& sdp) override;

  virtual Result SetRemoteDescription(JsepSdpType type,
                                      const std::string& sdp) override;

  virtual Result AddRemoteIceCandidate(const std::string& candidate,
                                       const std::string& mid,
                                       const Maybe<uint16_t>& level,
                                       const std::string& ufrag,
                                       std::string* transportId) override;

  virtual nsresult AddLocalIceCandidate(const std::string& candidate,
                                        const std::string& transportId,
                                        const std::string& ufrag,
                                        uint16_t* level, std::string* mid,
                                        bool* skipped) override;

  virtual nsresult UpdateDefaultCandidate(
      const std::string& defaultCandidateAddr, uint16_t defaultCandidatePort,
      const std::string& defaultRtcpCandidateAddr,
      uint16_t defaultRtcpCandidatePort,
      const std::string& transportId) override;

  virtual nsresult Close() override;

  virtual const std::string GetLastError() const override;

  virtual const std::vector<std::pair<size_t, std::string>>&
  GetLastSdpParsingErrors() const override;

  virtual bool IsIceControlling() const override { return mIceControlling; }

  virtual Maybe<bool> IsPendingOfferer() const override {
    return mIsPendingOfferer;
  }

  virtual Maybe<bool> IsCurrentOfferer() const override {
    return mIsCurrentOfferer;
  }

  virtual bool IsIceRestarting() const override {
    return !mOldIceUfrag.empty();
  }

  virtual std::set<std::pair<std::string, std::string>> GetLocalIceCredentials()
      const override;

  virtual const std::vector<RefPtr<JsepTransceiver>>& GetTransceivers()
      const override {
    return mTransceivers;
  }

  virtual std::vector<RefPtr<JsepTransceiver>>& GetTransceivers() override {
    return mTransceivers;
  }

  virtual nsresult AddTransceiver(RefPtr<JsepTransceiver> transceiver) override;

  virtual bool CheckNegotiationNeeded() const override;

 private:
  // Non-const so it can set mLastError
  nsresult CreateGenericSDP(UniquePtr<Sdp>* sdp);
  void AddExtmap(SdpMediaSection* msection);
  std::vector<SdpExtmapAttributeList::Extmap> GetRtpExtensions(
      const SdpMediaSection& msection);
  std::string GetNewMid();

  void AddCommonExtmaps(const SdpMediaSection& remoteMsection,
                        SdpMediaSection* msection);
  uint16_t GetNeverUsedExtmapEntry();
  nsresult SetupIds();
  void SetupDefaultCodecs();
  void SetupDefaultRtpExtensions();
  void SetState(JsepSignalingState state);
  // Non-const so it can set mLastError
  nsresult ParseSdp(const std::string& sdp, UniquePtr<Sdp>* parsedp);
  nsresult SetLocalDescriptionOffer(UniquePtr<Sdp> offer);
  nsresult SetLocalDescriptionAnswer(JsepSdpType type, UniquePtr<Sdp> answer);
  nsresult SetRemoteDescriptionOffer(UniquePtr<Sdp> offer);
  nsresult SetRemoteDescriptionAnswer(JsepSdpType type, UniquePtr<Sdp> answer);
  nsresult ValidateLocalDescription(const Sdp& description, JsepSdpType type);
  nsresult ValidateRemoteDescription(const Sdp& description);
  nsresult ValidateOffer(const Sdp& offer);
  nsresult ValidateAnswer(const Sdp& offer, const Sdp& answer);
  nsresult UpdateTransceiversFromRemoteDescription(const Sdp& remote);
  JsepTransceiver* GetTransceiverForLevel(size_t level) const;
  JsepTransceiver* GetTransceiverForMid(const std::string& mid) const;
  JsepTransceiver* GetTransceiverForLocal(size_t level);
  JsepTransceiver* GetTransceiverForRemote(const SdpMediaSection& msection);
  JsepTransceiver* GetTransceiverWithTransport(
      const std::string& transportId) const;
  // The w3c and IETF specs have a lot of "magical" behavior that happens when
  // addTrack is used. This was a deliberate design choice. Sadface.
  JsepTransceiver* FindUnassociatedTransceiver(SdpMediaSection::MediaType type,
                                               bool magic);
  // Called for rollback of local description
  void RollbackLocalOffer();
  // Called for rollback of remote description
  void RollbackRemoteOffer();
  nsresult HandleNegotiatedSession(const UniquePtr<Sdp>& local,
                                   const UniquePtr<Sdp>& remote);
  nsresult AddTransportAttributes(SdpMediaSection* msection,
                                  SdpSetupAttribute::Role dtlsRole);
  nsresult CopyPreviousTransportParams(const Sdp& oldAnswer,
                                       const Sdp& offerersPreviousSdp,
                                       const Sdp& newOffer, Sdp* newLocal);
  void EnsureMsid(Sdp* remote);
  void SetupBundle(Sdp* sdp) const;
  nsresult CreateOfferMsection(const JsepOfferOptions& options,
                               JsepTransceiver& transceiver, Sdp* local);
  nsresult CreateAnswerMsection(const JsepAnswerOptions& options,
                                JsepTransceiver& transceiver,
                                const SdpMediaSection& remoteMsection,
                                Sdp* sdp);
  nsresult DetermineAnswererSetupRole(const SdpMediaSection& remoteMsection,
                                      SdpSetupAttribute::Role* rolep);
  nsresult MakeNegotiatedTransceiver(const SdpMediaSection& remote,
                                     const SdpMediaSection& local,
                                     JsepTransceiver* transceiverOut);
  void EnsureHasOwnTransport(const SdpMediaSection& msection,
                             JsepTransceiver* transceiver);
  void CopyBundleTransports();

  nsresult FinalizeTransport(const SdpAttributeList& remote,
                             const SdpAttributeList& answer,
                             JsepTransport* transport);

  nsresult GetNegotiatedBundledMids(SdpHelper::BundledMids* bundledMids);

  nsresult EnableOfferMsection(SdpMediaSection* msection);

  mozilla::Sdp* GetParsedLocalDescription(
      JsepDescriptionPendingOrCurrent type) const;
  mozilla::Sdp* GetParsedRemoteDescription(
      JsepDescriptionPendingOrCurrent type) const;
  const Sdp* GetAnswer() const;
  void SetIceRestarting(bool restarting);

  // !!!NOT INDEXED BY LEVEL!!! The level mapping is done with
  // JsepTransceiver::mLevel. The keys are UUIDs.
  std::vector<RefPtr<JsepTransceiver>> mTransceivers;
  // So we can rollback. Not as simple as just going back to the old, though...
  std::vector<RefPtr<JsepTransceiver>> mOldTransceivers;

  UniquePtr<JsepUuidGenerator> mUuidGen;
  UniquePtr<Sdp> mGeneratedOffer;   // Created but not set.
  UniquePtr<Sdp> mGeneratedAnswer;  // Created but not set.
  UniquePtr<Sdp> mCurrentLocalDescription;
  UniquePtr<Sdp> mCurrentRemoteDescription;
  UniquePtr<Sdp> mPendingLocalDescription;
  UniquePtr<Sdp> mPendingRemoteDescription;
  std::vector<UniquePtr<JsepCodecDescription>> mSupportedCodecs;
  SdpHelper mSdpHelper;
  UniquePtr<SdpParser> mParser;
};

}  // namespace mozilla

#endif
