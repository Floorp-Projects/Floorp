/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

const {
  workerUtils: { WorkerDispatcher },
} = require("devtools-utils");

import type {
  OriginalFrame,
  Range,
  SourceLocation,
  SourceId,
} from "../../../src/types";
import type { SourceMapInput, LocationOptions } from "./source-map";

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

export const setAssetRootURL = async (assetRoot: string): Promise<void> =>
  dispatcher.invoke("setAssetRootURL", assetRoot);

export const getOriginalURLs = async (
  generatedSource: SourceMapInput
): Promise<?Array<{| id: SourceId, url: string |}>> =>
  dispatcher.invoke("getOriginalURLs", generatedSource);

export const hasOriginalURL = async (url: string): Promise<boolean> =>
  dispatcher.invoke("hasOriginalURL", url);

export const getOriginalRanges = async (
  sourceId: SourceId
): Promise<
  Array<{
    line: number,
    columnStart: number,
    columnEnd: number,
  }>
> => dispatcher.invoke("getOriginalRanges", sourceId);
export const getGeneratedRanges = async (
  location: SourceLocation
): Promise<
  Array<{
    line: number,
    columnStart: number,
    columnEnd: number,
  }>
> => _getGeneratedRanges(location);

export const getGeneratedLocation = async (
  location: SourceLocation
): Promise<SourceLocation> => _getGeneratedLocation(location);

export const getAllGeneratedLocations = async (
  location: SourceLocation
): Promise<Array<SourceLocation>> => _getAllGeneratedLocations(location);

export const getOriginalLocation = async (
  location: SourceLocation,
  options: LocationOptions = {}
): Promise<SourceLocation> => _getOriginalLocation(location, options);

export const getOriginalLocations = async (
  locations: SourceLocation[],
  options: LocationOptions = {}
): Promise<SourceLocation[]> =>
  dispatcher.invoke("getOriginalLocations", locations, options);

export const getGeneratedRangesForOriginal = async (
  sourceId: SourceId,
  mergeUnmappedRegions?: boolean
): Promise<Range[]> =>
  dispatcher.invoke(
    "getGeneratedRangesForOriginal",
    sourceId,
    mergeUnmappedRegions
  );

export const getFileGeneratedRange = async (
  originalSourceId: SourceId
): Promise<Range> =>
  dispatcher.invoke("getFileGeneratedRange", originalSourceId);

export const getOriginalSourceText = async (
  originalSourceId: SourceId
): Promise<?{
  text: string,
  contentType: string,
}> => dispatcher.invoke("getOriginalSourceText", originalSourceId);

export const applySourceMap = async (
  generatedId: string,
  url: string,
  code: string,
  mappings: Object
): Promise<void> =>
  dispatcher.invoke("applySourceMap", generatedId, url, code, mappings);

export const clearSourceMaps = async (): Promise<void> =>
  dispatcher.invoke("clearSourceMaps");

export const getOriginalStackFrames = async (
  generatedLocation: SourceLocation
): Promise<?Array<OriginalFrame>> =>
  dispatcher.invoke("getOriginalStackFrames", generatedLocation);

export {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
} from "./utils";

export const startSourceMapWorker = (url: string, assetRoot: string) => {
  dispatcher.start(url);
  setAssetRootURL(assetRoot);
};
export const stopSourceMapWorker = dispatcher.stop.bind(dispatcher);

import * as self from "devtools-source-map";
export default self;
