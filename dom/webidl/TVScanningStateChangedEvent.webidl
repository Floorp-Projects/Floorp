/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://seanyhlin.github.io/TV-Manager-API/
 */

enum TVScanningState {
  "cleared",
  "scanned",
  "completed",
  "stopped"
};

dictionary TVScanningStateChangedEventInit : EventInit {
  TVScanningState state = "cleared";
  TVChannel? channel = null;
};

[Pref="dom.tv.enabled",
 CheckAnyPermissions="tv",
 AvailableIn=CertifiedApps,
 Constructor(DOMString type, optional TVScanningStateChangedEventInit eventInitDict)]
interface TVScanningStateChangedEvent : Event {
  readonly attribute TVScanningState state;
  readonly attribute TVChannel? channel;
};
