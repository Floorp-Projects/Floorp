/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = [ "BingTranslation" ];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/rest.js");

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

/**
 * Translates a webpage using Bing's Translation API.
 *
 * @param translationDocument  The TranslationDocument object that represents
 *                             the webpage to be translated
 * @param sourceLanguage       The source language of the document
 * @param targetLanguage       The target language for the translation
 *
 * @returns {Promise}          A promise that will resolve when the translation
 *                             task is finished.
 */
this.BingTranslation = function(translationDocument, sourceLanguage, targetLanguage) {
  this.translationDocument = translationDocument;
  this.sourceLanguage = sourceLanguage;
  this.targetLanguage = targetLanguage;
  this._pendingRequests = 0;
  this._partialSuccess = false;
};

this.BingTranslation.prototype = {
  /**
   * Performs the translation, splitting the document into several chunks
   * respecting the data limits of the API.
   *
   * @returns {Promise}          A promise that will resolve when the translation
   *                             task is finished.
   */
  translate: function() {
    return Task.spawn(function *() {
      let currentIndex = 0;
      this._onFinishedDeferred = Promise.defer();

      // Let's split the document into various requests to be sent to
      // Bing's Translation API.
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
        let bingRequest = new BingRequest(request.data,
                                          this.sourceLanguage,
                                          this.targetLanguage);
        this._pendingRequests++;
        bingRequest.fireRequest().then(this._chunkCompleted.bind(this));

        currentIndex = request.lastIndex;
        if (request.finished) {
          break;
        }
      }

      return this._onFinishedDeferred.promise;
    }.bind(this));
  },

  /**
   * Function called when a request sent to the server is completed.
   * This function handles determining if the response was successful or not,
   * calling the function to parse the result, and resolving the promise
   * returned by the public `translate()` method when all chunks are completed.
   *
   * @param   request   The BingRequest sent to the server.
   */
  _chunkCompleted: function(bingRequest) {
     this._pendingRequests--;
     if (bingRequest.requestSucceeded &&
         this._parseChunkResult(bingRequest)) {
       // error on request
       this._partialSuccess = true;
     }

    // Check if all pending requests have been
    // completed and then resolves the promise.
    // If at least one chunk was successful, the
    // promise will be resolved positively which will
    // display the "Success" state for the infobar. Otherwise,
    // the "Error" state will appear.
    if (this._pendingRequests == 0) {
      if (this._partialSuccess) {
        this._onFinishedDeferred.resolve("success");
      } else {
        this._onFinishedDeferred.reject("failure");
      }
    }
  },

  _parseChunkResult() {
    // note: this function is implemented in the patch from bug 976556
  },

  /**
   * This function will determine what is the data to be used for
   * the Nth request we are generating, based on the input params.
   *
   * @param startIndex What is the index, in the roots list, that the
   *                   chunk should start.
   */
  _generateNextTranslationRequest: function(startIndex) {
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

      text = escapeXML(text);
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
 * Represents a request (for 1 chunk) sent off to Bing's service.
 *
 * @params translationData  The data to be used for this translation,
 *                          generated by the generateNextTranslationRequest...
 *                          function.
 * @param sourceLanguage    The source language of the document.
 * @param targetLanguage    The target language for the translation.
 *
 */
function BingRequest(translationData, sourceLanguage, targetLanguage) {
  this.translationData = translationData;
  this.sourceLanguage = sourceLanguage;
  this.targetLanguage = targetLanguage;
}

BingRequest.prototype = {
  /**
   * Initiates the request
   */
  fireRequest: function() {
    return Task.spawn(function *(){
      let token = yield BingTokenManager.getToken();
      let auth = "Bearer " + token;
      let request = new RESTRequest("https://api.microsofttranslator.com/v2/Http.svc/TranslateArray");
      request.setHeader("Content-type", "text/xml");
      request.setHeader("Authorization", auth);

      let requestString =
        '<TranslateArrayRequest>' +
          '<AppId/>' +
          '<From>' + this.sourceLanguage + '</From>' +
          '<Options>' +
            '<ContentType xmlns="http://schemas.datacontract.org/2004/07/Microsoft.MT.Web.Service.V2">text/html</ContentType>' +
            '<ReservedFlags xmlns="http://schemas.datacontract.org/2004/07/Microsoft.MT.Web.Service.V2" />' +
          '</Options>' +
          '<Texts xmlns:s="http://schemas.microsoft.com/2003/10/Serialization/Arrays">';

      for (let [, text] of this.translationData) {
        requestString += '<s:string>' + text + '</s:string>';
      }

      requestString += '</Texts>' +
          '<To>' + this.targetLanguage + '</To>' +
        '</TranslateArrayRequest>';

      let utf8 = CommonUtils.encodeUTF8(requestString);

      let deferred = Promise.defer();
      request.post(utf8, function(err) {
        deferred.resolve(this);
      }.bind(this));

      this.networkRequest = request;
      return deferred.promise;
    }.bind(this));
  },

  /**
   * Checks if the request succeeded. Only valid
   * after the request has finished.
   *
   * @returns    True if the request succeeded.
   */
  get requestSucceeded() {
    return !this.networkRequest.error &&
            this.networkRequest.response.success;
   }
};

/**
 * Escape a string to be valid XML content.
 */
function escapeXML(aStr) {
  return aStr.toString()
             .replace("&", "&amp;", "g")
             .replace('"', "&quot;", "g")
             .replace("'", "&apos;", "g")
             .replace("<", "&lt;", "g")
             .replace(">", "&gt;", "g");
}
