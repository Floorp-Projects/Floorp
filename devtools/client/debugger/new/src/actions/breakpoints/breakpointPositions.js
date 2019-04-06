/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isOriginalId, originalToGeneratedId } from "devtools-source-map";
import { uniqBy, zip } from "lodash";

import {
  getSource,
  getSourceFromId,
  hasBreakpointPositions,
  getBreakpointPositionsForSource
} from "../../selectors";

import type {
  MappedLocation,
  SourceLocation,
  BreakpointPositions,
  Context
} from "../../types";
import { makeBreakpointId } from "../../utils/breakpoint";
import {
  memoizeableAction,
  type MemoizedAction
} from "../../utils/memoizableAction";

import typeof SourceMaps from "../../../packages/devtools-source-map/src";

// const requests = new Map();

async function mapLocations(
  generatedLocations: SourceLocation[],
  { sourceMaps }: { sourceMaps: SourceMaps }
) {
  const originalLocations = await sourceMaps.getOriginalLocations(
    generatedLocations
  );

  return zip(originalLocations, generatedLocations).map(
    ([location, generatedLocation]) => ({ location, generatedLocation })
  );
}

// Filter out positions, that are not in the original source Id
function filterBySource(positions, sourceId) {
  if (!isOriginalId(sourceId)) {
    return positions;
  }
  return positions.filter(position => position.location.sourceId == sourceId);
}

function filterByUniqLocation(positions: MappedLocation[]) {
  return uniqBy(positions, ({ location }) => makeBreakpointId(location));
}

function convertToList(results, source) {
  const { id, url } = source;
  const positions = [];

  for (const line in results) {
    for (const column of results[line]) {
      positions.push({
        line: Number(line),
        column: column,
        sourceId: id,
        sourceUrl: url
      });
    }
  }

  return positions;
}

async function _setBreakpointPositions(cx, sourceId, thunkArgs) {
  const { client, dispatch, getState, sourceMaps } = thunkArgs;
  let generatedSource = getSource(getState(), sourceId);
  if (!generatedSource) {
    return;
  }

  let results = {};
  if (isOriginalId(sourceId)) {
    const ranges = await sourceMaps.getGeneratedRangesForOriginal(
      sourceId,
      generatedSource.url,
      true
    );
    const generatedSourceId = originalToGeneratedId(sourceId);
    generatedSource = getSourceFromId(getState(), generatedSourceId);

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

      const bps = await client.getBreakpointPositions(generatedSource, range);
      for (const line in bps) {
        results[line] = (results[line] || []).concat(bps[line]);
      }
    }
  } else {
    results = await client.getBreakpointPositions(generatedSource);
  }

  let positions = convertToList(results, generatedSource);
  positions = await mapLocations(positions, thunkArgs);

  positions = filterBySource(positions, sourceId);
  positions = filterByUniqLocation(positions);

  const source = getSource(getState(), sourceId);
  // NOTE: it's possible that the source was removed during a navigate
  if (!source) {
    return;
  }

  dispatch({
    type: "ADD_BREAKPOINT_POSITIONS",
    cx,
    source: source,
    positions
  });

  return positions;
}

export const setBreakpointPositions: MemoizedAction<
  { cx: Context, sourceId: string },
  ?BreakpointPositions
> = memoizeableAction("setBreakpointPositions", {
  hasValue: ({ sourceId }, { getState }) =>
    hasBreakpointPositions(getState(), sourceId),
  getValue: ({ sourceId }, { getState }) =>
    getBreakpointPositionsForSource(getState(), sourceId),
  createKey({ sourceId }, { getState }) {
    const generatedSource = getSource(
      getState(),
      isOriginalId(sourceId) ? originalToGeneratedId(sourceId) : sourceId
    );
    const actors = generatedSource
      ? generatedSource.actors.map(({ actor }) => actor)
      : [];
    return [sourceId, ...actors].join(":");
  },
  action: ({ cx, sourceId }, thunkArgs) =>
    _setBreakpointPositions(cx, sourceId, thunkArgs)
});
