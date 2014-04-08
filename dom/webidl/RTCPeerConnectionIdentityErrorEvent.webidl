/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Proposed only, not in spec yet:
 * http://lists.w3.org/Archives/Public/public-webrtc/2013Dec/0104.html
 */

dictionary RTCPeerConnectionIdentityErrorEventInit : EventInit {
  DOMString idp = "";
  DOMString protocol = "";
  DOMString? loginUrl = null;
};

[ChromeOnly,
 Constructor(DOMString type,
             optional RTCPeerConnectionIdentityErrorEventInit eventInitDict)]
interface RTCPeerConnectionIdentityErrorEvent : Event {
  readonly attribute DOMString idp;
  readonly attribute DOMString protocol;
  readonly attribute DOMString? loginUrl;
};
