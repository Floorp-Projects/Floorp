/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/presentation-api/#interface-presentationavailability
 */

[Pref="dom.presentation.controller.enabled"]
interface PresentationAvailability : EventTarget {
  /*
   * If there is at least one device discovered by UA, the value is |true|.
   * Otherwise, its value should be |false|.
   */
  readonly attribute boolean value;

  /*
   * It is called when device availability changes.
   */
  attribute EventHandler onchange;
};
