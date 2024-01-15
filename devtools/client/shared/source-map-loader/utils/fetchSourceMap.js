/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  networkRequest,
} = require("resource://devtools/client/shared/source-map-loader/utils/network-request");

const {
  SourceMapConsumer,
} = require("resource://devtools/client/shared/vendor/source-map/source-map.js");
const {
  getSourceMap,
  setSourceMap,
} = require("resource://devtools/client/shared/source-map-loader/utils/sourceMapRequests");
const {
  WasmRemap,
} = require("resource://devtools/client/shared/source-map-loader/utils/wasmRemap");
const {
  convertToJSON,
} = require("resource://devtools/client/shared/source-map-loader/wasm-dwarf/convertToJSON");

// URLs which have been seen in a completed source map request.
const originalURLs = new Set();

function clearOriginalURLs() {
  originalURLs.clear();
}

function hasOriginalURL(url) {
  return originalURLs.has(url);
}

function resolveSourceMapURL(source) {
  let { sourceMapBaseURL, sourceMapURL } = source;
  sourceMapBaseURL = sourceMapBaseURL || "";
  sourceMapURL = sourceMapURL || "";

  if (!sourceMapBaseURL) {
    // If the source doesn't have a URL, don't resolve anything.
    return { resolvedSourceMapURL: sourceMapURL, baseURL: sourceMapURL };
  }

  let resolvedString;
  let baseURL;

  // When the sourceMap is a data: URL, fall back to using the source's URL,
  // if possible. We don't use `new URL` here because it will be _very_ slow
  // for large inlined source-maps, and we don't actually need to parse them.
  if (sourceMapURL.startsWith("data:")) {
    resolvedString = sourceMapURL;
    baseURL = sourceMapBaseURL;
  } else {
    resolvedString = new URL(
      sourceMapURL,
      // If the URL is a data: URL, the sourceMapURL needs to be absolute, so
      // we might as well pass `undefined` to avoid parsing a potentially
      // very large data: URL for no reason.
      sourceMapBaseURL.startsWith("data:") ? undefined : sourceMapBaseURL
    ).toString();
    baseURL = resolvedString;
  }

  return { resolvedSourceMapURL: resolvedString, baseURL };
}

async function _fetch(generatedSource, resolvedSourceMapURL, baseURL) {
  let fetched = await networkRequest(resolvedSourceMapURL, {
    loadFromCache: false,
    // Blocking redirects on the sourceMappingUrl as its not easy to verify if the
    // redirect protocol matches the supported ones.
    allowRedirects: false,
    sourceMapBaseURL: generatedSource.sourceMapBaseURL,
  });

  if (fetched.isDwarf) {
    fetched = { content: await convertToJSON(fetched.content) };
  }

  // Create the source map and fix it up.
  let map = await new SourceMapConsumer(fetched.content, baseURL);

  if (generatedSource.isWasm) {
    map = new WasmRemap(map);
    // Check if experimental scope info exists.
    if (fetched.content.includes("x-scopes")) {
      const parsedJSON = JSON.parse(fetched.content);
      map.xScopes = parsedJSON["x-scopes"];
    }
  }

  // Extend the default map object with sourceMapBaseURL, used to check further
  // network requests made for this sourcemap.
  map.sourceMapBaseURL = generatedSource.sourceMapBaseURL;

  if (map && map.sources) {
    map.sources.forEach(url => originalURLs.add(url));
  }

  return map;
}

function fetchSourceMap(generatedSource, resolvedSourceMapURL, baseURL) {
  const existingRequest = getSourceMap(generatedSource.id);

  // If it has already been requested, return the request. Make sure
  // to do this even if sourcemapping is turned off, because
  // pretty-printing uses sourcemaps.
  //
  // An important behavior here is that if it's in the middle of
  // requesting it, all subsequent calls will block on the initial
  // request.
  if (existingRequest) {
    return existingRequest;
  }

  if (!generatedSource.sourceMapURL) {
    return null;
  }

  // Fire off the request, set it in the cache, and return it.
  const req = _fetch(generatedSource, resolvedSourceMapURL, baseURL);
  // Make sure the cached promise does not reject, because we only
  // want to report the error once.
  setSourceMap(
    generatedSource.id,
    req.catch(() => null)
  );
  return req;
}

module.exports = {
  fetchSourceMap,
  hasOriginalURL,
  clearOriginalURLs,
  resolveSourceMapURL,
};
