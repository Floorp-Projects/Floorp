"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = addBreakpoint;

var _breakpoint = require("../../utils/breakpoint/index");

var _selectors = require("../../selectors/index");

var _sourceMaps = require("../../utils/source-maps");

var _source = require("../../utils/source");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

async function addBreakpoint(getState, client, sourceMaps, breakpoint) {
  const state = getState();
  const source = (0, _selectors.getSource)(state, breakpoint.location.sourceId);

  const location = _objectSpread({}, breakpoint.location, {
    sourceUrl: source.url
  });

  const generatedLocation = await (0, _sourceMaps.getGeneratedLocation)(state, source, location, sourceMaps);
  const generatedSource = (0, _selectors.getSource)(state, generatedLocation.sourceId);
  (0, _breakpoint.assertLocation)(location);
  (0, _breakpoint.assertLocation)(generatedLocation);

  if ((0, _breakpoint.breakpointExists)(state, location)) {
    const newBreakpoint = _objectSpread({}, breakpoint, {
      location,
      generatedLocation
    });

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