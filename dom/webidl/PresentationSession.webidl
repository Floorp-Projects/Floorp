/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum PresentationSessionState
{
  // Existing presentation, and the communication channel is active.
  "connected",

  // Existing presentation, but the communication channel is inactive.
  "disconnected",

  // The presentation is nonexistent anymore. It could be terminated manually,
  // or either requesting page or presenting page is no longer available.
  "terminated"
};

[Pref="dom.presentation.enabled",
 CheckAnyPermissions="presentation"]
interface PresentationSession : EventTarget {
  /*
   * Unique id for all existing sessions.
   */
  [Constant]
  readonly attribute DOMString id;

  /*
   * Please refer to PresentationSessionStateEvent.webidl for the declaration of
   * PresentationSessionState.
   *
   * @value "connected", "disconnected", or "terminated".
   */
  readonly attribute PresentationSessionState state;

  /*
   * It is called when session state changes. New state is dispatched with the
   * event.
   */
  attribute EventHandler onstatechange;

  /*
   * After a communication channel has been established between the requesting
   * page and the presenting page, send() is called to send message out, and the
   * event handler "onmessage" will be invoked on the remote side.
   *
   * This function only works when state equals "connected".
   *
   * @data: String literal-only for current implementation.
   */
  [Throws]
  void send(DOMString data);

  /*
   * It is triggered when receiving messages.
   */
  attribute EventHandler onmessage;

  /*
   * Both the requesting page and the presenting page can close the session by
   * calling terminate(). Then, the session is destroyed and its state is
   * truned into "terminated". After getting into the state of "terminated",
   * resumeSession() is incapable of re-establishing the connection.
   *
   * This function does nothing if the state has already been "terminated".
   */
  void close();
};
