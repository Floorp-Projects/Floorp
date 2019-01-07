/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId
} = require("./utils");

const {
  workerUtils: { WorkerDispatcher }
} = require("devtools-utils");

const dispatcher = new WorkerDispatcher();

const setAssetRootURL = dispatcher.task("setAssetRootURL");
const getOriginalURLs = dispatcher.task("getOriginalURLs");
const getOriginalRanges = dispatcher.task("getOriginalRanges");
const getGeneratedRanges = dispatcher.task("getGeneratedRanges", {
  queue: true
});
const getGeneratedLocation = dispatcher.task("getGeneratedLocation", {
  queue: true
});
const getAllGeneratedLocations = dispatcher.task("getAllGeneratedLocations", {
  queue: true
});
const getOriginalLocation = dispatcher.task("getOriginalLocation");
const getFileGeneratedRange = dispatcher.task("getFileGeneratedRange");
const getLocationScopes = dispatcher.task("getLocationScopes");
const getOriginalSourceText = dispatcher.task("getOriginalSourceText");
const applySourceMap = dispatcher.task("applySourceMap");
const clearSourceMaps = dispatcher.task("clearSourceMaps");
const hasMappedSource = dispatcher.task("hasMappedSource");
const getOriginalStackFrames = dispatcher.task("getOriginalStackFrames");

module.exports = {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
  hasMappedSource,
  getOriginalURLs,
  getOriginalRanges,
  getGeneratedRanges,
  getGeneratedLocation,
  getAllGeneratedLocations,
  getOriginalLocation,
  getFileGeneratedRange,
  getLocationScopes,
  getOriginalSourceText,
  applySourceMap,
  clearSourceMaps,
  getOriginalStackFrames,
  startSourceMapWorker(url: string, assetRoot: string) {
    dispatcher.start(url);
    setAssetRootURL(assetRoot);
  },
  stopSourceMapWorker: dispatcher.stop.bind(dispatcher)
};
