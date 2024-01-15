/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  generatedToOriginalId,
} = require("resource://devtools/client/shared/source-map-loader/utils/index.js");

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

/**
 * For a given generated source, retrieve an object with many attributes:
 * @param {String} generatedSourceId
 *        The id of the generated source
 *
 * @return {Object} Meta data object with many attributes
 *     - map: The SourceMapConsumer or WasmRemap instance
 *     - urlsById Map of Original Source ID (string) to Source URL (string)
 *     - sources: Array of object with the two following attributes:
 *       - id: Original Source ID (string)
 *       - url: Original Source URL (string)
 */
function getSourceMapWithMetadata(generatedSourceId) {
  return sourceMapRequests.get(generatedSourceId);
}

/**
 * Retrieve the SourceMapConsumer or WasmRemap instance for a given generated source.
 *
 * @param {String} generatedSourceId
 *        The id of the generated source
 *
 * @return null | Promise<SourceMapConsumer | WasmRemap>
 */
function getSourceMap(generatedSourceId) {
  const request = getSourceMapWithMetadata(generatedSourceId);
  if (!request) {
    return null;
  }

  return request.then(result => result?.map);
}

/**
 * Record the SourceMapConsumer or WasmRemap instance for a given generated source.
 *
 * @param {String} generatedId
 *        The generated source ID.
 * @param {Promise<SourceMapConsumer or WasmRemap>} request
 *        A promise which should resolve to either a SourceMapConsume or WasmRemap instance.
 */
function setSourceMap(generatedId, request) {
  sourceMapRequests.set(
    generatedId,
    request.then(map => {
      if (!map || !map.sources) {
        return null;
      }

      const urlsById = new Map();

      for (const url of map.sources) {
        const id = generatedToOriginalId(generatedId, url);
        urlsById.set(id, url);
      }
      return { map, urlsById };
    })
  );
}

module.exports = {
  clearSourceMaps,
  getSourceMapWithMetadata,
  getSourceMap,
  setSourceMap,
};
