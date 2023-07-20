/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://w3c.github.io/webrtc-pc/#interface-definition
 */

callback RTCSessionDescriptionCallback = undefined (RTCSessionDescriptionInit description);
callback RTCPeerConnectionErrorCallback = undefined (DOMException error);
callback RTCStatsCallback = undefined (RTCStatsReport report);

enum RTCSignalingState {
    "stable",
    "have-local-offer",
    "have-remote-offer",
    "have-local-pranswer",
    "have-remote-pranswer",
    "closed"
};

enum RTCIceGatheringState {
    "new",
    "gathering",
    "complete"
};

enum RTCIceConnectionState {
    "new",
    "checking",
    "connected",
    "completed",
    "failed",
    "disconnected",
    "closed"
};

enum RTCPeerConnectionState {
  "closed",
  "failed",
  "disconnected",
  "new",
  "connecting",
  "connected"
};

enum mozPacketDumpType {
  "rtp", // dump unencrypted rtp as the MediaPipeline sees it
  "srtp", // dump encrypted rtp as the MediaPipeline sees it
  "rtcp", // dump unencrypted rtcp as the MediaPipeline sees it
  "srtcp" // dump encrypted rtcp as the MediaPipeline sees it
};

callback mozPacketCallback = undefined (unsigned long level,
                                        mozPacketDumpType type,
                                        boolean sending,
                                        ArrayBuffer packet);

dictionary RTCDataChannelInit {
  boolean        ordered = true;
  [EnforceRange]
  unsigned short maxPacketLifeTime;
  [EnforceRange]
  unsigned short maxRetransmits;
  DOMString      protocol = "";
  boolean        negotiated = false;
  [EnforceRange]
  unsigned short id;

  // These are deprecated due to renaming in the spec, but still supported for Fx53
  unsigned short maxRetransmitTime;
};

dictionary RTCOfferAnswerOptions {
//  boolean voiceActivityDetection = true; // TODO: support this (Bug 1184712)
};

dictionary RTCAnswerOptions : RTCOfferAnswerOptions {
};

dictionary RTCOfferOptions : RTCOfferAnswerOptions {
  boolean offerToReceiveVideo;
  boolean offerToReceiveAudio;
  boolean iceRestart = false;
};

[Pref="media.peerconnection.enabled",
 JSImplementation="@mozilla.org/dom/peerconnection;1",
 Exposed=Window]
interface RTCPeerConnection : EventTarget  {
  [Throws]
  constructor(optional RTCConfiguration configuration = {},
              optional object? constraints);

  [Throws, StaticClassOverride="mozilla::dom::RTCCertificate"]
  static Promise<RTCCertificate> generateCertificate (AlgorithmIdentifier keygenAlgorithm);

  undefined setIdentityProvider (DOMString provider,
                                 optional RTCIdentityProviderOptions options = {});
  Promise<DOMString> getIdentityAssertion();
  Promise<RTCSessionDescriptionInit> createOffer (optional RTCOfferOptions options = {});
  Promise<RTCSessionDescriptionInit> createAnswer (optional RTCAnswerOptions options = {});
  Promise<undefined> setLocalDescription (optional RTCSessionDescriptionInit description = {});
  Promise<undefined> setRemoteDescription (optional RTCSessionDescriptionInit description = {});
  readonly attribute RTCSessionDescription? localDescription;
  readonly attribute RTCSessionDescription? currentLocalDescription;
  readonly attribute RTCSessionDescription? pendingLocalDescription;
  readonly attribute RTCSessionDescription? remoteDescription;
  readonly attribute RTCSessionDescription? currentRemoteDescription;
  readonly attribute RTCSessionDescription? pendingRemoteDescription;
  readonly attribute RTCSignalingState signalingState;
  Promise<undefined> addIceCandidate (optional (RTCIceCandidateInit or RTCIceCandidate) candidate = {});
  readonly attribute boolean? canTrickleIceCandidates;
  readonly attribute RTCIceGatheringState iceGatheringState;
  readonly attribute RTCIceConnectionState iceConnectionState;
  readonly attribute RTCPeerConnectionState connectionState;
  undefined restartIce ();
  readonly attribute Promise<RTCIdentityAssertion> peerIdentity;
  readonly attribute DOMString? idpLoginUrl;

  [ChromeOnly]
  attribute DOMString id;

  RTCConfiguration      getConfiguration ();
  undefined setConfiguration(optional RTCConfiguration configuration = {});
  [Deprecated="RTCPeerConnectionGetStreams"]
  sequence<MediaStream> getLocalStreams ();
  [Deprecated="RTCPeerConnectionGetStreams"]
  sequence<MediaStream> getRemoteStreams ();
  undefined addStream (MediaStream stream);

  // replaces addStream; fails if already added
  // because a track can be part of multiple streams, stream parameters
  // indicate which particular streams should be referenced in signaling

  RTCRtpSender addTrack(MediaStreamTrack track,
                        MediaStream... streams);
  undefined removeTrack(RTCRtpSender sender);

  [Throws]
  RTCRtpTransceiver addTransceiver((MediaStreamTrack or DOMString) trackOrKind,
                                   optional RTCRtpTransceiverInit init = {});

  sequence<RTCRtpSender> getSenders();
  sequence<RTCRtpReceiver> getReceivers();
  sequence<RTCRtpTransceiver> getTransceivers();

  [ChromeOnly]
  undefined mozSetPacketCallback(mozPacketCallback callback);
  [ChromeOnly]
  undefined mozEnablePacketDump(unsigned long level,
                                mozPacketDumpType type,
                                boolean sending);
  [ChromeOnly]
  undefined mozDisablePacketDump(unsigned long level,
                                 mozPacketDumpType type,
                                 boolean sending);

  undefined close ();
  attribute EventHandler onnegotiationneeded;
  attribute EventHandler onicecandidate;
  attribute EventHandler onsignalingstatechange;
  attribute EventHandler onaddstream; // obsolete
  attribute EventHandler onaddtrack;  // obsolete
  attribute EventHandler ontrack;     // replaces onaddtrack and onaddstream.
  attribute EventHandler oniceconnectionstatechange;
  attribute EventHandler onicegatheringstatechange;
  attribute EventHandler onconnectionstatechange;

  Promise<RTCStatsReport> getStats (optional MediaStreamTrack? selector = null);

  readonly attribute RTCSctpTransport? sctp;
  // Data channel.
  RTCDataChannel createDataChannel (DOMString label,
                                    optional RTCDataChannelInit dataChannelDict = {});
  attribute EventHandler ondatachannel;
};

// Legacy callback API

partial interface RTCPeerConnection {

  // Dummy Promise<undefined> return values avoid "WebIDL.WebIDLError: error:
  // We have overloads with both Promise and non-Promise return types"

  Promise<undefined> createOffer (RTCSessionDescriptionCallback successCallback,
                                  RTCPeerConnectionErrorCallback failureCallback,
                                  optional RTCOfferOptions options = {});
  Promise<undefined> createAnswer (RTCSessionDescriptionCallback successCallback,
                                   RTCPeerConnectionErrorCallback failureCallback);
  Promise<undefined> setLocalDescription (RTCSessionDescriptionInit description,
                                          VoidFunction successCallback,
                                          RTCPeerConnectionErrorCallback failureCallback);
  Promise<undefined> setRemoteDescription (RTCSessionDescriptionInit description,
                                           VoidFunction successCallback,
                                           RTCPeerConnectionErrorCallback failureCallback);
  Promise<undefined> addIceCandidate (RTCIceCandidate candidate,
                                      VoidFunction successCallback,
                                      RTCPeerConnectionErrorCallback failureCallback);
};
