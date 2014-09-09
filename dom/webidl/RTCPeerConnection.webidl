/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2011/webrtc/editor/webrtc.html#idl-def-RTCPeerConnection
 */

callback RTCSessionDescriptionCallback = void (mozRTCSessionDescription sdp);
callback RTCPeerConnectionErrorCallback = void (DOMString errorInformation);
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

dictionary RTCOfferOptions {
  long    offerToReceiveVideo;
  long    offerToReceiveAudio;
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
// moz-prefixed until sufficiently standardized.
interface mozRTCPeerConnection : EventTarget  {
  [Pref="media.peerconnection.identity.enabled"]
  void setIdentityProvider (DOMString provider,
                            optional DOMString protocol,
                            optional DOMString username);
  [Pref="media.peerconnection.identity.enabled"]
  void getIdentityAssertion();
  void createOffer (RTCSessionDescriptionCallback successCallback,
                    RTCPeerConnectionErrorCallback failureCallback,
                    optional RTCOfferOptions options);
  void createAnswer (RTCSessionDescriptionCallback successCallback,
                     RTCPeerConnectionErrorCallback failureCallback);
  void setLocalDescription (mozRTCSessionDescription description,
                            optional VoidFunction successCallback,
                            optional RTCPeerConnectionErrorCallback failureCallback);
  void setRemoteDescription (mozRTCSessionDescription description,
                             optional VoidFunction successCallback,
                             optional RTCPeerConnectionErrorCallback failureCallback);
  readonly attribute mozRTCSessionDescription? localDescription;
  readonly attribute mozRTCSessionDescription? remoteDescription;
  readonly attribute RTCSignalingState signalingState;
  void updateIce (optional RTCConfiguration configuration);
  void addIceCandidate (mozRTCIceCandidate candidate,
                        optional VoidFunction successCallback,
                        optional RTCPeerConnectionErrorCallback failureCallback);
  readonly attribute RTCIceGatheringState iceGatheringState;
  readonly attribute RTCIceConnectionState iceConnectionState;
  [Pref="media.peerconnection.identity.enabled"]
  readonly attribute RTCIdentityAssertion? peerIdentity;

  [ChromeOnly]
  readonly attribute DOMString id;

  RTCConfiguration      getConfiguration ();
  sequence<MediaStream> getLocalStreams ();
  sequence<MediaStream> getRemoteStreams ();
  MediaStream? getStreamById (DOMString streamId);
  void addStream (MediaStream stream);
  void removeStream (MediaStream stream);

  // replaces addStream; fails if already added
  // because a track can be part of multiple streams, stream parameters
  // indicate which particular streams should be referenced in signaling

  RTCRtpSender addTrack(MediaStreamTrack track,
                        MediaStream stream,
                        MediaStream... moreStreams);
  void removeTrack(RTCRtpSender sender);

  sequence<RTCRtpSender> getSenders();
  sequence<RTCRtpReceiver> getReceivers();

  void close ();
  attribute EventHandler onnegotiationneeded;
  attribute EventHandler onicecandidate;
  attribute EventHandler onsignalingstatechange;
  attribute EventHandler onaddstream;
  attribute EventHandler onaddtrack;  // replaces onaddstream; see AddTrackEvent
  attribute EventHandler onremovestream;
  attribute EventHandler oniceconnectionstatechange;

  void getStats (MediaStreamTrack? selector,
                 RTCStatsCallback successCallback,
                 RTCPeerConnectionErrorCallback failureCallback);

  // Data channel.
  RTCDataChannel createDataChannel (DOMString label,
                                    optional RTCDataChannelInit dataChannelDict);
  attribute EventHandler ondatachannel;
  [Pref="media.peerconnection.identity.enabled"]
  attribute EventHandler onidentityresult;
  [Pref="media.peerconnection.identity.enabled"]
  attribute EventHandler onpeeridentity;
  [Pref="media.peerconnection.identity.enabled"]
  attribute EventHandler onidpassertionerror;
  [Pref="media.peerconnection.identity.enabled"]
  attribute EventHandler onidpvalidationerror;
};

