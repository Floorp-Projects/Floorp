/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FILTER_TEXT_SET,
  FILTER_TOGGLE,
  DEFAULT_FILTERS_RESET,
  EVALUATE_EXPRESSION,
  MESSAGES_ADD,
  PERSIST_TOGGLE,
  REVERSE_SEARCH_INPUT_TOGGLE,
  REVERSE_SEARCH_NEXT,
  REVERSE_SEARCH_BACK,
} = require("resource://devtools/client/webconsole/constants.js");

/**
 * Event telemetry middleware is responsible for logging specific events to telemetry.
 */
function eventTelemetryMiddleware(telemetry, store) {
  return next => action => {
    const oldState = store.getState();
    const res = next(action);

    const state = store.getState();

    const filterChangeActions = [
      FILTER_TEXT_SET,
      FILTER_TOGGLE,
      DEFAULT_FILTERS_RESET,
    ];

    if (filterChangeActions.includes(action.type)) {
      filterChange({
        action,
        state,
        oldState,
        telemetry,
      });
    } else if (action.type === MESSAGES_ADD) {
      messagesAdd({ action, telemetry });
    } else if (action.type === PERSIST_TOGGLE) {
      telemetry.recordEvent(
        "persist_changed",
        "webconsole",
        String(state.ui.persistLogs)
      );
    } else if (action.type === EVALUATE_EXPRESSION) {
      // Send telemetry event. If we are in the browser toolbox we send -1 as the
      // toolbox session id.

      telemetry.recordEvent("execute_js", "webconsole", null, {
        lines: action.expression.split(/\n/).length,
        input: state.ui.editor ? "multiline" : "inline",
      });

      if (action.from === "reverse-search") {
        telemetry.recordEvent("reverse_search", "webconsole", null, {
          functionality: "evaluate expression",
        });
      }
    } else if (
      action.type === REVERSE_SEARCH_INPUT_TOGGLE &&
      state.ui.reverseSearchInputVisible
    ) {
      telemetry.recordEvent("reverse_search", "webconsole", action.access, {
        functionality: "open",
      });
    } else if (action.type === REVERSE_SEARCH_NEXT) {
      telemetry.recordEvent("reverse_search", "webconsole", action.access, {
        functionality: "navigate next",
      });
    } else if (action.type === REVERSE_SEARCH_BACK) {
      telemetry.recordEvent("reverse_search", "webconsole", action.access, {
        functionality: "navigate previous",
      });
    }

    return res;
  };
}

function filterChange({ action, state, oldState, telemetry }) {
  const oldFilterState = oldState.filters;
  const filterState = state.filters;
  const activeFilters = [];
  const inactiveFilters = [];
  for (const [key, value] of Object.entries(filterState)) {
    if (value) {
      activeFilters.push(key);
    } else {
      inactiveFilters.push(key);
    }
  }

  let trigger;
  if (action.type === FILTER_TOGGLE) {
    trigger = action.filter;
  } else if (action.type === DEFAULT_FILTERS_RESET) {
    trigger = "reset";
  } else if (action.type === FILTER_TEXT_SET) {
    if (oldFilterState.text !== "" && filterState.text !== "") {
      return;
    }

    trigger = "text";
  }

  telemetry.recordEvent("filters_changed", "webconsole", null, {
    trigger,
    active: activeFilters.join(","),
    inactive: inactiveFilters.join(","),
  });
}

function messagesAdd({ action, telemetry }) {
  const { messages } = action;
  for (const message of messages) {
    if (message.level === "error" && message.source === "javascript") {
      telemetry
        .getKeyedHistogramById("DEVTOOLS_JAVASCRIPT_ERROR_DISPLAYED")
        .add(message.errorMessageName || "Unknown", true);
    }
  }
}

module.exports = eventTelemetryMiddleware;
