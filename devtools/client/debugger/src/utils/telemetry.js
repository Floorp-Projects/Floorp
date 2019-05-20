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
 *  telemetry.preparePendingEvent(this, "pause", "debugger", null, [
 *   "reason", "collapsed_callstacks"
 * ]);
 *
 * // Elsewhere in another codepath send the reason property
 * telemetry.addEventProperty(
 *   this, "pause", "debugger", null, "reason", "debugger-statement"
 * );
 *
 * // Elsewhere in another codepath send the collapsed_callstacks property
 * telemetry.addEventProperty(
 *   this, "pause", "debugger", null, "collapsed_callstacks", 1
 * );
 */

// @flow

import { Telemetry } from "devtools-modules";
import { isFirefoxPanel } from "devtools-environment";

const telemetry = new Telemetry();

/**
 * @memberof utils/telemetry
 * @static
 */
export function recordEvent(eventName: string, fields: {} = {}) {
  let sessionId = -1;

  if (typeof window !== "object") {
    return;
  }

  if (window.parent.frameElement) {
    sessionId = window.parent.frameElement.getAttribute("session_id");
  }

  /* eslint-disable camelcase */
  telemetry.recordEvent(eventName, "debugger", null, {
    session_id: sessionId,
    ...fields,
  });
  /* eslint-enable camelcase */

  if (!isFirefoxPanel() && window.dbg) {
    const events = window.dbg._telemetry.events;
    if (!events[eventName]) {
      events[eventName] = [];
    }
    events[eventName].push(fields);
  }
}
