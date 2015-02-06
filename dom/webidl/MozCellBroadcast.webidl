/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Cell Broadcast short message service (CBS) permits a number of
 * unacknowledged general CBS messages to be broadcast to all receivers within
 * a particular region.
 */
[Pref="dom.cellbroadcast.enabled",
 CheckPermissions="cellbroadcast",
 AvailableIn="CertifiedApps"]
interface MozCellBroadcast : EventTarget
{
  /**
   * Cell Broadcast messages received.
   */
  attribute EventHandler onreceived;
};
