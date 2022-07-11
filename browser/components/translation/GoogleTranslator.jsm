/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GoogleTranslator"];

const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { httpRequest } = ChromeUtils.import("resource://gre/modules/Http.jsm");

// The maximum amount of net data allowed per request on Google's API.
const MAX_REQUEST_DATA = 5000; // XXX This is the Bing value

// The maximum number of chunks allowed to be translated in a single
// request.
const MAX_REQUEST_CHUNKS = 128; // Undocumented, but the de facto upper limit.

// Self-imposed limit of 15 requests. This means that a page that would need
// to be broken in more than 15 requests won't be fully translated.
// The maximum amount of data that we will translate for a single page
// is MAX_REQUESTS * MAX_REQUEST_DATA.
const MAX_REQUESTS = 15;

const URL = "https://translation.googleapis.com/language/translate/v2";

/**
 * Translates a webpage using Google's Translation API.
 *
 * @param translationDocument  The TranslationDocument object that represents
 *                             the webpage to be translated
 * @param sourceLanguage       The source language of the document
 * @param targetLanguage       The target language for the translation
 *
 * @returns {Promise}          A promise that will resolve when the translation
 *                             task is finished.
 */
var GoogleTranslator = function(
  translationDocument,
  sourceLanguage,
  targetLanguage
) {
  this.translationDocument = translationDocument;
  this.sourceLanguage = sourceLanguage;
  this.targetLanguage = targetLanguage;
  this._pendingRequests = 0;
  this._partialSuccess = false;
  this._translatedCharacterCount = 0;
};

