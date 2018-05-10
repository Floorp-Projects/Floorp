"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getVisibleBreakpoints = getVisibleBreakpoints;

var _breakpoints = require("../reducers/breakpoints");

var _sources = require("../reducers/sources");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getLocation(breakpoint, isGeneratedSource) {
  return isGeneratedSource ? breakpoint.generatedLocation || breakpoint.location : breakpoint.location;
}

function formatBreakpoint(breakpoint, selectedSource) {
  const {
    condition,
    loading,
    disabled,
    hidden
  } = breakpoint;
  const sourceId = selectedSource.get("id");
  const isGeneratedSource = (0, _devtoolsSourceMap.isGeneratedId)(sourceId);
  return {
    location: getLocation(breakpoint, isGeneratedSource),
    condition,
    loading,
    disabled,
    hidden
  };
}

function isVisible(breakpoint, selectedSource) {
  const sourceId = selectedSource.get("id");
  const isGeneratedSource = (0, _devtoolsSourceMap.isGeneratedId)(sourceId);
  const location = getLocation(breakpoint, isGeneratedSource);
  return location.sourceId === sourceId;
}
/*
 * Finds the breakpoints, which appear in the selected source.
 *
 * This
 */


function getVisibleBreakpoints(state) {
  const selectedSource = (0, _sources.getSelectedSource)(state);

  if (!selectedSource) {
    return null;
  }

  return (0, _breakpoints.getBreakpoints)(state).filter(bp => isVisible(bp, selectedSource)).map(bp => formatBreakpoint(bp, selectedSource));
}