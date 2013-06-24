/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TCPSocketEvent is the event object which is passed as the
 * first argument to all the event handler callbacks. It contains
 * the socket that was associated with the event, the type of event,
 * and the data associated with the event (if any).
 */

[Constructor(DOMString type, optional TCPSocketEventInit eventInitDict),
 Func="TCPSocketUtils::SocketEnabled", Exposed=(Window,System)]
interface TCPSocketEvent : Event {
  /**
   * The data related to this event, if any. In the ondata callback,
   * data will be the bytes read from the network; if the binaryType
   * of the socket was "arraybuffer", this value will be of type ArrayBuffer;
   * otherwise, it will be a normal JavaScript string.
   *
   * In the onerror callback, data will be a string with a description
   * of the error.
   *
   * In the other callbacks, data will be an empty string.
   */
  //TODO: make this (ArrayBuffer or DOMString) after sorting out the rooting required. bug 1121634
  readonly attribute any data;
};

dictionary TCPSocketEventInit : EventInit {
  //TODO: make this (ArrayBuffer or DOMString) after sorting out the rooting required. bug 1121634
  any data = null;
};
