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
 JSImplementation="@mozilla.org/dom/peerconnectionobserver;1",
 Exposed=Window]
interface PeerConnectionObserver
{
  [Throws]
  constructor(RTCPeerConnection domPC);

  /* JSEP callbacks */
  undefined onCreateOfferSuccess(DOMString offer);
  undefined onCreateOfferError(PCErrorData error);
  undefined onCreateAnswerSuccess(DOMString answer);
  undefined onCreateAnswerError(PCErrorData error);
  undefined onSetDescriptionSuccess();
  undefined onSetDescriptionError(PCErrorData error);
  undefined onAddIceCandidateSuccess();
  undefined onAddIceCandidateError(PCErrorData error);
  undefined onIceCandidate(unsigned short level, DOMString mid, DOMString candidate, DOMString ufrag);

  /* Data channel callbacks */
  undefined notifyDataChannel(RTCDataChannel channel);

  /* Notification of one of several types of state changed */
  undefined onStateChange(PCObserverStateType state);

  /*
    Lets PeerConnectionImpl fire track events on the RTCPeerConnection
  */
  undefined fireTrackEvent(RTCRtpReceiver receiver, sequence<MediaStream> streams);

  /*
    Lets PeerConnectionImpl fire addstream events on the RTCPeerConnection
  */
  undefined fireStreamEvent(MediaStream stream);

  undefined fireNegotiationNeededEvent();

  /* Packet dump callback */
  undefined onPacket(unsigned long level, mozPacketDumpType type, boolean sending,
                     ArrayBuffer packet);
};
