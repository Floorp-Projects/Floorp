"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.shouldStep = shouldStep;

var _lodash = require("devtools/client/shared/vendor/lodash");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _selectors = require("../../selectors/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getFrameLocation(source, frame) {
  if (!frame) {
    return null;
  }

  return (0, _devtoolsSourceMap.isOriginalId)(source.id) ? frame.location : frame.generatedLocation;
}

function shouldStep(rootFrame, state, sourceMaps) {
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  const previousFrameInfo = (0, _selectors.getPreviousPauseFrameLocation)(state);

  if (!rootFrame || !selectedSource) {
    return false;
  }

  const previousFrameLoc = getFrameLocation(selectedSource, previousFrameInfo);
  const frameLoc = getFrameLocation(selectedSource, rootFrame);
  const sameLocation = previousFrameLoc && (0, _lodash.isEqual)(previousFrameLoc, frameLoc);
  const pausePoint = (0, _selectors.getPausePoint)(state, frameLoc);
  const invalidPauseLocation = pausePoint && !pausePoint.step; // We always want to pause in generated locations

  if (!frameLoc || (0, _devtoolsSourceMap.isGeneratedId)(frameLoc.sourceId)) {
    return false;
  }

  return sameLocation || invalidPauseLocation;
}