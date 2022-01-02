/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { generatedToOriginalId } = require(".");

const sourceMapRequests = new Map();

function clearSourceMaps() {
  for (const [, metadataPromise] of sourceMapRequests) {
    // The source-map module leaks memory unless `.destroy` is called on
    // the consumer instances when they are no longer being used.
    metadataPromise.then(
      metadata => {
        if (metadata) {
          metadata.map.destroy();
        }
      },
      // We don't want this to cause any unhandled rejection errors.
      () => {}
    );
  }

  sourceMapRequests.clear();
}

function getSourceMapWithMetadata(generatedSourceId) {
  return sourceMapRequests.get(generatedSourceId);
}

function getSourceMap(generatedSourceId) {
  const request = getSourceMapWithMetadata(generatedSourceId);
  if (!request) {
    return null;
  }

  return request.then(result => (result ? result.map : null));
}

function setSourceMap(generatedId, request) {
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
