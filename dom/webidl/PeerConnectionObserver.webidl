/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsISupports;

dictionary PCErrorData
{
  required PCError name;
  required DOMString message;
  // Will need to add more stuff (optional) for RTCError
};

[ChromeOnly,
 JSImplementation="@mozilla.org/dom/peerconnectionobserver;1"]
interface PeerConnectionObserver
{
  [Throws]
  constructor(RTCPeerConnection domPC);

  /* JSEP callbacks */
  void onCreateOfferSuccess(DOMString offer);
  void onCreateOfferError(PCErrorData error);
  void onCreateAnswerSuccess(DOMString answer);
  void onCreateAnswerError(PCErrorData error);
  void onSetLocalDescriptionSuccess();
  void onSetRemoteDescriptionSuccess();
  void onSetLocalDescriptionError(PCErrorData error);
  void onSetRemoteDescriptionError(PCErrorData error);
  void onAddIceCandidateSuccess();
  void onAddIceCandidateError(PCErrorData error);
  void onIceCandidate(unsigned short level, DOMString mid, DOMString candidate, DOMString ufrag);

  /* Stats callbacks */
  void onGetStatsSuccess(optional RTCStatsReportInternal report = {});
  void onGetStatsError(DOMString message);

  /* Data channel callbacks */
  void notifyDataChannel(RTCDataChannel channel);

  /* Notification of one of several types of state changed */
  void onStateChange(PCObserverStateType state);

  /* Transceiver management; called when setRemoteDescription causes a
     transceiver to be created on the C++ side */
  void onTransceiverNeeded(DOMString kind, TransceiverImpl transceiverImpl);

  /* DTMF callback */
  void onDTMFToneChange(MediaStreamTrack track, DOMString tone);

  /* Packet dump callback */
  void onPacket(unsigned long level, mozPacketDumpType type, boolean sending,
                ArrayBuffer packet);

  /* Transceiver sync */
  void syncTransceivers();
};
