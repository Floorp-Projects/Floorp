/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { containsLocation, containsPosition } from "./utils/contains";

import { getInternalSymbols } from "./getSymbols";

function findSymbols(source) {
  const { functions, comments } = getInternalSymbols(source);
  return { functions, comments };
}

/**
 * Returns the location for a given function path. If the path represents a
 * function declaration, the location will begin after the function identifier
 * but before the function parameters.
 */

function getLocation(func) {
  const location = { ...func.location };

  // if the function has an identifier, start the block after it so the
  // identifier is included in the "scope" of its parent
  const identifierEnd = func?.identifier?.loc?.end;
  if (identifierEnd) {
    location.start = identifierEnd;
  }

  return location;
}

/**
 * Find the nearest location containing the input position and
 * return inner locations under that nearest location
 *
 * @param {Array<Object>} locations Notice! The locations MUST be sorted by `sortByStart`
 *                  so that we can do linear time complexity operation.
 * @returns {Array<Object>}
 */
function getInnerLocations(locations, position) {
  // First, let's find the nearest position-enclosing function location,
  // which is to find the last location enclosing the position.
  let parentIndex;
  for (let i = locations.length - 1; i >= 0; i--) {
    const loc = locations[i];
    if (containsPosition(loc, position)) {
      parentIndex = i;
      break;
    }
  }

  if (parentIndex == undefined) {
    return [];
  }
  const parentLoc = locations[parentIndex];

  // Then, from the nearest location, loop locations again and put locations into
  // the innerLocations array until we get to a location not enclosed by the nearest location.
  const innerLocations = [];
  for (let i = parentIndex + 1; i < locations.length; i++) {
    const loc = locations[i];
    if (!containsLocation(parentLoc, loc)) {
      break;
    }

    innerLocations.push(loc);
  }

  return innerLocations;
}

/**
 * Return an new locations array which excludes
 * items that are completely enclosed by another location in the input locations
 *
 * @param locations Notice! The locations MUST be sorted by `sortByStart`
 *                  so that we can do linear time complexity operation.
 */
function removeOverlaps(locations) {
  if (!locations.length) {
    return [];
  }
  const firstParent = locations[0];
  return locations.reduce(deduplicateNode, [firstParent]);
}

function deduplicateNode(nodes, location) {
  const parent = nodes[nodes.length - 1];
  if (!containsLocation(parent, location)) {
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
function findOutOfScopeLocations(location) {
  const { functions, comments } = findSymbols(location.source.id);
  const commentLocations = comments.map(c => c.location);
  const locations = functions
    .map(getLocation)
    .concat(commentLocations)
    .sort(sortByStart);

  const innerLocations = getInnerLocations(locations, location);
  const outerLocations = locations.filter(loc => {
    if (innerLocations.includes(loc)) {
      return false;
    }

    return !containsPosition(loc, location);
  });
  return removeOverlaps(outerLocations);
}

export default findOutOfScopeLocations;
