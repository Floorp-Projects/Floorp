/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TELEMETRY_RECORD } = require("../constants");

/**
 * If a given event cannot be mapped to an existing action, use this action that will only
 * be processed by the event recording middleware.
 */
function recordTelemetryEvent(method, details) {
  return (dispatch, getState) => {
    dispatch({ type: TELEMETRY_RECORD, method, details });
  };
}

module.exports = {
  recordTelemetryEvent,
};
