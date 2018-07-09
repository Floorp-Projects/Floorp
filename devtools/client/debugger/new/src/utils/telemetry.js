"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.recordEvent = recordEvent;

var _telemetry = require("devtools/client/shared/telemetry");

var _telemetry2 = _interopRequireDefault(_telemetry);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Usage:
 *
 * import { recordEvent } from "src/utils/telemetry";
 *
 * // Event without extra properties
 * recordEvent("add_breakpoint");
 *
 * // Event with extra properties
 * recordEvent("pause", {
 *   "reason": "debugger-statement",
 *   "collapsed_callstacks": 1
 * });
 *
 * // If the properties are in multiple code paths and you can't send them all
 * // in one go you will need to use the full telemetry API.
 *
 * import { Telemetry } from "devtools-modules";
 *
 * const telemetry = new Telemetry();
 *
 * // Prepare the event and define which properties to expect.
 * //
 * // NOTE: You CAN send properties before preparing the event.
 * //
 * telemetry.preparePendingEvent("devtools.main", "pause", "debugger", null, [
 *   "reason", "collapsed_callstacks"
 * ]);
 *
 * // Elsewhere in another codepath send the reason property
 * telemetry.addEventProperty(
 *   "devtools.main", "pause", "debugger", null, "reason", "debugger-statement"
 * );
 *
 * // Elsewhere in another codepath send the collapsed_callstacks property
 * telemetry.addEventProperty(
 *   "devtools.main", "pause", "debugger", null, "collapsed_callstacks", 1
 * );
 */
const telemetry = new _telemetry2.default();
/**
 * @memberof utils/telemetry
 * @static
 */

function recordEvent(eventName, fields = {}) {
  let sessionId = -1;

  if (typeof window === "object" && window.parent.frameElement) {
    sessionId = window.parent.frameElement.getAttribute("session_id");
  }
  /* eslint-disable camelcase */


  telemetry.recordEvent("devtools.main", eventName, "debugger", null, {
    session_id: sessionId,
    ...fields
  });
  /* eslint-enable camelcase */
}