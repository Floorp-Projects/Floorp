/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/**
 * Source Map Worker
 * @module utils/source-map-worker
 */

const {
  SourceMapConsumer,
} = require("resource://devtools/client/shared/vendor/source-map/source-map.js");

// Initialize the source-map library right away so that all other code can use it.
SourceMapConsumer.initialize({
  "lib/mappings.wasm":
    "resource://devtools/client/shared/vendor/source-map/lib/mappings.wasm",
});

const {
  networkRequest,
} = require("resource://devtools/client/shared/source-map-loader/utils/network-request.js");
const assert = require("resource://devtools/client/shared/source-map-loader/utils/assert.js");
const {
  fetchSourceMap,
  hasOriginalURL,
  clearOriginalURLs,
} = require("resource://devtools/client/shared/source-map-loader/utils/fetchSourceMap.js");
const {
  getSourceMap,
  getSourceMapWithMetadata,
  setSourceMap,
  clearSourceMaps: clearSourceMapsRequests,
} = require("resource://devtools/client/shared/source-map-loader/utils/sourceMapRequests.js");
const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
  getContentType,
} = require("resource://devtools/client/shared/source-map-loader/utils/index.js");
const {
  clearWasmXScopes,
} = require("resource://devtools/client/shared/source-map-loader/wasm-dwarf/wasmXScopes.js");

async function getOriginalURLs(generatedSource) {
  await fetchSourceMap(generatedSource);
  const data = await getSourceMapWithMetadata(generatedSource.id);
  return data ? data.sources : null;
}

async function getSourceMapIgnoreList(generatedSourceId) {
  const data = await getSourceMapWithMetadata(generatedSourceId);
  return data ? data.ignoreListUrls : [];
}

const COMPUTED_SPANS = new WeakSet();

const SOURCE_MAPPINGS = new WeakMap();
async function getOriginalRanges(sourceId) {
  if (!isOriginalId(sourceId)) {
    return [];
  }

  const generatedSourceId = originalToGeneratedId(sourceId);
  const data = await getSourceMapWithMetadata(generatedSourceId);
  if (!data) {
    return [];
  }
  const { map } = data;
  const url = data.urlsById.get(sourceId);

  let mappings = SOURCE_MAPPINGS.get(map);
  if (!mappings) {
    mappings = new Map();
    SOURCE_MAPPINGS.set(map, mappings);
  }

  let fileMappings = mappings.get(url);
  if (!fileMappings) {
    fileMappings = [];
    mappings.set(url, fileMappings);

    const originalMappings = fileMappings;
    map.eachMapping(
      mapping => {
        if (mapping.source !== url) {
          return;
        }

        const last = originalMappings[originalMappings.length - 1];

        if (last && last.line === mapping.originalLine) {
          if (last.columnStart < mapping.originalColumn) {
            last.columnEnd = mapping.originalColumn;
          } else {
            // Skip this duplicate original location,
            return;
          }
        }

        originalMappings.push({
          line: mapping.originalLine,
          columnStart: mapping.originalColumn,
          columnEnd: Infinity,
        });
      },
      null,
      SourceMapConsumer.ORIGINAL_ORDER
    );
  }

  return fileMappings;
}

/**
 * Given an original location, find the ranges on the generated file that
 * are mapped from the original range containing the location.
 */
async function getGeneratedRanges(location) {
  if (!isOriginalId(location.sourceId)) {
    return [];
  }

  const generatedSourceId = originalToGeneratedId(location.sourceId);
  const data = await getSourceMapWithMetadata(generatedSourceId);
  if (!data) {
    return [];
  }
  const { urlsById, map } = data;

  if (!COMPUTED_SPANS.has(map)) {
    COMPUTED_SPANS.add(map);
    map.computeColumnSpans();
  }

  // We want to use 'allGeneratedPositionsFor' to get the _first_ generated
  // location, but it hard-codes SourceMapConsumer.LEAST_UPPER_BOUND as the
  // bias, making it search in the wrong direction for this usecase.
  // To work around this, we use 'generatedPositionFor' and then look up the
  // exact original location, making any bias value unnecessary, and then
  // use that location for the call to 'allGeneratedPositionsFor'.
  const genPos = map.generatedPositionFor({
    source: urlsById.get(location.sourceId),
    line: location.line,
    column: location.column == null ? 0 : location.column,
    bias: SourceMapConsumer.GREATEST_LOWER_BOUND,
  });
  if (genPos.line === null) {
    return [];
  }

  const positions = map.allGeneratedPositionsFor(
    map.originalPositionFor({
      line: genPos.line,
      column: genPos.column,
    })
  );

  return positions
    .map(mapping => ({
      line: mapping.line,
      columnStart: mapping.column,
      columnEnd: mapping.lastColumn,
    }))
    .sort((a, b) => {
      const line = a.line - b.line;
      return line === 0 ? a.column - b.column : line;
    });
}

