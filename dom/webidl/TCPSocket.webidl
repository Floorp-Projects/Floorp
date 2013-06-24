/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsITCPSocketInternal;

/**
 * MozTCPSocket exposes a TCP client socket (no server sockets yet)
 * to highly privileged apps. It provides a buffered, non-blocking
 * interface for sending. For receiving, it uses an asynchronous,
 * event handler based interface.
 */

enum TCPSocketBinaryType {
  "arraybuffer",
  "string"
};

dictionary SocketOptions {
  boolean useSSL = false;
  TCPSocketBinaryType binaryType = "string";
  boolean doNotConnect = false;
};

[Constructor(DOMString host, unsigned short port, optional SocketOptions options),
 Func="TCPSocketUtils::SocketEnabled",
 JSImplementation="@mozilla.org/tcp-socket;1", Exposed=(Window,System)]
interface mozTCPSocket : EventTarget {
  /**
   * The host of this socket object.
   */
  readonly attribute USVString host;

  /**
   * The port of this socket object.
   */
  readonly attribute unsigned short port;

  /**
   * True if this socket object is an SSL socket.
   */
  readonly attribute boolean ssl;

  /**
   * The number of bytes which have previously been buffered by calls to
   * send on this socket.
   */
  readonly attribute unsigned long bufferedAmount;

  /**
   * Pause reading incoming data and invocations of the ondata handler until
   * resume is called.
   */
  void suspend();

  /**
   * Resume reading incoming data and invoking ondata as usual.
   */
  void resume();

  /**
   * Close the socket.
   */
  void close();

  /**
   * Write data to the socket.
   *
   * @param data The data to write to the socket. If
   *             binaryType: "arraybuffer" was passed in the options
   *             object, then this object should be an ArrayBuffer instance.
   *             If binaryType: "string" was passed, or if no binaryType
   *             option was specified, then this object should be an
   *             ordinary JavaScript string.
   * @param byteOffset The offset within the data from which to begin writing.
   *                   Has no effect on non-ArrayBuffer data.
   * @param byteLength The number of bytes to write. Has no effect on
   *                   non-ArrayBuffer data.
   *
   * @return Send returns true or false as a hint to the caller that
   *         they may either continue sending more data immediately, or
   *         may want to wait until the other side has read some of the
   *         data which has already been written to the socket before
   *         buffering more. If send returns true, then less than 64k
   *         has been buffered and it's safe to immediately write more.
   *         If send returns false, then more than 64k has been buffered,
   *         and the caller may wish to wait until the ondrain event
   *         handler has been called before buffering more data by more
   *         calls to send.
   */
  boolean send((USVString or ArrayBuffer) data, optional unsigned long byteOffset, optional unsigned long byteLength);

  /**
   * The readyState attribute indicates which state the socket is currently
   * in. The state will be either "connecting", "open", "closing", or "closed".
   */
  readonly attribute DOMString readyState;

  /**
   * The binaryType attribute indicates which mode this socket uses for
   * sending and receiving data. If the binaryType: "arraybuffer" option
   * was passed to the open method that created this socket, binaryType
   * will be "arraybuffer". Otherwise, it will be "string".
   */
  readonly attribute TCPSocketBinaryType binaryType;

  /**
   * The onopen event handler is called when the connection to the server
   * has been established. If the connection is refused, onerror will be
   * called, instead.
   */
  attribute EventHandler onopen;

  /**
   * After send has buffered more than 64k of data, it returns false to
   * indicate that the client should pause before sending more data, to
   * avoid accumulating large buffers. This is only advisory, and the client
   * is free to ignore it and buffer as much data as desired, but if reducing
   * the size of buffers is important (especially for a streaming application)
   * ondrain will be called once the previously-buffered data has been written
   * to the network, at which point the client can resume calling send again.
   */
  attribute EventHandler ondrain;

  /**
   * The ondata handler will be called repeatedly and asynchronously after
   * onopen has been called, every time some data was available from the server
   * and was read. If binaryType: "arraybuffer" was passed to open, the data
   * attribute of the event object will be an ArrayBuffer. If not, it will be a
   * normal JavaScript string.
   *
   * At any time, the client may choose to pause reading and receiving ondata
   * callbacks, by calling the socket's suspend() method. Further invocations
   * of ondata will be paused until resume() is called.
   */
  attribute EventHandler ondata;

  /**
   * The onerror handler will be called when there is an error. The data
   * attribute of the event passed to the onerror handler will have a
   * description of the kind of error.
   *
   * If onerror is called before onopen, the error was connection refused,
   * and onclose will not be called. If onerror is called after onopen,
   * the connection was lost, and onclose will be called after onerror.
   */
  attribute EventHandler onerror;

  /**
   * The onclose handler is called once the underlying network socket
   * has been closed, either by the server, or by the client calling
   * close.
   *
   * If onerror was not called before onclose, then either side cleanly
   * closed the connection.
   */
  attribute EventHandler onclose;

  [ChromeOnly]
  nsITCPSocketInternal getInternalSocket();
};
