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

const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
} = require("resource://devtools/client/shared/source-map-loader/utils/index.js");

class SourceMapLoader extends WorkerDispatcher {
  #setSourceMapForGeneratedSources = this.task(
    "setSourceMapForGeneratedSources"
  );
  #getOriginalURLs = this.task("getOriginalURLs");
  #getOriginalSourceText = this.task("getOriginalSourceText");

  constructor() {
    super(SOURCE_MAP_WORKER_URL);
  }

  async getOriginalURLs(urlInfo) {
    try {
      return await this.#getOriginalURLs(urlInfo);
    } catch (error) {
      const message = L10N.getFormatStr(
        "toolbox.sourceMapFailure",
        error,
        urlInfo.url,
        urlInfo.sourceMapURL
      );
      this.emit("source-map-error", message);

      // It's ok to swallow errors here, because a null
      // result just means that no source map was found.
      return null;
    }
  }

  hasOriginalURL = this.task("hasOriginalURL");
  getOriginalRanges = this.task("getOriginalRanges");

  getGeneratedRanges = this.task("getGeneratedRanges", {
    queue: true,
  });
  getGeneratedLocation = this.task("getGeneratedLocation", {
    queue: true,
  });
  getOriginalLocation = this.task("getOriginalLocation", {
    queue: true,
  });

  getOriginalLocations = this.task("getOriginalLocations");
  getGeneratedRangesForOriginal = this.task("getGeneratedRangesForOriginal");
  getFileGeneratedRange = this.task("getFileGeneratedRange");
  getSourceMapIgnoreList = this.task("getSourceMapIgnoreList");

  async getOriginalSourceText(originalSourceId) {
    try {
      return await this.#getOriginalSourceText(originalSourceId);
    } catch (error) {
      const message = L10N.getFormatStr(
        "toolbox.sourceMapSourceFailure",
        error.message,
        error.metadata ? error.metadata.url : "<unknown>"
      );
      this.emit("source-map-error", message);

      // Also replace the result with the error text.
      // Note that this result has to have the same form
      // as whatever the upstream getOriginalSourceText
      // returns.
      return {
        text: message,
        contentType: "text/plain",
      };
    }
  }

  clearSourceMaps = this.task("clearSourceMaps");
  getOriginalStackFrames = this.task("getOriginalStackFrames");

  async setSourceMapForGeneratedSources(generatedIds, sourceMap) {
    const rv = await this.#setSourceMapForGeneratedSources(
      generatedIds,
      sourceMap
    );

    // Notify and ensure waiting for the SourceMapURLService to process the source map before resolving.
    // Otherwise tests start failing because of pending request made by this component.
    await this.emitAsync("source-map-created", generatedIds);

    return rv;
  }

  stopSourceMapWorker = this.stop.bind(this);
}
EventEmitter.decorate(SourceMapLoader.prototype);

module.exports = {
  SourceMapLoader,
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
};