async function getGeneratedLocation(location) {
  if (!isOriginalId(location.sourceId)) {
    return null;
  }

  const generatedSourceId = originalToGeneratedId(location.sourceId);
  const data = await getSourceMapWithMetadata(generatedSourceId);
  if (!data) {
    return null;
  }
  const { urlsById, map } = data;

  const positions = map.allGeneratedPositionsFor({
    source: urlsById.get(location.sourceId),
    line: location.line,
    column: location.column == null ? 0 : location.column,
  });

  // Prior to source-map 0.7, the source-map module returned the earliest
  // generated location in the file when there were multiple generated
  // locations. The current comparison fn in 0.7 does not appear to take
  // generated location into account properly.
  let match;
  for (const pos of positions) {
    if (!match || pos.line < match.line || pos.column < match.column) {
      match = pos;
    }
  }

  if (!match) {
    match = map.generatedPositionFor({
      source: urlsById.get(location.sourceId),
      line: location.line,
      column: location.column == null ? 0 : location.column,
      bias: SourceMapConsumer.LEAST_UPPER_BOUND,
    });
  }

  return {
    sourceId: generatedSourceId,
    line: match.line,
    column: match.column,
  };
}

/**
 * Map the breakable positions (line and columns) from generated to original locations.
 *
 * @param {Object} breakpointPositions
 *        List of columns per line refering to the breakable columns per line
 *        for a given source:
 *        {
 *           1: [2, 6], // On line 1, column 2 and 6 are breakable.
 *           ...
 *        }
 * @param {string} sourceId
 *        The ID for the generated source.
 */
async function getOriginalLocations(breakpointPositions, sourceId) {
  const map = await getSourceMap(sourceId);
  if (!map) {
    return null;
  }
  for (const line in breakpointPositions) {
    const breakableColumnsPerLine = breakpointPositions[line];
    for (let i = 0; i < breakableColumnsPerLine.length; i++) {
      const column = breakableColumnsPerLine[i];
      const mappedLocation = getOriginalLocationSync(map, {
        sourceId,
        line: parseInt(line, 10),
        column,
      });
      if (mappedLocation) {
        // As we replace the `column` with the mappedLocation,
        // also transfer the generated column so that we can compute both original and generated locations
        // in the main thread.
        mappedLocation.generatedColumn = column;
        breakableColumnsPerLine[i] = mappedLocation;
      }
    }
  }
  return breakpointPositions;
}

function getOriginalLocationSync(map, location) {
  // First check for an exact match
  const {
    source: sourceUrl,
    line,
    column,
  } = map.originalPositionFor({
    line: location.line,
    column: location.column == null ? 0 : location.column,
  });

  if (sourceUrl == null) {
    // No url means the location didn't map.
    return null;
  }

  return {
    sourceId: generatedToOriginalId(location.sourceId, sourceUrl),
    sourceUrl,
    line,
    column,
  };
}

async function getOriginalLocation(location) {
  if (!isGeneratedId(location.sourceId)) {
    return null;
  }

  const map = await getSourceMap(location.sourceId);
  if (!map) {
    return null;
  }

  return getOriginalLocationSync(map, location);
}

async function getOriginalSourceText(originalSourceId) {
  assert(isOriginalId(originalSourceId), "Source is not an original source");

  const generatedSourceId = originalToGeneratedId(originalSourceId);
  const data = await getSourceMapWithMetadata(generatedSourceId);
  if (!data) {
    return null;
  }
  const { urlsById, map } = data;

  const url = urlsById.get(originalSourceId);
  let text = map.sourceContentFor(url, true);
  if (!text) {
    try {
      const response = await networkRequest(url, {
        sourceMapBaseURL: map.sourceMapBaseURL,
        loadFromCache: false,
        allowsRedirects: false,
      });
      text = response.content;
    } catch (err) {
      // Workers exceptions are processed by worker-utils module and
      // only metadata attribute is transferred between threads.
      // Notify the main thread about which url failed loading.
      err.metadata = {
        url,
      };
      throw err;
    }
  }

  return {
    text,
    contentType: getContentType(url || ""),
  };
}

/**
 * Find the set of ranges on the generated file that map from the original
 * file's locations.
 *
 * @param sourceId - The original ID of the file we are processing.
 * @param url - The original URL of the file we are processing.
 * @param mergeUnmappedRegions - If unmapped regions are encountered between
 *   two mappings for the given original file, allow the two mappings to be
 *   merged anyway. This is useful if you are more interested in the general
 *   contiguous ranges associated with a file, rather than the specifics of
 *   the ranges provided by the sourcemap.
 */
