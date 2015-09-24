/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.presentation.enabled",
 CheckAnyPermissions="presentation"]
interface PresentationReceiver : EventTarget {
  /*
   * Get the first connected presentation session in a receiving browsing
   * context.
   */
  [Throws]
  Promise<PresentationSession> getSession();

  /*
   * Get all connected presentation sessions in a receiving browsing context.
   */
  [Throws]
  Promise<sequence<PresentationSession>> getSessions();

  /*
   * It is called when an incoming session is connecting.
   */
  attribute EventHandler onsessionavailable;
};
