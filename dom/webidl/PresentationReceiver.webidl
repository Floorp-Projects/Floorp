/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.presentation.enabled",
 CheckAnyPermissions="presentation"]
interface PresentationReceiver : EventTarget {
  /*
   * Get the first connected presentation connection in a receiving browsing
   * context.
   */
  [Throws]
  Promise<PresentationConnection> getConnection();

  /*
   * Get all connected presentation connections in a receiving browsing context.
   */
  [Throws]
  Promise<sequence<PresentationConnection>> getConnections();

  /*
   * It is called when an incoming connection is connecting.
   */
  attribute EventHandler onconnectionavailable;
};
