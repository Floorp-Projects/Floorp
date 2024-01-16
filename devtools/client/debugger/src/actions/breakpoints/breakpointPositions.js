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
  sourceMapToDebuggerLocation,
  createLocation,
} from "../../utils/location";
import { validateSource } from "../../utils/context";

/**
 * Helper function which consumes breakpoints positions sent by the server
 * and map them to location objects.
 * During this process, the SourceMapLoader will be queried to map the positions from generated to original locations.
 *
 * @param {Object} breakpointPositions
 *        The positions to map related to the generated source:
 *          {
 *            1: [ 2, 6 ], // Line 1 is breakable on column 2 and 6
 *            2: [ 2 ], // Line 2 is only breakable on column 2
 *          }
 * @param {Object} generatedSource
 * @param {Object} location
 *        The current location we are computing breakable positions.
 * @param {Object} thunk arguments
 * @return {Object}
 *         The mapped breakable locations in the original source:
 *          {
 *            1: [ { source, line: 1, column: 2} , { source, line: 1, column 6 } ], // Line 1 is not mapped as location are same as breakpointPositions.
 *            10: [ { source, line: 10, column: 28 } ], // Line 2 is mapped and locations and line key refers to the original source positions.
 *          }
 */
async function mapToLocations(
  breakpointPositions,
  generatedSource,
  mappedLocation,
  { getState, sourceMapLoader }
) {
  // Map breakable positions from generated to original locations.
  let mappedBreakpointPositions = await sourceMapLoader.getOriginalLocations(
    breakpointPositions,
    generatedSource.id
  );
  // The Source Map Loader will return null when there is no source map for that generated source.
  // Consider the map as unrelated to source map and process the source actor positions as-is.
  if (!mappedBreakpointPositions) {
    mappedBreakpointPositions = breakpointPositions;
  }

  const positions = {};

  // Ensure that we have an entry for the line fetched
  if (typeof mappedLocation.line === "number") {
    positions[mappedLocation.line] = [];
  }

  const handledBreakpointIds = new Set();
  const isOriginal = mappedLocation.source.isOriginal;
  const originalSourceId = mappedLocation.source.id;

  for (let line in mappedBreakpointPositions) {
    // createLocation expects a number and not a string.
    line = parseInt(line, 10);
    for (const columnOrSourceMapLocation of mappedBreakpointPositions[line]) {
      let location, generatedLocation;

      // When processing a source unrelated to source map, `mappedBreakpointPositions` will be equal to `breakpointPositions`.
      // and columnOrSourceMapLocation will always be a number.
      // But it will also be a number if we process a source mapped file and SourceMapLoader didn't find any valid mapping
      // for the current position (line and column).
      // When this happen to be a number it means it isn't mapped and columnOrSourceMapLocation refers to the column index.
      if (typeof columnOrSourceMapLocation == "number") {
        // If columnOrSourceMapLocation is a number, it means that this location doesn't mapped to an original source.
        // So if we are currently computation positions for an original source, we can skip this breakable positions.
        if (isOriginal) {
          continue;
        }
        location = generatedLocation = createLocation({
          line,
          column: columnOrSourceMapLocation,
          source: generatedSource,
        });
      } else {
        // Otherwise, for sources which are mapped. `columnOrSourceMapLocation` will be a SourceMapLoader location object.
        // This location object will refer to the location where the current column (columnOrSourceMapLocation.generatedColumn)
        // mapped in the original file.

        // When computing positions for an original source, ignore the location if that mapped to another original source.
        if (
          isOriginal &&
          columnOrSourceMapLocation.sourceId != originalSourceId
        ) {
          continue;
        }

        location = sourceMapToDebuggerLocation(
          getState(),
          columnOrSourceMapLocation
        );

        // Merge positions that refer to duplicated positions.
        // Some sourcemaped positions might refer to the exact same source/line/column triple.
        const breakpointId = makeBreakpointId(location);
        if (handledBreakpointIds.has(breakpointId)) {
          continue;
        }
        handledBreakpointIds.add(breakpointId);

        generatedLocation = createLocation({
          line,
          column: columnOrSourceMapLocation.generatedColumn,
          source: generatedSource,
        });
      }

      // The positions stored in redux will be keyed by original source's line (if we are
      // computing the original source positions), or the generated source line.
      // Note that when we compute the bundle positions, location may refer to the original source,
      // but we still want to use the generated location as key.
      const keyLocation = isOriginal ? location : generatedLocation;
      const keyLine = keyLocation.line;
      if (!positions[keyLine]) {
        positions[keyLine] = [];
      }
      positions[keyLine].push({ location, generatedLocation });
    }
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

  const positions = await mapToLocations(
    results,
    generatedSource,
    location,
    thunkArgs
  );
  // `mapToLocations` may compute for a little while asynchronously,
  // ensure that the location is still valid before continuing.
  validateSource(getState(), location.source);

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
