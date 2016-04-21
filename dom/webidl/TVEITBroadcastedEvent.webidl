/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://seanyhlin.github.io/TV-Manager-API/
 */

dictionary TVEITBroadcastedEventInit : EventInit {
  sequence<TVProgram> programs = [];
};

[Pref="dom.tv.enabled",
 ChromeOnly,
 Constructor(DOMString type, optional TVEITBroadcastedEventInit eventInitDict)]
interface TVEITBroadcastedEvent : Event {
  [Pure, Cached] readonly attribute sequence<TVProgram> programs;
};
