/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  WorkerDispatcher,
} = require("resource://devtools/client/shared/worker-utils.js");
const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");

const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

const SOURCE_MAP_WORKER_URL =
  "resource://devtools/client/shared/source-map-loader/worker.js";

const dispatcher = new WorkerDispatcher();

const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
} = require("resource://devtools/client/shared/source-map-loader/utils/index.js");

const _applySourceMap = dispatcher.task("applySourceMap");
const _getOriginalURLs = dispatcher.task("getOriginalURLs");
const _getOriginalSourceText = dispatcher.task("getOriginalSourceText");

const SourceMapLoader = {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,

  async getOriginalURLs(urlInfo) {
    try {
      return await _getOriginalURLs(urlInfo);
    } catch (error) {
      const message = L10N.getFormatStr(
        "toolbox.sourceMapFailure",
        error,
        urlInfo.url,
        urlInfo.sourceMapURL
      );
      SourceMapLoader.emit("source-map-error", message);

      // It's ok to swallow errors here, because a null
      // result just means that no source map was found.
      return null;
    }
  },

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

  async getOriginalSourceText(originalSourceId) {
    try {
      return await _getOriginalSourceText(originalSourceId);
    } catch (error) {
      const message = L10N.getFormatStr(
        "toolbox.sourceMapSourceFailure",
        error.message,
        error.metadata ? error.metadata.url : "<unknown>"
      );
      SourceMapLoader.emit("source-map-error", message);

      // Also replace the result with the error text.
      // Note that this result has to have the same form
      // as whatever the upstream getOriginalSourceText
      // returns.
      return {
        text: message,
        contentType: "text/plain",
      };
    }
  },

  clearSourceMaps: dispatcher.task("clearSourceMaps"),
  getOriginalStackFrames: dispatcher.task("getOriginalStackFrames"),

  async applySourceMap(generatedId, ...rest) {
    const rv = await _applySourceMap(generatedId, ...rest);

    // Notify and ensure waiting for the SourceMapURLService to process the source map before resolving.
    // Otherwise tests start failing because of pending request made by this component.
    await this.emitAsync("source-map-applied", generatedId);

    return rv;
  },

  startSourceMapWorker: dispatcher.start.bind(
    dispatcher,
    SOURCE_MAP_WORKER_URL
  ),
  stopSourceMapWorker: dispatcher.stop.bind(dispatcher),
};
EventEmitter.decorate(SourceMapLoader);
module.exports = SourceMapLoader;
