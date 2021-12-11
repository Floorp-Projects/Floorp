/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { actions } = require("devtools/client/memory/constants");
const { refresh } = require("devtools/client/memory/actions/refresh");

/**
 * Change the display we use for labeling individual nodes and refresh the
 * current data.
 */
exports.setLabelDisplayAndRefresh = function(heapWorker, display) {
  return async function({ dispatch, getState }) {
    // Clears out all stored census data and sets the display.
    dispatch(setLabelDisplay(display));
    await dispatch(refresh(heapWorker));
  };
};

/**
 * Change the display we use for labeling individual nodes.
 *
 * @param {labelDisplayModel} display
 */
const setLabelDisplay = (exports.setLabelDisplay = function(display) {
  assert(
    typeof display === "object" &&
      display &&
      display.breakdown &&
      display.breakdown.by,
    "Breakdowns must be an object with a `by` property, attempted to set: " +
      JSON.stringify(display)
  );

  return {
    type: actions.SET_LABEL_DISPLAY,
    display,
  };
});
