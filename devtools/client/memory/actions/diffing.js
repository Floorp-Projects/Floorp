/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { assert, reportException } = require("devtools/shared/DevToolsUtils");
const { actions, diffingState, viewState } = require("../constants");
const {
  getSnapshot,
  censusIsUpToDate,
  snapshotIsDiffable,
  findSelectedSnapshot,
} = require("../utils");

/**
 * Toggle diffing mode on or off.
 */
exports.toggleDiffing = function() {
  return function(dispatch, getState) {
    dispatch({
      type: actions.CHANGE_VIEW,
      newViewState: getState().diffing ? viewState.CENSUS : viewState.DIFFING,
      oldDiffing: getState().diffing,
      oldSelected: findSelectedSnapshot(getState()),
    });
  };
};

/**
 * Select the given snapshot for diffing.
 *
 * @param {snapshotModel} snapshot
 */
const selectSnapshotForDiffing = exports.selectSnapshotForDiffing = function(snapshot) {
  assert(snapshotIsDiffable(snapshot),
         "To select a snapshot for diffing, it must be diffable");
  return { type: actions.SELECT_SNAPSHOT_FOR_DIFFING, snapshot };
};

/**
 * Compute the difference between the first and second snapshots.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {snapshotModel} first
 * @param {snapshotModel} second
 */
const takeCensusDiff = exports.takeCensusDiff = function(heapWorker, first, second) {
  return async function(dispatch, getState) {
    assert(snapshotIsDiffable(first),
           `First snapshot must be in a diffable state, found ${first.state}`);
    assert(snapshotIsDiffable(second),
           `Second snapshot must be in a diffable state, found ${second.state}`);

    let report, parentMap;
    let display = getState().censusDisplay;
    let filter = getState().filter;

    if (censusIsUpToDate(filter, display, getState().diffing.census)) {
      return;
    }

    do {
      if (!getState().diffing
          || getState().diffing.firstSnapshotId !== first.id
          || getState().diffing.secondSnapshotId !== second.id) {
        // If we stopped diffing or stopped and then started diffing a different
        // pair of snapshots, then just give up with diffing this pair. In the
        // latter case, a newly spawned task will handle the diffing for the new
        // pair.
        return;
      }

      display = getState().censusDisplay;
      filter = getState().filter;

      dispatch({
        type: actions.TAKE_CENSUS_DIFF_START,
        first,
        second,
        filter,
        display,
      });

      const opts = display.inverted
        ? { asInvertedTreeNode: true }
        : { asTreeNode: true };
      opts.filter = filter || null;

      try {
        ({ delta: report, parentMap } = await heapWorker.takeCensusDiff(
          first.path,
          second.path,
          { breakdown: display.breakdown },
          opts));
      } catch (error) {
        reportException("actions/diffing/takeCensusDiff", error);
        dispatch({ type: actions.DIFFING_ERROR, error });
        return;
      }
    }
    while (filter !== getState().filter
           || display !== getState().censusDisplay);

    dispatch({
      type: actions.TAKE_CENSUS_DIFF_END,
      first,
      second,
      report,
      parentMap,
      filter,
      display,
    });
  };
};

/**
 * Ensure that the current diffing data is up to date with the currently
 * selected display, filter, etc. If the state is not up-to-date, then a
 * recompute is triggered.
 *
 * @param {HeapAnalysesClient} heapWorker
 */
const refreshDiffing = exports.refreshDiffing = function(heapWorker) {
  return function(dispatch, getState) {
    if (getState().diffing.secondSnapshotId === null) {
      return;
    }

    assert(getState().diffing.firstSnapshotId,
           "Should have first snapshot id");

    if (getState().diffing.state === diffingState.TAKING_DIFF) {
      // There is an existing task that will ensure that the diffing data is
      // up-to-date.
      return;
    }

    const { firstSnapshotId, secondSnapshotId } = getState().diffing;

    const first = getSnapshot(getState(), firstSnapshotId);
    const second = getSnapshot(getState(), secondSnapshotId);
    dispatch(takeCensusDiff(heapWorker, first, second));
  };
};

/**
 * Select the given snapshot for diffing and refresh the diffing data if
 * necessary (for example, if two snapshots are now selected for diffing).
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {snapshotModel} snapshot
 */
exports.selectSnapshotForDiffingAndRefresh = function(heapWorker, snapshot) {
  return async function(dispatch, getState) {
    assert(getState().diffing,
           "If we are selecting for diffing, we must be in diffing mode");
    dispatch(selectSnapshotForDiffing(snapshot));
    await dispatch(refreshDiffing(heapWorker));
  };
};

/**
 * Expand the given node in the diffing's census's delta-report.
 *
 * @param {CensusTreeNode} node
 */
exports.expandDiffingCensusNode = function(node) {
  return {
    type: actions.EXPAND_DIFFING_CENSUS_NODE,
    node,
  };
};

/**
 * Collapse the given node in the diffing's census's delta-report.
 *
 * @param {CensusTreeNode} node
 */
exports.collapseDiffingCensusNode = function(node) {
  return {
    type: actions.COLLAPSE_DIFFING_CENSUS_NODE,
    node,
  };
};

/**
 * Focus the given node in the snapshot's census's report.
 *
 * @param {DominatorTreeNode} node
 */
exports.focusDiffingCensusNode = function(node) {
  return {
    type: actions.FOCUS_DIFFING_CENSUS_NODE,
    node,
  };
};
