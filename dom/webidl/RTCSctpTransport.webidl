/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webrtc-pc/#dom-rtcsctptransport
 */

enum RTCSctpTransportState {
  "connecting",
  "connected",
  "closed"
};

[Pref="media.peerconnection.enabled",
 Exposed=Window]
interface RTCSctpTransport : EventTarget {
  readonly attribute RTCDtlsTransport transport;
  readonly attribute RTCSctpTransportState state;
  readonly attribute unrestricted double maxMessageSize;
  readonly attribute unsigned short? maxChannels;
  attribute EventHandler onstatechange;
};
