/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  blackboxingSpec,
} = require("resource://devtools/shared/specs/blackboxing.js");

const {
  SessionDataHelpers,
} = require("resource://devtools/server/actors/watcher/SessionDataHelpers.jsm");
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
class BlackboxingActor extends Actor {
  constructor(watcherActor) {
    super(watcherActor.conn, blackboxingSpec);
    this.watcherActor = watcherActor;
  }

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
    if (!ranges.length) {
      return this.watcherActor.addOrSetDataEntry(
        BLACKBOXING,
        [{ url, range: null }],
        "add"
      );
    }
    return this.watcherActor.addOrSetDataEntry(
      BLACKBOXING,
      ranges.map(range => {
        return {
          url,
          range,
        };
      }),
      "add"
    );
  }

  /**
   * Request to unblackbox some JS sources.
   *
   * See `blackbox` for more info.
   */
  unblackbox(url, ranges) {
    if (!ranges.length) {
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
  }
}

exports.BlackboxingActor = BlackboxingActor;
