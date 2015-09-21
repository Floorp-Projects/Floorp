/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString url),
 Pref="dom.presentation.enabled",
 CheckAnyPermissions="presentation"]
interface PresentationRequest : EventTarget {
  /*
   * A requesting page use start() to start a new session, and the session will
   * be returned with the promise. UA may show a prompt box with a list of
   * available devices and ask the user to grant permission, choose a device, or
   * cancel the operation.
   *
   * The promise is resolved when the presenting page is successfully loaded and
   * the communication channel is established, i.e., the session state is
   * "connected".
   *
   * The promise may be rejected duo to one of the following reasons:
   * - "OperationError": Unexpected error occurs.
   * - "NotFoundError":  No available device.
   * - "AbortError":     User dismiss/cancel the device prompt box.
   * - "NetworkError":   Failed to establish the control channel or data channel.
   * - "TimeoutError":   Presenting page takes too long to load.
   */
  [Throws]
  Promise<PresentationSession> start();

 /*
  * UA triggers device discovery mechanism periodically and monitor device
  * availability.
  *
  * The promise may be rejected duo to one of the following reasons:
  * - "NotSupportedError": Unable to continuously monitor the availability.
  */
  [Throws]
  Promise<PresentationAvailability> getAvailability();

  /*
   * It is called when a session associated with a PresentationRequest is created.
   * The event is fired for all sessions that are created for the controller.
   */
  attribute EventHandler onsessionconnect;
};
