/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  breakpointListSpec,
} = require("resource://devtools/shared/specs/breakpoint-list.js");

const {
  SessionDataHelpers,
} = require("resource://devtools/server/actors/watcher/SessionDataHelpers.jsm");
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
class BreakpointListActor extends Actor {
  constructor(watcherActor) {
    super(watcherActor.conn, breakpointListSpec);
    this.watcherActor = watcherActor;
  }

  setBreakpoint(location, options) {
    return this.watcherActor.addOrSetDataEntry(
      BREAKPOINTS,
      [{ location, options }],
      "add"
    );
  }

  removeBreakpoint(location, options) {
    return this.watcherActor.removeDataEntry(BREAKPOINTS, [
      { location, options },
    ]);
  }

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
    return this.watcherActor.addOrSetDataEntry(
      XHR_BREAKPOINTS,
      [{ path, method }],
      "add"
    );
  }

  /**
   * Stop breakpoint on requests we ask to break on via setXHRBreakpoint.
   *
   * See setXHRBreakpoint for arguments definition.
   */
  removeXHRBreakpoint(path, method) {
    return this.watcherActor.removeDataEntry(XHR_BREAKPOINTS, [
      { path, method },
    ]);
  }

  /**
   * Set the active breakpoints
   *
   * @param {Array<String>} ids
   *                        An array of eventlistener breakpoint ids. These
   *                        are unique identifiers for event breakpoints.
   *                        See devtools/server/actors/utils/event-breakpoints.js
   *                        for details.
   */
  async setActiveEventBreakpoints(ids) {
    await this.watcherActor.addOrSetDataEntry(EVENT_BREAKPOINTS, ids, "set");
  }
}

exports.BreakpointListActor = BreakpointListActor;
