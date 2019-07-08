/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { viewState } = require("../constants");
const { refreshDiffing } = require("./diffing");
const snapshot = require("./snapshot");

/**
 * Refresh the main thread's data from the heap analyses worker, if needed.
 *
 * @param {HeapAnalysesWorker} heapWorker
 */
exports.refresh = function(heapWorker) {
  return async function(dispatch, getState) {
    switch (getState().view.state) {
      case viewState.DIFFING:
        assert(
          getState().diffing,
          "Should have diffing state if in diffing view"
        );
        await dispatch(refreshDiffing(heapWorker));
        return;

      case viewState.CENSUS:
        await dispatch(snapshot.refreshSelectedCensus(heapWorker));
        return;

      case viewState.DOMINATOR_TREE:
        await dispatch(snapshot.refreshSelectedDominatorTree(heapWorker));
        return;

      case viewState.TREE_MAP:
        await dispatch(snapshot.refreshSelectedTreeMap(heapWorker));
        return;

      case viewState.INDIVIDUALS:
        await dispatch(snapshot.refreshIndividuals(heapWorker));
        return;

      default:
        assert(false, `Unexpected view state: ${getState().view.state}`);
    }
  };
};
