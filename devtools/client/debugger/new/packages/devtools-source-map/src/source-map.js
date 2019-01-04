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
const { fetchSourceMap } = require("./utils/fetchSourceMap");
const {
  getSourceMap,
  setSourceMap,
  clearSourceMaps: clearSourceMapsRequests
} = require("./utils/sourceMapRequests");
const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
  getContentType
} = require("./utils");
const { clearWasmXScopes } = require("./utils/wasmXScopes");

import type { SourceLocation, Source, SourceId } from "debugger-html";

async function getOriginalURLs(generatedSource: Source) {
  const map = await fetchSourceMap(generatedSource);
  return map && map.sources;
}

const COMPUTED_SPANS = new WeakSet();

const SOURCE_MAPPINGS = new WeakMap();
async function getOriginalRanges(sourceId: SourceId, url: string) {
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
          columnEnd: Infinity
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
    columnEnd: number
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
    bias: SourceMapConsumer.GREATEST_LOWER_BOUND
  });
  if (genPos.line === null) {
    return [];
  }

  const positions = map.allGeneratedPositionsFor(
    map.originalPositionFor({
      line: genPos.line,
      column: genPos.column
    })
  );

  return positions
    .map(mapping => ({
      line: mapping.line,
      columnStart: mapping.column,
      columnEnd: mapping.lastColumn
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
    column: location.column == null ? 0 : location.column
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
      bias: SourceMapConsumer.LEAST_UPPER_BOUND
    });
  }

  return {
    sourceId: generatedSourceId,
    line: match.line,
    column: match.column
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
    column: location.column == null ? 0 : location.column
  });

  return positions.map(({ line, column }) => ({
    sourceId: generatedSourceId,
    line,
    column
  }));
}

type locationOptions = {
  search?: "LEAST_UPPER_BOUND" | "GREATEST_LOWER_BOUND"
};
async function getOriginalLocation(
  location: SourceLocation,
  { search }: locationOptions = {}
): Promise<SourceLocation> {
  if (!isGeneratedId(location.sourceId)) {
    return location;
  }

  const map = await getSourceMap(location.sourceId);
  if (!map) {
    return location;
  }

  // First check for an exact match
  let match = map.originalPositionFor({
    line: location.line,
    column: location.column == null ? 0 : location.column
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
        bias: SourceMapConsumer[search]
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
    column
  };
}

async function getOriginalSourceText(originalSource: Source) {
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
    contentType: getContentType(originalSource.url || "")
  };
}

async function getFileGeneratedRange(originalSource: Source) {
  assert(isOriginalId(originalSource.id), "Source is not an original source");

  const map = await getSourceMap(originalToGeneratedId(originalSource.id));
  if (!map) {
    return;
  }

  const start = map.generatedPositionFor({
    source: originalSource.url,
    line: 1,
    column: 0,
    bias: SourceMapConsumer.LEAST_UPPER_BOUND
  });

  const end = map.generatedPositionFor({
    source: originalSource.url,
    line: Number.MAX_SAFE_INTEGER,
    column: Number.MAX_SAFE_INTEGER,
    bias: SourceMapConsumer.GREATEST_LOWER_BOUND
  });

  return {
    start,
    end
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
}

module.exports = {
  getOriginalURLs,
  getOriginalRanges,
  getGeneratedRanges,
  getGeneratedLocation,
  getAllGeneratedLocations,
  getOriginalLocation,
  getOriginalSourceText,
  getFileGeneratedRange,
  applySourceMap,
  clearSourceMaps,
  hasMappedSource
};
