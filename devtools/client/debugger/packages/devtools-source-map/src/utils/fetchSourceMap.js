/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { SourceMapInput } from "../source-map";

const { networkRequest } = require("devtools-utils");
const { getSourceMap, setSourceMap } = require("./sourceMapRequests");
const { WasmRemap } = require("./wasmRemap");
const { SourceMapConsumer } = require("source-map");
const { convertToJSON } = require("devtools-wasm-dwarf");
const { createConsumer } = require("./createConsumer");

// URLs which have been seen in a completed source map request.
const originalURLs = new Set();

function clearOriginalURLs() {
  originalURLs.clear();
}

function hasOriginalURL(url: string): boolean {
  return originalURLs.has(url);
}

function _resolveSourceMapURL(source: SourceMapInput) {
  let { url = "", sourceMapURL } = source;

  if (!url) {
    // If the source doesn't have a URL, don't resolve anything.
    return { sourceMapURL, baseURL: sourceMapURL };
  }
  sourceMapURL = sourceMapURL || "";

  let resolvedString;
  let baseURL;

  // When the sourceMap is a data: URL, fall back to using the source's URL,
  // if possible. We don't use `new URL` here because it will be _very_ slow
  // for large inlined source-maps, and we don't actually need to parse them.
  if (sourceMapURL.startsWith("data:")) {
    resolvedString = sourceMapURL;
    baseURL = url;
  } else {
    resolvedString = new URL(
      sourceMapURL,
      // If the URL is a data: URL, the sourceMapURL needs to be absolute, so
      // we might as well pass `undefined` to avoid parsing a potentially
      // very large data: URL for no reason.
      url.startsWith("data:") ? undefined : url
    ).toString();
    baseURL = resolvedString;
  }

  return { sourceMapURL: resolvedString, baseURL };
}

async function _resolveAndFetch(
  generatedSource: SourceMapInput
): SourceMapConsumer {
  // Fetch the sourcemap over the network and create it.
  const { sourceMapURL, baseURL } = _resolveSourceMapURL(generatedSource);

  let fetched = await networkRequest(sourceMapURL, { loadFromCache: false });

  if (fetched.isDwarf) {
    fetched = { content: await convertToJSON(fetched.content) };
  }

  // Create the source map and fix it up.
  let map = await createConsumer(fetched.content, baseURL);
  if (generatedSource.isWasm) {
    map = new WasmRemap(map);
    // Check if experimental scope info exists.
    if (fetched.content.includes("x-scopes")) {
      const parsedJSON = JSON.parse(fetched.content);
      map.xScopes = parsedJSON["x-scopes"];
    }
  }

  if (map && map.sources) {
    map.sources.forEach(url => originalURLs.add(url));
  }

  return map;
}

function fetchSourceMap(generatedSource: SourceMapInput): SourceMapConsumer {
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
  const req = _resolveAndFetch(generatedSource);
  // Make sure the cached promise does not reject, because we only
  // want to report the error once.
  setSourceMap(
    generatedSource.id,
    req.catch(() => null)
  );
  return req;
}

module.exports = { fetchSourceMap, hasOriginalURL, clearOriginalURLs };
