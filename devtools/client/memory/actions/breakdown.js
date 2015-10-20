/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// @TODO 1215606
// Use this assert instead of utils when fixed.
// const { assert } = require("devtools/shared/DevToolsUtils");
const { breakdownEquals, createSnapshot, assert } = require("../utils");
const { actions, snapshotState: states } = require("../constants");
const { takeCensus } = require("./snapshot");

const setBreakdownAndRefresh = exports.setBreakdownAndRefresh = function (heapWorker, breakdown) {
  return function *(dispatch, getState) {
    // Clears out all stored census data and sets
    // the breakdown
    dispatch(setBreakdown(breakdown));
    let snapshot = getState().snapshots.find(s => s.selected);

    // If selected snapshot does not have updated census if the breakdown
    // changed, retake the census with new breakdown
    if (snapshot && !breakdownEquals(snapshot.breakdown, breakdown)) {
      yield dispatch(takeCensus(heapWorker, snapshot));
    }
  };
};

/**
 * Clears out all census data in the snapshots and sets
 * a new breakdown.
 *
 * @param {Breakdown} breakdown
 */
const setBreakdown = exports.setBreakdown = function (breakdown) {
  // @TODO 1215606
  assert(typeof breakdown === "object" && breakdown.by,
    `Breakdowns must be an object with a \`by\` property, attempted to set: ${uneval(breakdown)}`);

  return {
    type: actions.SET_BREAKDOWN,
    breakdown,
  }
};
