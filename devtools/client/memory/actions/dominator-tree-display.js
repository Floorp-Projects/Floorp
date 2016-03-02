/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { actions } = require("../constants");
const { refresh } = require("./refresh");

exports.setDominatorTreeDisplayAndRefresh = function(heapWorker, display) {
  return function*(dispatch, getState) {
    // Clears out all stored census data and sets the display.
    dispatch(setDominatorTreeDisplay(display));
    yield dispatch(refresh(heapWorker));
  };
};

/**
 * Clears out all census data in the snapshots and sets
 * a new display.
 *
 * @param {dominatorTreeDisplayModel} display
 */
const setDominatorTreeDisplay = exports.setDominatorTreeDisplay = function (display) {
  assert(typeof display === "object"
         && display
         && display.breakdown
         && display.breakdown.by,
    `Breakdowns must be an object with a \`by\` property, attempted to set: ${uneval(display)}`);

  return {
    type: actions.SET_DOMINATOR_TREE_DISPLAY,
    display,
  };
};
