/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  APPEND_TO_HISTORY,
  CLEAR_HISTORY,
  HISTORY_LOADED,
  UPDATE_HISTORY_POSITION,
} = require("devtools/client/webconsole/constants");

/**
 * Append a new value in the history of executed expressions,
 * or overwrite the most recent entry. The most recent entry may
 * contain the last edited input value that was not evaluated yet.
 */
function appendToHistory(expression) {
  return {
    type: APPEND_TO_HISTORY,
    expression: expression,
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

module.exports = {
  appendToHistory,
  clearHistory,
  historyLoaded,
  updateHistoryPosition,
};
