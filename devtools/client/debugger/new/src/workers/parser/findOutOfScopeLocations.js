"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _get = require("devtools/client/shared/vendor/lodash").get;

var _get2 = _interopRequireDefault(_get);

var _findIndex = require("devtools/client/shared/vendor/lodash").findIndex;

var _findIndex2 = _interopRequireDefault(_findIndex);

var _findLastIndex = require("devtools/client/shared/vendor/lodash").findLastIndex;

var _findLastIndex2 = _interopRequireDefault(_findLastIndex);

var _contains = require("./utils/contains");

var _getSymbols = require("./getSymbols");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function findSymbols(source) {
  const {
    functions,
    comments
  } = (0, _getSymbols.getSymbols)(source);
  return {
    functions,
    comments
  };
}
/**
 * Returns the location for a given function path. If the path represents a
 * function declaration, the location will begin after the function identifier
 * but before the function parameters.
 */


function getLocation(func) {
  const location = _objectSpread({}, func.location); // if the function has an identifier, start the block after it so the
  // identifier is included in the "scope" of its parent


  const identifierEnd = (0, _get2.default)(func, "identifier.loc.end");

  if (identifierEnd) {
    location.start = identifierEnd;
  }

  return location;
}
/**
 * Find the nearest location containing the input position and
 * return new locations without inner locations under that nearest location
 *
 * @param locations Notice! The locations MUST be sorted by `sortByStart`
 *                  so that we can do linear time complexity operation.
 */


function removeInnerLocations(locations, position) {
  // First, let's find the nearest position-enclosing function location,
  // which is to find the last location enclosing the position.
  const newLocs = locations.slice();
  const parentIndex = (0, _findLastIndex2.default)(newLocs, loc => (0, _contains.containsPosition)(loc, position));

  if (parentIndex < 0) {
    return newLocs;
  } // Second, from the nearest location, loop locations again, stop looping
  // once seeing the 1st location not enclosed by the nearest location
  // to find the last inner locations inside the nearest location.


  const innerStartIndex = parentIndex + 1;
  const parentLoc = newLocs[parentIndex];
  const outerBoundaryIndex = (0, _findIndex2.default)(newLocs, loc => !(0, _contains.containsLocation)(parentLoc, loc), innerStartIndex);
  const innerBoundaryIndex = outerBoundaryIndex < 0 ? newLocs.length - 1 : outerBoundaryIndex - 1; // Third, remove those inner functions

  newLocs.splice(innerStartIndex, innerBoundaryIndex - parentIndex);
  return newLocs;
}
/**
 * Return an new locations array which excludes
 * items that are completely enclosed by another location in the input locations
 *
 * @param locations Notice! The locations MUST be sorted by `sortByStart`
 *                  so that we can do linear time complexity operation.
 */


function removeOverlaps(locations) {
  if (locations.length == 0) {
    return [];
  }

  const firstParent = locations[0];
  return locations.reduce(deduplicateNode, [firstParent]);
}

function deduplicateNode(nodes, location) {
  const parent = nodes[nodes.length - 1];

  if (!(0, _contains.containsLocation)(parent, location)) {
    nodes.push(location);
  }

  return nodes;
}
/**
 * Sorts an array of locations by start position
 */


function sortByStart(a, b) {
  if (a.start.line < b.start.line) {
    return -1;
  } else if (a.start.line === b.start.line) {
    return a.start.column - b.start.column;
  }

  return 1;
}
/**
 * Returns an array of locations that are considered out of scope for the given
 * location.
 */


function findOutOfScopeLocations(sourceId, position) {
  const {
    functions,
    comments
  } = findSymbols(sourceId);
  const commentLocations = comments.map(c => c.location);
  let locations = functions.map(getLocation).concat(commentLocations).sort(sortByStart); // Must remove inner locations then filter, otherwise,
  // we will mis-judge in-scope inner locations as out of scope.

  locations = removeInnerLocations(locations, position).filter(loc => !(0, _contains.containsPosition)(loc, position));
  return removeOverlaps(locations);
}

exports.default = findOutOfScopeLocations;