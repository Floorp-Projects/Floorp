/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WEBRTC_GLOBAL_H_
#define _WEBRTC_GLOBAL_H_

#include "WebrtcIPCTraits.h"
#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/RTCDataChannelBinding.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/UniquePtr.h"

typedef mozilla::dom::RTCStatsReportInternal StatsReport;
typedef nsTArray<mozilla::UniquePtr<StatsReport>> RTCReports;
typedef mozilla::dom::Sequence<nsString> WebrtcGlobalLog;

namespace mozilla {
namespace dom {
// Calls aFunction with all public members of aStats.
// Typical usage would have aFunction take a parameter pack.
// To avoid inconsistencies, this should be the only explicit list of the
// public RTCStatscollection members in C++.
template <typename Collection, typename Function>
static auto ForAllPublicRTCStatsCollectionMembers(Collection& aStats,
                                                  Function aFunction) {
  static_assert(std::is_same_v<typename std::remove_const<Collection>::type,
                               RTCStatsCollection>,
                "aStats must be a const or non-const RTCStatsCollection");
  return aFunction(
      aStats.mInboundRtpStreamStats, aStats.mOutboundRtpStreamStats,
      aStats.mRemoteInboundRtpStreamStats, aStats.mRemoteOutboundRtpStreamStats,
      aStats.mMediaSourceStats, aStats.mVideoSourceStats,
      aStats.mPeerConnectionStats, aStats.mRtpContributingSourceStats,
      aStats.mIceCandidatePairStats, aStats.mIceCandidateStats,
      aStats.mTrickledIceCandidateStats, aStats.mDataChannelStats,
      aStats.mCodecStats);
}

// Calls aFunction with all members of aStats, including internal ones.
// Typical usage would have aFunction take a parameter pack.
// To avoid inconsistencies, this should be the only explicit list of the
// internal RTCStatscollection members in C++.
template <typename Collection, typename Function>
static auto ForAllRTCStatsCollectionMembers(Collection& aStats,
                                            Function aFunction) {
  static_assert(std::is_same_v<typename std::remove_const<Collection>::type,
                               RTCStatsCollection>,
                "aStats must be a const or non-const RTCStatsCollection");
  return ForAllPublicRTCStatsCollectionMembers(aStats, [&](auto&... aMember) {
    return aFunction(aMember..., aStats.mRawLocalCandidates,
                     aStats.mRawRemoteCandidates, aStats.mVideoFrameHistories,
                     aStats.mBandwidthEstimations);
  });
}
}  // namespace dom
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::RTCStatsType>
    : public ContiguousEnumSerializer<mozilla::dom::RTCStatsType,
                                      mozilla::dom::RTCStatsType::Codec,
                                      mozilla::dom::RTCStatsType::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::RTCStatsIceCandidatePairState>
    : public ContiguousEnumSerializer<
          mozilla::dom::RTCStatsIceCandidatePairState,
          mozilla::dom::RTCStatsIceCandidatePairState::Frozen,
          mozilla::dom::RTCStatsIceCandidatePairState::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::RTCIceCandidateType>
    : public ContiguousEnumSerializer<
          mozilla::dom::RTCIceCandidateType,
          mozilla::dom::RTCIceCandidateType::Host,
          mozilla::dom::RTCIceCandidateType::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::RTCBundlePolicy>
    : public ContiguousEnumSerializer<
          mozilla::dom::RTCBundlePolicy,
          mozilla::dom::RTCBundlePolicy::Balanced,
          mozilla::dom::RTCBundlePolicy::EndGuard_> {};

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCIceServerInternal, mUrls,
                                  mCredentialProvided, mUserNameProvided);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCConfigurationInternal,
                                  mBundlePolicy, mCertificatesProvided,
                                  mIceServers, mIceTransportPolicy,
                                  mPeerIdentityProvided, mSdpSemantics);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCSdpParsingErrorInternal,
                                  mLineNumber, mError);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCSdpHistoryEntryInternal,
                                  mTimestamp, mIsLocal, mSdp, mErrors);

template <>
struct ParamTraits<mozilla::dom::RTCStatsCollection> {
  static void Write(MessageWriter* aWriter,
                    const mozilla::dom::RTCStatsCollection& aParam) {
    mozilla::dom::ForAllRTCStatsCollectionMembers(
        aParam,
        [&](const auto&... aMember) { WriteParams(aWriter, aMember...); });
  }

