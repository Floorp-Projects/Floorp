/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.presentation.enabled",
 CheckAnyPermissions="presentation",
 AvailableIn="PrivilegedApps"]
interface Presentation : EventTarget {
  /*
   * A requesting page use startSession() to start a new session, and the
   * session will be returned with the promise. UA may show a prompt box with a
   * list of available devices and ask the user to grant permission, choose a
   * device, or cancel the operation.
   *
   * @url: The URL of presenting page.
   * @sessionId: Optional. If it's not specified, a random alphanumeric value of
   *             at least 16 characters drawn from the character [A-Za-z0-9] is
   *             automatically generated as the id of the session.
   *
   * The promise is resolved when the presenting page is successfully loaded and
   * the communication channel is established, i.e., the session state is
   * "connected".
   *
   * The promise may be rejected duo to one of the following reasons:
   * - "InternalError":        Unexpected internal error occurs.
   * - "NoDeviceAvailable":    No available device.
   * - "PermissionDenied":     User dismiss the device prompt box.
   * - "ControlChannelFailed": Failed to establish control channel.
   * - "NoApplicationFound":  app:// scheme is supported on Firefox OS, but no
   *                           corresponding application is found on remote side.
   * - "PageLoadTimeout":      Presenting page takes too long to load.
   * - "DataChannelFailed":    Failed to establish data channel.
   */
  [Throws]
  Promise<PresentationSession> startSession(DOMString url,
                                            optional DOMString sessionId);

  /*
   * This attribute is only available on the presenting page. It should be
   * created when loading the presenting page, and it's ready to be used after
   * 'onload' event is dispatched.
   */
  [Pure]
  readonly attribute PresentationSession? session;

 /*
  * Device availability. If there is more than one device discovered by UA,
  * the value is |true|. Otherwise, its value should be |false|.
  *
  * UA triggers device discovery mechanism periodically and cache the latest
  * result in this attribute. Thus, it may be out-of-date when we're not in
  * discovery mode, however, it is still useful to give the developers an idea
  * that whether there are devices nearby some time ago.
  */
  readonly attribute boolean cachedAvailable;

  /*
   * It is called when device availability changes. New value is dispatched with
   * the event.
   */
  attribute EventHandler onavailablechange;
};
