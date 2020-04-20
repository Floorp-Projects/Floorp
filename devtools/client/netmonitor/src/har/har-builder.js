/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const appInfo = Services.appinfo;
const { LocalizationHelper } = require("devtools/shared/l10n");
const { CurlUtils } = require("devtools/client/shared/curl");
const {
  getFormDataSections,
  getUrlQuery,
  parseQueryString,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  buildHarLog,
} = require("devtools/client/netmonitor/src/har/har-builder-utils");
const L10N = new LocalizationHelper("devtools/client/locales/har.properties");
const { TIMING_KEYS } = require("devtools/client/netmonitor/src/constants");

/**
 * This object is responsible for building HAR file. See HAR spec:
 * https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/HAR/Overview.html
 * http://www.softwareishard.com/blog/har-12-spec/
 *
 * @param {Object} options configuration object
 *
 * The following options are supported:
 *
 * - items {Array}: List of Network requests to be exported.
 *
 * - id {String}: ID of the exported page.
 *
 * - title {String}: Title of the exported page.
 *
 * - includeResponseBodies {Boolean}: Set to true to include HTTP response
 *   bodies in the result data structure.
 */
var HarBuilder = function(options) {
  this._options = options;
  this._pageMap = [];
};

HarBuilder.prototype = {
  // Public API

  /**
   * This is the main method used to build the entire result HAR data.
   * The process is asynchronous since it can involve additional RDP
   * communication (e.g. resolving long strings).
   *
   * @returns {Promise} A promise that resolves to the HAR object when
   * the entire build process is done.
   */
  build: async function() {
    this.promises = [];

    // Build basic structure for data.
    const log = buildHarLog(appInfo);

    // Build entries.
    for (const file of this._options.items) {
      log.log.entries.push(await this.buildEntry(log.log, file));
    }

    // Some data needs to be fetched from the backend during the
    // build process, so wait till all is done.
    await Promise.all(this.promises);

    return log;
  },

  // Helpers

  buildPage: function(file) {
    const page = {};

    // Page start time is set when the first request is processed
    // (see buildEntry)
    page.startedDateTime = 0;
    page.id = "page_" + this._options.id;
    page.title = this._options.title;

    return page;
  },

  getPage: function(log, file) {
    const { id } = this._options;
    let page = this._pageMap[id];
    if (page) {
      return page;
    }

    this._pageMap[id] = page = this.buildPage(file);
    log.pages.push(page);

    return page;
  },

  buildEntry: async function(log, file) {
    const page = this.getPage(log, file);

    const entry = {};
    entry.pageref = page.id;
    entry.startedDateTime = dateToJSON(new Date(file.startedMs));

    let { eventTimings } = file;
    if (!eventTimings && this._options.requestData) {
      eventTimings = await this._options.requestData(file.id, "eventTimings");
    }

    entry.request = await this.buildRequest(file);
    entry.response = await this.buildResponse(file);
    entry.cache = await this.buildCache(file);
    entry.timings = eventTimings ? eventTimings.timings : {};

    // Calculate total time by summing all timings. Note that
    // `file.totalTime` can't be used since it doesn't have to
    // correspond to plain summary of individual timings.
    // With TCP Fast Open and TLS early data sending data can
    // start at the same time as connect (we can send data on
    // TCP syn packet). Also TLS handshake can carry application
    // data thereby overlapping a sending data period and TLS
    // handshake period.
    entry.time = TIMING_KEYS.reduce((sum, type) => {
      const time = entry.timings[type];
      return typeof time != "undefined" ? sum + time : sum;
    }, 0);

    // Security state isn't part of HAR spec, and so create
    // custom field that needs to use '_' prefix.
    entry._securityState = file.securityState;

    if (file.remoteAddress) {
      entry.serverIPAddress = file.remoteAddress;
    }

    if (file.remotePort) {
      entry.connection = file.remotePort + "";
    }

    // Compute page load start time according to the first request start time.
    if (!page.startedDateTime) {
      page.startedDateTime = entry.startedDateTime;
      page.pageTimings = this.buildPageTimings(page, file);
    }

    return entry;
  },

  buildPageTimings: function(page, file) {
    // Event timing info isn't available
    const timings = {
      onContentLoad: -1,
      onLoad: -1,
    };

    const { getTimingMarker } = this._options;
    if (getTimingMarker) {
      timings.onContentLoad = getTimingMarker(
        "firstDocumentDOMContentLoadedTimestamp"
      );
      timings.onLoad = getTimingMarker("firstDocumentLoadTimestamp");
    }

    return timings;
  },

  buildRequest: async function(file) {
    // When using HarAutomation, HarCollector will automatically fetch requestHeaders
    // and requestCookies, but when we use it from netmonitor, FirefoxDataProvider
    // should fetch it itself lazily, via requestData.

    let { requestHeaders } = file;
    if (!requestHeaders && this._options.requestData) {
      requestHeaders = await this._options.requestData(
        file.id,
        "requestHeaders"
      );
    }

    let { requestCookies } = file;
    if (!requestCookies && this._options.requestData) {
      requestCookies = await this._options.requestData(
        file.id,
        "requestCookies"
      );
    }

    const request = {
      bodySize: 0,
    };
    request.method = file.method;
    request.url = file.url;
    request.httpVersion = file.httpVersion || "";
    request.headers = this.buildHeaders(requestHeaders);
    request.headers = this.appendHeadersPostData(request.headers, file);
    request.cookies = this.buildCookies(requestCookies);
    request.queryString = parseQueryString(getUrlQuery(file.url)) || [];
    request.headersSize = requestHeaders.headersSize;
    request.postData = await this.buildPostData(file);

    if (request.postData?.text) {
      request.bodySize = request.postData.text.length;
    }

    return request;
  },

  /**
   * Fetch all header values from the backend (if necessary) and
   * build the result HAR structure.
   *
   * @param {Object} input Request or response header object.
   */
  buildHeaders: function(input) {
    if (!input) {
      return [];
    }

    return this.buildNameValuePairs(input.headers);
  },

  appendHeadersPostData: function(input = [], file) {
    if (!file.requestPostData) {
      return input;
    }

    this.fetchData(file.requestPostData.postData.text).then(value => {
      const multipartHeaders = CurlUtils.getHeadersFromMultipartText(value);
      for (const header of multipartHeaders) {
        input.push(header);
      }
    });

    return input;
  },

  buildCookies: function(input) {
    if (!input) {
      return [];
    }

    return this.buildNameValuePairs(input.cookies || input);
  },

  buildNameValuePairs: function(entries) {
    const result = [];

    // HAR requires headers array to be presented, so always
    // return at least an empty array.
    if (!entries) {
      return result;
    }

    // Make sure header values are fully fetched from the server.
    entries.forEach(entry => {
      this.fetchData(entry.value).then(value => {
        result.push({
          name: entry.name,
          value: value,
        });
      });
    });

    return result;
  },

  buildPostData: async function(file) {
    // When using HarAutomation, HarCollector will automatically fetch requestPostData
    // and requestHeaders, but when we use it from netmonitor, FirefoxDataProvider
    // should fetch it itself lazily, via requestData.
    let { requestPostData } = file;
    let { requestHeaders } = file;
    let requestHeadersFromUploadStream;

    if (!requestPostData && this._options.requestData) {
      requestPostData = await this._options.requestData(
        file.id,
        "requestPostData"
      );
      requestHeadersFromUploadStream = requestPostData.uploadHeaders;
    }

    if (!requestPostData.postData.text) {
      return undefined;
    }

    if (!requestHeaders && this._options.requestData) {
      requestHeaders = await this._options.requestData(
        file.id,
        "requestHeaders"
      );
    }

    const postData = {
      mimeType: findValue(requestHeaders.headers, "content-type"),
      params: [],
      text: requestPostData.postData.text,
    };

    if (requestPostData.postDataDiscarded) {
      postData.comment = L10N.getStr("har.requestBodyNotIncluded");
      return postData;
    }

    // If we are dealing with URL encoded body, parse parameters.
    if (
      CurlUtils.isUrlEncodedRequest({
        headers: requestHeaders.headers,
        postDataText: postData.text,
      })
    ) {
      postData.mimeType = "application/x-www-form-urlencoded";
      // Extract form parameters and produce nice HAR array.
      const formDataSections = await getFormDataSections(
        requestHeaders,
        requestHeadersFromUploadStream,
        requestPostData,
        this._options.getString
      );

      formDataSections.forEach(section => {
        const paramsArray = parseQueryString(section);
        if (paramsArray) {
          postData.params = [...postData.params, ...paramsArray];
        }
      });
    }

    return postData;
  },

  buildResponse: async function(file) {
    // When using HarAutomation, HarCollector will automatically fetch responseHeaders
    // and responseCookies, but when we use it from netmonitor, FirefoxDataProvider
    // should fetch it itself lazily, via requestData.

    let { responseHeaders } = file;
    if (!responseHeaders && this._options.requestData) {
      responseHeaders = await this._options.requestData(
        file.id,
        "responseHeaders"
      );
    }

    let { responseCookies } = file;
    if (!responseCookies && this._options.requestData) {
      responseCookies = await this._options.requestData(
        file.id,
        "responseCookies"
      );
    }

    const response = {
      status: 0,
    };

    // Arbitrary value if it's aborted to make sure status has a number
    if (file.status) {
      response.status = parseInt(file.status, 10);
    }
    response.statusText = file.statusText || "";
    response.httpVersion = file.httpVersion || "";

    response.headers = this.buildHeaders(responseHeaders);
    response.cookies = this.buildCookies(responseCookies);
    response.content = await this.buildContent(file);

    const headers = responseHeaders ? responseHeaders.headers : null;
    const headersSize = responseHeaders ? responseHeaders.headersSize : -1;

    response.redirectURL = findValue(headers, "Location");
    response.headersSize = headersSize;

    // 'bodySize' is size of the received response body in bytes.
    // Set to zero in case of responses coming from the cache (304).
    // Set to -1 if the info is not available.
    if (typeof file.transferredSize != "number") {
      response.bodySize = response.status == 304 ? 0 : -1;
    } else {
      response.bodySize = file.transferredSize;
    }

    return response;
  },

  buildContent: async function(file) {
    const content = {
      mimeType: file.mimeType,
      size: -1,
    };

    // When using HarAutomation, HarCollector will automatically fetch responseContent,
    // but when we use it from netmonitor, FirefoxDataProvider should fetch it itself
    // lazily, via requestData.
    let { responseContent } = file;
    if (!responseContent && this._options.requestData) {
      responseContent = await this._options.requestData(
        file.id,
        "responseContent"
      );
    }
    if (responseContent?.content) {
      content.size = responseContent.content.size;
      content.encoding = responseContent.content.encoding;
    }

    const includeBodies = this._options.includeResponseBodies;
    const contentDiscarded = responseContent
      ? responseContent.contentDiscarded
      : false;

    // The comment is appended only if the response content
    // is explicitly discarded.
    if (!includeBodies || contentDiscarded) {
      content.comment = L10N.getStr("har.responseBodyNotIncluded");
      return content;
    }

    if (responseContent) {
      const { text } = responseContent.content;
      this.fetchData(text).then(value => {
        content.text = value;
      });
    }

    return content;
  },

  buildCache: async function(file) {
    const cache = {};

    // if resource has changed, return early
    if (file.status != "304") {
      return cache;
    }

    if (file.responseCacheAvailable && this._options.requestData) {
      const responseCache = await this._options.requestData(
        file.id,
        "responseCache"
      );
      if (responseCache.cache) {
        cache.afterRequest = this.buildCacheEntry(responseCache.cache);
      }
    } else if (file.responseCache?.cache) {
      cache.afterRequest = this.buildCacheEntry(file.responseCache.cache);
    } else {
      cache.afterRequest = null;
    }

    return cache;
  },

  buildCacheEntry: function(cacheEntry) {
    const cache = {};

    if (typeof cacheEntry !== "undefined") {
      cache.expires = findKeys(cacheEntry, ["expires"]);
      cache.lastFetched = findKeys(cacheEntry, ["lastFetched"]);
      cache.eTag = findKeys(cacheEntry, ["eTag"]);
      cache.fetchCount = findKeys(cacheEntry, ["fetchCount"]);

      // har-importer.js, along with other files, use buildCacheEntry
      // initial value comes from properties without underscores.
      // this checks for both in appropriate order.
      cache._dataSize = findKeys(cacheEntry, ["dataSize", "_dataSize"]);
      cache._lastModified = findKeys(cacheEntry, [
        "lastModified",
        "_lastModified",
      ]);
      cache._device = findKeys(cacheEntry, ["device", "_device"]);
    }

    return cache;
  },

  getBlockingEndTime: function(file) {
    if (file.resolveStarted && file.connectStarted) {
      return file.resolvingTime;
    }

    if (file.connectStarted) {
      return file.connectingTime;
    }

    if (file.sendStarted) {
      return file.sendingTime;
    }

    return file.sendingTime > file.startTime
      ? file.sendingTime
      : file.waitingForTime;
  },

  // RDP Helpers

  fetchData: function(string) {
    const promise = this._options.getString(string).then(value => {
      return value;
    });

    // Building HAR is asynchronous and not done till all
    // collected promises are resolved.
    this.promises.push(promise);

    return promise;
  },
};

