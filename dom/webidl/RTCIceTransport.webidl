/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webrtc-pc/#dom-rtcicetransport
 */

enum RTCIceTransportState {
  "closed",
  "failed",
  "disconnected",
  "new",
  "checking",
  "completed",
  "connected"
};

enum RTCIceGathererState {
  "new",
  "gathering",
  "complete"
};

[Exposed=Window]
interface RTCIceTransport : EventTarget {
  // TODO(bug 1307994)
  // readonly attribute RTCIceRole role;
  // readonly attribute RTCIceComponent component;
  readonly attribute RTCIceTransportState state;
  readonly attribute RTCIceGathererState gatheringState;
  // TODO(bug 1307994)
  // sequence<RTCIceCandidate> getLocalCandidates();
  // sequence<RTCIceCandidate> getRemoteCandidates();
  // RTCIceCandidatePair? getSelectedCandidatePair();
  // RTCIceParameters? getLocalParameters();
  // RTCIceParameters? getRemoteParameters();
  attribute EventHandler onstatechange;
  attribute EventHandler ongatheringstatechange;
  // TODO(bug 1307994)
  // attribute EventHandler onselectedcandidatepairchange;
};
