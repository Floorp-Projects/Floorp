/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// MOCK FOR TimelineFront
class TimelineFront {
  constructor(client, { timelineActor }) {}
  start({ withDocLoadingEvents }) {}
  destroy() {}
  on(evt, cb) {}
  off(evt, cb) {}
}

exports.TimelineFront = TimelineFront;
