/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  APPEND_TO_HISTORY,
  CLEAR_HISTORY,
  HISTORY_LOADED,
  UPDATE_HISTORY_POSITION,
  REVERSE_SEARCH_INPUT_CHANGE,
  REVERSE_SEARCH_BACK,
  REVERSE_SEARCH_NEXT,
} = require("devtools/client/webconsole/constants");

/**
 * Append a new value in the history of executed expressions,
 * or overwrite the most recent entry. The most recent entry may
 * contain the last edited input value that was not evaluated yet.
 */
function appendToHistory(expression) {
  return {
    type: APPEND_TO_HISTORY,
    expression,
  };
}

/**
 * Clear the console history altogether. Note that this will not affect
 * other consoles that are already opened (since they have their own copy),
 * but it will reset the array for all newly-opened consoles.
 */
function clearHistory() {
  return {
    type: CLEAR_HISTORY,
  };
}

/**
 * Fired when the console history from previous Firefox sessions is loaded.
 */
function historyLoaded(entries) {
  return {
    type: HISTORY_LOADED,
    entries,
  };
}

/**
 * Update place-holder position in the history list.
 */
function updateHistoryPosition(direction, expression) {
  return {
    type: UPDATE_HISTORY_POSITION,
    direction,
    expression,
  };
}

function reverseSearchInputChange(value) {
  return {
    type: REVERSE_SEARCH_INPUT_CHANGE,
    value,
  };
}

function showReverseSearchNext({ access } = {}) {
  return {
    type: REVERSE_SEARCH_NEXT,
    access,
  };
}

function showReverseSearchBack({ access } = {}) {
  return {
    type: REVERSE_SEARCH_BACK,
    access,
  };
}

module.exports = {
  appendToHistory,
  clearHistory,
  historyLoaded,
  updateHistoryPosition,
  reverseSearchInputChange,
  showReverseSearchNext,
  showReverseSearchBack,
};
