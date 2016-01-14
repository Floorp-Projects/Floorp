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
exports.refresh = function (heapWorker) {
  return function* (dispatch, getState) {
    switch (getState().view) {
      case viewState.DIFFING:
        assert(getState().diffing, "Should have diffing state if in diffing view");
        yield dispatch(refreshDiffing(heapWorker));
        return;

      case viewState.CENSUS:
        yield dispatch(snapshot.refreshSelectedCensus(heapWorker));
        return;

      case viewState.DOMINATOR_TREE:
        yield dispatch(snapshot.refreshSelectedDominatorTree(heapWorker));
        return;

      default:
        assert(false, `Unexpected view state: ${getState().view}`);
    }
  };
};
