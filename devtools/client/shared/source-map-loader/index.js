/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  WorkerDispatcher,
} = require("resource://devtools/client/shared/worker-utils.js");

const SOURCE_MAP_WORKER_URL =
  "resource://devtools/client/shared/source-map-loader/worker.js";

const dispatcher = new WorkerDispatcher();

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

  getOriginalURLs: dispatcher.task("getOriginalURLs"),
  hasOriginalURL: dispatcher.task("hasOriginalURL"),
  getOriginalRanges: dispatcher.task("getOriginalRanges"),

  getGeneratedRanges: dispatcher.task("getGeneratedRanges", {
    queue: true,
  }),
  getGeneratedLocation: dispatcher.task("getGeneratedLocation", {
    queue: true,
  }),
  getOriginalLocation: dispatcher.task("getOriginalLocation", {
    queue: true,
  }),

  getOriginalLocations: dispatcher.task("getOriginalLocations"),
  getGeneratedRangesForOriginal: dispatcher.task(
    "getGeneratedRangesForOriginal"
  ),
  getFileGeneratedRange: dispatcher.task("getFileGeneratedRange"),
  getOriginalSourceText: dispatcher.task("getOriginalSourceText"),
  applySourceMap: dispatcher.task("applySourceMap"),
  clearSourceMaps: dispatcher.task("clearSourceMaps"),
  getOriginalStackFrames: dispatcher.task("getOriginalStackFrames"),

  startSourceMapWorker: dispatcher.start.bind(
    dispatcher,
    SOURCE_MAP_WORKER_URL
  ),
  stopSourceMapWorker: dispatcher.stop.bind(dispatcher),
};
