/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://seanyhlin.github.io/TV-Manager-API/
 */

[Pref="dom.tv.enabled", CheckPermissions="tv", Func="Navigator::HasTVSupport"]
interface TVProgram {
  sequence<DOMString> getAudioLanguages();

  sequence<DOMString> getSubtitleLanguages();

  readonly attribute DOMString eventId;

  readonly attribute TVChannel channel;

  readonly attribute DOMString title;

  readonly attribute unsigned long long startTime;

  readonly attribute unsigned long long duration;

  readonly attribute DOMString? description;

  readonly attribute DOMString? rating;
};
