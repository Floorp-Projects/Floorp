/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  APPEND_TO_HISTORY,
  CLEAR_HISTORY,
  EVALUATE_EXPRESSION,
  HISTORY_LOADED,
  UPDATE_HISTORY_POSITION,
  HISTORY_BACK,
  HISTORY_FORWARD,
  REVERSE_SEARCH_INPUT_TOGGLE,
  REVERSE_SEARCH_INPUT_CHANGE,
  REVERSE_SEARCH_BACK,
  REVERSE_SEARCH_NEXT,
  SET_TERMINAL_INPUT,
  SET_TERMINAL_EAGER_RESULT,
} = require("resource://devtools/client/webconsole/constants.js");

/**
 * Create default initial state for this reducer.
 */
function getInitialState() {
  return {
    // Array with history entries
    entries: [],

    // Holds position (index) in history entries that the user is
    // currently viewing. This is reset to this.entries.length when
    // APPEND_TO_HISTORY action is fired.
    position: undefined,

    // Backups the original user value (if any) that can be set in
    // the input field. It might be used again if the user doesn't
    // pick up anything from the history and wants to return all
    // the way back to see the original input text.
    originalUserValue: null,

    reverseSearchEnabled: false,
    currentReverseSearchResults: null,
    currentReverseSearchResultsPosition: null,

    terminalInput: null,
    terminalEagerResult: null,
  };
}

function history(state = getInitialState(), action, prefsState) {
  switch (action.type) {
    case APPEND_TO_HISTORY:
    case EVALUATE_EXPRESSION:
      return appendToHistory(state, prefsState, action.expression);
    case CLEAR_HISTORY:
      return clearHistory(state);
    case HISTORY_LOADED:
      return historyLoaded(state, action.entries);
    case UPDATE_HISTORY_POSITION:
      return updateHistoryPosition(state, action.direction, action.expression);
    case REVERSE_SEARCH_INPUT_TOGGLE:
      return reverseSearchInputToggle(state, action);
    case REVERSE_SEARCH_INPUT_CHANGE:
      return reverseSearchInputChange(state, action.value);
    case REVERSE_SEARCH_BACK:
      return reverseSearchBack(state);
    case REVERSE_SEARCH_NEXT:
      return reverseSearchNext(state);
    case SET_TERMINAL_INPUT:
      return setTerminalInput(state, action.expression);
    case SET_TERMINAL_EAGER_RESULT:
      return setTerminalEagerResult(state, action.result);
  }
  return state;
}

function appendToHistory(state, prefsState, expression) {
  // Clone state
  state = { ...state };
  state.entries = [...state.entries];

  // Append new expression only if it isn't the same as
  // the one recently added.
  if (expression.trim() != state.entries[state.entries.length - 1]) {
    state.entries.push(expression);
  }

  // Remove entries if the limit is reached
  if (state.entries.length > prefsState.historyCount) {
    state.entries.splice(0, state.entries.length - prefsState.historyCount);
  }

  state.position = state.entries.length;
  state.originalUserValue = null;

  return state;
}

function clearHistory(state) {
  return getInitialState();
}

/**
 * Handling HISTORY_LOADED action that is fired when history
 * entries created in previous Firefox session are loaded
 * from async-storage.
 *
 * Loaded entries are appended before the ones that were
 * added to the state in this session.
 */
function historyLoaded(state, entries) {
  const newEntries = [...entries, ...state.entries];
  return {
    ...state,
    entries: newEntries,
    // Default position is at the end of the list
    // (at the latest inserted item).
    position: newEntries.length,
    originalUserValue: null,
  };
}

function updateHistoryPosition(state, direction, expression) {
  // Handle UP arrow key => HISTORY_BACK
  // Handle DOWN arrow key => HISTORY_FORWARD
  if (direction == HISTORY_BACK) {
    if (state.position <= 0) {
      return state;
    }

    // Clone state
    state = { ...state };

    // Store the current input value when the user starts
    // browsing through the history.
    if (state.position == state.entries.length) {
      state.originalUserValue = expression || "";
    }

    state.position--;
  } else if (direction == HISTORY_FORWARD) {
    if (state.position >= state.entries.length) {
      return state;
    }

    state = {
      ...state,
      position: state.position + 1,
    };
  }

  return state;
}

function reverseSearchInputToggle(state, action) {
  const { initialValue = "" } = action;

  // We're going to close the reverse search, let's clean the state
  if (state.reverseSearchEnabled) {
    return {
      ...state,
      reverseSearchEnabled: false,
      position: undefined,
      currentReverseSearchResults: null,
      currentReverseSearchResultsPosition: null,
    };
  }

  // If we're enabling the reverse search, we treat it as a reverse search input change,
  // since we can have an initial value.
  return reverseSearchInputChange(state, initialValue);
}

function reverseSearchInputChange(state, searchString) {
  if (searchString === "") {
    return {
      ...state,
      position: undefined,
      currentReverseSearchResults: null,
      currentReverseSearchResultsPosition: null,
    };
  }

  searchString = searchString.toLocaleLowerCase();
  const matchingEntries = state.entries.filter(entry =>
    entry.toLocaleLowerCase().includes(searchString)
  );
  // We only return unique entries, but we want to keep the latest entry in the array if
  // it's duplicated (e.g. if we have [1,2,1], we want to get [2,1], not [1,2]).
  // To do that, we need to reverse the matching entries array, provide it to a Set,
  // transform it back to an array and reverse it again.
  const uniqueEntries = new Set(matchingEntries.reverse());
  const currentReverseSearchResults = Array.from(
    new Set(uniqueEntries)
  ).reverse();

  return {
    ...state,
    position: undefined,
    currentReverseSearchResults,
    currentReverseSearchResultsPosition: currentReverseSearchResults.length - 1,
  };
}

function reverseSearchBack(state) {
  let nextPosition = state.currentReverseSearchResultsPosition - 1;
  if (nextPosition < 0) {
    nextPosition = state.currentReverseSearchResults.length - 1;
  }

  return {
    ...state,
    currentReverseSearchResultsPosition: nextPosition,
  };
}

function reverseSearchNext(state) {
  let previousPosition = state.currentReverseSearchResultsPosition + 1;
  if (previousPosition >= state.currentReverseSearchResults.length) {
    previousPosition = 0;
  }

  return {
    ...state,
    currentReverseSearchResultsPosition: previousPosition,
  };
}

function setTerminalInput(state, expression) {
  return {
    ...state,
    terminalInput: expression,
    terminalEagerResult: !expression ? null : state.terminalEagerResult,
  };
}

function setTerminalEagerResult(state, result) {
  return {
    ...state,
    terminalEagerResult: result,
  };
}

exports.history = history;
