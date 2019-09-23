/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createSelector } = require("devtools/client/shared/vendor/reselect");
const { REQUESTS_WATERFALL } = require("../constants");

const EPSILON = 0.001;

const getWaterfallScale = createSelector(
  state => state.requests,
  state => state.timingMarkers,
  state => state.ui,
  (requests, timingMarkers, ui) => {
    if (requests.firstStartedMs === +Infinity || ui.waterfallWidth === null) {
      return null;
    }

    const lastEventMs = Math.max(
      requests.lastEndedMs,
      timingMarkers.firstDocumentDOMContentLoadedTimestamp,
      timingMarkers.firstDocumentLoadTimestamp
    );
    const longestWidth = lastEventMs - requests.firstStartedMs;

    // Reduce 20px for the last request's requests-list-timings-total
    return Math.min(
      Math.max(
        (ui.waterfallWidth - REQUESTS_WATERFALL.LABEL_WIDTH - 20) /
          longestWidth,
        EPSILON
      ),
      1
    );
  }
);

module.exports = {
  getWaterfallScale,
};
