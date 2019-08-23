/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { generatedToOriginalId } = require(".");

const sourceMapRequests = new Map();

function clearSourceMaps() {
  sourceMapRequests.clear();
}

function getSourceMapWithMetadata(
  generatedSourceId: string
): ?Promise<?{
  map: SourceMapConsumer,
  urlsById: Map<SourceId, url>,
  sources: Array<{ id: SourceId, url: string }>,
}> {
  return sourceMapRequests.get(generatedSourceId);
}

function getSourceMap(generatedSourceId: string): ?Promise<?SourceMapConsumer> {
  const request = getSourceMapWithMetadata(generatedSourceId);
  if (!request) {
    return null;
  }

  return request.then(result => (result ? result.map : null));
}

function setSourceMap(generatedId: string, request) {
  sourceMapRequests.set(
    generatedId,
    request.then(map => {
      if (!map || !map.sources) {
        return null;
      }

      const urlsById = new Map();
      const sources = [];

      for (const url of map.sources) {
        const id = generatedToOriginalId(generatedId, url);

        urlsById.set(id, url);
        sources.push({ id, url });
      }
      return { map, urlsById, sources };
    })
  );
}

module.exports = {
  clearSourceMaps,
  getSourceMapWithMetadata,
  getSourceMap,
  setSourceMap,
};
