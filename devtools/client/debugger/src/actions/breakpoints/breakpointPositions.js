/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  isOriginalId,
  isGeneratedId,
  originalToGeneratedId,
} from "devtools-source-map";
import { uniqBy, zip } from "lodash";

import {
  getSource,
  getSourceFromId,
  hasBreakpointPositions,
  hasBreakpointPositionsForLine,
  getBreakpointPositionsForSource,
  getSourceActorsForSource,
} from "../../selectors";

import type {
  MappedLocation,
  Range,
  SourceLocation,
  BreakpointPositions,
  Context,
} from "../../types";

import { makeBreakpointId } from "../../utils/breakpoint";
import {
  memoizeableAction,
  type MemoizedAction,
} from "../../utils/memoizableAction";
import type { ThunkArgs } from "../../actions/types";

async function mapLocations(
  generatedLocations: SourceLocation[],
  { sourceMaps }: ThunkArgs
) {
  if (generatedLocations.length == 0) {
    return [];
  }

  const { sourceId } = generatedLocations[0];

  const originalLocations = await sourceMaps.getOriginalLocations(
    sourceId,
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
        sourceUrl: url,
      });
    }
  }

  return positions;
}

function groupByLine(results, sourceId, line) {
  const isOriginal = isOriginalId(sourceId);
  const positions = {};

  // Ensure that we have an entry for the line fetched
  if (typeof line === "number") {
    positions[line] = [];
  }

  for (const result of results) {
    const location = isOriginal ? result.location : result.generatedLocation;

    if (!positions[location.line]) {
      positions[location.line] = [];
    }

    positions[location.line].push(result);
  }

  return positions;
}

async function _setBreakpointPositions(cx, sourceId, line, thunkArgs) {
  const { client, dispatch, getState, sourceMaps } = thunkArgs;
  let generatedSource = getSource(getState(), sourceId);
  if (!generatedSource) {
    return;
  }

  let results = {};
  if (isOriginalId(sourceId)) {
    // Explicitly typing ranges is required to work around the following issue
    // https://github.com/facebook/flow/issues/5294
    const ranges: Range[] = await sourceMaps.getGeneratedRangesForOriginal(
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
        range.end = {
          line: range.end.line + 1,
          column: 0,
        };
      }

      const bps = await client.getBreakpointPositions(
        getSourceActorsForSource(getState(), generatedSource.id),
        range
      );
      for (const bpLine in bps) {
        results[bpLine] = (results[bpLine] || []).concat(bps[bpLine]);
      }
    }
  } else {
    if (typeof line !== "number") {
      throw new Error("Line is required for generated sources");
    }

    results = await client.getBreakpointPositions(
      getSourceActorsForSource(getState(), generatedSource.id),
      { start: { line, column: 0 }, end: { line: line + 1, column: 0 } }
    );
  }

  let positions = convertToList(results, generatedSource);
  positions = await mapLocations(positions, thunkArgs);

  positions = filterBySource(positions, sourceId);
  positions = filterByUniqLocation(positions);
  positions = groupByLine(positions, sourceId, line);

  const source = getSource(getState(), sourceId);
  // NOTE: it's possible that the source was removed during a navigate
  if (!source) {
    return;
  }

  dispatch({
    type: "ADD_BREAKPOINT_POSITIONS",
    cx,
    source: source,
    positions,
  });

  return positions;
}

function generatedSourceActorKey(state, sourceId) {
  const generatedSource = getSource(
    state,
    isOriginalId(sourceId) ? originalToGeneratedId(sourceId) : sourceId
  );
  const actors = generatedSource
    ? getSourceActorsForSource(state, generatedSource.id).map(
        ({ actor }) => actor
      )
    : [];
  return [sourceId, ...actors].join(":");
}

export const setBreakpointPositions: MemoizedAction<
  { cx: Context, sourceId: string, line?: number },
  ?BreakpointPositions
> = memoizeableAction("setBreakpointPositions", {
  hasValue: ({ sourceId, line }, { getState }) =>
    isGeneratedId(sourceId) && line
      ? hasBreakpointPositionsForLine(getState(), sourceId, line)
      : hasBreakpointPositions(getState(), sourceId),
  getValue: ({ sourceId, line }, { getState }) =>
    getBreakpointPositionsForSource(getState(), sourceId),
  createKey({ sourceId, line }, { getState }) {
    const key = generatedSourceActorKey(getState(), sourceId);
    return isGeneratedId(sourceId) && line ? `${key}-${line}` : key;
  },
  action: async ({ cx, sourceId, line }, thunkArgs) =>
    _setBreakpointPositions(cx, sourceId, line, thunkArgs),
});
