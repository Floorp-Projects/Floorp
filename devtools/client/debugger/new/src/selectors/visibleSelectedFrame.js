"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getVisibleSelectedFrame = undefined;

var _sources = require("../reducers/sources");

var _pause = require("../reducers/pause");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getLocation(frame, location) {
  if (!location) {
    return frame.location;
  }

  return !(0, _devtoolsSourceMap.isOriginalId)(location.sourceId) ? frame.generatedLocation || frame.location : frame.location;
}

const getVisibleSelectedFrame = exports.getVisibleSelectedFrame = (0, _reselect.createSelector)(_sources.getSelectedLocation, _pause.getSelectedFrame, (selectedLocation, selectedFrame) => {
  if (!selectedFrame) {
    return null;
  }

  const {
    id
  } = selectedFrame;
  return {
    id,
    location: getLocation(selectedFrame, selectedLocation)
  };
});