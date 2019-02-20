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

    let range;
    if (isOriginalId(sourceId)) {
      range = await sourceMaps.getFileGeneratedRange(source);
      sourceId = originalToGeneratedId(sourceId);
      source = getSourceFromId(getState(), sourceId);
    }

    const results = await client.getBreakpointPositions(
      source.actors[0],
      range
    );

    let positions = convertToList(results, sourceId);
    positions = await mapLocations(positions, getState(), source, sourceMaps);
    return dispatch({ type: "ADD_BREAKPOINT_POSITIONS", positions, location });
  };
}
