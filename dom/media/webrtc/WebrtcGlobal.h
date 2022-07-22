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
// webidl dictionaries don't have move semantics, which is something that ipdl
// needs for async returns. So, we create a "moveable" subclass that just
// copies. _Really_ lame, but it gets the job done.
struct NotReallyMovableButLetsPretendItIsRTCStatsCollection
    : public RTCStatsCollection {
  NotReallyMovableButLetsPretendItIsRTCStatsCollection() = default;
  explicit NotReallyMovableButLetsPretendItIsRTCStatsCollection(
      RTCStatsCollection&& aStats) {
    RTCStatsCollection::operator=(aStats);
  }
  explicit NotReallyMovableButLetsPretendItIsRTCStatsCollection(
      const RTCStatsCollection& aStats) {
    RTCStatsCollection::operator=(aStats);
  }
};

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
      aStats.mRtpContributingSourceStats, aStats.mIceCandidatePairStats,
      aStats.mIceCandidateStats, aStats.mTrickledIceCandidateStats,
      aStats.mDataChannelStats, aStats.mCodecStats);
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
struct ParamTraits<
    mozilla::dom::NotReallyMovableButLetsPretendItIsRTCStatsCollection> {
  typedef mozilla::dom::NotReallyMovableButLetsPretendItIsRTCStatsCollection
      paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter,
               static_cast<const mozilla::dom::RTCStatsCollection&>(aParam));
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader,
                     static_cast<mozilla::dom::RTCStatsCollection*>(aResult));
  }
};

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
    mClosed, mLocalSdp, mSdpHistory, mPcid, mBrowserId, mRemoteSdp, mTimestamp,
    mCallDurationMs, mIceRestarts, mIceRollbacks, mOfferer, mConfiguration);

typedef mozilla::dom::RTCStats RTCStats;

static void WriteRTCStats(MessageWriter* aWriter, const RTCStats& aParam) {
  // RTCStats base class
  WriteParam(aWriter, aParam.mId);
  WriteParam(aWriter, aParam.mTimestamp);
  WriteParam(aWriter, aParam.mType);
}

static bool ReadRTCStats(MessageReader* aReader, RTCStats* aResult) {
  // RTCStats base class
  if (!ReadParam(aReader, &(aResult->mId)) ||
      !ReadParam(aReader, &(aResult->mTimestamp)) ||
      !ReadParam(aReader, &(aResult->mType))) {
    return false;
  }

  return true;
}

template <>
struct ParamTraits<mozilla::dom::RTCIceCandidatePairStats> {
  typedef mozilla::dom::RTCIceCandidatePairStats paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mTransportId);
    WriteParam(aWriter, aParam.mLocalCandidateId);
    WriteParam(aWriter, aParam.mPriority);
    WriteParam(aWriter, aParam.mNominated);
    WriteParam(aWriter, aParam.mWritable);
    WriteParam(aWriter, aParam.mReadable);
    WriteParam(aWriter, aParam.mRemoteCandidateId);
    WriteParam(aWriter, aParam.mSelected);
    WriteParam(aWriter, aParam.mComponentId);
    WriteParam(aWriter, aParam.mState);
    WriteParam(aWriter, aParam.mBytesSent);
    WriteParam(aWriter, aParam.mBytesReceived);
    WriteParam(aWriter, aParam.mLastPacketSentTimestamp);
    WriteParam(aWriter, aParam.mLastPacketReceivedTimestamp);
    WriteRTCStats(aWriter, aParam);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mTransportId)) ||
        !ReadParam(aReader, &(aResult->mLocalCandidateId)) ||
        !ReadParam(aReader, &(aResult->mPriority)) ||
        !ReadParam(aReader, &(aResult->mNominated)) ||
        !ReadParam(aReader, &(aResult->mWritable)) ||
        !ReadParam(aReader, &(aResult->mReadable)) ||
        !ReadParam(aReader, &(aResult->mRemoteCandidateId)) ||
        !ReadParam(aReader, &(aResult->mSelected)) ||
        !ReadParam(aReader, &(aResult->mComponentId)) ||
        !ReadParam(aReader, &(aResult->mState)) ||
        !ReadParam(aReader, &(aResult->mBytesSent)) ||
        !ReadParam(aReader, &(aResult->mBytesReceived)) ||
        !ReadParam(aReader, &(aResult->mLastPacketSentTimestamp)) ||
        !ReadParam(aReader, &(aResult->mLastPacketReceivedTimestamp)) ||
        !ReadRTCStats(aReader, aResult)) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::RTCIceCandidateStats> {
  typedef mozilla::dom::RTCIceCandidateStats paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mCandidateType);
    WriteParam(aWriter, aParam.mPriority);
    WriteParam(aWriter, aParam.mTransportId);
    WriteParam(aWriter, aParam.mAddress);
    WriteParam(aWriter, aParam.mRelayProtocol);
    WriteParam(aWriter, aParam.mPort);
    WriteParam(aWriter, aParam.mProtocol);
    WriteParam(aWriter, aParam.mProxied);
    WriteRTCStats(aWriter, aParam);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mCandidateType)) ||
        !ReadParam(aReader, &(aResult->mPriority)) ||
        !ReadParam(aReader, &(aResult->mTransportId)) ||
        !ReadParam(aReader, &(aResult->mAddress)) ||
        !ReadParam(aReader, &(aResult->mRelayProtocol)) ||
        !ReadParam(aReader, &(aResult->mPort)) ||
        !ReadParam(aReader, &(aResult->mProtocol)) ||
        !ReadParam(aReader, &(aResult->mProxied)) ||
        !ReadRTCStats(aReader, aResult)) {
      return false;
    }

    return true;
  }
};

