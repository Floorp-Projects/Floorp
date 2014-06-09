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
  _parseChunkResult: function(bingRequest) {
    let domParser = Cc["@mozilla.org/xmlextras/domparser;1"]
                      .createInstance(Ci.nsIDOMParser);

    let results;
    try {
      let doc = domParser.parseFromString(bingRequest.networkRequest
                                                     .response.body, "text/xml");
      results = doc.querySelectorAll("TranslatedText");
    } catch (e) {
      return false;
    }

    let len = results.length;
    if (len != bingRequest.translationData.length) {
      // This should never happen, but if the service returns a different number
      // of items (from the number of items submitted), we can't use this chunk
      // because all items would be paired incorrectly.
      return false;
    }

    let error = false;
    for (let i = 0; i < len; i++) {
      try {
        let result = results[i].firstChild.nodeValue;
        let root = bingRequest.translationData[i][0];

        if (root.isSimpleRoot) {
          // Workaround for Bing's service problem in which "&" chars in
          // plain-text TranslationItems are double-escaped.
          result = result.replace("&amp;", "&", "g");
        }

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
 * Authentication Token manager for the API
 */
let BingTokenManager = {
  _currentToken: null,
  _currentExpiryTime: 0,
  _pendingRequest: null,

  /**
   * Get a valid, non-expired token to be used for the API calls.
   *
   * @returns {Promise}  A promise that resolves with the token
   *                     string once it is obtained. The token returned
   *                     can be the same one used in the past if it is still
   *                     valid.
   */
  getToken: function() {
    if (this._pendingRequest) {
      return this._pendingRequest;
    }

    let remainingMs = this._currentExpiryTime - new Date();
    // Our existing token is still good for more than a minute, let's use it.
    if (remainingMs > 60 * 1000) {
      return Promise.resolve(this._currentToken);
    }

    return this._getNewToken();
  },

  /**
   * Generates a new token from the server.
   *
   * @returns {Promise}  A promise that resolves with the token
   *                     string once it is obtained.
   */
  _getNewToken: function() {
    let request = new RESTRequest("https://datamarket.accesscontrol.windows.net/v2/OAuth2-13");
    request.setHeader("Content-type", "application/x-www-form-urlencoded");
    let params = [
      "grant_type=client_credentials",
      "scope=" + encodeURIComponent("http://api.microsofttranslator.com"),
      "client_id=" +
      getAuthTokenParam("%BING_API_CLIENTID%", "browser.translation.bing.clientIdOverride"),
      "client_secret=" +
      getAuthTokenParam("%BING_API_KEY%", "browser.translation.bing.apiKeyOverride")
    ];

    let deferred = Promise.defer();
    this._pendingRequest = deferred.promise;
    request.post(params.join("&"), function(err) {
      BingTokenManager._pendingRequest = null;

      if (err) {
        deferred.reject(err);
      }

      try {
        let json = JSON.parse(this.response.body);
        let token = json.access_token;
        let expires_in = json.expires_in;
        BingTokenManager._currentToken = token;
        BingTokenManager._currentExpiryTime = new Date(Date.now() + expires_in * 1000);
        deferred.resolve(token);
      } catch (e) {
        deferred.reject(e);
      }
    });

    return deferred.promise;
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

/**
 * Fetch an auth token (clientID or client secret), which may be overridden by
 * a pref if it's set.
 */
function getAuthTokenParam(key, prefName) {
  let val;
  try {
    val = Services.prefs.getCharPref(prefName);
  } catch(ex) {}

  return encodeURIComponent(Services.urlFormatter.formatURL(val || key));
}
