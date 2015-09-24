/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file includes all the deprecated mozRTC prefixed interfaces.
 *
 * The declaration of each should match the declaration of the real, unprefixed
 * interface.
 */

[NoInterfaceObject]
interface WebrtcDeprecated
{
  [Deprecated="WebrtcDeprecatedPrefix", Pref="media.peerconnection.enabled"]
  readonly attribute object mozRTCIceCandidate;
  [Deprecated="WebrtcDeprecatedPrefix", Pref="media.peerconnection.enabled"]
  readonly attribute object mozRTCPeerConnection;
  [Deprecated="WebrtcDeprecatedPrefix", Pref="media.peerconnection.enabled"]
  readonly attribute object mozRTCSessionDescription;
};
