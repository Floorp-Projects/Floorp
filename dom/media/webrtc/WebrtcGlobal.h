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
}  // namespace dom
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::RTCStatsType>
    : public ContiguousEnumSerializer<mozilla::dom::RTCStatsType,
                                      mozilla::dom::RTCStatsType::Inbound_rtp,
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
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg,
               static_cast<const mozilla::dom::RTCStatsCollection&>(aParam));
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter,
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

DEFINE_IPC_SERIALIZER_WITH_FIELDS(
    mozilla::dom::RTCStatsCollection, mIceCandidatePairStats,
    mIceCandidateStats, mInboundRtpStreamStats, mOutboundRtpStreamStats,
    mRemoteInboundRtpStreamStats, mRemoteOutboundRtpStreamStats,
    mRtpContributingSourceStats, mTrickledIceCandidateStats,
    mRawLocalCandidates, mRawRemoteCandidates, mDataChannelStats,
    mVideoFrameHistories, mBandwidthEstimations);

DEFINE_IPC_SERIALIZER_WITH_SUPER_CLASS_AND_FIELDS(
    mozilla::dom::RTCStatsReportInternal, mozilla::dom::RTCStatsCollection,
    mClosed, mLocalSdp, mSdpHistory, mPcid, mBrowserId, mRemoteSdp, mTimestamp,
    mCallDurationMs, mIceRestarts, mIceRollbacks, mOfferer, mConfiguration);

typedef mozilla::dom::RTCStats RTCStats;

static void WriteRTCStats(Message* aMsg, const RTCStats& aParam) {
  // RTCStats base class
  WriteParam(aMsg, aParam.mId);
  WriteParam(aMsg, aParam.mTimestamp);
  WriteParam(aMsg, aParam.mType);
}

static bool ReadRTCStats(const Message* aMsg, PickleIterator* aIter,
                         RTCStats* aResult) {
  // RTCStats base class
  if (!ReadParam(aMsg, aIter, &(aResult->mId)) ||
      !ReadParam(aMsg, aIter, &(aResult->mTimestamp)) ||
      !ReadParam(aMsg, aIter, &(aResult->mType))) {
    return false;
  }

  return true;
}

template <>
struct ParamTraits<mozilla::dom::RTCIceCandidatePairStats> {
  typedef mozilla::dom::RTCIceCandidatePairStats paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mTransportId);
    WriteParam(aMsg, aParam.mLocalCandidateId);
    WriteParam(aMsg, aParam.mPriority);
    WriteParam(aMsg, aParam.mNominated);
    WriteParam(aMsg, aParam.mWritable);
    WriteParam(aMsg, aParam.mReadable);
    WriteParam(aMsg, aParam.mRemoteCandidateId);
    WriteParam(aMsg, aParam.mSelected);
    WriteParam(aMsg, aParam.mComponentId);
    WriteParam(aMsg, aParam.mState);
    WriteParam(aMsg, aParam.mBytesSent);
    WriteParam(aMsg, aParam.mBytesReceived);
    WriteParam(aMsg, aParam.mLastPacketSentTimestamp);
    WriteParam(aMsg, aParam.mLastPacketReceivedTimestamp);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->mTransportId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mLocalCandidateId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPriority)) ||
        !ReadParam(aMsg, aIter, &(aResult->mNominated)) ||
        !ReadParam(aMsg, aIter, &(aResult->mWritable)) ||
        !ReadParam(aMsg, aIter, &(aResult->mReadable)) ||
        !ReadParam(aMsg, aIter, &(aResult->mRemoteCandidateId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mSelected)) ||
        !ReadParam(aMsg, aIter, &(aResult->mComponentId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mState)) ||
        !ReadParam(aMsg, aIter, &(aResult->mBytesSent)) ||
        !ReadParam(aMsg, aIter, &(aResult->mBytesReceived)) ||
        !ReadParam(aMsg, aIter, &(aResult->mLastPacketSentTimestamp)) ||
        !ReadParam(aMsg, aIter, &(aResult->mLastPacketReceivedTimestamp)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::RTCIceCandidateStats> {
  typedef mozilla::dom::RTCIceCandidateStats paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mCandidateType);
    WriteParam(aMsg, aParam.mPriority);
    WriteParam(aMsg, aParam.mTransportId);
    WriteParam(aMsg, aParam.mAddress);
    WriteParam(aMsg, aParam.mRelayProtocol);
    WriteParam(aMsg, aParam.mPort);
    WriteParam(aMsg, aParam.mProtocol);
    WriteParam(aMsg, aParam.mProxied);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->mCandidateType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPriority)) ||
        !ReadParam(aMsg, aIter, &(aResult->mTransportId)) ||
        !ReadParam(aMsg, aIter, &(aResult->mAddress)) ||
        !ReadParam(aMsg, aIter, &(aResult->mRelayProtocol)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPort)) ||
        !ReadParam(aMsg, aIter, &(aResult->mProtocol)) ||
        !ReadParam(aMsg, aIter, &(aResult->mProxied)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
      return false;
    }

    return true;
  }
};