GoogleTranslator.prototype = {
  /**
   * Performs the translation, splitting the document into several chunks
   * respecting the data limits of the API.
   *
   * @returns {Promise}          A promise that will resolve when the translation
   *                             task is finished.
   */
  async translate() {
    let currentIndex = 0;
    this._onFinishedDeferred = PromiseUtils.defer();

    // Let's split the document into various requests to be sent to
    // Google's Translation API.
    for (let requestCount = 0; requestCount < MAX_REQUESTS; requestCount++) {
      // Generating the text for each request can be expensive, so
      // let's take the opportunity of the chunkification process to
      // allow for the event loop to attend other pending events
      // before we continue.
      await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

      // Determine the data for the next request.
      let request = this._generateNextTranslationRequest(currentIndex);

      // Create a real request to the server, and put it on the
      // pending requests list.
      let googleRequest = new GoogleRequest(
        request.data,
        this.sourceLanguage,
        this.targetLanguage
      );
      this._pendingRequests++;
      googleRequest
        .fireRequest()
        .then(this._chunkCompleted.bind(this), this._chunkFailed.bind(this));

      currentIndex = request.lastIndex;
      if (request.finished) {
        break;
      }
    }

    return this._onFinishedDeferred.promise;
  },

  /**
   * Function called when a request sent to the server completed successfully.
   * This function handles calling the function to parse the result and the
   * function to resolve the promise returned by the public `translate()`
   * method when there's no pending request left.
   *
   * @param   request   The GoogleRequest sent to the server.
   */
  _chunkCompleted(googleRequest) {
    if (this._parseChunkResult(googleRequest)) {
      this._partialSuccess = true;
      // Count the number of characters successfully translated.
      this._translatedCharacterCount += googleRequest.characterCount;
    }

    this._checkIfFinished();
  },

  /**
   * Function called when a request sent to the server has failed.
   * This function handles deciding if the error is transient or means the
   * service is unavailable (zero balance on the key or request credentials are
   * not in an active state) and calling the function to resolve the promise
   * returned by the public `translate()` method when there's no pending.
   * request left.
   *
   * @param   aError   [optional] The XHR object of the request that failed.
   */
  _chunkFailed(aError) {
    this._checkIfFinished();
  },

  /**
   * Function called when a request sent to the server has completed.
   * This function handles resolving the promise
   * returned by the public `translate()` method when all chunks are completed.
   */
  _checkIfFinished() {
    // Check if all pending requests have been
    // completed and then resolves the promise.
    // If at least one chunk was successful, the
    // promise will be resolved positively which will
    // display the "Success" state for the infobar. Otherwise,
    // the "Error" state will appear.
    if (--this._pendingRequests == 0) {
      if (this._partialSuccess) {
        this._onFinishedDeferred.resolve({
          characterCount: this._translatedCharacterCount,
        });
      } else {
        this._onFinishedDeferred.reject("failure");
      }
    }
  },

  /**
   * This function parses the result returned by Bing's Http.svc API,
   * which is a XML file that contains a number of elements. To our
   * particular interest, the only part of the response that matters
   * are the <TranslatedText> nodes, which contains the resulting
   * items that were sent to be translated.
   *
   * @param   request      The request sent to the server.
   * @returns boolean      True if parsing of this chunk was successful.
   */
  _parseChunkResult(googleRequest) {
    let results;
    try {
      let response = googleRequest.networkRequest.response;
      results = JSON.parse(response).data.translations;
    } catch (e) {
      return false;
    }
    let len = results.length;
    if (len != googleRequest.translationData.length) {
      // This should never happen, but if the service returns a different number
      // of items (from the number of items submitted), we can't use this chunk
      // because all items would be paired incorrectly.
      return false;
    }

    let error = false;
    for (let i = 0; i < len; i++) {
      try {
        let result = results[i].translatedText;
        let root = googleRequest.translationData[i][0];
        if (root.isSimpleRoot && result.includes("&")) {
          // If the result contains HTML entities, we need to convert them as
          // simple roots expect a plain text result.
          let doc = new DOMParser().parseFromString(result, "text/html");
          result = doc.body.firstChild.nodeValue;
        }
        root.parseResult(result);
      } catch (e) {
        error = true;
      }
    }

    return !error;
  },

  /**
   * This function will determine what is the data to be used for
   * the Nth request we are generating, based on the input params.
   *
   * @param startIndex What is the index, in the roots list, that the
   *                   chunk should start.
   */
  _generateNextTranslationRequest(startIndex) {
    let currentDataSize = 0;
    let currentChunks = 0;
    let output = [];
    let rootsList = this.translationDocument.roots;

    for (let i = startIndex; i < rootsList.length; i++) {
      let root = rootsList[i];
      let text = this.translationDocument.generateTextForItem(root);
      if (!text) {
        continue;
      }

      let newCurSize = currentDataSize + text.length;
      let newChunks = currentChunks + 1;

      if (newCurSize > MAX_REQUEST_DATA || newChunks > MAX_REQUEST_CHUNKS) {
        // If we've reached the API limits, let's stop accumulating data
        // for this request and return. We return information useful for
        // the caller to pass back on the next call, so that the function
        // can keep working from where it stopped.
        return {
          data: output,
          finished: false,
          lastIndex: i,
        };
      }

      currentDataSize = newCurSize;
      currentChunks = newChunks;
      output.push([root, text]);
    }

    return {
      data: output,
      finished: true,
      lastIndex: 0,
    };
  },
};

/**
 * Represents a request (for 1 chunk) sent off to Google's service.
 *
 * @params translationData  The data to be used for this translation,
 *                          generated by the generateNextTranslationRequest...
 *                          function.
 * @param sourceLanguage    The source language of the document.
 * @param targetLanguage    The target language for the translation.
 *
 */
function GoogleRequest(translationData, sourceLanguage, targetLanguage) {
  this.translationData = translationData;
  this.sourceLanguage = sourceLanguage;
  this.targetLanguage = targetLanguage;
  this.characterCount = 0;
}

GoogleRequest.prototype = {
  /**
   * Initiates the request
   */
  fireRequest() {
    let key =
      Services.cpmm.sharedData.get("translationKey") ||
      Services.prefs.getStringPref("browser.translation.google.apiKey", "");
    if (!key) {
      return Promise.reject("no API key");
    }

    // Prepare the request body.
    let postData = [
      ["key", key],
      ["source", this.sourceLanguage],
      ["target", this.targetLanguage],
    ];

    for (let [, text] of this.translationData) {
      postData.push(["q", text]);
      this.characterCount += text.length;
    }

    // Set up request options.
    return new Promise((resolve, reject) => {
      let options = {
        onLoad: (responseText, xhr) => {
          resolve(this);
        },
        onError(e, responseText, xhr) {
          reject(xhr);
        },
        postData,
      };

      // Fire the request.
      this.networkRequest = httpRequest(URL, options);
    });
  },
};
