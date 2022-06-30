/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { breakpointListSpec } = require("devtools/shared/specs/breakpoint-list");
const {
  SessionDataHelpers,
} = require("devtools/server/actors/watcher/SessionDataHelpers.jsm");
const { SUPPORTED_DATA } = SessionDataHelpers;
const { BREAKPOINTS, XHR_BREAKPOINTS, EVENT_BREAKPOINTS } = SUPPORTED_DATA;

/**
 * This actor manages the breakpoints list.
 *
 * Breakpoints should be available as early as possible to new targets and
 * will be forwarded to the WatcherActor to populate the shared session data available to
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

  /**
   * Request to break on next XHR or Fetch request for a given URL and HTTP Method.
   *
   * @param {String} path
   *                 If empty, will pause on regardless or the request's URL.
   *                 Otherwise, will pause on any request whose URL includes this string.
   *                 This is not specific to URL's path. It can match the URL origin.
   * @param {String} method
   *                 If set to "ANY", will pause regardless of which method is used.
   *                 Otherwise, should be set to any valid HTTP Method (GET, POST, ...)
   */
  setXHRBreakpoint(path, method) {
    return this.watcherActor.addDataEntry(XHR_BREAKPOINTS, [{ path, method }]);
  },

  /**
   * Stop breakpoint on requests we ask to break on via setXHRBreakpoint.
   *
   * See setXHRBreakpoint for arguments definition.
   */
  removeXHRBreakpoint(path, method) {
    return this.watcherActor.removeDataEntry(XHR_BREAKPOINTS, [
      { path, method },
    ]);
  },

  /**
   * Set the active breakpoints
   *
   * @param {Array<String>} ids
   *                        An array of eventlistener breakpoint ids. These
   *                        are unique identifiers for event breakpoints.
   *                        See devtools/server/actors/utils/event-breakpoints.js
   *                        for details.
   */
  setActiveEventBreakpoints(ids) {
    const existingIds =
      this.watcherActor.getSessionDataForType(EVENT_BREAKPOINTS) || [];
    const addIds = ids.filter(id => !existingIds.includes(id));
    const removeIds = existingIds.filter(id => !ids.includes(id));

    if (addIds.length) {
      this.watcherActor.addDataEntry(EVENT_BREAKPOINTS, addIds);
    }
    if (removeIds.length) {
      this.watcherActor.removeDataEntry(EVENT_BREAKPOINTS, removeIds);
    }
  },
});

exports.BreakpointListActor = BreakpointListActor;
