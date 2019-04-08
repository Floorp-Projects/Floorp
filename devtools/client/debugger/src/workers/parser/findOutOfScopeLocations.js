/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { AstLocation, AstPosition } from "./types";

import get from "lodash/get";
import findIndex from "lodash/findIndex";
import findLastIndex from "lodash/findLastIndex";

import { containsLocation, containsPosition } from "./utils/contains";

import { getSymbols } from "./getSymbols";

function findSymbols(source) {
  const { functions, comments } = getSymbols(source);
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
  const identifierEnd = get(func, "identifier.loc.end");
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
function removeInnerLocations(locations: AstLocation[], position: AstPosition) {
  // First, let's find the nearest position-enclosing function location,
  // which is to find the last location enclosing the position.
  const newLocs = locations.slice();
  const parentIndex = findLastIndex(newLocs, loc =>
    containsPosition(loc, position)
  );
  if (parentIndex < 0) {
    return newLocs;
  }

  // Second, from the nearest location, loop locations again, stop looping
  // once seeing the 1st location not enclosed by the nearest location
  // to find the last inner locations inside the nearest location.
  const innerStartIndex = parentIndex + 1;
  const parentLoc = newLocs[parentIndex];
  const outerBoundaryIndex = findIndex(
    newLocs,
    loc => !containsLocation(parentLoc, loc),
    innerStartIndex
  );
  const innerBoundaryIndex =
    outerBoundaryIndex < 0 ? newLocs.length - 1 : outerBoundaryIndex - 1;

  // Third, remove those inner functions
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
function removeOverlaps(locations: AstLocation[]) {
  if (locations.length == 0) {
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
function sortByStart(a: AstLocation, b: AstLocation) {
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
function findOutOfScopeLocations(
  sourceId: string,
  position: AstPosition
): AstLocation[] {
  const { functions, comments } = findSymbols(sourceId);
  const commentLocations = comments.map(c => c.location);
  let locations = functions
    .map(getLocation)
    .concat(commentLocations)
    .sort(sortByStart);
  // Must remove inner locations then filter, otherwise,
  // we will mis-judge in-scope inner locations as out of scope.
  locations = removeInnerLocations(locations, position).filter(
    loc => !containsPosition(loc, position)
  );
  return removeOverlaps(locations);
}

export default findOutOfScopeLocations;
