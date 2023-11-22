/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSelectedLocation } from "./selected-location";
import { getSource } from "../selectors";

/**
 * Note that arguments can be created via `createLocation`.
 * But they can also be created via `createPendingLocation` in reducer/pending-breakpoints.js.
 * Both will have similar line and column attributes.
 */
export function comparePosition(a, b) {
  return a && b && a.line == b.line && a.column == b.column;
}

export function createLocation({
  source,
  sourceActor = null,

  // Line 0 represents no specific line chosen for action
  line = 0,
  column,
}) {
  return {
    source,
    sourceActor,
    sourceActorId: sourceActor?.id,

    // `line` is 1-based while `column` is 0-based.
    // This data is mostly coming from and driven by
    // JSScript::lineno and JSScript::column
    // https://searchfox.org/mozilla-central/rev/d81e60336d9f498ad3985491dc17c2b77969ade4/js/src/vm/JSScript.h#1544-1547
    line,
    column,
  };
}

/**
 * Convert location objects created via `createLocation` into
 * the format used by the Source Map Loader/Worker.
 * It only needs sourceId, line and column attributes.
 */
export function debuggerToSourceMapLocation(location) {
  return {
    sourceId: location.source.id,
    line: location.line,
    column: location.column,
  };
}

/**
 * Pending location only need these three attributes,
 * and especially doesn't need the large source and sourceActor objects of the regular location objects.
 *
 * @param {Object} location
 */
export function createPendingSelectedLocation(location) {
  return {
    url: location.source.url,

    line: location.line,
    column: location.column,
  };
}

export function sortSelectedLocations(locations, selectedSource) {
  return Array.from(locations).sort((locationA, locationB) => {
    const aSelected = getSelectedLocation(locationA, selectedSource);
    const bSelected = getSelectedLocation(locationB, selectedSource);

    // Order the locations by line number…
    if (aSelected.line < bSelected.line) {
      return -1;
    }

    if (aSelected.line > bSelected.line) {
      return 1;
    }

    // … and if we have the same line, we want to return location with undefined columns
    // first, and then order them by column
    if (aSelected.column == bSelected.column) {
      return 0;
    }

    if (aSelected.column === undefined) {
      return -1;
    }

    if (bSelected.column === undefined) {
      return 1;
    }

    return aSelected.column < bSelected.column ? -1 : 1;
  });
}

/**
 * Source map Loader/Worker and debugger frontend don't use the same objects for locations.
 * Worker uses 'sourceId' attributes whereas the frontend has 'source' attribute.
 */
export function sourceMapToDebuggerLocation(state, location) {
  // From MapScopes modules, we might re-process the exact same location objects
  // for which we would already have computed the source object,
  // and which would lack sourceId attribute.
  if (location.source) {
    return location;
  }

  // SourceMapLoader doesn't known about debugger's source objects
  // so that we have to fetch it from here
  const source = getSource(state, location.sourceId);
  if (!source) {
    throw new Error(`Could not find source-map source ${location.sourceId}`);
  }

  return createLocation({
    ...location,
    source,
  });
}
