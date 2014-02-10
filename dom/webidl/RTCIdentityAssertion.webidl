/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/2013/WD-webrtc-20130910/#idl-def-RTCIdentityAssertion
 */

[Pref="media.peerconnection.identity.enabled",
 JSImplementation="@mozilla.org/dom/rtcidentityassertion;1",
 Constructor(DOMString idp, DOMString name)]
interface RTCIdentityAssertion {
  attribute DOMString idp;
  attribute DOMString name;
};
