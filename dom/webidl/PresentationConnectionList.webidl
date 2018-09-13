/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/presentation-api/#interface-presentationconnectionlist
 */

[Pref="dom.presentation.receiver.enabled"]
interface PresentationConnectionList : EventTarget {
  /*
   * Return the non-terminated set of presentation connections in the
   * set of presentation controllers.
   * TODO: Use FrozenArray once available. (Bug 1236777)
   * readonly attribute FrozenArray<PresentationConnection> connections;
   */
  [Frozen, Cached, Pure]
  readonly attribute sequence<PresentationConnection> connections;

  /*
   * It is called when an incoming connection is connected.
   */
  attribute EventHandler onconnectionavailable;
};
