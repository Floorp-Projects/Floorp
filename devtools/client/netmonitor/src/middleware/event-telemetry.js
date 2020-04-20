/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { gDevTools } = require("devtools/client/framework/devtools");

const {
  TOGGLE_REQUEST_FILTER_TYPE,
  ENABLE_REQUEST_FILTER_TYPE_ONLY,
  SET_REQUEST_FILTER_TEXT,
  SELECT_DETAILS_PANEL_TAB,
  SEND_CUSTOM_REQUEST,
  ENABLE_PERSISTENT_LOGS,
  WS_SELECT_FRAME,
} = require("devtools/client/netmonitor/src/constants");

const {
  CHANGE_NETWORK_THROTTLING,
} = require("devtools/client/shared/components/throttling/actions");

/**
 * Event telemetry middleware is responsible for logging
 * various events to telemetry. This helps to track Network
 * panel usage.
 */
function eventTelemetryMiddleware(connector, telemetry) {
  return store => next => action => {
    const oldState = store.getState();
    const res = next(action);
    const toolbox = gDevTools.getToolbox(connector.getTabTarget());
    if (!toolbox) {
      return res;
    }

    if (action.skipTelemetry) {
      return res;
    }

    const state = store.getState();
    const { sessionId } = toolbox;

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
        sessionId,
      });
    }

    // Record telemetry event when side panel is selected.
    if (action.type == SELECT_DETAILS_PANEL_TAB) {
      sidePanelChange({
        state,
        oldState,
        telemetry,
        sessionId,
      });
    }

    // Record telemetry event when a request is resent.
    if (action.type == SEND_CUSTOM_REQUEST) {
      sendCustomRequest({
        telemetry,
        sessionId,
      });
    }

    // Record telemetry event when throttling is changed.
    if (action.type == CHANGE_NETWORK_THROTTLING) {
      throttlingChange({
        action,
        telemetry,
        sessionId,
      });
    }

    // Record telemetry event when log persistence changes.
    if (action.type == ENABLE_PERSISTENT_LOGS) {
      persistenceChange({
        telemetry,
        state,
        sessionId,
      });
    }

    // Record telemetry event when WS frame is selected.
    if (action.type == WS_SELECT_FRAME) {
      selectWSFrame({
        telemetry,
        sessionId,
      });
    }

    return res;
  };
}

/**
 * This helper function is executed when filter related action is fired.
 * It's responsible for recording "filters_changed" telemetry event.
 */
function filterChange({ action, state, oldState, telemetry, sessionId }) {
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
    trigger: trigger,
    active: activeFilters.join(","),
    inactive: inactiveFilters.join(","),
    session_id: sessionId,
  });
}

/**
 * This helper function is executed when side panel is selected.
 * It's responsible for recording "sidepanel_tool_changed"
 * telemetry event.
 */
function sidePanelChange({ state, oldState, telemetry, sessionId }) {
  telemetry.recordEvent("sidepanel_changed", "netmonitor", null, {
    oldpanel: oldState.ui.detailsPanelSelectedTab,
    newpanel: state.ui.detailsPanelSelectedTab,
    session_id: sessionId,
  });
}

/**
 * This helper function is executed when a request is resent.
 * It's responsible for recording "edit_resend" telemetry event.
 */
function sendCustomRequest({ telemetry, sessionId }) {
  telemetry.recordEvent("edit_resend", "netmonitor", null, {
    session_id: sessionId,
  });
}

/**
 * This helper function is executed when network throttling is changed.
 * It's responsible for recording "throttle_changed" telemetry event.
 */
function throttlingChange({ action, telemetry, sessionId }) {
  telemetry.recordEvent("throttle_changed", "netmonitor", null, {
    mode: action.profile,
    session_id: sessionId,
  });
}

/**
 * This helper function is executed when log persistence is changed.
 * It's responsible for recording "persist_changed" telemetry event.
 */
function persistenceChange({ telemetry, state, sessionId }) {
  telemetry.recordEvent(
    "persist_changed",
    "netmonitor",
    String(state.ui.persistentLogsEnabled),
    {
      session_id: sessionId,
    }
  );
}

/**
 * This helper function is executed when a WS frame is selected.
 * It's responsible for recording "select_ws_frame" telemetry event.
 */
function selectWSFrame({ telemetry, sessionId }) {
  telemetry.recordEvent("select_ws_frame", "netmonitor", null, {
    session_id: sessionId,
  });
}

module.exports = eventTelemetryMiddleware;
