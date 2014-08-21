/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://seanyhlin.github.io/TV-Manager-API/
 */

enum TVChannelType {
  "tv",
  "radio",
  "data"
};

dictionary TVGetProgramsOptions {
  unsigned long long startTime;
  unsigned long long duration;
};

[Pref="dom.tv.enabled", CheckPermissions="tv", Func="Navigator::HasTVSupport"]
interface TVChannel : EventTarget {
  [Throws]
  Promise<sequence<TVProgram>> getPrograms(optional TVGetProgramsOptions options);

  [Throws]
  Promise<TVProgram> getCurrentProgram();

  readonly attribute DOMString networkId;

  readonly attribute DOMString transportStreamId;

  readonly attribute DOMString serviceId;

  readonly attribute TVSource source;

  readonly attribute TVChannelType type;

  readonly attribute DOMString name;

  readonly attribute DOMString number;

  readonly attribute boolean isEmergency;

  readonly attribute boolean isFree;
};
