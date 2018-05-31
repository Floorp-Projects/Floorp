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
  UPDATE_HISTORY_PLACEHOLDER,
  HISTORY_BACK,
  HISTORY_FORWARD,
} = require("devtools/client/webconsole/constants");

/**
 * Create default initial state for this reducer.
 */
function getInitialState() {
  return {
    // Array with history entries
    entries: [],

    // Holds the index of the history entry that the user is currently
    // viewing. This is reset to this.history.length when APPEND_TO_HISTORY
    // action is fired.
    placeHolder: undefined,

    // Holds the number of entries in history. This value is incremented
    // when APPEND_TO_HISTORY action is fired and used to get previous
    // value from the command line when the user goes backward.
    index: 0,
  };
}

function history(state = getInitialState(), action, prefsState) {
  switch (action.type) {
    case APPEND_TO_HISTORY:
      return appendToHistory(state, prefsState, action.expression);
    case CLEAR_HISTORY:
      return clearHistory(state);
    case HISTORY_LOADED:
      return historyLoaded(state, action.entries);
    case UPDATE_HISTORY_PLACEHOLDER:
      return updatePlaceHolder(state, action.direction, action.expression);
  }
  return state;
}

function appendToHistory(state, prefsState, expression) {
  // Clone state
  state = {...state};
  state.entries = [...state.entries];

  // Append new expression
  state.entries[state.index++] = expression;
  state.placeHolder = state.entries.length;

  // Remove entries if the limit is reached
  if (state.entries.length > prefsState.historyCount) {
    state.entries.splice(0, state.entries.length - prefsState.historyCount);
    state.index = state.placeHolder = state.entries.length;
  }

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
  let newEntries = [...entries, ...state.entries];
  return {
    ...state,
    entries: newEntries,
    placeHolder: newEntries.length,
    index: newEntries.length,
  };
}

function updatePlaceHolder(state, direction, expression) {
  // Handle UP arrow key => HISTORY_BACK
  // Handle DOWN arrow key => HISTORY_FORWARD
  if (direction == HISTORY_BACK) {
    if (state.placeHolder <= 0) {
      return state;
    }

    // Clone state
    state = {...state};

    // Save the current input value as the latest entry in history, only if
    // the user is already at the last entry.
    // Note: this code does not store changes to items that are already in
    // history.
    if (state.placeHolder == state.index) {
      state.entries = [...state.entries];
      state.entries[state.index] = expression || "";
    }

    state.placeHolder--;
  } else if (direction == HISTORY_FORWARD) {
    if (state.placeHolder >= (state.entries.length - 1)) {
      return state;
    }

    state = {
      ...state,
      placeHolder: state.placeHolder + 1,
    };
  }

  return state;
}

exports.history = history;
