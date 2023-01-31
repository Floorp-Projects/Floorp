/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");

/**
 * A helper class that is listened to by the ChangesActor, and can be
 * used to send changes to the ChangesActor.
 */
class TrackChangeEmitter extends EventEmitter {
  trackChange(change) {
    this.emit("track-change", change);
  }
}

module.exports = new TrackChangeEmitter();