static void WriteRTCRtpStreamStats(
    MessageWriter* aWriter, const mozilla::dom::RTCRtpStreamStats& aParam) {
  WriteParam(aWriter, aParam.mSsrc);
  WriteParam(aWriter, aParam.mMediaType);
  WriteParam(aWriter, aParam.mKind);
  WriteParam(aWriter, aParam.mTransportId);
  WriteParam(aWriter, aParam.mCodecId);
  WriteRTCStats(aWriter, aParam);
}

static bool ReadRTCRtpStreamStats(MessageReader* aReader,
                                  mozilla::dom::RTCRtpStreamStats* aResult) {
  return ReadParam(aReader, &(aResult->mSsrc)) &&
         ReadParam(aReader, &(aResult->mMediaType)) &&
         ReadParam(aReader, &(aResult->mKind)) &&
         ReadParam(aReader, &(aResult->mTransportId)) &&
         ReadParam(aReader, &(aResult->mCodecId)) &&
         ReadRTCStats(aReader, aResult);
}

static void WriteRTCReceivedRtpStreamStats(
    MessageWriter* aWriter,
    const mozilla::dom::RTCReceivedRtpStreamStats& aParam) {
  WriteParam(aWriter, aParam.mPacketsReceived);
  WriteParam(aWriter, aParam.mPacketsLost);
  WriteParam(aWriter, aParam.mJitter);
  WriteParam(aWriter, aParam.mDiscardedPackets);
  WriteParam(aWriter, aParam.mPacketsDiscarded);
  WriteRTCRtpStreamStats(aWriter, aParam);
}

static bool ReadRTCReceivedRtpStreamStats(
    MessageReader* aReader, mozilla::dom::RTCReceivedRtpStreamStats* aResult) {
  return ReadParam(aReader, &(aResult->mPacketsReceived)) &&
         ReadParam(aReader, &(aResult->mPacketsLost)) &&
         ReadParam(aReader, &(aResult->mJitter)) &&
         ReadParam(aReader, &(aResult->mDiscardedPackets)) &&
         ReadParam(aReader, &(aResult->mPacketsDiscarded)) &&
         ReadRTCRtpStreamStats(aReader, aResult);
}

template <>
struct ParamTraits<mozilla::dom::RTCInboundRtpStreamStats> {
  typedef mozilla::dom::RTCInboundRtpStreamStats paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mRemoteId);
    WriteParam(aWriter, aParam.mFramesDecoded);
    WriteParam(aWriter, aParam.mFrameWidth);
    WriteParam(aWriter, aParam.mFrameHeight);
    WriteParam(aWriter, aParam.mFramesPerSecond);
    WriteParam(aWriter, aParam.mBytesReceived);
    WriteParam(aWriter, aParam.mNackCount);
    WriteParam(aWriter, aParam.mFirCount);
    WriteParam(aWriter, aParam.mPliCount);
    WriteParam(aWriter, aParam.mFramesReceived);
    WriteParam(aWriter, aParam.mJitterBufferDelay);
    WriteParam(aWriter, aParam.mJitterBufferEmittedCount);
    WriteParam(aWriter, aParam.mTotalSamplesReceived);
    WriteParam(aWriter, aParam.mConcealedSamples);
    WriteParam(aWriter, aParam.mSilentConcealedSamples);
    WriteRTCReceivedRtpStreamStats(aWriter, aParam);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &(aResult->mRemoteId)) &&
           ReadParam(aReader, &(aResult->mFramesDecoded)) &&
           ReadParam(aReader, &(aResult->mFrameWidth)) &&
           ReadParam(aReader, &(aResult->mFrameHeight)) &&
           ReadParam(aReader, &(aResult->mFramesPerSecond)) &&
           ReadParam(aReader, &(aResult->mBytesReceived)) &&
           ReadParam(aReader, &(aResult->mNackCount)) &&
           ReadParam(aReader, &(aResult->mFirCount)) &&
           ReadParam(aReader, &(aResult->mPliCount)) &&
           ReadParam(aReader, &(aResult->mFramesReceived)) &&
           ReadParam(aReader, &(aResult->mJitterBufferDelay)) &&
           ReadParam(aReader, &(aResult->mJitterBufferEmittedCount)) &&
           ReadParam(aReader, &(aResult->mTotalSamplesReceived)) &&
           ReadParam(aReader, &(aResult->mConcealedSamples)) &&
           ReadParam(aReader, &(aResult->mSilentConcealedSamples)) &&
           ReadRTCReceivedRtpStreamStats(aReader, aResult);
  }
};

