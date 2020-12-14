/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { breakpointListSpec } = require("devtools/shared/specs/breakpoint-list");
const {
  WatchedDataHelpers,
} = require("devtools/server/actors/watcher/WatchedDataHelpers.jsm");
const { SUPPORTED_DATA } = WatchedDataHelpers;
const { BREAKPOINTS } = SUPPORTED_DATA;

/**
 * This actor manages the breakpoints list.
 *
 * Breakpoints should be available as early as possible to new targets and
 * will be forwarded to the WatcherActor to populate the shared watcher data available to
 * all DevTools targets.
 *
 * @constructor
 *
 */
const BreakpointListActor = ActorClassWithSpec(breakpointListSpec, {
  initialize(watcherActor) {
    this.watcherActor = watcherActor;
    Actor.prototype.initialize.call(this, this.watcherActor.conn);
  },

  destroy(conn) {
    Actor.prototype.destroy.call(this, conn);
  },

  setBreakpoint(location, options) {
    return this.watcherActor.addDataEntry(BREAKPOINTS, [{ location, options }]);
  },

  removeBreakpoint(location, options) {
    return this.watcherActor.removeDataEntry(BREAKPOINTS, [
      { location, options },
    ]);
  },
});

exports.BreakpointListActor = BreakpointListActor;
