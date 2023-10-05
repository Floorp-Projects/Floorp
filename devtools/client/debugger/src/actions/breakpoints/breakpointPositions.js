/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { originalToGeneratedId } from "devtools/client/shared/source-map-loader/index";

import {
  getSource,
  getSourceFromId,
  getBreakpointPositionsForSource,
  getSourceActorsForSource,
} from "../../selectors";

import { makeBreakpointId } from "../../utils/breakpoint";
import { memoizeableAction } from "../../utils/memoizableAction";
import { fulfilled } from "../../utils/async-value";
import {
  debuggerToSourceMapLocation,
  sourceMapToDebuggerLocation,
  createLocation,
} from "../../utils/location";
import { validateSource } from "../../utils/context";

async function mapLocations(generatedLocations, { getState, sourceMapLoader }) {
  if (!generatedLocations.length) {
    return [];
  }

  const originalLocations = await sourceMapLoader.getOriginalLocations(
    generatedLocations.map(debuggerToSourceMapLocation)
  );
  return originalLocations.map((location, index) => ({
    // If location is null, this particular location doesn't map to any original source.
    location: location
      ? sourceMapToDebuggerLocation(getState(), location)
      : generatedLocations[index],
    generatedLocation: generatedLocations[index],
  }));
}

// Filter out positions, that are not in the original source Id
function filterBySource(positions, source) {
  if (!source.isOriginal) {
    return positions;
  }
  return positions.filter(position => position.location.source == source);
}

/**
 * Merge positions that refer to duplicated positions.
 * Some sourcemaped positions might refer to the exact same source/line/column triple.
 *
 * @param {Array<{location, generatedLocation}>} positions: List of possible breakable positions
 * @returns {Array<{location, generatedLocation}>} A new, filtered array.
 */
function filterByUniqLocation(positions) {
  const handledBreakpointIds = new Set();
  return positions.filter(({ location }) => {
    const breakpointId = makeBreakpointId(location);
    if (handledBreakpointIds.has(breakpointId)) {
      return false;
    }

    handledBreakpointIds.add(breakpointId);
    return true;
  });
}

function convertToList(results, source) {
  const positions = [];

  for (const line in results) {
    for (const column of results[line]) {
      positions.push(
        createLocation({
          line: Number(line),
          column,
          source,
        })
      );
    }
  }

  return positions;
}

function groupByLine(results, source, line) {
  const isOriginal = source.isOriginal;
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

async function _setBreakpointPositions(location, thunkArgs) {
  const { client, dispatch, getState, sourceMapLoader } = thunkArgs;
  const results = {};
  let generatedSource = location.source;
  if (location.source.isOriginal) {
    const ranges = await sourceMapLoader.getGeneratedRangesForOriginal(
      location.source.id,
      true
    );
    const generatedSourceId = originalToGeneratedId(location.source.id);
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

      // Retrieve the positions for all the source actors for the related generated source.
      // There might be many if it is loaded many times.
      // We limit the retrieval of positions within the given range, so that we don't
      // retrieve the whole bundle positions.
      const allActorsPositions = await Promise.all(
        getSourceActorsForSource(getState(), generatedSourceId).map(actor =>
          client.getSourceActorBreakpointPositions(actor, range)
        )
      );

      // `allActorsPositions` looks like this:
      // [
      //   { // Positions for the first source actor
      //     1: [ 2, 6 ], // Line 1 is breakable on column 2 and 6
      //     2: [ 2 ], // Line 2 is only breakable on column 2
      //   },
      //   {...} // Positions for another source actor
      // ]
      for (const actorPositions of allActorsPositions) {
        for (const rangeLine in actorPositions) {
          const columns = actorPositions[rangeLine];

          // Merge all actors's breakable columns and avoid duplication of columns reported as breakable
          const existing = results[rangeLine];
          if (existing) {
            for (const column of columns) {
              if (!existing.includes(column)) {
                existing.push(column);
              }
            }
          } else {
            results[rangeLine] = columns;
          }
        }
      }
    }
  } else {
    const { line } = location;
    if (typeof line !== "number") {
      throw new Error("Line is required for generated sources");
    }

    // We only retrieve the positions for the given requested line, that, for each source actor.
    // There might be many source actor, if it is loaded many times.
    // Or if this is an html page, with many inline scripts.
    const allActorsBreakableColumns = await Promise.all(
      getSourceActorsForSource(getState(), location.source.id).map(
        async actor => {
          const positions = await client.getSourceActorBreakpointPositions(
            actor,
            {
              // Only retrieve positions for the given line
              start: { line, column: 0 },
              end: { line: line + 1, column: 0 },
            }
          );
          return positions[line] || [];
        }
      )
    );

    for (const columns of allActorsBreakableColumns) {
      // Merge all actors's breakable columns and avoid duplication of columns reported as breakable
      const existing = results[line];
      if (existing) {
        for (const column of columns) {
          if (!existing.includes(column)) {
            existing.push(column);
          }
        }
      } else {
        results[line] = columns;
      }
    }
  }

  let positions = convertToList(results, generatedSource);
  positions = await mapLocations(positions, thunkArgs);
  // `mapLocations` may compute for a little while asynchronously,
  // ensure that the location is still valid before continuing.
  validateSource(getState(), location.source);

  positions = filterBySource(positions, location.source);
  positions = filterByUniqLocation(positions);
  positions = groupByLine(positions, location.source, location.line);

  dispatch({
    type: "ADD_BREAKPOINT_POSITIONS",
    source: location.source,
    positions,
  });
}

function generatedSourceActorKey(state, source) {
  const generatedSource = getSource(
    state,
    source.isOriginal ? originalToGeneratedId(source.id) : source.id
  );
  const actors = generatedSource
    ? getSourceActorsForSource(state, generatedSource.id).map(
        ({ actor }) => actor
      )
    : [];
  return [source.id, ...actors].join(":");
}

/**
 * This method will force retrieving the breakable positions for a given source, on a given line.
 * If this data has already been computed, it will returned the cached data.
 *
 * For original sources, this will query the SourceMap worker.
 * For generated sources, this will query the DevTools server and the related source actors.
 *
 * @param Object options
 *        Dictionary object with many arguments:
 * @param String options.sourceId
 *        The source we want to fetch breakable positions
 * @param Number options.line
 *        The line we want to know which columns are breakable.
 *        (note that this seems to be optional for original sources)
 * @return Array<Object>
 *         The list of all breakable positions, each object of this array will be like this:
 *         {
 *           line: Number
 *           column: Number
 *           source: Source object
 *         }
 */
export const setBreakpointPositions = memoizeableAction(
  "setBreakpointPositions",
  {
    getValue: (location, { getState }) => {
      const positions = getBreakpointPositionsForSource(
        getState(),
        location.source.id
      );
      if (!positions) {
        return null;
      }

      if (
        !location.source.isOriginal &&
        location.line &&
        !positions[location.line]
      ) {
        // We always return the full position dataset, but if a given line is
        // not available, we treat the whole set as loading.
        return null;
      }

      return fulfilled(positions);
    },
    createKey(location, { getState }) {
      const key = generatedSourceActorKey(getState(), location.source);
      return !location.source.isOriginal && location.line
        ? `${key}-${location.line}`
        : key;
    },
    action: async (location, thunkArgs) =>
      _setBreakpointPositions(location, thunkArgs),
  }
);
