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
  Source,
  SourceId,
} from "../../../src/types";
import type { SourceMapConsumer } from "source-map";
import type { LocationOptions } from "./source-map";

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
  generatedSource: Source
): Promise<SourceMapConsumer> =>
  dispatcher.invoke("getOriginalURLs", generatedSource);

export const hasOriginalURL = async (url: string): Promise<boolean> =>
  dispatcher.invoke("hasOriginalURL", url);

export const getOriginalRanges = async (
  sourceId: SourceId,
  url: string
): Promise<
  Array<{
    line: number,
    columnStart: number,
    columnEnd: number,
  }>
> => dispatcher.invoke("getOriginalRanges", sourceId, url);
export const getGeneratedRanges = async (
  location: SourceLocation,
  originalSource: Source
): Promise<
  Array<{
    line: number,
    columnStart: number,
    columnEnd: number,
  }>
> => _getGeneratedRanges(location, originalSource);

export const getGeneratedLocation = async (
  location: SourceLocation,
  originalSource: Source
): Promise<SourceLocation> => _getGeneratedLocation(location, originalSource);

export const getAllGeneratedLocations = async (
  location: SourceLocation,
  originalSource: Source
): Promise<Array<SourceLocation>> =>
  _getAllGeneratedLocations(location, originalSource);

export const getOriginalLocation = async (
  location: SourceLocation,
  options: LocationOptions = {}
): Promise<SourceLocation> => _getOriginalLocation(location, options);

export const getOriginalLocations = async (
  sourceId: SourceId,
  locations: SourceLocation[],
  options: LocationOptions = {}
): Promise<SourceLocation[]> =>
  dispatcher.invoke("getOriginalLocations", sourceId, locations, options);

export const getGeneratedRangesForOriginal = async (
  sourceId: SourceId,
  url: string,
  mergeUnmappedRegions?: boolean
): Promise<Range[]> =>
  dispatcher.invoke(
    "getGeneratedRangesForOriginal",
    sourceId,
    url,
    mergeUnmappedRegions
  );

export const getFileGeneratedRange = async (
  originalSource: Source
): Promise<Range> => dispatcher.invoke("getFileGeneratedRange", originalSource);

export const getLocationScopes = dispatcher.task("getLocationScopes");

export const getOriginalSourceText = async (
  originalSource: Source
): Promise<?{
  text: string,
  contentType: string,
}> => dispatcher.invoke("getOriginalSourceText", originalSource);

export const applySourceMap = async (
  generatedId: string,
  url: string,
  code: string,
  mappings: Object
): Promise<void> =>
  dispatcher.invoke("applySourceMap", generatedId, url, code, mappings);

export const clearSourceMaps = async (): Promise<void> =>
  dispatcher.invoke("clearSourceMaps");

export const hasMappedSource = async (
  location: SourceLocation
): Promise<boolean> => dispatcher.invoke("hasMappedSource", location);

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
