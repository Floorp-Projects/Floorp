/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TCPServerSocket
 *
 * An interface to a server socket that can accept incoming connections for gaia apps.
 */

dictionary ServerSocketOptions {
  TCPSocketBinaryType binaryType = "string";
};

[Func="mozilla::dom::TCPSocket::ShouldTCPSocketExist",
 Exposed=Window]
interface TCPServerSocket : EventTarget {
  [Throws]
  constructor(unsigned short port, optional ServerSocketOptions options = {},
              optional unsigned short backlog = 0);

   /**
   * The port of this server socket object.
   */
  readonly attribute unsigned short localPort;

  /**
   * The "connect" event is dispatched when a client connection is accepted.
   * The event object will be a TCPServerSocketEvent containing a TCPSocket
   * instance, which is used for communication between client and server.
   */
  attribute EventHandler onconnect;

  /**
   * The "error" event will be dispatched when a listening server socket is
   * unexpectedly disconnected.
   */
  attribute EventHandler onerror;

  /**
   * Close the server socket.
   */
  void close();
};
