/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { sortBy } from "lodash";
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
  return sortBy(locations, [
    // Priority: line number, undefined column, column number
    location => getSelectedLocation(location, selectedSource).line,
    location => {
      const selectedLocation = getSelectedLocation(location, selectedSource);
      return selectedLocation.column === undefined || selectedLocation.column;
    },
  ]);
}
