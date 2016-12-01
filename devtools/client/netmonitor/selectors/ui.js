/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getDisplayedRequests } = require("./requests");

function isSidebarToggleButtonDisabled(state) {
  return getDisplayedRequests(state).isEmpty();
}

const EPSILON = 0.001;

function getWaterfallScale(state) {
  const { requests, timingMarkers, ui } = state;

  if (requests.firstStartedMillis == +Infinity) {
    return null;
  }

  const lastEventMillis = Math.max(requests.lastEndedMillis,
                                   timingMarkers.firstDocumentDOMContentLoadedTimestamp,
                                   timingMarkers.firstDocumentLoadTimestamp);
  const longestWidth = lastEventMillis - requests.firstStartedMillis;
  return Math.min(Math.max(ui.waterfallWidth / longestWidth, EPSILON), 1);
}

module.exports = {
  isSidebarToggleButtonDisabled,
  getWaterfallScale,
};
