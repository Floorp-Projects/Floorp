/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["LanguageDetector"];

Components.utils.import("resource://gre/modules/Timer.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// Since Emscripten can handle heap growth, but not heap shrinkage, we
// need to refresh the worker after we've processed a particularly large
// string in order to prevent unnecessary resident memory growth.
//
// These values define the cut-off string length and the idle timeout
// (in milliseconds) before destroying a worker. Once a string of the
// maximum size has been processed, the worker is marked for
// destruction, and is terminated as soon as it has been idle for the
// given timeout.
//
// 1.5MB. This is the approximate string length that forces heap growth
// for a 2MB heap.
var LARGE_STRING = 1.5 * 1024 * 1024;
var IDLE_TIMEOUT = 10 * 1000;

const WORKER_URL = "resource:///modules/translation/cld-worker.js";

var workerManager = {
  detectionQueue: [],

  detectLanguage(aParams) {
    return this.workerReady.then(worker => {
      return new Promise(resolve => {
        this.detectionQueue.push({resolve});
        worker.postMessage(aParams);
      });
    }).then(result => {
      // We have our asynchronous result from the worker.
      //
      // Determine if our input was large enough to trigger heap growth,
      // or if we're already waiting to destroy the worker when it's
      // idle. If so, schedule termination after the idle timeout.
      if (aParams.text.length >= LARGE_STRING || this._idleTimeout != null)
        this.flushWorker();

      return result;
    });
  },

  _worker: null,
  _workerReadyPromise: null,

  get workerReady() {
    if (!this._workerReadyPromise)
      this._workerReadyPromise = new Promise(resolve => {
        let worker = new Worker(WORKER_URL);
        worker.onmessage = (aMsg) => {
          if (aMsg.data == "ready")
            resolve(worker);
          else
            this.detectionQueue.shift().resolve(aMsg.data);
        };
        this._worker = worker;
      });

    return this._workerReadyPromise;
  },

  // Holds the ID of the current pending idle cleanup setTimeout.
  _idleTimeout: null,

  // Schedule the current worker to be terminated after the idle timeout.
  flushWorker() {
    if (this._idleTimeout != null)
      clearTimeout(this._idleTimeout);

    this._idleTimeout = setTimeout(this._flushWorker.bind(this), IDLE_TIMEOUT);
  },

  // Immediately terminate the worker, as long as there no pending
  // results. Otherwise, reschedule termination until after the next
  // idle timeout.
  _flushWorker() {
    if (this.detectionQueue.length)
      this.flushWorker();
    else {
      if (this._worker)
        this._worker.terminate();

      this._worker = null;
      this._workerReadyPromise = null;
      this._idleTimeout = null;
    }
  },
};

this.LanguageDetector = {
  /**
   * Detect the language of a given string.
   *
   * The argument may be either a string containing the text to analyze,
   * or an object with the following properties:
   *
   *  - 'text' The text to analyze.
   *
   *  - 'isHTML' (optional) A boolean, indicating whether the text
   *      should be analyzed as HTML rather than plain text.
   *
   *  - 'language' (optional) A string indicating the expected language.
   *      For text extracted from HTTP documents, this is expected to
   *      come from the Content-Language header.
   *
   *  - 'tld' (optional) A string indicating the top-level domain of the
   *      document the text was extracted from.
   *
   *  - 'encoding' (optional) A string describing the encoding of the
   *      document the string was extracted from. Note that, regardless
   *      of the value of this property, the 'text' property must be a
   *      UTF-16 JavaScript string.
   *
   * @returns {Promise<Object>}
   * @resolves When detection is finished, with a object containing
   * these fields:
   *  - 'language' (string with a language code)
   *  - 'confident' (boolean) Whether the detector is confident of the
   *      result.
   *  - 'languages' (array) An array of up to three elements, containing
   *      the most prevalent languages detected. It contains a
   *      'languageCode' property, containing the ISO language code of
   *      the language, and a 'percent' property, describing the
   *      approximate percentage of the input which is in that language.
   *      For text of an unknown language, the result may contain an
   *      entry with the languge code 'un', indicating the percent of
   *      the text which is unknown.
   */
  detectLanguage(aParams) {
    if (typeof aParams == "string")
      aParams = { text: aParams };

    return workerManager.detectLanguage(aParams);
  },
};
