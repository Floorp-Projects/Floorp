/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsIDOMTCPSocket;

[Constructor(DOMString type, optional TCPServerSocketEventInit eventInitDict),
 Func="TCPSocketUtils::SocketEnabled", Exposed=(Window,System)]
interface TCPServerSocketEvent : Event {
  readonly attribute mozTCPSocket socket;
};

dictionary TCPServerSocketEventInit : EventInit {
  mozTCPSocket? socket = null;
};
