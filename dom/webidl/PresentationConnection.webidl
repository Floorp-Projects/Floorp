/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum PresentationConnectionState
{
  // Existing presentation, and the communication channel is active.
  "connected",

  // Existing presentation, but the communication channel is inactive.
  "closed",

  // The presentation is nonexistent anymore. It could be terminated manually,
  // or either controlling or receiving browsing context is no longer available.
  "terminated"
};

[Pref="dom.presentation.enabled",
 CheckAnyPermissions="presentation"]
interface PresentationConnection : EventTarget {
  /*
   * Unique id for all existing connections.
   */
  [Constant]
  readonly attribute DOMString id;

  /*
   * @value "connected", "closed", or "terminated".
   */
  readonly attribute PresentationConnectionState state;

  /*
   * It is called when connection state changes.
   */
  attribute EventHandler onstatechange;

  /*
   * After a communication channel has been established between the controlling
   * and receiving context, this function is called to send message out, and the
   * event handler "onmessage" will be invoked at the remote side.
   *
   * This function only works when the state is "connected".
   *
   * TODO bug 1148307 Implement PresentationSessionTransport with DataChannel to
   * support other binary types.
   */
  [Throws]
  void send(DOMString data);

  /*
   * It is triggered when receiving messages.
   */
  attribute EventHandler onmessage;

  /*
   * Both the controlling and receiving browsing context can close the
   * connection. Then the connection state should turn into "closed".
   *
   * This function only works when the state is not "connected".
   */
  // TODO Bug 1210340 - Support close semantics.
  // [Throws]
  // void close();

  /*
   * Both the controlling and receiving browsing context can terminate the
   * connection. Then the connection state should turn into "terminated".
   *
   * This function only works when the state is not "connected".
   */
   [Throws]
   void terminate();
};
