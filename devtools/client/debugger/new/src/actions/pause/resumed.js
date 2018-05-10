"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.resumed = resumed;

var _selectors = require("../../selectors/index");

var _expressions = require("../expressions");

var _pause = require("../../utils/pause/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Debugger has just resumed
 *
 * @memberof actions/pause
 * @static
 */
function resumed() {
  return async ({
    dispatch,
    client,
    getState
  }) => {
    const why = (0, _selectors.getPauseReason)(getState());
    const wasPausedInEval = (0, _pause.inDebuggerEval)(why);
    const wasStepping = (0, _selectors.isStepping)(getState());
    dispatch({
      type: "RESUME"
    });

    if (!wasStepping && !wasPausedInEval) {
      await dispatch((0, _expressions.evaluateExpressions)());
    }
  };
}