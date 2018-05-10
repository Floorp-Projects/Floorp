"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.selectFrame = selectFrame;

var _sources = require("../sources/index");

var _expressions = require("../expressions");

var _fetchScopes = require("./fetchScopes");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * @memberof actions/pause
 * @static
 */
function selectFrame(frame) {
  return async ({
    dispatch,
    client,
    getState,
    sourceMaps
  }) => {
    dispatch({
      type: "SELECT_FRAME",
      frame
    });
    dispatch((0, _sources.selectLocation)(frame.location));
    dispatch((0, _expressions.evaluateExpressions)());
    dispatch((0, _fetchScopes.fetchScopes)());
  };
}