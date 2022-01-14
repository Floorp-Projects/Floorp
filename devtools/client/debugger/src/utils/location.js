/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSelectedLocation } from "./selected-location";

export function comparePosition(a, b) {
  return a && b && a.line == b.line && a.column == b.column;
}

export function createLocation({
  sourceId,
  // Line 0 represents no specific line chosen for action
  line = 0,
  column,
  sourceUrl = "",
}) {
  return {
    sourceId,
    line,
    column,
    sourceUrl,
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
