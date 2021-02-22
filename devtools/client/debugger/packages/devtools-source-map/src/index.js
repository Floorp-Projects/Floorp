/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  workerUtils: { WorkerDispatcher },
} = require("devtools-utils");

export const dispatcher = new WorkerDispatcher();

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

export const setAssetRootURL = async assetRoot =>
  dispatcher.invoke("setAssetRootURL", assetRoot);

export const getOriginalURLs = async generatedSource =>
  dispatcher.invoke("getOriginalURLs", generatedSource);

export const hasOriginalURL = async url =>
  dispatcher.invoke("hasOriginalURL", url);

export const getOriginalRanges = async sourceId =>
  dispatcher.invoke("getOriginalRanges", sourceId);
export const getGeneratedRanges = async location =>
  _getGeneratedRanges(location);

export const getGeneratedLocation = async location =>
  _getGeneratedLocation(location);

export const getAllGeneratedLocations = async location =>
  _getAllGeneratedLocations(location);

export const getOriginalLocation = async (location, options = {}) =>
  _getOriginalLocation(location, options);

export const getOriginalLocations = async (locations, options = {}) =>
  dispatcher.invoke("getOriginalLocations", locations, options);

export const getGeneratedRangesForOriginal = async (
  sourceId,
  mergeUnmappedRegions
) =>
  dispatcher.invoke(
    "getGeneratedRangesForOriginal",
    sourceId,
    mergeUnmappedRegions
  );

export const getFileGeneratedRange = async originalSourceId =>
  dispatcher.invoke("getFileGeneratedRange", originalSourceId);

export const getOriginalSourceText = async originalSourceId =>
  dispatcher.invoke("getOriginalSourceText", originalSourceId);

export const applySourceMap = async (generatedId, url, code, mappings) =>
  dispatcher.invoke("applySourceMap", generatedId, url, code, mappings);

export const clearSourceMaps = async () => dispatcher.invoke("clearSourceMaps");

export const getOriginalStackFrames = async generatedLocation =>
  dispatcher.invoke("getOriginalStackFrames", generatedLocation);

export {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
} from "./utils";

export const startSourceMapWorker = (url, assetRoot) => {
  dispatcher.start(url);
  setAssetRootURL(assetRoot);
};
export const stopSourceMapWorker = dispatcher.stop.bind(dispatcher);

import * as self from "devtools-source-map";
export default self;
