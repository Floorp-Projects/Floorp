/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://seanyhlin.github.io/TV-Manager-API/
 */

[Pref="dom.tv.enabled", CheckAnyPermissions="tv", AvailableIn=CertifiedApps]
interface TVTuner : EventTarget {
  [Throws]
  sequence<TVSourceType> getSupportedSourceTypes();

  [Throws]
  Promise<sequence<TVSource>> getSources();

  [Throws]
  Promise<void> setCurrentSource(TVSourceType sourceType);

  readonly attribute DOMString id;

  readonly attribute TVSource? currentSource;

  readonly attribute MediaStream? stream;

  attribute EventHandler oncurrentsourcechanged;
};
