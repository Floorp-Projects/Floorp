/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webrtc-pc/#rtcicecandidate-interface
 */

dictionary RTCIceCandidateInit {
  DOMString candidate = "";
  DOMString? sdpMid = null;
  unsigned short? sdpMLineIndex = null;
  DOMString? usernameFragment = null;
};

enum RTCIceComponent {
  "rtp",
  "rtcp"
};

enum RTCIceProtocol {
  "udp",
  "tcp"
};

enum RTCIceCandidateType {
  "host",
  "srflx",
  "prflx",
  "relay"
};

enum RTCIceTcpCandidateType {
  "active",
  "passive",
  "so"
};

[Pref="media.peerconnection.enabled",
 JSImplementation="@mozilla.org/dom/rtcicecandidate;1",
 Exposed=Window]
interface RTCIceCandidate {
  [Throws]
  constructor(optional RTCIceCandidateInit candidateInitDict = {});
  readonly attribute DOMString candidate;
  readonly attribute DOMString? sdpMid;
  readonly attribute unsigned short? sdpMLineIndex;
  readonly attribute DOMString? foundation;
  readonly attribute RTCIceComponent? component;
  readonly attribute unsigned long? priority;
  readonly attribute DOMString? address;
  readonly attribute RTCIceProtocol? protocol;
  readonly attribute unsigned short? port;
  readonly attribute RTCIceCandidateType? type;
  readonly attribute RTCIceTcpCandidateType? tcpType;
  readonly attribute DOMString? relatedAddress;
  readonly attribute unsigned short? relatedPort;
  readonly attribute DOMString? usernameFragment;
  // TODO: add remaining members relayProtocol and url (bug 1886013)
  // readonly attribute RTCIceServerTransportProtocol? relayProtocol;
  // readonly attribute DOMString? url;
  RTCIceCandidateInit toJSON();
};
