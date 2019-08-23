/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// @flow

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
} = require("./source-map");

const { getOriginalStackFrames } = require("./utils/getOriginalStackFrames");
const { setAssetRootURL } = require("./utils/assetRoot");

const {
  workerUtils: { workerHandler },
} = require("devtools-utils");

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
