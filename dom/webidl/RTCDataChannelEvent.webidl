/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/2011/webrtc/editor/webrtc.html#idl-def-RTCDataChannelEvent
 */

interface RTCDataChannel;

dictionary RTCDataChannelEventInit : EventInit {
    RTCDataChannel? channel = null;
};

[Pref="media.peerconnection.enabled",
 JSImplementation="@mozilla.org/dom/datachannelevent;1",
 Constructor(DOMString type, optional RTCDataChannelEventInit eventInitDict)]
interface RTCDataChannelEvent : Event {
  readonly attribute RTCDataChannel? channel;
};
