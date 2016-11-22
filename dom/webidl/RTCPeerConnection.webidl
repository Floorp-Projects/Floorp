/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://w3c.github.io/webrtc-pc/#interface-definition
 */

callback RTCSessionDescriptionCallback = void (RTCSessionDescription sdp);
callback RTCPeerConnectionErrorCallback = void (DOMError error);
callback VoidFunction = void ();
callback RTCStatsCallback = void (RTCStatsReport report);

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

dictionary RTCDataChannelInit {
  boolean         ordered = true;
  unsigned short? maxRetransmitTime = null;
  unsigned short? maxRetransmits = null;
  DOMString       protocol = "";
  boolean         negotiated = false; // spec currently says 'true'; we disagree
  unsigned short? id = null;

  // these are deprecated due to renaming in the spec, but still supported for Fx22
  boolean outOfOrderAllowed; // now ordered, and the default changes to keep behavior the same
  unsigned short maxRetransmitNum; // now maxRetransmits
  boolean preset; // now negotiated
  unsigned short stream; // now id
};

dictionary RTCOfferAnswerOptions {
//  boolean voiceActivityDetection = true; // TODO: support this (Bug 1184712)
};

dictionary RTCAnswerOptions : RTCOfferAnswerOptions {
};

dictionary RTCOfferOptions : RTCOfferAnswerOptions {
  long    offerToReceiveVideo;
  long    offerToReceiveAudio;
  boolean iceRestart = false;

  // Mozilla proprietary options (at risk: Bug 1196974)
  boolean mozDontOfferDataChannel;
  boolean mozBundleOnly;

  // TODO: Remove old constraint-like RTCOptions support soon (Bug 1064223).
  DeprecatedRTCOfferOptionsSet mandatory;
  sequence<DeprecatedRTCOfferOptionsSet> _optional;
};

dictionary DeprecatedRTCOfferOptionsSet {
  boolean OfferToReceiveAudio;     // Note the uppercase 'O'
  boolean OfferToReceiveVideo;     // Note the uppercase 'O'
  boolean MozDontOfferDataChannel; // Note the uppercase 'M'
  boolean MozBundleOnly;           // Note the uppercase 'M'
};

interface RTCDataChannel;

[Pref="media.peerconnection.enabled",
 JSImplementation="@mozilla.org/dom/peerconnection;1",
 Constructor (optional RTCConfiguration configuration,
              optional object? constraints)]
interface RTCPeerConnection : EventTarget  {
  [Throws, StaticClassOverride="mozilla::dom::RTCCertificate"]
  static Promise<RTCCertificate> generateCertificate (AlgorithmIdentifier keygenAlgorithm);

  [Pref="media.peerconnection.identity.enabled"]
  void setIdentityProvider (DOMString provider,
                            optional DOMString protocol,
                            optional DOMString username);
  [Pref="media.peerconnection.identity.enabled"]
  Promise<DOMString> getIdentityAssertion();
  Promise<RTCSessionDescription> createOffer (optional RTCOfferOptions options);
  Promise<RTCSessionDescription> createAnswer (optional RTCAnswerOptions options);
  Promise<void> setLocalDescription (RTCSessionDescription description);
  Promise<void> setRemoteDescription (RTCSessionDescription description);
  readonly attribute RTCSessionDescription? localDescription;
  readonly attribute RTCSessionDescription? remoteDescription;
  readonly attribute RTCSignalingState signalingState;
  Promise<void> addIceCandidate (RTCIceCandidate candidate);
  readonly attribute boolean? canTrickleIceCandidates;
  readonly attribute RTCIceGatheringState iceGatheringState;
  readonly attribute RTCIceConnectionState iceConnectionState;
  [Pref="media.peerconnection.identity.enabled"]
  readonly attribute Promise<RTCIdentityAssertion> peerIdentity;
  [Pref="media.peerconnection.identity.enabled"]
  readonly attribute DOMString? idpLoginUrl;

  [ChromeOnly]
  attribute DOMString id;

  RTCConfiguration      getConfiguration ();
  [UnsafeInPrerendering, Deprecated="RTCPeerConnectionGetStreams"]
  sequence<MediaStream> getLocalStreams ();
  [UnsafeInPrerendering, Deprecated="RTCPeerConnectionGetStreams"]
  sequence<MediaStream> getRemoteStreams ();
  void addStream (MediaStream stream);

  // replaces addStream; fails if already added
  // because a track can be part of multiple streams, stream parameters
  // indicate which particular streams should be referenced in signaling

  RTCRtpSender addTrack(MediaStreamTrack track,
                        MediaStream stream,
                        MediaStream... moreStreams);
  void removeTrack(RTCRtpSender sender);

  sequence<RTCRtpSender> getSenders();
  sequence<RTCRtpReceiver> getReceivers();

  [ChromeOnly]
  void mozSelectSsrc(RTCRtpReceiver receiver, unsigned short ssrcIndex);

  void close ();
  attribute EventHandler onnegotiationneeded;
  attribute EventHandler onicecandidate;
  attribute EventHandler onsignalingstatechange;
  attribute EventHandler onaddstream; // obsolete
  attribute EventHandler onaddtrack;  // obsolete
  attribute EventHandler ontrack;     // replaces onaddtrack and onaddstream.
  attribute EventHandler onremovestream;
  attribute EventHandler oniceconnectionstatechange;
  attribute EventHandler onicegatheringstatechange;

  Promise<RTCStatsReport> getStats (optional MediaStreamTrack? selector);

  // Data channel.
  RTCDataChannel createDataChannel (DOMString label,
                                    optional RTCDataChannelInit dataChannelDict);
  attribute EventHandler ondatachannel;
};

// Legacy callback API

partial interface RTCPeerConnection {

  // Dummy Promise<void> return values avoid "WebIDL.WebIDLError: error:
  // We have overloads with both Promise and non-Promise return types"

  Promise<void> createOffer (RTCSessionDescriptionCallback successCallback,
                             RTCPeerConnectionErrorCallback failureCallback,
                             optional RTCOfferOptions options);
  Promise<void> createAnswer (RTCSessionDescriptionCallback successCallback,
                              RTCPeerConnectionErrorCallback failureCallback);
  Promise<void> setLocalDescription (RTCSessionDescription description,
                                     VoidFunction successCallback,
                                     RTCPeerConnectionErrorCallback failureCallback);
  Promise<void> setRemoteDescription (RTCSessionDescription description,
                                      VoidFunction successCallback,
                                      RTCPeerConnectionErrorCallback failureCallback);
  Promise<void> addIceCandidate (RTCIceCandidate candidate,
                                 VoidFunction successCallback,
                                 RTCPeerConnectionErrorCallback failureCallback);
  Promise<void> getStats (MediaStreamTrack? selector,
                          RTCStatsCallback successCallback,
                          RTCPeerConnectionErrorCallback failureCallback);
};
