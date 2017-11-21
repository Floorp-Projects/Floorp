/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { REQUESTS_WATERFALL } = require("../constants");
const { getDisplayedRequests } = require("./requests");

function isNetworkDetailsToggleButtonDisabled(state) {
  return getDisplayedRequests(state).isEmpty();
}

const EPSILON = 0.001;

function getWaterfallScale(state) {
  const { requests, timingMarkers, ui } = state;

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

module.exports = {
  isNetworkDetailsToggleButtonDisabled,
  getWaterfallScale,
};
