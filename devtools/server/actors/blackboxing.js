/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { blackboxingSpec } = require("devtools/shared/specs/blackboxing");
const {
  SessionDataHelpers,
} = require("devtools/server/actors/watcher/SessionDataHelpers.jsm");
const { SUPPORTED_DATA } = SessionDataHelpers;
const { BLACKBOXING } = SUPPORTED_DATA;

/**
 * This actor manages the blackboxing of sources.
 *
 * Blackboxing data should be available as early as possible to new targets and
 * will be forwarded to the WatcherActor to populate the shared session data available to
 * all DevTools targets.
 *
 * @constructor
 *
 */
const BlackboxingActor = ActorClassWithSpec(blackboxingSpec, {
  initialize(watcherActor) {
    this.watcherActor = watcherActor;
    Actor.prototype.initialize.call(this, this.watcherActor.conn);
  },

  destroy(conn) {
    Actor.prototype.destroy.call(this, conn);
  },

  /**
   * Request to blackbox a new JS file either completely if no range is passed.
   * Or only a precise subset of lines described by range attribute.
   *
   * @param {String} url
   *                 Mandatory argument to mention what URL of JS file should be blackboxed.
   * @param {Array<Objects>} ranges
   *                 The whole file will be blackboxed if this array is empty.
   *                 Each range is made of an object like this:
   *                 {
   *                   start: { line: 1, column: 1 },
   *                   end: { line: 10, column: 10 },
   *                 }
   */
  blackbox(url, ranges) {
    if (ranges.length == 0) {
      return this.watcherActor.addDataEntry(BLACKBOXING, [
        { url, range: null },
      ]);
    }
    return this.watcherActor.addDataEntry(
      BLACKBOXING,
      ranges.map(range => {
        return {
          url,
          range,
        };
      })
    );
  },

  /**
   * Request to unblackbox some JS sources.
   *
   * See `blackbox` for more info.
   */
  unblackbox(url, ranges) {
    if (ranges.length == 0) {
      const existingRanges = (
        this.watcherActor.getSessionDataForType(BLACKBOXING) || []
      ).filter(entry => entry.url == url);

      return this.watcherActor.removeDataEntry(BLACKBOXING, existingRanges);
    }
    return this.watcherActor.removeDataEntry(
      BLACKBOXING,
      ranges.map(range => {
        return {
          url,
          range,
        };
      })
    );
  },
});

exports.BlackboxingActor = BlackboxingActor;
