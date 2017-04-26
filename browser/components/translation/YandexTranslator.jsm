/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = [ "YandexTranslator" ];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Http.jsm");

// The maximum amount of net data allowed per request on Bing's API.
const MAX_REQUEST_DATA = 5000; // Documentation says 10000 but anywhere
                               // close to that is refused by the service.

// The maximum number of chunks allowed to be translated in a single
// request.
const MAX_REQUEST_CHUNKS = 1000; // Documentation says 2000.

// Self-imposed limit of 15 requests. This means that a page that would need
// to be broken in more than 15 requests won't be fully translated.
// The maximum amount of data that we will translate for a single page
// is MAX_REQUESTS * MAX_REQUEST_DATA.
const MAX_REQUESTS = 15;

const YANDEX_RETURN_CODE_OK = 200;

const YANDEX_ERR_KEY_INVALID               = 401; // Invalid API key
const YANDEX_ERR_KEY_BLOCKED               = 402; // This API key has been blocked
const YANDEX_ERR_DAILY_REQ_LIMIT_EXCEEDED  = 403; // Daily limit for requests reached
const YANDEX_ERR_DAILY_CHAR_LIMIT_EXCEEDED = 404; // Daily limit of chars reached
const YANDEX_ERR_TEXT_TOO_LONG             = 413; // The text size exceeds the maximum
const YANDEX_ERR_UNPROCESSABLE_TEXT        = 422; // The text could not be translated
const YANDEX_ERR_LANG_NOT_SUPPORTED        = 501; // The specified translation direction is not supported

// Errors that should activate the service unavailable handling
const YANDEX_PERMANENT_ERRORS = [
  YANDEX_ERR_KEY_INVALID,
  YANDEX_ERR_KEY_BLOCKED,
  YANDEX_ERR_DAILY_REQ_LIMIT_EXCEEDED,
  YANDEX_ERR_DAILY_CHAR_LIMIT_EXCEEDED,
];

/**
 * Translates a webpage using Yandex's Translation API.
 *
 * @param translationDocument  The TranslationDocument object that represents
 *                             the webpage to be translated
 * @param sourceLanguage       The source language of the document
 * @param targetLanguage       The target language for the translation
 *
 * @returns {Promise}          A promise that will resolve when the translation
 *                             task is finished.
 */
this.YandexTranslator = function(translationDocument, sourceLanguage, targetLanguage) {
  this.translationDocument = translationDocument;
  this.sourceLanguage = sourceLanguage;
  this.targetLanguage = targetLanguage;
  this._pendingRequests = 0;
  this._partialSuccess = false;
  this._serviceUnavailable = false;
  this._translatedCharacterCount = 0;
};

