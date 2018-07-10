"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = addBreakpoint;

var _breakpoint = require("../../utils/breakpoint/index");

var _selectors = require("../../selectors/index");

var _sourceMaps = require("../../utils/source-maps");

var _source = require("../../utils/source");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
async function addBreakpoint(getState, client, sourceMaps, breakpoint) {
  const state = getState();
  const source = (0, _selectors.getSource)(state, breakpoint.location.sourceId);
  const location = { ...breakpoint.location,
    sourceUrl: source.url
  };
  const generatedLocation = await (0, _sourceMaps.getGeneratedLocation)(state, source, location, sourceMaps);
  const generatedSource = (0, _selectors.getSource)(state, generatedLocation.sourceId);
  (0, _breakpoint.assertLocation)(location);
  (0, _breakpoint.assertLocation)(generatedLocation);

  if ((0, _breakpoint.breakpointExists)(state, location)) {
    const newBreakpoint = { ...breakpoint,
      location,
      generatedLocation
    };
    (0, _breakpoint.assertBreakpoint)(newBreakpoint);
    return {
      breakpoint: newBreakpoint
    };
  }

  const {
    id,
    hitCount,
    actualLocation
  } = await client.setBreakpoint(generatedLocation, breakpoint.condition, sourceMaps.isOriginalId(location.sourceId));
  const newGeneratedLocation = actualLocation || generatedLocation;
  const newLocation = await sourceMaps.getOriginalLocation(newGeneratedLocation);
  const symbols = (0, _selectors.getSymbols)(getState(), source);
  const astLocation = await (0, _breakpoint.getASTLocation)(source, symbols, newLocation);
  const originalText = (0, _source.getTextAtPosition)(source, location);
  const text = (0, _source.getTextAtPosition)(generatedSource, actualLocation);
  const newBreakpoint = {
    id,
    disabled: false,
    hidden: breakpoint.hidden,
    loading: false,
    condition: breakpoint.condition,
    location: newLocation,
    astLocation,
    hitCount,
    generatedLocation: newGeneratedLocation,
    text,
    originalText
  };
  (0, _breakpoint.assertBreakpoint)(newBreakpoint);
  const previousLocation = (0, _breakpoint.locationMoved)(location, newLocation) ? location : null;
  return {
    breakpoint: newBreakpoint,
    previousLocation
  };
}