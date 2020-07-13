/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  START_WORKER,
  UNREGISTER_WORKER,
  UPDATE_SELECTED_PAGE,
} = require("devtools/client/application/src/constants.js");

function eventTelemetryMiddleware(telemetry, sessionId) {
  function recordEvent(method, details = {}) {
    const eventDetails = Object.assign({}, details, { session_id: sessionId });
    telemetry.recordEvent(method, "application", null, eventDetails);
  }

  return store => next => action => {
    switch (action.type) {
      // ui telemetry
      case UPDATE_SELECTED_PAGE:
        recordEvent("select_page", { page_type: action.selectedPage });
        break;
      // service-worker related telemetry
      case UNREGISTER_WORKER:
        recordEvent("unregister_worker");
        break;
      case START_WORKER:
        recordEvent("start_worker");
        break;
    }

    return next(action);
  };
}

module.exports = eventTelemetryMiddleware;
