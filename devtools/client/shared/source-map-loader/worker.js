/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

importScripts("resource://gre/modules/workers/require.js");

const {
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
  loadSourceMap,
  clearSourceMaps,
  setSourceMapForGeneratedSources,
} = require("resource://devtools/client/shared/source-map-loader/source-map.js");

const {
  getOriginalStackFrames,
} = require("resource://devtools/client/shared/source-map-loader/utils/getOriginalStackFrames.js");

const {
  workerHandler,
} = require("resource://devtools/client/shared/worker-utils.js");

// The interface is implemented in source-map to be
// easier to unit test.
self.onmessage = workerHandler({
  getOriginalURLs,
  hasOriginalURL,
  getOriginalRanges,
  getGeneratedRanges,
  getGeneratedLocation,
  getOriginalLocation,
  getOriginalLocations,
  getOriginalSourceText,
  getOriginalStackFrames,
  getGeneratedRangesForOriginal,
  getFileGeneratedRange,
  loadSourceMap,
  setSourceMapForGeneratedSources,
  clearSourceMaps,
});
