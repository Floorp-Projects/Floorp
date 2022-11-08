/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint-env worker */

"use strict";

importScripts("resource://gre/modules/workers/require.js");

const {
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
  clearSourceMaps,
  applySourceMap,
} = require("resource://devtools/client/shared/source-map-loader/source-map.js");

const {
  getOriginalStackFrames,
} = require("resource://devtools/client/shared/source-map-loader/utils/getOriginalStackFrames.js");
const {
  setAssetRootURL,
} = require("resource://devtools/client/shared/source-map-loader/utils/assetRoot.js");

const {
  workerHandler,
} = require("resource://devtools/client/shared/worker-utils.js");

// The interface is implemented in source-map to be
// easier to unit test.
self.onmessage = workerHandler({
  setAssetRootURL,
  getOriginalURLs,
  hasOriginalURL,
  getOriginalRanges,
  getGeneratedRanges,
  getGeneratedLocation,
  getAllGeneratedLocations,
  getOriginalLocation,
  getOriginalLocations,
  getOriginalSourceText,
  getOriginalStackFrames,
  getGeneratedRangesForOriginal,
  getFileGeneratedRange,
  applySourceMap,
  clearSourceMaps,
});
