"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.syncClientBreakpoint = syncClientBreakpoint;

var _breakpoint = require("../../utils/breakpoint/index");

var _sourceMaps = require("../../utils/source-maps");

var _source = require("../../utils/source");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _selectors = require("../../selectors/index");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

async function makeScopedLocation({
  name,
  offset
}, location, source) {
  const scope = await (0, _breakpoint.findScopeByName)(source, name); // fallback onto the location line, if the scope is not found
  // note: we may at some point want to delete the breakpoint if the scope
  // disappears

  const line = scope ? scope.location.start.line + offset.line : location.line;
  return {
    line,
    column: location.column,
    sourceUrl: source.url,
    sourceId: source.id
  };
}

function createSyncData(id, pendingBreakpoint, location, generatedLocation, previousLocation, text, originalText) {
  const overrides = _objectSpread({}, pendingBreakpoint, {
    generatedLocation,
    id,
    text,
    originalText
  });

  const breakpoint = (0, _breakpoint.createBreakpoint)(location, overrides);
  (0, _breakpoint.assertBreakpoint)(breakpoint);
  return {
    breakpoint,
    previousLocation
  };
} // we have three forms of syncing: disabled syncing, existing server syncing
// and adding a new breakpoint


async function syncClientBreakpoint(getState, client, sourceMaps, sourceId, pendingBreakpoint) {
  (0, _breakpoint.assertPendingBreakpoint)(pendingBreakpoint);
  const source = (0, _selectors.getSource)(getState(), sourceId);
  const generatedSourceId = sourceMaps.isOriginalId(sourceId) ? (0, _devtoolsSourceMap.originalToGeneratedId)(sourceId) : sourceId;
  const generatedSource = (0, _selectors.getSource)(getState(), generatedSourceId);

  if (!source) {
    return null;
  }

  const {
    location,
    astLocation
  } = pendingBreakpoint;

  const previousLocation = _objectSpread({}, location, {
    sourceId
  });

  const scopedLocation = await makeScopedLocation(astLocation, previousLocation, source);
  const scopedGeneratedLocation = await (0, _sourceMaps.getGeneratedLocation)(getState(), source, scopedLocation, sourceMaps); // this is the generatedLocation of the pending breakpoint, with
  // the source id updated to reflect the new connection

  const generatedLocation = _objectSpread({}, pendingBreakpoint.generatedLocation, {
    sourceId: generatedSourceId
  });

  const isSameLocation = !(0, _breakpoint.locationMoved)(generatedLocation, scopedGeneratedLocation);
  const existingClient = client.getBreakpointByLocation(generatedLocation);
  /** ******* CASE 1: No server change ***********/
  // early return if breakpoint is disabled or we are in the sameLocation
  // send update only to redux

  if (pendingBreakpoint.disabled || existingClient && isSameLocation) {
    const id = pendingBreakpoint.disabled ? "" : existingClient.id;
    const originalText = (0, _source.getTextAtPosition)(source, previousLocation);
    const text = (0, _source.getTextAtPosition)(generatedSource, generatedLocation);
    return createSyncData(id, pendingBreakpoint, scopedLocation, scopedGeneratedLocation, previousLocation, text, originalText);
  } // clear server breakpoints if they exist and we have moved


  if (existingClient) {
    await client.removeBreakpoint(generatedLocation);
  }
  /** ******* Case 2: Add New Breakpoint ***********/
  // If we are not disabled, set the breakpoint on the server and get
  // that info so we can set it on our breakpoints.


  if (!scopedGeneratedLocation.line) {
    return {
      previousLocation,
      breakpoint: null
    };
  }

  const {
    id,
    actualLocation
  } = await client.setBreakpoint(scopedGeneratedLocation, pendingBreakpoint.condition, sourceMaps.isOriginalId(sourceId)); // the breakpoint might have slid server side, so we want to get the location
  // based on the server's return value

  const newGeneratedLocation = actualLocation;
  const newLocation = await sourceMaps.getOriginalLocation(newGeneratedLocation);
  const originalText = (0, _source.getTextAtPosition)(source, newLocation);
  const text = (0, _source.getTextAtPosition)(generatedSource, newGeneratedLocation);
  return createSyncData(id, pendingBreakpoint, newLocation, newGeneratedLocation, previousLocation, text, originalText);
}