static void WriteRTCSentRtpStreamStats(
    MessageWriter* aWriter, const mozilla::dom::RTCSentRtpStreamStats& aParam) {
  WriteParam(aWriter, aParam.mPacketsSent);
  WriteParam(aWriter, aParam.mBytesSent);
  WriteRTCRtpStreamStats(aWriter, aParam);
}

static bool ReadRTCSentRtpStreamStats(
    MessageReader* aReader, mozilla::dom::RTCSentRtpStreamStats* aResult) {
  return ReadParam(aReader, &(aResult->mPacketsSent)) &&
         ReadParam(aReader, &(aResult->mBytesSent)) &&
         ReadRTCRtpStreamStats(aReader, aResult);
}

template <>
struct ParamTraits<mozilla::dom::RTCOutboundRtpStreamStats> {
  typedef mozilla::dom::RTCOutboundRtpStreamStats paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mRemoteId);
    WriteParam(aWriter, aParam.mFramesEncoded);
    WriteParam(aWriter, aParam.mQpSum);
    WriteParam(aWriter, aParam.mNackCount);
    WriteParam(aWriter, aParam.mFirCount);
    WriteParam(aWriter, aParam.mPliCount);
    WriteParam(aWriter, aParam.mHeaderBytesSent);
    WriteParam(aWriter, aParam.mRetransmittedPacketsSent);
    WriteParam(aWriter, aParam.mRetransmittedBytesSent);
    WriteParam(aWriter, aParam.mTotalEncodedBytesTarget);
    WriteParam(aWriter, aParam.mFrameWidth);
    WriteParam(aWriter, aParam.mFrameHeight);
    WriteParam(aWriter, aParam.mFramesSent);
    WriteParam(aWriter, aParam.mHugeFramesSent);
    WriteParam(aWriter, aParam.mTotalEncodeTime);
    WriteRTCSentRtpStreamStats(aWriter, aParam);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &(aResult->mRemoteId)) &&
           ReadParam(aReader, &(aResult->mFramesEncoded)) &&
           ReadParam(aReader, &(aResult->mQpSum)) &&
           ReadParam(aReader, &(aResult->mNackCount)) &&
           ReadParam(aReader, &(aResult->mFirCount)) &&
           ReadParam(aReader, &(aResult->mPliCount)) &&
           ReadParam(aReader, &(aResult->mHeaderBytesSent)) &&
           ReadParam(aReader, &(aResult->mRetransmittedPacketsSent)) &&
           ReadParam(aReader, &(aResult->mRetransmittedBytesSent)) &&
           ReadParam(aReader, &(aResult->mTotalEncodedBytesTarget)) &&
           ReadParam(aReader, &(aResult->mFrameWidth)) &&
           ReadParam(aReader, &(aResult->mFrameHeight)) &&
           ReadParam(aReader, &(aResult->mFramesSent)) &&
           ReadParam(aReader, &(aResult->mHugeFramesSent)) &&
           ReadParam(aReader, &(aResult->mTotalEncodeTime)) &&
           ReadRTCSentRtpStreamStats(aReader, aResult);
  }
};

template <>
struct ParamTraits<mozilla::dom::RTCRemoteInboundRtpStreamStats> {
  typedef mozilla::dom::RTCRemoteInboundRtpStreamStats paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mLocalId);
    WriteParam(aWriter, aParam.mRoundTripTime);
    WriteRTCReceivedRtpStreamStats(aWriter, aParam);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &(aResult->mLocalId)) &&
           ReadParam(aReader, &(aResult->mRoundTripTime)) &&
           ReadRTCReceivedRtpStreamStats(aReader, aResult);
  }
};

template <>
struct ParamTraits<mozilla::dom::RTCRemoteOutboundRtpStreamStats> {
  typedef mozilla::dom::RTCRemoteOutboundRtpStreamStats paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mLocalId);
    WriteParam(aWriter, aParam.mRemoteTimestamp);
    WriteRTCSentRtpStreamStats(aWriter, aParam);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &(aResult->mLocalId)) &&
           ReadParam(aReader, &(aResult->mRemoteTimestamp)) &&
           ReadRTCSentRtpStreamStats(aReader, aResult);
  }
};

template <>
struct ParamTraits<mozilla::dom::RTCRTPContributingSourceStats> {
  typedef mozilla::dom::RTCRTPContributingSourceStats paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mContributorSsrc);
    WriteParam(aWriter, aParam.mInboundRtpStreamId);
    WriteRTCStats(aWriter, aParam);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mContributorSsrc)) ||
        !ReadParam(aReader, &(aResult->mInboundRtpStreamId)) ||
        !ReadRTCStats(aReader, aResult)) {
      return false;
    }
    return true;
  }
};

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
