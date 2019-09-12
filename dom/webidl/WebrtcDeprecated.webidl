/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file includes all the deprecated mozRTC prefixed interfaces.
 *
 * The declaration of each should match the declaration of the real, unprefixed
 * interface.  These aliases will be removed at some point (Bug 1155923).
 */

[Deprecated="WebrtcDeprecatedPrefix",
 Pref="media.peerconnection.enabled",
 JSImplementation="@mozilla.org/dom/rtcicecandidate;1"]
interface mozRTCIceCandidate : RTCIceCandidate {
  [Throws]
  constructor(optional RTCIceCandidateInit candidateInitDict = {});
};

[Deprecated="WebrtcDeprecatedPrefix",
 Pref="media.peerconnection.enabled",
 JSImplementation="@mozilla.org/dom/peerconnection;1"]
interface mozRTCPeerConnection : RTCPeerConnection {
  [Throws]
  constructor(optional RTCConfiguration configuration = {},
              optional object? constraints);
};

[Deprecated="WebrtcDeprecatedPrefix",
 Pref="media.peerconnection.enabled",
 JSImplementation="@mozilla.org/dom/rtcsessiondescription;1"]
interface mozRTCSessionDescription : RTCSessionDescription {
  [Throws]
  constructor(optional RTCSessionDescriptionInit descriptionInitDict = {});
};
