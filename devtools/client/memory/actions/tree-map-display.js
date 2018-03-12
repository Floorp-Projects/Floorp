/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { actions } = require("../constants");
const { refresh } = require("./refresh");
/**
 * Sets the tree map display as the current display and refreshes the tree map
 * census.
 */
exports.setTreeMapAndRefresh = function (heapWorker, display) {
  return function* (dispatch, getState) {
    dispatch(setTreeMap(display));
    yield dispatch(refresh(heapWorker));
  };
};

/**
 * Clears out all cached census data in the snapshots and sets new display data
 * for tree maps.
 *
 * @param {treeMapModel} display
 */
const setTreeMap = exports.setTreeMap = function (display) {
  assert(typeof display === "object"
         && display
         && display.breakdown
         && display.breakdown.by,
    "Breakdowns must be an object with a \`by\` property, attempted to set: " +
    uneval(display));

  return {
    type: actions.SET_TREE_MAP_DISPLAY,
    display,
  };
};
