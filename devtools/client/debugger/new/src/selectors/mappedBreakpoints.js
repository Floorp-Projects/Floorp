"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getMappedBreakpoints = undefined;

var _breakpoint = require("../utils/breakpoint/index");

var _immutable = require("devtools/client/shared/vendor/immutable");

var _immutable2 = _interopRequireDefault(_immutable);

var _selectors = require("../selectors/index");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function formatBreakpoint(sources, selectedSource, breakpoint) {
  let location = breakpoint.location;
  let text = breakpoint.originalText;
  const condition = breakpoint.condition;
  const disabled = breakpoint.disabled;
  const locationId = (0, _breakpoint.makeLocationId)(location);
  const source = (0, _selectors.getSourceInSources)(sources, location.sourceId);

  if ((0, _devtoolsSourceMap.isGeneratedId)(selectedSource.id)) {
    location = breakpoint.generatedLocation || breakpoint.location;
    text = breakpoint.text;
  }

  const localBP = {
    locationId,
    location,
    text,
    source,
    condition,
    disabled
  };
  return localBP;
}

function _getMappedBreakpoints(breakpoints, sources, selectedSource) {
  if (!selectedSource) {
    return _immutable2.default.Map();
  }

  return breakpoints.map(bp => formatBreakpoint(sources, selectedSource, bp)).filter(bp => bp.source && !bp.source.isBlackBoxed);
}

const getMappedBreakpoints = exports.getMappedBreakpoints = (0, _reselect.createSelector)(_selectors.getBreakpoints, _selectors.getSources, _selectors.getSelectedSource, _selectors.getTopFrame, _selectors.getPauseReason, _getMappedBreakpoints);