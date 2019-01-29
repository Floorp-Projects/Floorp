/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Telemetry = require("devtools/client/shared/telemetry");
loader.lazyGetter(this, "telemetry", () => new Telemetry());
// This is a unique id that should be submitted with all about:debugging events.
loader.lazyGetter(this, "sessionId", () => parseInt(telemetry.msSinceProcessStart(), 10));

const {
  SELECT_PAGE_SUCCESS,
  TELEMETRY_RECORD,
} = require("../constants");

function recordEvent(method, details) {
  // Add the session id to the event details.
  const eventDetails = Object.assign({}, details, { "session_id": sessionId });
  telemetry.recordEvent(method, "aboutdebugging", null, eventDetails);
}

/**
 * This middleware will record events to telemetry for some specific actions.
 */
function eventRecordingMiddleware() {
  return next => action => {
    switch (action.type) {
      case SELECT_PAGE_SUCCESS:
        recordEvent("select_page", { "page_type": action.page });
        break;
      case TELEMETRY_RECORD:
        const { method, details } = action;
        if (method) {
          recordEvent(method, details);
        } else {
          console.error(`[RECORD EVENT FAILED] ${action.type}: no "method" property`);
        }
        break;
    }

    return next(action);
  };
}

module.exports = eventRecordingMiddleware;
