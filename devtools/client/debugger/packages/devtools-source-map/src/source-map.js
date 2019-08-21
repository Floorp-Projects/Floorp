/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Source Map Worker
 * @module utils/source-map-worker
 */

const { networkRequest } = require("devtools-utils");
const { SourceMapConsumer, SourceMapGenerator } = require("source-map");

const { createConsumer } = require("./utils/createConsumer");
const assert = require("./utils/assert");
const {
  fetchSourceMap,
  hasOriginalURL,
  clearOriginalURLs,
} = require("./utils/fetchSourceMap");
const {
  getSourceMap,
  setSourceMap,
  clearSourceMaps: clearSourceMapsRequests,
} = require("./utils/sourceMapRequests");
const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
  getContentType,
} = require("./utils");
const { clearWasmXScopes } = require("devtools-wasm-dwarf");

import type { SourceLocation, Source, SourceId } from "debugger-html";

type Range = {
  start: {
    line: number,
    column: number,
  },
  end: {
    line: number,
    column: number,
  },
};

export type LocationOptions = {
  search?: "LEAST_UPPER_BOUND" | "GREATEST_LOWER_BOUND",
};

async function getOriginalURLs(
  generatedSource: Source
): Promise<?Array<{| id: SourceId, url: string |}>> {
  const map = await fetchSourceMap(generatedSource);
  if (!map || !map.sources) {
    return null;
  }

  return map.sources.map(url => ({
    id: generatedToOriginalId(generatedSource.id, url),
    url,
  }));
}

const COMPUTED_SPANS = new WeakSet();

const SOURCE_MAPPINGS = new WeakMap();
async function getOriginalRanges(
  sourceId: SourceId,
  url: string
): Promise<
  Array<{
    line: number,
    columnStart: number,
    columnEnd: number,
  }>
