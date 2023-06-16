/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  HISTORY_BACK,
  HISTORY_FORWARD,
} = require("resource://devtools/client/webconsole/constants.js");

function getHistory(state) {
  return state.history;
}

function getHistoryEntries(state) {
  return state.history.entries;
}

function getHistoryValue(state, direction) {
  if (direction == HISTORY_BACK) {
    return getPreviousHistoryValue(state);
  }
  if (direction == HISTORY_FORWARD) {
    return getNextHistoryValue(state);
  }
  return null;
}

function getNextHistoryValue(state) {
  if (state.history.position < state.history.entries.length - 1) {
    return state.history.entries[state.history.position + 1];
  }

  // The user didn't pick up anything from the history and returned
  // back to the previous value (if any) that was in the input box.
  return state.history.originalUserValue;
}

function getPreviousHistoryValue(state) {
  if (state.history.position > 0) {
    return state.history.entries[state.history.position - 1];
  }
  return null;
}

function getReverseSearchResult(state) {
  const { history } = state;
  const { currentReverseSearchResults, currentReverseSearchResultsPosition } =
    history;

  if (
    !Array.isArray(currentReverseSearchResults) ||
    currentReverseSearchResults.length === 0 ||
    !Number.isInteger(currentReverseSearchResultsPosition)
  ) {
    return null;
  }
  return currentReverseSearchResults[currentReverseSearchResultsPosition];
}

function getReverseSearchResultPosition(state) {
  const { history } = state;
  const { currentReverseSearchResultsPosition } = history;
  if (!Number.isInteger(currentReverseSearchResultsPosition)) {
    return currentReverseSearchResultsPosition;
  }

  return currentReverseSearchResultsPosition + 1;
}

function getReverseSearchTotalResults(state) {
  const { history } = state;
  const { currentReverseSearchResults } = history;
  if (!currentReverseSearchResults) {
    return null;
  }

  return currentReverseSearchResults.length;
}

function getTerminalEagerResult(state) {
  const { history } = state;
  return history.terminalEagerResult;
}

module.exports = {
  getHistory,
  getHistoryEntries,
  getHistoryValue,
  getReverseSearchResult,
  getReverseSearchResultPosition,
  getReverseSearchTotalResults,
  getTerminalEagerResult,
};
