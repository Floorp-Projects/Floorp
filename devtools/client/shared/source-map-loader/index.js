/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  WorkerDispatcher,
} = require("resource://devtools/client/shared/worker-utils.js");

const dispatcher = new WorkerDispatcher();

const _getGeneratedRanges = dispatcher.task("getGeneratedRanges", {
  queue: true,
});

const _getGeneratedLocation = dispatcher.task("getGeneratedLocation", {
  queue: true,
});
const _getAllGeneratedLocations = dispatcher.task("getAllGeneratedLocations", {
  queue: true,
});

const _getOriginalLocation = dispatcher.task("getOriginalLocation", {
  queue: true,
});

const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
} = require("resource://devtools/client/shared/source-map-loader/utils/index.js");

module.exports = {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,

  setAssetRootURL() {
    return dispatcher.invoke("setAssetRootURL");
  },

  getOriginalURLs(generatedSource) {
    return dispatcher.invoke("getOriginalURLs", generatedSource);
  },

  hasOriginalURL(url) {
    return dispatcher.invoke("hasOriginalURL", url);
  },

  getOriginalRanges(sourceId) {
    return dispatcher.invoke("getOriginalRanges", sourceId);
  },

  getGeneratedRanges(location) {
    return _getGeneratedRanges(location);
  },

  getGeneratedLocation(location) {
    return _getGeneratedLocation(location);
  },

  getAllGeneratedLocations(location) {
    return _getAllGeneratedLocations(location);
  },

  getOriginalLocation(location, options = {}) {
    return _getOriginalLocation(location, options);
  },

  getOriginalLocations(locations, options = {}) {
    return dispatcher.invoke("getOriginalLocations", locations, options);
  },

  getGeneratedRangesForOriginal(sourceId, mergeUnmappedRegions) {
    return dispatcher.invoke(
      "getGeneratedRangesForOriginal",
      sourceId,
      mergeUnmappedRegions
    );
  },

  getFileGeneratedRange(originalSourceId) {
    return dispatcher.invoke("getFileGeneratedRange", originalSourceId);
  },

  getOriginalSourceText(originalSourceId) {
    return dispatcher.invoke("getOriginalSourceText", originalSourceId);
  },

  applySourceMap(generatedId, url, code, mappings) {
    return dispatcher.invoke(
      "applySourceMap",
      generatedId,
      url,
      code,
      mappings
    );
  },

  clearSourceMaps() {
    return dispatcher.invoke("clearSourceMaps");
  },

  getOriginalStackFrames(generatedLocation) {
    return dispatcher.invoke("getOriginalStackFrames", generatedLocation);
  },

  startSourceMapWorker(url) {
    dispatcher.start(url);
    module.exports.setAssetRootURL();
  },

  stopSourceMapWorker: dispatcher.stop.bind(dispatcher),
};