> {
  if (!isOriginalId(sourceId)) {
    return [];
  }

  const generatedSourceId = originalToGeneratedId(sourceId);
  const map = await getSourceMap(generatedSourceId);
  if (!map) {
    return [];
  }

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
async function getGeneratedRanges(
  location: SourceLocation,
  originalSource: Source
): Promise<
  Array<{
    line: number,
    columnStart: number,
    columnEnd: number,
  }>
> {
  if (!isOriginalId(location.sourceId)) {
    return [];
  }

  const generatedSourceId = originalToGeneratedId(location.sourceId);
  const map = await getSourceMap(generatedSourceId);
  if (!map) {
    return [];
  }

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
    source: originalSource.url,
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

async function getGeneratedLocation(
  location: SourceLocation,
  originalSource: Source
): Promise<SourceLocation> {
  if (!isOriginalId(location.sourceId)) {
    return location;
  }

  const generatedSourceId = originalToGeneratedId(location.sourceId);
  const map = await getSourceMap(generatedSourceId);
  if (!map) {
    return location;
  }

  const positions = map.allGeneratedPositionsFor({
    source: originalSource.url,
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
      source: originalSource.url,
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

async function getAllGeneratedLocations(
  location: SourceLocation,
  originalSource: Source
): Promise<Array<SourceLocation>> {
  if (!isOriginalId(location.sourceId)) {
    return [];
  }

  const generatedSourceId = originalToGeneratedId(location.sourceId);
  const map = await getSourceMap(generatedSourceId);
  if (!map) {
    return [];
  }

  const positions = map.allGeneratedPositionsFor({
    source: originalSource.url,
    line: location.line,
    column: location.column == null ? 0 : location.column,
  });

  return positions.map(({ line, column }) => ({
    sourceId: generatedSourceId,
    line,
    column,
  }));
}

async function getOriginalLocations(
  sourceId: string,
  locations: SourceLocation[],
  options: LocationOptions = {}
): Promise<SourceLocation[]> {
  if (locations.some(location => location.sourceId != sourceId)) {
    throw new Error("Generated locations must belong to the same source");
  }

  const map = await getSourceMap(sourceId);
  if (!map) {
    return locations;
  }

  return locations.map(location =>
    getOriginalLocationSync(map, location, options)
  );
}

function getOriginalLocationSync(
  map,
  location: SourceLocation,
  { search }: LocationOptions = {}
): SourceLocation {
  // First check for an exact match
  let match = map.originalPositionFor({
    line: location.line,
    column: location.column == null ? 0 : location.column,
  });

  // If there is not an exact match, look for a match with a bias at the
  // current location and then on subsequent lines
  if (search) {
    let line = location.line;
    let column = location.column == null ? 0 : location.column;

    while (match.source === null) {
      match = map.originalPositionFor({
        line,
        column,
        bias: SourceMapConsumer[search],
      });

      line += search == "LEAST_UPPER_BOUND" ? 1 : -1;
      column = search == "LEAST_UPPER_BOUND" ? 0 : Infinity;
    }
  }

  const { source: sourceUrl, line, column } = match;
  if (sourceUrl == null) {
    // No url means the location didn't map.
    return location;
  }

  return {
    sourceId: generatedToOriginalId(location.sourceId, sourceUrl),
    sourceUrl,
    line,
    column,
  };
}

async function getOriginalLocation(
  location: SourceLocation,
  options: LocationOptions = {}
): Promise<SourceLocation> {
  if (!isGeneratedId(location.sourceId)) {
    return location;
  }

  const map = await getSourceMap(location.sourceId);
  if (!map) {
    return location;
  }

  return getOriginalLocationSync(map, location, options);
}

async function getOriginalSourceText(
  originalSource: Source
): Promise<?{
  text: string,
  contentType: string,
}> {
  assert(isOriginalId(originalSource.id), "Source is not an original source");

  const generatedSourceId = originalToGeneratedId(originalSource.id);
  const map = await getSourceMap(generatedSourceId);
  if (!map) {
    return null;
  }

  let text = map.sourceContentFor(originalSource.url);
  if (!text) {
    text = (await networkRequest(originalSource.url, { loadFromCache: false }))
      .content;
  }

  return {
    text,
    contentType: getContentType(originalSource.url || ""),
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
  sourceId: SourceId,
  url: string,
  mergeUnmappedRegions: boolean = false
): Promise<Range[]> {
  assert(isOriginalId(sourceId), "Source is not an original source");

  const map = await getSourceMap(originalToGeneratedId(sourceId));

  // NOTE: this is only needed for Flow
  if (!map) {
    return [];
  }

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
      } else if (
        typeof mapping.source === "string" &&
        currentGroup.length > 0
      ) {
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
      if (group.length > 0) {
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

function wrappedMappingPosition(pos: {
  line: number,
  column: number,
}): {
  line: number,
  column: number,
} {
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

async function getFileGeneratedRange(
  originalSource: Source
): Promise<?{ start: any, end: any }> {
  assert(isOriginalId(originalSource.id), "Source is not an original source");

  const map = await getSourceMap(originalToGeneratedId(originalSource.id));
  if (!map) {
    return;
  }

  const start = map.generatedPositionFor({
    source: originalSource.url,
    line: 1,
    column: 0,
    bias: SourceMapConsumer.LEAST_UPPER_BOUND,
  });

  const end = map.generatedPositionFor({
    source: originalSource.url,
    line: Number.MAX_SAFE_INTEGER,
    column: Number.MAX_SAFE_INTEGER,
    bias: SourceMapConsumer.GREATEST_LOWER_BOUND,
  });

  return {
    start,
    end,
  };
}

async function hasMappedSource(location: SourceLocation): Promise<boolean> {
  if (isOriginalId(location.sourceId)) {
    return true;
  }

  const loc = await getOriginalLocation(location);
  return loc.sourceId !== location.sourceId;
}

function applySourceMap(
  generatedId: string,
  url: string,
  code: string,
  mappings: Object
) {
  const generator = new SourceMapGenerator({ file: url });
  mappings.forEach(mapping => generator.addMapping(mapping));
  generator.setSourceContent(url, code);

  const map = createConsumer(generator.toJSON());
  setSourceMap(generatedId, Promise.resolve(map));
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
  getAllGeneratedLocations,
  getOriginalLocation,
  getOriginalLocations,
  getOriginalSourceText,
  getGeneratedRangesForOriginal,
  getFileGeneratedRange,
  applySourceMap,
  clearSourceMaps,
  hasMappedSource,
};
