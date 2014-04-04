/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2011/webrtc/editor/webrtc.html#rtcstatsreport-object
 */

enum RTCStatsType {
  "inboundrtp",
  "outboundrtp",
  "session",
  "track",
  "transport",
  "candidatepair",
  "localcandidate",
  "remotecandidate"
};

dictionary RTCStats {
  DOMHighResTimeStamp timestamp;
  RTCStatsType type;
  DOMString id;
};

dictionary RTCRTPStreamStats : RTCStats {
  DOMString ssrc;
  DOMString remoteId;
  boolean isRemote = false;
  DOMString mediaTrackId;
  DOMString transportId;
  DOMString codecId;
};

dictionary RTCInboundRTPStreamStats : RTCRTPStreamStats {
  unsigned long packetsReceived;
  unsigned long long bytesReceived;
  double jitter;
  unsigned long packetsLost;
  long mozAvSyncDelay;
  long mozJitterBufferDelay;
  long mozRtt;
};

dictionary RTCOutboundRTPStreamStats : RTCRTPStreamStats {
  unsigned long packetsSent;
  unsigned long long bytesSent;
};

dictionary RTCMediaStreamTrackStats : RTCStats {
  DOMString trackIdentifier;      // track.id property
  boolean remoteSource;
  sequence<DOMString> ssrcIds;
  unsigned long audioLevel;       // Only for audio, the rest are only for video
  unsigned long frameWidth;
  unsigned long frameHeight;
  double framesPerSecond;         // The nominal FPS value
  unsigned long framesSent;
  unsigned long framesReceived;   // Only for remoteSource=true
  unsigned long framesDecoded;
};

dictionary RTCMediaStreamStats : RTCStats {
  DOMString streamIdentifier;     // stream.id property
  sequence<DOMString> trackIds;   // Note: stats object ids, not track.id
};

dictionary RTCTransportStats: RTCStats {
  unsigned long bytesSent;
  unsigned long bytesReceived;
};

dictionary RTCIceComponentStats : RTCStats {
  DOMString transportId;
  long component;
  unsigned long bytesSent;
  unsigned long bytesReceived;
  boolean activeConnection;
};

enum RTCStatsIceCandidatePairState {
  "frozen",
  "waiting",
  "inprogress",
  "failed",
  "succeeded",
  "cancelled"
};

dictionary RTCIceCandidatePairStats : RTCStats {
  DOMString componentId;
  DOMString localCandidateId;
  DOMString remoteCandidateId;
  RTCStatsIceCandidatePairState state;
  unsigned long long mozPriority;
  boolean readable;
  boolean nominated;
  boolean selected;
};

enum RTCStatsIceCandidateType {
  "host",
  "serverreflexive",
  "peerreflexive",
  "relayed"
};

dictionary RTCIceCandidateStats : RTCStats {
  DOMString componentId;
  DOMString candidateId;
  DOMString ipAddress;
  DOMString transport;
  DOMString mozLocalTransport; // needs standardization
  long portNumber;
  RTCStatsIceCandidateType candidateType;
};

dictionary RTCCodecStats : RTCStats {
  unsigned long payloadType;       // As used in RTP encoding.
  DOMString codec;                 // video/vp8 or equivalent
  unsigned long clockRate;
  unsigned long channels;          // 2=stereo, missing for most other cases.
  DOMString parameters;            // From SDP description line
};

callback RTCStatsReportCallback = void (RTCStatsReport obj);

// This is the internal representation of the report in this implementation
// to be received from c++

dictionary RTCStatsReportInternal {
  DOMString                           pcid = "";
  sequence<RTCRTPStreamStats>         rtpStreamStats;
  sequence<RTCInboundRTPStreamStats>  inboundRTPStreamStats;
  sequence<RTCOutboundRTPStreamStats> outboundRTPStreamStats;
  sequence<RTCMediaStreamTrackStats>  mediaStreamTrackStats;
  sequence<RTCMediaStreamStats>       mediaStreamStats;
  sequence<RTCTransportStats>         transportStats;
  sequence<RTCIceComponentStats>      iceComponentStats;
  sequence<RTCIceCandidatePairStats>  iceCandidatePairStats;
  sequence<RTCIceCandidateStats>      iceCandidateStats;
  sequence<RTCCodecStats>             codecStats;
};

[Pref="media.peerconnection.enabled",
// TODO: Use MapClass here once it's available (Bug 928114)
// MapClass(DOMString, object)
 JSImplementation="@mozilla.org/dom/rtcstatsreport;1"]
interface RTCStatsReport {
  [ChromeOnly]
  readonly attribute DOMString mozPcid;
  void forEach(RTCStatsReportCallback callbackFn, optional any thisArg);
  object get(DOMString key);
  boolean has(DOMString key);
};
