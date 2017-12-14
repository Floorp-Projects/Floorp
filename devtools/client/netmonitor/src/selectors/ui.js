/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createSelector } = require("devtools/client/shared/vendor/reselect");
const { REQUESTS_WATERFALL } = require("../constants");
const { getDisplayedRequests } = require("./requests");

const EPSILON = 0.001;

function isNetworkDetailsToggleButtonDisabled(state) {
  return getDisplayedRequests(state).length == 0;
}

const getWaterfallScale = createSelector(
  state => state.requests,
  state => state.timingMarkers,
  state => state.ui,
  (requests, timingMarkers, ui) => {
    if (requests.firstStartedMillis === +Infinity || ui.waterfallWidth === null) {
      return null;
    }

    const lastEventMillis = Math.max(requests.lastEndedMillis,
                                     timingMarkers.firstDocumentDOMContentLoadedTimestamp,
                                     timingMarkers.firstDocumentLoadTimestamp);
    const longestWidth = lastEventMillis - requests.firstStartedMillis;
    return Math.min(Math.max(
      (ui.waterfallWidth - REQUESTS_WATERFALL.LABEL_WIDTH) / longestWidth, EPSILON), 1);
  }
);

module.exports = {
  isNetworkDetailsToggleButtonDisabled,
  getWaterfallScale,
};