  static bool Read(MessageReader* aReader,
                   mozilla::dom::RTCStatsCollection* aResult) {
    return mozilla::dom::ForAllRTCStatsCollectionMembers(
        *aResult,
        [&](auto&... aMember) { return ReadParams(aReader, aMember...); });
  }
};

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCStatsReportInternal, mozilla::dom::RTCStatsCollection,
    mClosed, mSdpHistory, mPcid, mBrowserId, mTimestamp, mCallDurationMs,
    mIceRestarts, mIceRollbacks, mOfferer, mConfiguration);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCStats, mId, mTimestamp,
                                  mType);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCIceCandidatePairStats, mozilla::dom::RTCStats,
    mTransportId, mLocalCandidateId, mPriority, mNominated, mWritable,
    mReadable, mRemoteCandidateId, mSelected, mComponentId, mState, mBytesSent,
    mBytesReceived, mLastPacketSentTimestamp, mLastPacketReceivedTimestamp);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCIceCandidateStats, mozilla::dom::RTCStats, mCandidateType,
    mPriority, mTransportId, mAddress, mRelayProtocol, mPort, mProtocol,
    mProxied);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCReceivedRtpStreamStats, mozilla::dom::RTCRtpStreamStats,
    mPacketsReceived, mPacketsLost, mJitter, mDiscardedPackets,
    mPacketsDiscarded);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCInboundRtpStreamStats,
    mozilla::dom::RTCReceivedRtpStreamStats, mTrackIdentifier, mRemoteId,
    mFramesDecoded, mFramesDropped, mFrameWidth, mFrameHeight, mFramesPerSecond,
    mQpSum, mTotalDecodeTime, mTotalInterFrameDelay,
    mTotalSquaredInterFrameDelay, mLastPacketReceivedTimestamp,
    mHeaderBytesReceived, mFecPacketsReceived, mFecPacketsDiscarded,
    mBytesReceived, mNackCount, mFirCount, mPliCount, mTotalProcessingDelay,
    // Always missing from libwebrtc stats
    // mEstimatedPlayoutTimestamp,
    mFramesReceived, mJitterBufferDelay, mJitterBufferEmittedCount,
    mTotalSamplesReceived, mConcealedSamples, mSilentConcealedSamples,
    mConcealmentEvents, mInsertedSamplesForDeceleration,
    mRemovedSamplesForAcceleration, mAudioLevel, mTotalAudioEnergy,
    mTotalSamplesDuration);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCRtpStreamStats, mozilla::dom::RTCStats, mSsrc, mKind,
    mMediaType, mTransportId, mCodecId);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCSentRtpStreamStats, mozilla::dom::RTCRtpStreamStats,
    mPacketsSent, mBytesSent);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCOutboundRtpStreamStats,
    mozilla::dom::RTCSentRtpStreamStats, mRemoteId, mFramesEncoded, mQpSum,
    mNackCount, mFirCount, mPliCount, mHeaderBytesSent,
    mRetransmittedPacketsSent, mRetransmittedBytesSent,
    mTotalEncodedBytesTarget, mFrameWidth, mFrameHeight, mFramesPerSecond,
    mFramesSent, mHugeFramesSent, mTotalEncodeTime);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCRemoteInboundRtpStreamStats,
    mozilla::dom::RTCReceivedRtpStreamStats, mLocalId, mRoundTripTime,
    mTotalRoundTripTime, mFractionLost, mRoundTripTimeMeasurements);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCRemoteOutboundRtpStreamStats,
    mozilla::dom::RTCSentRtpStreamStats, mLocalId, mRemoteTimestamp);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCMediaSourceStats, mId,
                                  mTimestamp, mType, mTrackIdentifier, mKind);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCVideoSourceStats, mozilla::dom::RTCMediaSourceStats,
    mWidth, mHeight, mFrames, mFramesPerSecond);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCRTPContributingSourceStats, mozilla::dom::RTCStats,
    mContributorSsrc, mInboundRtpStreamId);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCPeerConnectionStats, mozilla::dom::RTCStats,
    mDataChannelsOpened, mDataChannelsClosed);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::dom::RTCVideoFrameHistoryEntryInternal, mWidth, mHeight,
    mRotationAngle, mFirstFrameTimestamp, mLastFrameTimestamp,
    mConsecutiveFrames, mLocalSsrc, mRemoteSsrc);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCVideoFrameHistoryInternal,
                                  mTrackIdentifier, mEntries);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCBandwidthEstimationInternal,
                                  mTrackIdentifier, mSendBandwidthBps,
                                  mMaxPaddingBps, mReceiveBandwidthBps,
                                  mPacerDelayMs, mRttMs);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCDataChannelStats, mId,
                                  mTimestamp, mType, mLabel, mProtocol,
                                  mDataChannelIdentifier, mState, mMessagesSent,
                                  mBytesSent, mMessagesReceived, mBytesReceived)

template <>
struct ParamTraits<mozilla::dom::RTCDataChannelState>
    : public ContiguousEnumSerializer<
          mozilla::dom::RTCDataChannelState,
          mozilla::dom::RTCDataChannelState::Connecting,
          mozilla::dom::RTCDataChannelState::EndGuard_> {};

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::RTCCodecStats, mTimestamp,
                                  mType, mId, mPayloadType, mCodecType,
                                  mTransportId, mMimeType, mClockRate,
                                  mChannels, mSdpFmtpLine)

template <>
struct ParamTraits<mozilla::dom::RTCCodecType>
    : public ContiguousEnumSerializer<mozilla::dom::RTCCodecType,
                                      mozilla::dom::RTCCodecType::Encode,
                                      mozilla::dom::RTCCodecType::EndGuard_> {};
}  // namespace IPC

#endif  // _WEBRTC_GLOBAL_H_
