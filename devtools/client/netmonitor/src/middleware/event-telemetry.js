/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { gDevTools } = require("devtools/client/framework/devtools");

const {
  TOGGLE_REQUEST_FILTER_TYPE,
  ENABLE_REQUEST_FILTER_TYPE_ONLY,
  SET_REQUEST_FILTER_TEXT,
} = require("../constants");

/**
 * Event telemetry middleware is responsible for logging
 * specific filter events to telemetry. This telemetry
 * helps to track Net panel filtering usage.
 */
function eventTelemetryMiddleware(connector, telemetry) {
  return store => next => action => {
    const oldState = store.getState();
    const res = next(action);
    const toolbox = gDevTools.getToolbox(connector.getTabTarget());
    if (!toolbox) {
      return res;
    }

    const state = store.getState();

    const filterChangeActions = [
      TOGGLE_REQUEST_FILTER_TYPE,
      ENABLE_REQUEST_FILTER_TYPE_ONLY,
      SET_REQUEST_FILTER_TEXT,
    ];

    if (filterChangeActions.includes(action.type)) {
      filterChange({
        action,
        state,
        oldState,
        telemetry,
        sessionId: toolbox.sessionId,
      });
    }

    return res;
  };
}

function filterChange({action, state, oldState, telemetry, sessionId}) {
  const oldFilterState = oldState.filters;
  const filterState = state.filters;
  const activeFilters = [];
  const inactiveFilters = [];

  for (const [key, value] of Object.entries(filterState.requestFilterTypes)) {
    if (value) {
      activeFilters.push(key);
    } else {
      inactiveFilters.push(key);
    }
  }

  let trigger;
  if (action.type === TOGGLE_REQUEST_FILTER_TYPE ||
      action.type === ENABLE_REQUEST_FILTER_TYPE_ONLY) {
    trigger = action.filter;
  } else if (action.type === SET_REQUEST_FILTER_TEXT) {
    if (oldFilterState.requestFilterText !== "" &&
        filterState.requestFilterText !== "") {
      return;
    }

    trigger = "text";
  }

  telemetry.recordEvent("devtools.main", "filters_changed", "netmonitor", null, {
    "trigger": trigger,
    "active": activeFilters.join(","),
    "inactive": inactiveFilters.join(","),
    "session_id": sessionId
  });
}

module.exports = eventTelemetryMiddleware;
