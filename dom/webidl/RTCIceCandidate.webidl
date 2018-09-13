/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2011/webrtc/editor/webrtc.html#idl-def-RTCIceCandidate
 */

dictionary RTCIceCandidateInit {
  required DOMString candidate;
  DOMString? sdpMid = null;
  unsigned short? sdpMLineIndex = null;
};

[Pref="media.peerconnection.enabled",
 JSImplementation="@mozilla.org/dom/rtcicecandidate;1",
 Constructor(RTCIceCandidateInit candidateInitDict)]
interface RTCIceCandidate {
  attribute DOMString       candidate;
  attribute DOMString?      sdpMid;
  attribute unsigned short? sdpMLineIndex;

  [Default] object toJSON();
};
