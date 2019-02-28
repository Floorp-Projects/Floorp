/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isOriginalId, originalToGeneratedId } from "devtools-source-map";

import {
  getSourceFromId,
  getBreakpointPositionsForSource
} from "../../selectors";

import type { SourceLocation } from "../../types";
import type { ThunkArgs } from "../../actions/types";
import { getOriginalLocation } from "../../utils/source-maps";

async function mapLocations(generatedLocations, state, source, sourceMaps) {
  return Promise.all(
    generatedLocations.map(async generatedLocation => {
      const location = await getOriginalLocation(
        generatedLocation,
        source,
        sourceMaps
      );

      return { location, generatedLocation };
    })
  );
}

function convertToList(results, sourceId) {
  const positions = [];

  for (const line in results) {
    for (const column of results[line]) {
      positions.push({ line: Number(line), column: column, sourceId });
    }
  }

  return positions;
}

export function setBreakpointPositions(location: SourceLocation) {
  return async ({ getState, dispatch, client, sourceMaps }: ThunkArgs) => {
    let sourceId = location.sourceId;
    if (getBreakpointPositionsForSource(getState(), sourceId)) {
      return;
    }

    let source = getSourceFromId(getState(), sourceId);

    let results = {};
    if (isOriginalId(sourceId)) {
      const ranges = await sourceMaps.getGeneratedRangesForOriginal(
        sourceId,
        source.url,
        true
      );
      sourceId = originalToGeneratedId(sourceId);
      source = getSourceFromId(getState(), sourceId);

      // Note: While looping here may not look ideal, in the vast majority of
      // cases, the number of ranges here should be very small, and is quite
      // likely to only be a single range.
      for (const range of ranges) {
        // Wrap infinite end positions to the next line to keep things simple
        // and because we know we don't care about the end-line whitespace
        // in this case.
        if (range.end.column === Infinity) {
          range.end.line += 1;
          range.end.column = 0;
        }

        const bps = await client.getBreakpointPositions(
          source.actors[0],
          range
        );
        for (const line in bps) {
          results[line] = (results[line] || []).concat(bps[line]);
        }
      }
    } else {
      results = await client.getBreakpointPositions(source.actors[0]);
    }

    let positions = convertToList(results, sourceId);
    positions = await mapLocations(positions, getState(), source, sourceMaps);
    return dispatch({ type: "ADD_BREAKPOINT_POSITIONS", positions, location });
  };
}