static void WriteRTCRtpStreamStats(
    Message* aMsg, const mozilla::dom::RTCRtpStreamStats& aParam) {
  WriteParam(aMsg, aParam.mSsrc);
  WriteParam(aMsg, aParam.mMediaType);
  WriteParam(aMsg, aParam.mKind);
  WriteParam(aMsg, aParam.mTransportId);
  WriteRTCStats(aMsg, aParam);
}

static bool ReadRTCRtpStreamStats(const Message* aMsg, PickleIterator* aIter,
                                  mozilla::dom::RTCRtpStreamStats* aResult) {
  return ReadParam(aMsg, aIter, &(aResult->mSsrc)) &&
         ReadParam(aMsg, aIter, &(aResult->mMediaType)) &&
         ReadParam(aMsg, aIter, &(aResult->mKind)) &&
         ReadParam(aMsg, aIter, &(aResult->mTransportId)) &&
         ReadRTCStats(aMsg, aIter, aResult);
}

static void WriteRTCReceivedRtpStreamStats(
    Message* aMsg, const mozilla::dom::RTCReceivedRtpStreamStats& aParam) {
  WriteParam(aMsg, aParam.mPacketsReceived);
  WriteParam(aMsg, aParam.mPacketsLost);
  WriteParam(aMsg, aParam.mJitter);
  WriteParam(aMsg, aParam.mDiscardedPackets);
  WriteParam(aMsg, aParam.mPacketsDiscarded);
  WriteRTCRtpStreamStats(aMsg, aParam);
}

static bool ReadRTCReceivedRtpStreamStats(
    const Message* aMsg, PickleIterator* aIter,
    mozilla::dom::RTCReceivedRtpStreamStats* aResult) {
  return ReadParam(aMsg, aIter, &(aResult->mPacketsReceived)) &&
         ReadParam(aMsg, aIter, &(aResult->mPacketsLost)) &&
         ReadParam(aMsg, aIter, &(aResult->mJitter)) &&
         ReadParam(aMsg, aIter, &(aResult->mDiscardedPackets)) &&
         ReadParam(aMsg, aIter, &(aResult->mPacketsDiscarded)) &&
         ReadRTCRtpStreamStats(aMsg, aIter, aResult);
}

template <>
struct ParamTraits<mozilla::dom::RTCInboundRtpStreamStats> {
  typedef mozilla::dom::RTCInboundRtpStreamStats paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mRemoteId);
    WriteParam(aMsg, aParam.mFramesDecoded);
    WriteParam(aMsg, aParam.mBytesReceived);
    WriteParam(aMsg, aParam.mNackCount);
    WriteParam(aMsg, aParam.mFirCount);
    WriteParam(aMsg, aParam.mPliCount);
    WriteRTCReceivedRtpStreamStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &(aResult->mRemoteId)) &&
           ReadParam(aMsg, aIter, &(aResult->mFramesDecoded)) &&
           ReadParam(aMsg, aIter, &(aResult->mBytesReceived)) &&
           ReadParam(aMsg, aIter, &(aResult->mNackCount)) &&
           ReadParam(aMsg, aIter, &(aResult->mFirCount)) &&
           ReadParam(aMsg, aIter, &(aResult->mPliCount)) &&
           ReadRTCReceivedRtpStreamStats(aMsg, aIter, aResult);
  }
};

