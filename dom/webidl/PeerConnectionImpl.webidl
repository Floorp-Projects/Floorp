/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * PeerConnection.js' interface to the C++ PeerConnectionImpl.
 *
 * Do not confuse with RTCPeerConnection. This interface is purely for
 * communication between the PeerConnection JS DOM binding and the C++
 * implementation.
 *
 * See media/webrtc/signaling/include/PeerConnectionImpl.h
 *
 */

interface nsISupports;

/* Must be created first. Observer events will be dispatched on the thread provided */
[ChromeOnly,
 Exposed=Window]
interface PeerConnectionImpl  {
  constructor();

  /* Must be called first. Observer events dispatched on the thread provided */
  [Throws]
  void initialize(PeerConnectionObserver observer, Window window,
                  RTCConfiguration iceServers,
                  nsISupports thread);

  /* JSEP calls */
  [Throws]
  void createOffer(optional RTCOfferOptions options = {});
  [Throws]
  void createAnswer();
  [Throws]
  void setLocalDescription(long action, DOMString sdp);
  [Throws]
  void setRemoteDescription(long action, DOMString sdp);

  Promise<RTCStatsReport> getStats(MediaStreamTrack? selector);

  sequence<MediaStream> getRemoteStreams();

  /* Adds the tracks created by GetUserMedia */
  [Throws]
  TransceiverImpl createTransceiverImpl(DOMString kind,
                                        MediaStreamTrack? track);
  [Throws]
  boolean checkNegotiationNeeded();

  [Throws]
  void replaceTrackNoRenegotiation(TransceiverImpl transceiverImpl,
                                   MediaStreamTrack? withTrack);
  [Throws]
  void closeStreams();

  [Throws]
  void enablePacketDump(unsigned long level,
                        mozPacketDumpType type,
                        boolean sending);

  [Throws]
  void disablePacketDump(unsigned long level,
                         mozPacketDumpType type,
                         boolean sending);

  /* As the ICE candidates roll in this one should be called each time
   * in order to keep the candidate list up-to-date for the next SDP-related
   * call PeerConnectionImpl does not parse ICE candidates, just sticks them
   * into the SDP.
   */
  [Throws]
  void addIceCandidate(DOMString candidate,
                       DOMString mid,
                       DOMString ufrag,
                       unsigned short? level);

  /* Shuts down threads, deletes state */
  [Throws]
  void close();

  /* Notify DOM window if this plugin crash is ours. */
  boolean pluginCrash(unsigned long long pluginId, DOMString name);

  /* Attributes */
  /* This provides the implementation with the certificate it uses to
   * authenticate itself.  The JS side must set this before calling
   * createOffer/createAnswer or retrieving the value of fingerprint.  This has
   * to be delayed because generating the certificate takes some time. */
  attribute RTCCertificate certificate;
  [Constant]
  readonly attribute DOMString fingerprint;
  readonly attribute DOMString currentLocalDescription;
  readonly attribute DOMString pendingLocalDescription;
  readonly attribute DOMString currentRemoteDescription;
  readonly attribute DOMString pendingRemoteDescription;
  readonly attribute boolean? currentOfferer;
  readonly attribute boolean? pendingOfferer;

  readonly attribute RTCIceConnectionState iceConnectionState;
  readonly attribute RTCIceGatheringState iceGatheringState;
  readonly attribute RTCSignalingState signalingState;
  attribute DOMString id;

  [SetterThrows]
  attribute DOMString peerIdentity;
  readonly attribute boolean privacyRequested;

  /* Data channels */
  [Throws]
  RTCDataChannel createDataChannel(DOMString label, DOMString protocol,
    unsigned short type, boolean ordered,
    unsigned short maxTime, unsigned short maxNum,
    boolean externalNegotiated, unsigned short stream);
};
