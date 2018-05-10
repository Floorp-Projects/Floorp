"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.continueToHere = continueToHere;

var _selectors = require("../../selectors/index");

var _breakpoints = require("../breakpoints");

var _commands = require("./commands");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function continueToHere(line) {
  return async function ({
    dispatch,
    getState
  }) {
    const selectedSource = (0, _selectors.getSelectedSource)(getState());

    if (!(0, _selectors.isPaused)(getState()) || !selectedSource) {
      return;
    }

    const selectedFrame = (0, _selectors.getSelectedFrame)(getState());
    const debugLine = selectedFrame.location.line;

    if (debugLine == line) {
      return;
    }

    const action = (0, _selectors.getCanRewind)(getState()) && line < debugLine ? _commands.rewind : _commands.resume;
    await dispatch((0, _breakpoints.addHiddenBreakpoint)({
      line,
      column: undefined,
      sourceId: selectedSource.id
    }));
    dispatch(action());
  };
}