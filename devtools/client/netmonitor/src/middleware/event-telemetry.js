/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TOGGLE_REQUEST_FILTER_TYPE,
  ENABLE_REQUEST_FILTER_TYPE_ONLY,
  SET_REQUEST_FILTER_TEXT,
  SELECT_DETAILS_PANEL_TAB,
  SEND_CUSTOM_REQUEST,
  ENABLE_PERSISTENT_LOGS,
  MSG_SELECT,
} = require("resource://devtools/client/netmonitor/src/constants.js");

const {
  CHANGE_NETWORK_THROTTLING,
} = require("resource://devtools/client/shared/components/throttling/actions.js");

/**
 * Event telemetry middleware is responsible for logging
 * various events to telemetry. This helps to track Network
 * panel usage.
 */
function eventTelemetryMiddleware(connector, telemetry) {
  return store => next => action => {
    const oldState = store.getState();
    const res = next(action);
    const toolbox = connector.getToolbox();
    if (!toolbox) {
      return res;
    }

    if (action.skipTelemetry) {
      return res;
    }

    const state = store.getState();

    const filterChangeActions = [
      TOGGLE_REQUEST_FILTER_TYPE,
      ENABLE_REQUEST_FILTER_TYPE_ONLY,
      SET_REQUEST_FILTER_TEXT,
    ];

    // Record telemetry event when filter changes.
    if (filterChangeActions.includes(action.type)) {
      filterChange({
        action,
        state,
        oldState,
        telemetry,
      });
    }

    // Record telemetry event when side panel is selected.
    if (action.type == SELECT_DETAILS_PANEL_TAB) {
      sidePanelChange({
        state,
        oldState,
        telemetry,
      });
    }

    // Record telemetry event when a request is resent.
    if (action.type == SEND_CUSTOM_REQUEST) {
      sendCustomRequest({
        telemetry,
      });
    }

    // Record telemetry event when throttling is changed.
    if (action.type == CHANGE_NETWORK_THROTTLING) {
      throttlingChange({
        action,
        telemetry,
      });
    }

    // Record telemetry event when log persistence changes.
    if (action.type == ENABLE_PERSISTENT_LOGS) {
      persistenceChange({
        telemetry,
        state,
      });
    }

    // Record telemetry event when message is selected.
    if (action.type == MSG_SELECT) {
      selectMessage({
        telemetry,
      });
    }

    return res;
  };
}

/**
 * This helper function is executed when filter related action is fired.
 * It's responsible for recording "filters_changed" telemetry event.
 */
function filterChange({ action, state, oldState, telemetry }) {
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
  if (
    action.type === TOGGLE_REQUEST_FILTER_TYPE ||
    action.type === ENABLE_REQUEST_FILTER_TYPE_ONLY
  ) {
    trigger = action.filter;
  } else if (action.type === SET_REQUEST_FILTER_TEXT) {
    if (
      oldFilterState.requestFilterText !== "" &&
      filterState.requestFilterText !== ""
    ) {
      return;
    }

    trigger = "text";
  }

  telemetry.recordEvent("filters_changed", "netmonitor", null, {
    trigger,
    active: activeFilters.join(","),
    inactive: inactiveFilters.join(","),
  });
}

/**
 * This helper function is executed when side panel is selected.
 * It's responsible for recording "sidepanel_tool_changed"
 * telemetry event.
 */
function sidePanelChange({ state, oldState, telemetry }) {
  telemetry.recordEvent("sidepanel_changed", "netmonitor", null, {
    oldpanel: oldState.ui.detailsPanelSelectedTab,
    newpanel: state.ui.detailsPanelSelectedTab,
  });
}

/**
 * This helper function is executed when a request is resent.
 * It's responsible for recording "edit_resend" telemetry event.
 */
function sendCustomRequest({ telemetry }) {
  telemetry.recordEvent("edit_resend", "netmonitor");
}

/**
 * This helper function is executed when network throttling is changed.
 * It's responsible for recording "throttle_changed" telemetry event.
 */
function throttlingChange({ action, telemetry }) {
  telemetry.recordEvent("throttle_changed", "netmonitor", null, {
    mode: action.profile,
  });
}

/**
 * This helper function is executed when log persistence is changed.
 * It's responsible for recording "persist_changed" telemetry event.
 */
function persistenceChange({ telemetry, state }) {
  telemetry.recordEvent(
    "persist_changed",
    "netmonitor",
    String(state.ui.persistentLogsEnabled)
  );
}

/**
 * This helper function is executed when a WS frame is selected.
 * It's responsible for recording "select_ws_frame" telemetry event.
 */
function selectMessage({ telemetry }) {
  telemetry.recordEvent("select_ws_frame", "netmonitor");
}

module.exports = eventTelemetryMiddleware;
