"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.timeTravelTo = timeTravelTo;
exports.clearHistory = clearHistory;

var _selectors = require("../selectors/index");

var _sources = require("./sources/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for replay
 * @module actions/replay
 */
function timeTravelTo(position) {
  return ({
    dispatch,
    getState
  }) => {
    const data = (0, _selectors.getHistoryFrame)(getState(), position);
    dispatch({
      type: "TRAVEL_TO",
      data,
      position
    });
    dispatch((0, _sources.selectLocation)(data.paused.frames[0].location));
  };
}

function clearHistory() {
  return ({
    dispatch,
    getState
  }) => {
    dispatch({
      type: "CLEAR_HISTORY"
    });
  };
}