this.YandexTranslator.prototype = {
  /**
   * Performs the translation, splitting the document into several chunks
   * respecting the data limits of the API.
   *
   * @returns {Promise}          A promise that will resolve when the translation
   *                             task is finished.
   */
  translate() {
    return Task.spawn(function *() {
      let currentIndex = 0;
      this._onFinishedDeferred = Promise.defer();

      // Let's split the document into various requests to be sent to
      // Yandex's Translation API.
      for (let requestCount = 0; requestCount < MAX_REQUESTS; requestCount++) {
        // Generating the text for each request can be expensive, so
        // let's take the opportunity of the chunkification process to
        // allow for the event loop to attend other pending events
        // before we continue.
        yield CommonUtils.laterTickResolvingPromise();

        // Determine the data for the next request.
        let request = this._generateNextTranslationRequest(currentIndex);

        // Create a real request to the server, and put it on the
        // pending requests list.
        let yandexRequest = new YandexRequest(request.data,
                                          this.sourceLanguage,
                                          this.targetLanguage);
        this._pendingRequests++;
        yandexRequest.fireRequest().then(this._chunkCompleted.bind(this),
                                       this._chunkFailed.bind(this));

        currentIndex = request.lastIndex;
        if (request.finished) {
          break;
        }
      }

      return this._onFinishedDeferred.promise;
    }.bind(this));
  },

  /**
   * Function called when a request sent to the server completed successfully.
   * This function handles calling the function to parse the result and the
   * function to resolve the promise returned by the public `translate()`
   * method when there are no pending requests left.
   *
   * @param   request   The YandexRequest sent to the server
   */
  _chunkCompleted(yandexRequest) {
    if (this._parseChunkResult(yandexRequest)) {
      this._partialSuccess = true;
      // Count the number of characters successfully translated.
      this._translatedCharacterCount += yandexRequest.characterCount;
    }

    this._checkIfFinished();
  },

  /**
   * Function called when a request sent to the server has failed.
   * This function handles deciding if the error is transient or means the
   * service is unavailable (zero balance on the key or request credentials are
   * not in an active state) and calling the function to resolve the promise
   * returned by the public `translate()` method when there are no pending
   * requests left.
   *
   * @param   aError   [optional] The XHR object of the request that failed.
   */
  _chunkFailed(aError) {
    if (aError instanceof Ci.nsIXMLHttpRequest) {
      let body = aError.responseText;
      let json = { code: 0 };
      try {
        json = JSON.parse(body);
      } catch (e) {}

      if (json.code && YANDEX_PERMANENT_ERRORS.indexOf(json.code) != -1)
        this._serviceUnavailable = true;
    }

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
          characterCount: this._translatedCharacterCount
        });
      } else {
        let error = this._serviceUnavailable ? "unavailable" : "failure";
        this._onFinishedDeferred.reject(error);
      }
    }
  },

  /**
   * This function parses the result returned by Yandex's Translation API,
   * which returns a JSON result that contains a number of elements. The
   * API is documented here:
   * http://api.yandex.com/translate/doc/dg/reference/translate.xml
   *
   * @param   request      The request sent to the server.
   * @returns boolean      True if parsing of this chunk was successful.
   */
  _parseChunkResult(yandexRequest) {
    let results;
    try {
      let result = JSON.parse(yandexRequest.networkRequest.responseText);
      if (result.code != 200) {
        Services.console.logStringMessage("YandexTranslator: Result is " + result.code);
        return false;
      }
      results = result.text
    } catch (e) {
      return false;
    }

    let len = results.length;
    if (len != yandexRequest.translationData.length) {
      // This should never happen, but if the service returns a different number
      // of items (from the number of items submitted), we can't use this chunk
      // because all items would be paired incorrectly.
      return false;
    }

    let error = false;
    for (let i = 0; i < len; i++) {
      try {
        let result = results[i];
        let root = yandexRequest.translationData[i][0];
        root.parseResult(result);
      } catch (e) { error = true; }
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

      if (newCurSize > MAX_REQUEST_DATA ||
          newChunks > MAX_REQUEST_CHUNKS) {

        // If we've reached the API limits, let's stop accumulating data
        // for this request and return. We return information useful for
        // the caller to pass back on the next call, so that the function
        // can keep working from where it stopped.
        return {
          data: output,
          finished: false,
          lastIndex: i
        };
      }

      currentDataSize = newCurSize;
      currentChunks = newChunks;
      output.push([root, text]);
    }

    return {
      data: output,
      finished: true,
      lastIndex: 0
    };
  }
};

/**
 * Represents a request (for 1 chunk) sent off to Yandex's service.
 *
 * @params translationData  The data to be used for this translation,
 *                          generated by the generateNextTranslationRequest...
 *                          function.
 * @param sourceLanguage    The source language of the document.
 * @param targetLanguage    The target language for the translation.
 *
 */
function YandexRequest(translationData, sourceLanguage, targetLanguage) {
  this.translationData = translationData;
  this.sourceLanguage = sourceLanguage;
  this.targetLanguage = targetLanguage;
  this.characterCount = 0;
}

YandexRequest.prototype = {
  /**
   * Initiates the request
   */
  fireRequest() {
    return Task.spawn(function *() {
      // Prepare URL.
      let url = getUrlParam("https://translate.yandex.net/api/v1.5/tr.json/translate",
                            "browser.translation.yandex.translateURLOverride");

      // Prepare the request body.
      let apiKey = getUrlParam("%YANDEX_API_KEY%", "browser.translation.yandex.apiKeyOverride");
      let params = [
        ["key", apiKey],
        ["format", "html"],
        ["lang", this.sourceLanguage + "-" + this.targetLanguage],
      ];

      for (let [, text] of this.translationData) {
        params.push(["text", text]);
        this.characterCount += text.length;
      }

      // Set up request options.
      let deferred = Promise.defer();
      let options = {
        onLoad: (responseText, xhr) => {
          deferred.resolve(this);
        },
        onError(e, responseText, xhr) {
          deferred.reject(xhr);
        },
        postData: params
      };

      // Fire the request.
      this.networkRequest = httpRequest(url, options);

      return deferred.promise;
    }.bind(this));
  }
};

/**
 * Fetch an auth token (clientID or client secret), which may be overridden by
 * a pref if it's set.
 */
function getUrlParam(paramValue, prefName) {
  if (Services.prefs.getPrefType(prefName))
    paramValue = Services.prefs.getCharPref(prefName);
  paramValue = Services.urlFormatter.formatURL(paramValue);
  return paramValue;
}