const GENERATED_MAPPINGS = new WeakMap();
async function getGeneratedRangesForOriginal(
  sourceId,
  mergeUnmappedRegions = false
) {
  assert(isOriginalId(sourceId), "Source is not an original source");

  const data = await getSourceMapWithMetadata(originalToGeneratedId(sourceId));
  // NOTE: this is only needed for Flow
  if (!data) {
    return [];
  }
  const { urlsById, map } = data;
  const url = urlsById.get(sourceId);

  if (!COMPUTED_SPANS.has(map)) {
    COMPUTED_SPANS.add(map);
    map.computeColumnSpans();
  }

  if (!GENERATED_MAPPINGS.has(map)) {
    GENERATED_MAPPINGS.set(map, new Map());
  }

  const generatedRangesMap = GENERATED_MAPPINGS.get(map);
  if (!generatedRangesMap) {
    return [];
  }

  if (generatedRangesMap.has(sourceId)) {
    // NOTE we need to coerce the result to an array for Flow
    return generatedRangesMap.get(sourceId) || [];
  }

  // Gather groups of mappings on the generated file, with new groups created
  // if we cross a mapping for a different file.
  let currentGroup = [];
  const originalGroups = [currentGroup];
  map.eachMapping(
    mapping => {
      if (mapping.source === url) {
        currentGroup.push({
          start: {
            line: mapping.generatedLine,
            column: mapping.generatedColumn,
          },
          end: {
            line: mapping.generatedLine,
            // The lastGeneratedColumn value is an inclusive value so we add
            // one to it to get the exclusive end position.
            column: mapping.lastGeneratedColumn + 1,
          },
        });
      } else if (typeof mapping.source === "string" && currentGroup.length) {
        // If there is a URL, but it is for a _different_ file, we create a
        // new group of mappings so that we can tell
        currentGroup = [];
        originalGroups.push(currentGroup);
      }
    },
    null,
    SourceMapConsumer.GENERATED_ORDER
  );

  const generatedMappingsForOriginal = [];
  if (mergeUnmappedRegions) {
    // If we don't care about excluding unmapped regions, then we just need to
    // create a range that is the fully encompasses each group, ignoring the
    // empty space between each individual range.
    for (const group of originalGroups) {
      if (group.length) {
        generatedMappingsForOriginal.push({
          start: group[0].start,
          end: group[group.length - 1].end,
        });
      }
    }
  } else {
    let lastEntry;
    for (const group of originalGroups) {
      lastEntry = null;
      for (const { start, end } of group) {
        const lastEnd = lastEntry
          ? wrappedMappingPosition(lastEntry.end)
          : null;

        // If this entry comes immediately after the previous one, extend the
        // range of the previous entry instead of adding a new one.
        if (
          lastEntry &&
          lastEnd &&
          lastEnd.line === start.line &&
          lastEnd.column === start.column
        ) {
          lastEntry.end = end;
        } else {
          const newEntry = { start, end };
          generatedMappingsForOriginal.push(newEntry);
          lastEntry = newEntry;
        }
      }
    }
  }

  generatedRangesMap.set(sourceId, generatedMappingsForOriginal);
  return generatedMappingsForOriginal;
}

function wrappedMappingPosition(pos) {
  if (pos.column !== Infinity) {
    return pos;
  }

  // If the end of the entry consumes the whole line, treat it as wrapping to
  // the next line.
  return {
    line: pos.line + 1,
    column: 0,
  };
}

async function getFileGeneratedRange(originalSourceId) {
  assert(isOriginalId(originalSourceId), "Source is not an original source");

  const data = await getSourceMapWithMetadata(
    originalToGeneratedId(originalSourceId)
  );
  if (!data) {
    return null;
  }
  const { urlsById, map } = data;

  const start = map.generatedPositionFor({
    source: urlsById.get(originalSourceId),
    line: 1,
    column: 0,
    bias: SourceMapConsumer.LEAST_UPPER_BOUND,
  });

  const end = map.generatedPositionFor({
    source: urlsById.get(originalSourceId),
    line: Number.MAX_SAFE_INTEGER,
    column: Number.MAX_SAFE_INTEGER,
    bias: SourceMapConsumer.GREATEST_LOWER_BOUND,
  });

  return {
    start,
    end,
  };
}

/**
 * Set the sourceMap for multiple passed source ids.
 *
 * @param {Array<string>} generatedSourceIds
 * @param {Object} map: An actual sourcemap (as generated with SourceMapGenerator#toJSON)
 */
function setSourceMapForGeneratedSources(generatedSourceIds, map) {
  const sourceMapConsumer = new SourceMapConsumer(map);
  for (const generatedId of generatedSourceIds) {
    setSourceMap(generatedId, Promise.resolve(sourceMapConsumer));
  }
}

function clearSourceMaps() {
  clearSourceMapsRequests();
  clearWasmXScopes();
  clearOriginalURLs();
}

module.exports = {
  getOriginalURLs,
  hasOriginalURL,
  getOriginalRanges,
  getGeneratedRanges,
  getGeneratedLocation,
  getOriginalLocation,
  getOriginalLocations,
  getOriginalSourceText,
  getGeneratedRangesForOriginal,
  getFileGeneratedRange,
  getSourceMapIgnoreList,
  setSourceMapForGeneratedSources,
  clearSourceMaps,
};
