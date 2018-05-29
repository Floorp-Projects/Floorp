"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.findScopeByName = exports.getASTLocation = undefined;

var _astBreakpointLocation = require("./astBreakpointLocation");

Object.defineProperty(exports, "getASTLocation", {
  enumerable: true,
  get: function () {
    return _astBreakpointLocation.getASTLocation;
  }
});
Object.defineProperty(exports, "findScopeByName", {
  enumerable: true,
  get: function () {
    return _astBreakpointLocation.findScopeByName;
  }
});
exports.firstString = firstString;
exports.locationMoved = locationMoved;
exports.makeLocationId = makeLocationId;
exports.getLocationWithoutColumn = getLocationWithoutColumn;
exports.makePendingLocationId = makePendingLocationId;
exports.assertBreakpoint = assertBreakpoint;
exports.assertPendingBreakpoint = assertPendingBreakpoint;
exports.assertLocation = assertLocation;
exports.assertPendingLocation = assertPendingLocation;
exports.breakpointAtLocation = breakpointAtLocation;
exports.breakpointExists = breakpointExists;
exports.createBreakpoint = createBreakpoint;
exports.createPendingBreakpoint = createPendingBreakpoint;

var _selectors = require("../../selectors/index");

var _assert = require("../assert");

var _assert2 = _interopRequireDefault(_assert);

var _prefs = require("../prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

// Return the first argument that is a string, or null if nothing is a
// string.
function firstString(...args) {
  for (const arg of args) {
    if (typeof arg === "string") {
      return arg;
    }
  }

  return null;
}

function locationMoved(location, newLocation) {
  return location.line !== newLocation.line || location.column !== newLocation.column;
}

function makeLocationId(location) {
  const {
    sourceId,
    line,
    column
  } = location;
  const columnString = column || "";
  return `${sourceId}:${line}:${columnString}`;
}

function getLocationWithoutColumn(location) {
  const {
    sourceId,
    line
  } = location;
  return `${sourceId}:${line}`;
}

function makePendingLocationId(location) {
  assertPendingLocation(location);
  const {
    sourceUrl,
    line,
    column
  } = location;
  const sourceUrlString = sourceUrl || "";
  const columnString = column || "";
  return `${sourceUrlString}:${line}:${columnString}`;
}

function assertBreakpoint(breakpoint) {
  assertLocation(breakpoint.location);
  assertLocation(breakpoint.generatedLocation);
}

function assertPendingBreakpoint(pendingBreakpoint) {
  assertPendingLocation(pendingBreakpoint.location);
  assertPendingLocation(pendingBreakpoint.generatedLocation);
}

function assertLocation(location) {
  assertPendingLocation(location);
  const {
    sourceId
  } = location;
  (0, _assert2.default)(!!sourceId, "location must have a source id");
}

function assertPendingLocation(location) {
  (0, _assert2.default)(!!location, "location must exist");
  const {
    sourceUrl
  } = location; // sourceUrl is null when the source does not have a url

  (0, _assert2.default)(sourceUrl !== undefined, "location must have a source url");
  (0, _assert2.default)(location.hasOwnProperty("line"), "location must have a line");
  (0, _assert2.default)(location.hasOwnProperty("column") != null, "location must have a column");
} // syncing


function breakpointAtLocation(breakpoints, {
  line,
  column
}) {
  return breakpoints.find(breakpoint => {
    const sameLine = breakpoint.location.line === line;

    if (!sameLine) {
      return false;
    } // NOTE: when column breakpoints are disabled we want to find
    // the first breakpoint


    if (!_prefs.features.columnBreakpoints) {
      return true;
    }

    return breakpoint.location.column === column;
  });
}

function breakpointExists(state, location) {
  const currentBp = (0, _selectors.getBreakpoint)(state, location);
  return currentBp && !currentBp.disabled;
}

function createBreakpoint(location, overrides = {}) {
  const {
    condition,
    disabled,
    hidden,
    generatedLocation,
    astLocation,
    id,
    text,
    originalText
  } = overrides;
  const defaultASTLocation = {
    name: undefined,
    offset: location
  };
  const properties = {
    id,
    condition: condition || null,
    disabled: disabled || false,
    hidden: hidden || false,
    loading: false,
    astLocation: astLocation || defaultASTLocation,
    generatedLocation: generatedLocation || location,
    location,
    text,
    originalText
  };
  return properties;
}

function createPendingLocation(location) {
  const {
    sourceUrl,
    line,
    column
  } = location;
  return {
    sourceUrl,
    line,
    column
  };
}

function createPendingBreakpoint(bp) {
  const pendingLocation = createPendingLocation(bp.location);
  const pendingGeneratedLocation = createPendingLocation(bp.generatedLocation);
  assertPendingLocation(pendingLocation);
  return {
    condition: bp.condition,
    disabled: bp.disabled,
    location: pendingLocation,
    astLocation: bp.astLocation,
    generatedLocation: pendingGeneratedLocation
  };
}