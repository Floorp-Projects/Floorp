/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { refreshDiffing } = require("./diffing");
const { refreshSelectedCensus } = require("./snapshot");

/**
 * Refresh the main thread's data from the heap analyses worker, if needed.
 *
 * @param {HeapAnalysesWorker} heapWorker
 */
exports.refresh = function (heapWorker) {
  return function* (dispatch, getState) {
    if (getState().diffing) {
      yield dispatch(refreshDiffing(heapWorker));
    } else {
      yield dispatch(refreshSelectedCensus(heapWorker));
    }
  };
};
