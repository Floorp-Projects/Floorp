/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import { sortBy } from "lodash";
import { getSelectedLocation } from "./selected-location";

import type {
  MappedLocation,
  PartialPosition,
  SourceLocation,
  SourceId,
  Source,
} from "../types";

type IncompleteLocation = {
  sourceId: SourceId,
  line?: number,
  column?: number,
  sourceUrl?: string,
};

export function comparePosition(a: ?PartialPosition, b: ?PartialPosition) {
  return a && b && a.line == b.line && a.column == b.column;
}

export function createLocation({
  sourceId,
  line = 1,
  column,
  sourceUrl = "",
}: IncompleteLocation): SourceLocation {
  return {
    sourceId,
    line,
    column,
    sourceUrl,
  };
}

export function sortSelectedLocations<T: MappedLocation>(
  locations: $ReadOnlyArray<T>,
  selectedSource: ?Source
): Array<T> {
  return sortBy(locations, [
    // Priority: line number, undefined column, column number
    location => getSelectedLocation(location, selectedSource).line,
    location => {
      const selectedLocation = getSelectedLocation(location, selectedSource);
      return selectedLocation.column === undefined || selectedLocation.column;
    },
  ]);
}
