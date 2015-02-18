/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsISupports;

[ChromeOnly,
 JSImplementation="@mozilla.org/dom/peerconnectionobserver;1",
 Constructor (mozRTCPeerConnection domPC)]
interface PeerConnectionObserver
{
  /* JSEP callbacks */
  void onCreateOfferSuccess(DOMString offer);
  void onCreateOfferError(unsigned long name, DOMString message);
  void onCreateAnswerSuccess(DOMString answer);
  void onCreateAnswerError(unsigned long name, DOMString message);
  void onSetLocalDescriptionSuccess();
  void onSetRemoteDescriptionSuccess();
  void onSetLocalDescriptionError(unsigned long name, DOMString message);
  void onSetRemoteDescriptionError(unsigned long name, DOMString message);
  void onAddIceCandidateSuccess();
  void onAddIceCandidateError(unsigned long name, DOMString message);
  void onIceCandidate(unsigned short level, DOMString mid, DOMString candidate);
  void onNegotiationNeeded();

  /* Stats callbacks */
  void onGetStatsSuccess(optional RTCStatsReportInternal report);
  void onGetStatsError(unsigned long name, DOMString message);

  /* replaceTrack callbacks */
  void onReplaceTrackSuccess();
  void onReplaceTrackError(unsigned long name, DOMString message);

  /* Data channel callbacks */
  void notifyDataChannel(DataChannel channel);

  /* Notification of one of several types of state changed */
  void onStateChange(PCObserverStateType state);

  /* Changes to MediaStreamTracks */
  void onAddStream(MediaStream stream);
  void onRemoveStream(MediaStream stream);
  void onAddTrack(MediaStreamTrack track);
  void onRemoveTrack(MediaStreamTrack track);
};
