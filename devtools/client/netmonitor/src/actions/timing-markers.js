/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ADD_TIMING_MARKER, CLEAR_TIMING_MARKERS } = require("../constants");

exports.addTimingMarker = (marker) => {
  return {
    type: ADD_TIMING_MARKER,
    marker
  };
};

exports.clearTimingMarkers = () => {
  return {
    type: CLEAR_TIMING_MARKERS
  };
};
