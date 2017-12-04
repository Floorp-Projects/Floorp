/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_TIMING_MARKER,
  CLEAR_TIMING_MARKERS,
  CLEAR_REQUESTS,
} = require("../constants");

function TimingMarkers() {
  return {
    firstDocumentDOMContentLoadedTimestamp: -1,
    firstDocumentLoadTimestamp: -1,
  };
}

function addTimingMarker(state, action) {
  state = { ...state };

  if (action.marker.name === "document::DOMContentLoaded" &&
      state.firstDocumentDOMContentLoadedTimestamp === -1) {
    state.firstDocumentDOMContentLoadedTimestamp = action.marker.unixTime / 1000;
    return state;
  }

  if (action.marker.name === "document::Load" &&
      state.firstDocumentLoadTimestamp === -1) {
    state.firstDocumentLoadTimestamp = action.marker.unixTime / 1000;
    return state;
  }

  return state;
}

function clearTimingMarkers(state) {
  return new TimingMarkers();
}

function timingMarkers(state = new TimingMarkers(), action) {
  switch (action.type) {
    case ADD_TIMING_MARKER:
      return addTimingMarker(state, action);

    case CLEAR_REQUESTS:
    case CLEAR_TIMING_MARKERS:
      return clearTimingMarkers(state);

    default:
      return state;
  }
}

module.exports = {
  TimingMarkers,
  timingMarkers
};