// Helpers

/**
 * Find specified keys within an object.
 * Searches object for keys passed in, returns first value returned,
 * or an empty string.
 *
 * @param obj (object)
 * @param keys (array)
 * @returns {string}
 */
function findKeys(obj, keys) {
  if (!keys) {
    return "";
  }

  const keyFound = keys.filter(key => obj[key]);
  if (!keys.length) {
    return "";
  }

  const value = obj[keyFound[0]];
  if (typeof value === "undefined" || typeof value === "object") {
    return "";
  }

  return String(value);
}

/**
 * Find specified value within an array of name-value pairs
 * (used for headers, cookies and cache entries)
 */
function findValue(arr, name) {
  if (!arr) {
    return "";
  }

  name = name.toLowerCase();
  const result = arr.find(entry => entry.name.toLowerCase() == name);
  return result ? result.value : "";
}

/**
 * Generate HAR representation of a date.
 * (YYYY-MM-DDThh:mm:ss.sTZD, e.g. 2009-07-24T19:20:30.45+01:00)
 * See also HAR Schema: http://janodvarko.cz/har/viewer/
 *
 * Note: it would be great if we could utilize Date.toJSON(), but
 * it doesn't return proper time zone offset.
 *
 * An example:
 * This helper returns:    2015-05-29T16:10:30.424+02:00
 * Date.toJSON() returns:  2015-05-29T14:10:30.424Z
 *
 * @param date {Date} The date object we want to convert.
 */
function dateToJSON(date) {
  function f(n, c) {
    if (!c) {
      c = 2;
    }
    let s = new String(n);
    while (s.length < c) {
      s = "0" + s;
    }
    return s;
  }

  const result =
    date.getFullYear() +
    "-" +
    f(date.getMonth() + 1) +
    "-" +
    f(date.getDate()) +
    "T" +
    f(date.getHours()) +
    ":" +
    f(date.getMinutes()) +
    ":" +
    f(date.getSeconds()) +
    "." +
    f(date.getMilliseconds(), 3);

  let offset = date.getTimezoneOffset();
  const positive = offset > 0;

  // Convert to positive number before using Math.floor (see issue 5512)
  offset = Math.abs(offset);
  const offsetHours = Math.floor(offset / 60);
  const offsetMinutes = Math.floor(offset % 60);
  const prettyOffset =
    (positive > 0 ? "-" : "+") + f(offsetHours) + ":" + f(offsetMinutes);

  return result + prettyOffset;
}

// Exports from this module
exports.HarBuilder = HarBuilder;
