/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  HISTORY_BACK,
  HISTORY_FORWARD,
} = require("devtools/client/webconsole/constants");

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
  if (state.history.placeHolder < (state.history.entries.length - 1)) {
    return state.history.entries[state.history.placeHolder + 1];
  }
  return null;
}

function getPreviousHistoryValue(state) {
  if (state.history.placeHolder > 0) {
    return state.history.entries[state.history.placeHolder - 1];
  }
  return null;
}

module.exports = {
  getHistory,
  getHistoryEntries,
  getHistoryValue,
};
