/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.presentation.enabled",
 CheckAnyPermissions="presentation"]
interface Presentation : EventTarget {
 /*
  * This should be used by the UA as the default presentation request for the
  * controller. When the UA wishes to initiate a PresentationSession on the
  * controller's behalf, it MUST start a presentation session using the default
  * presentation request (as if the controller had called |defaultRequest.start()|).
  *
  * Only used by controlling browsing context (senders).
  */
  attribute PresentationRequest? defaultRequest;

  /*
   * Get the first connected presentation session in a presenting browsing
   * context.
   *
   * Only used by presenting browsing context (receivers).
   */
  [Throws]
  Promise<PresentationSession> getSession();

  /*
   * Get all connected presentation sessions in a presenting browsing context.
   *
   * Only used by presenting browsing context (receivers).
   */
  [Throws]
  Promise<sequence<PresentationSession>> getSessions();

  /*
   * It is called when an incoming session is connecting.
   *
   * Only used by presenting browsing context (receivers).
   */
  attribute EventHandler onsessionavailable;
};
