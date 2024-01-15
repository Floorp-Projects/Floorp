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
  constructor(targetCommand) {
    super(SOURCE_MAP_WORKER_URL);
    this.#targetCommand = targetCommand;
  }

  #setSourceMapForGeneratedSources = this.task(
    "setSourceMapForGeneratedSources"
  );
  #getOriginalURLs = this.task("getOriginalURLs");
  #getOriginalSourceText = this.task("getOriginalSourceText");
  #targetCommand = null;

  async getOriginalURLs(urlInfo) {
    try {
      return await this.#getOriginalURLs(urlInfo);
    } catch (error) {
      // Note that tests may not pass the targetCommand
      if (this.#targetCommand) {
        // Catch all errors and log them to the Web Console for users to see.
        const message = L10N.getFormatStr(
          "toolbox.sourceMapFailure",
          error,
          urlInfo.url,
          urlInfo.sourceMapURL
        );
        this.#targetCommand.targetFront.logWarningInPage(message, "source map");
      }

      // And re-throw the error so that the debugger can also interpret the error
      throw error;
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
  loadSourceMap = this.task("loadSourceMap");

  async getOriginalSourceText(originalSourceId) {
    try {
      return await this.#getOriginalSourceText(originalSourceId);
    } catch (error) {
      const message = L10N.getFormatStr(
        "toolbox.sourceMapSourceFailure",
        error.message,
        error.metadata ? error.metadata.url : "<unknown>"
      );

      // Note that tests may not pass the targetCommand
      if (this.#targetCommand) {
        // Catch all errors and log them to the Web Console for users to see.
        this.#targetCommand.targetFront.logWarningInPage(message, "source map");
      }

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

  destroy() {
    // Request to stop the underlying DOM Worker
    this.stop();
    // Unregister all listeners
    this.clearEvents();
    // SourceMapLoader may be leaked and so it is important to clear most outer references
    this.#targetCommand = null;
  }
}
EventEmitter.decorate(SourceMapLoader.prototype);

module.exports = {
  SourceMapLoader,
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
};
