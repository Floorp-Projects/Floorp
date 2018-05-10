"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isSelectedFrameVisible = isSelectedFrameVisible;

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _pause = require("../reducers/pause");

var _sources = require("../reducers/sources");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getGeneratedId(sourceId) {
  if ((0, _devtoolsSourceMap.isOriginalId)(sourceId)) {
    return (0, _devtoolsSourceMap.originalToGeneratedId)(sourceId);
  }

  return sourceId;
}
/*
 * Checks to if the selected frame's source is currently
 * selected.
 */


function isSelectedFrameVisible(state) {
  const selectedLocation = (0, _sources.getSelectedLocation)(state);
  const selectedFrame = (0, _pause.getSelectedFrame)(state);

  if (!selectedFrame || !selectedLocation) {
    return false;
  }

  if ((0, _devtoolsSourceMap.isOriginalId)(selectedLocation.sourceId)) {
    return selectedLocation.sourceId === selectedFrame.location.sourceId;
  }

  return selectedLocation.sourceId === getGeneratedId(selectedFrame.location.sourceId);
}