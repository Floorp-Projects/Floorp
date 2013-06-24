/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsITCPServerSocketInternal;

/**
 * mozTCPServerSocket
 *
 * An interface to a server socket that can accept incoming connections for gaia apps.
 */

enum TCPServerSocketBinaryType {
  "arraybuffer",
  "string"
};

dictionary ServerSocketOptions {
  TCPServerSocketBinaryType binaryType = "string";
};

[Constructor(unsigned short port, optional ServerSocketOptions options, optional unsigned short backlog),
 JSImplementation="@mozilla.org/tcp-server-socket;1", Func="TCPSocketUtils::SocketEnabled",
 Exposed=(Window,System)]
interface mozTCPServerSocket : EventTarget {
  /**
   * The port of this server socket object.
   */
  readonly attribute unsigned short localPort;

  /**
   * The onconnect event handler is called when a client connection is accepted.
   * The socket attribute of the event passed to the onconnect handler will be a TCPSocket
   * instance, which is used for communication between client and server.
   */
  attribute EventHandler onconnect;

  /**
   * The onerror handler will be called when the listen of a server socket is aborted.
   * The data attribute of the event passed to the onerror handler will have a
   * description of the kind of error.
   */
  attribute EventHandler onerror;

  /**
   * Close the server socket.
   */
  void close();

  [ChromeOnly]
  nsITCPServerSocketInternal getInternalSocket();
};