static void WriteRTCSentRtpStreamStats(
    Message* aMsg, const mozilla::dom::RTCSentRtpStreamStats& aParam) {
  WriteParam(aMsg, aParam.mPacketsSent);
  WriteParam(aMsg, aParam.mBytesSent);
  WriteRTCRtpStreamStats(aMsg, aParam);
}

static bool ReadRTCSentRtpStreamStats(
    const Message* aMsg, PickleIterator* aIter,
    mozilla::dom::RTCSentRtpStreamStats* aResult) {
  return ReadParam(aMsg, aIter, &(aResult->mPacketsSent)) &&
         ReadParam(aMsg, aIter, &(aResult->mBytesSent)) &&
         ReadRTCRtpStreamStats(aMsg, aIter, aResult);
}

template <>
struct ParamTraits<mozilla::dom::RTCOutboundRtpStreamStats> {
  typedef mozilla::dom::RTCOutboundRtpStreamStats paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mRemoteId);
    WriteParam(aMsg, aParam.mFramesEncoded);
    WriteParam(aMsg, aParam.mQpSum);
    WriteParam(aMsg, aParam.mNackCount);
    WriteParam(aMsg, aParam.mFirCount);
    WriteParam(aMsg, aParam.mPliCount);
    WriteRTCSentRtpStreamStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &(aResult->mRemoteId)) &&
           ReadParam(aMsg, aIter, &(aResult->mFramesEncoded)) &&
           ReadParam(aMsg, aIter, &(aResult->mQpSum)) &&
           ReadParam(aMsg, aIter, &(aResult->mNackCount)) &&
           ReadParam(aMsg, aIter, &(aResult->mFirCount)) &&
           ReadParam(aMsg, aIter, &(aResult->mPliCount)) &&
           ReadRTCSentRtpStreamStats(aMsg, aIter, aResult);
  }
};

template <>
struct ParamTraits<mozilla::dom::RTCRemoteInboundRtpStreamStats> {
  typedef mozilla::dom::RTCRemoteInboundRtpStreamStats paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mLocalId);
    WriteParam(aMsg, aParam.mRoundTripTime);
    WriteRTCReceivedRtpStreamStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &(aResult->mLocalId)) &&
           ReadParam(aMsg, aIter, &(aResult->mRoundTripTime)) &&
           ReadRTCReceivedRtpStreamStats(aMsg, aIter, aResult);
  }
};

template <>
struct ParamTraits<mozilla::dom::RTCRemoteOutboundRtpStreamStats> {
  typedef mozilla::dom::RTCRemoteOutboundRtpStreamStats paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mLocalId);
    WriteParam(aMsg, aParam.mRemoteTimestamp);
    WriteRTCSentRtpStreamStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &(aResult->mLocalId)) &&
           ReadParam(aMsg, aIter, &(aResult->mRemoteTimestamp)) &&
           ReadRTCSentRtpStreamStats(aMsg, aIter, aResult);
  }
};

template <>
struct ParamTraits<mozilla::dom::RTCRTPContributingSourceStats> {
  typedef mozilla::dom::RTCRTPContributingSourceStats paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mContributorSsrc);
    WriteParam(aMsg, aParam.mInboundRtpStreamId);
    WriteRTCStats(aMsg, aParam);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->mContributorSsrc)) ||
        !ReadParam(aMsg, aIter, &(aResult->mInboundRtpStreamId)) ||
        !ReadRTCStats(aMsg, aIter, aResult)) {
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
}  // namespace IPC

#endif  // _WEBRTC_GLOBAL_H_
