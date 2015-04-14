/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Reference https://wiki.mozilla.org/Input_Port_API#Basic_Port_Interface
 */

[Pref="dom.inputport.enabled", CheckPermissions="inputport", AvailableIn=CertifiedApps]
interface InputPort : EventTarget {
  readonly attribute DOMString id;
  readonly attribute MediaStream stream;
  readonly attribute boolean connected;
  attribute EventHandler onconnect;
  attribute EventHandler ondisconnect;
};
