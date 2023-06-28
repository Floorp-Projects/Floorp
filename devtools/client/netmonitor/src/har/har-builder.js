/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const appInfo = Services.appinfo;
const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const { CurlUtils } = require("resource://devtools/client/shared/curl.js");
const {
  getFormDataSections,
  getUrlQuery,
  parseQueryString,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");
const {
  buildHarLog,
} = require("resource://devtools/client/netmonitor/src/har/har-builder-utils.js");
const L10N = new LocalizationHelper("devtools/client/locales/har.properties");
const {
  TIMING_KEYS,
} = require("resource://devtools/client/netmonitor/src/constants.js");

/**
 * This object is responsible for building HAR file. See HAR spec:
 * https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/HAR/Overview.html
 * http://www.softwareishard.com/blog/har-12-spec/
 *
 * @param {Object} options
 *        configuration object
 * @param {Boolean} options.connector
 *        Set to true to include HTTP response bodies in the result data
 *        structure.
 * @param {String} options.id
 *        ID of the exported page.
 * @param {Boolean} options.includeResponseBodies
 *        Set to true to include HTTP response bodies in the result data
 *        structure.
 * @param {Array} options.items
 *        List of network events to be exported.
 * @param {Boolean} options.supportsMultiplePages
 *        Set to true to create distinct page entries for each navigation.
 */
var HarBuilder = function (options) {
  this._connector = options.connector;
  this._id = options.id;
  this._includeResponseBodies = options.includeResponseBodies;
  this._items = options.items;
  // Page id counter, only used when options.supportsMultiplePages is true.
  this._pageId = options.supportsMultiplePages ? 0 : options.id;
  this._pageMap = [];
  this._supportsMultiplePages = options.supportsMultiplePages;
  this._url = this._connector.currentTarget.url;
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
  async build() {
    this.promises = [];

    // Build basic structure for data.
    const harLog = buildHarLog(appInfo);

    // Build pages.
    this.buildPages(harLog.log);

    // Build entries.
    for (const request of this._items) {
      const entry = await this.buildEntry(harLog.log, request);
      if (entry) {
        harLog.log.entries.push(entry);
      }
    }

    // Some data needs to be fetched from the backend during the
    // build process, so wait till all is done.
    await Promise.all(this.promises);

    return harLog;
  },

  // Helpers

  buildPages(log) {
    if (this._supportsMultiplePages) {
      this.buildPagesFromTargetTitles(log);
    } else if (this._items.length) {
      const firstRequest = this._items[0];
      const page = this.buildPage(this._url, firstRequest);
      log.pages.push(page);
      this._pageMap[this._id] = page;
    }
  },

  buildPagesFromTargetTitles(log) {
    // Retrieve the additional HAR data collected by the connector.
    const { initialURL, navigationRequests } = this._connector.getHarData();
    const firstNavigationRequest = navigationRequests[0];
    const firstRequest = this._items[0];

    if (
      !firstNavigationRequest ||
      firstRequest.resourceId !== firstNavigationRequest.resourceId
    ) {
      // If the first request is not a navigation request, it must be related
      // to the initial page. Create a first page entry for such early requests.
      const initialPage = this.buildPage(initialURL, firstRequest);
      log.pages.push(initialPage);
    }

    for (const request of navigationRequests) {
      const page = this.buildPage(request.url, request);
      log.pages.push(page);
    }
  },

  buildPage(url, networkEvent) {
    const page = {};

    page.id = "page_" + this._pageId;
    page.pageTimings = this.buildPageTimings(page, networkEvent);
    page.startedDateTime = dateToHarString(new Date(networkEvent.startedMs));

    // To align with other existing implementations of HAR exporters, the title
    // should contain the page URL and not the page title.
    page.title = url;

    // Increase the pageId, for upcoming calls to buildPage.
    // If supportsMultiplePages is disabled this method is only called once.
    this._pageId++;

    return page;
  },

  getPage(log, entry) {
    const existingPage = log.pages.findLast(
      ({ startedDateTime }) => startedDateTime <= entry.startedDateTime
    );

    if (!existingPage) {
      throw new Error(
        "Could not find a page for request: " + entry.request.url
      );
    }

    return existingPage;
  },

  async buildEntry(log, networkEvent) {
    const entry = {};
    entry.startedDateTime = dateToHarString(new Date(networkEvent.startedMs));

    let { eventTimings, id } = networkEvent;
    try {
      if (!eventTimings && this._connector.requestData) {
        eventTimings = await this._connector.requestData(id, "eventTimings");
      }

      entry.request = await this.buildRequest(networkEvent);
      entry.response = await this.buildResponse(networkEvent);
      entry.cache = await this.buildCache(networkEvent);
    } catch (e) {
      // Ignore any request for which we can't retrieve lazy data
      // The request has most likely been destroyed on the server side,
      // either because persist is disabled or the request's target/WindowGlobal/process
      // has been destroyed.
      console.warn("HAR builder failed on", networkEvent.url, e, e.stack);
      return null;
    }
    entry.timings = eventTimings ? eventTimings.timings : {};

    // Calculate total time by summing all timings. Note that
    // `networkEvent.totalTime` can't be used since it doesn't have to
    // correspond to plain summary of individual timings.
    // With TCP Fast Open and TLS early data sending data can
    // start at the same time as connect (we can send data on
    // TCP syn packet). Also TLS handshake can carry application
    // data thereby overlapping a sending data period and TLS
    // handshake period.
    entry.time = TIMING_KEYS.reduce((sum, type) => {
      const time = entry.timings[type];
      return typeof time != "undefined" && time != -1 ? sum + time : sum;
    }, 0);

    // Security state isn't part of HAR spec, and so create
    // custom field that needs to use '_' prefix.
    entry._securityState = networkEvent.securityState;

    if (networkEvent.remoteAddress) {
      entry.serverIPAddress = networkEvent.remoteAddress;
    }

    if (networkEvent.remotePort) {
      entry.connection = networkEvent.remotePort + "";
    }

    const page = this.getPage(log, entry);
    entry.pageref = page.id;

    return entry;
  },

  buildPageTimings(page, networkEvent) {
    // Event timing info isn't available
    const timings = {
      onContentLoad: -1,
      onLoad: -1,
    };

    // TODO: This method currently ignores the networkEvent and always retrieves
    // the same timing markers for all pages. Seee Bug 1833806.
    if (this._connector.getTimingMarker) {
      timings.onContentLoad = this._connector.getTimingMarker(
        "firstDocumentDOMContentLoadedTimestamp"
      );
      timings.onLoad = this._connector.getTimingMarker(
        "firstDocumentLoadTimestamp"
      );
    }

    return timings;
  },

  async buildRequest(networkEvent) {
    // When using HarAutomation, HarCollector will automatically fetch requestHeaders
    // and requestCookies, but when we use it from netmonitor, FirefoxDataProvider
    // should fetch it itself lazily, via requestData.

    let { id, requestHeaders } = networkEvent;
    if (!requestHeaders && this._connector.requestData) {
      requestHeaders = await this._connector.requestData(id, "requestHeaders");
    }

    let { requestCookies } = networkEvent;
    if (!requestCookies && this._connector.requestData) {
      requestCookies = await this._connector.requestData(id, "requestCookies");
    }

    const request = {
      bodySize: 0,
    };
    request.method = networkEvent.method;
    request.url = networkEvent.url;
    request.httpVersion = networkEvent.httpVersion || "";
    request.headers = this.buildHeaders(requestHeaders);
    request.headers = this.appendHeadersPostData(request.headers, networkEvent);
    request.cookies = this.buildCookies(requestCookies);
    request.queryString = parseQueryString(getUrlQuery(networkEvent.url)) || [];
    request.headersSize = requestHeaders.headersSize;
    request.postData = await this.buildPostData(networkEvent);

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
  buildHeaders(input) {
    if (!input) {
      return [];
    }

    return this.buildNameValuePairs(input.headers);
  },

  appendHeadersPostData(input = [], networkEvent) {
    if (!networkEvent.requestPostData) {
      return input;
    }

    this.fetchData(networkEvent.requestPostData.postData.text).then(value => {
      const multipartHeaders = CurlUtils.getHeadersFromMultipartText(value);
      for (const header of multipartHeaders) {
        input.push(header);
      }
    });

    return input;
  },

  buildCookies(input) {
    if (!input) {
      return [];
    }

    return this.buildNameValuePairs(input.cookies || input);
  },

  buildNameValuePairs(entries) {
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
          value,
        });
      });
    });

    return result;
  },

  async buildPostData(networkEvent) {
    // When using HarAutomation, HarCollector will automatically fetch requestPostData
    // and requestHeaders, but when we use it from netmonitor, FirefoxDataProvider
    // should fetch it itself lazily, via requestData.
    let { id, requestHeaders, requestPostData } = networkEvent;
    let requestHeadersFromUploadStream;

    if (!requestPostData && this._connector.requestData) {
      requestPostData = await this._connector.requestData(
        id,
        "requestPostData"
      );
      requestHeadersFromUploadStream = requestPostData.uploadHeaders;
    }

    if (!requestPostData.postData.text) {
      return undefined;
    }

    if (!requestHeaders && this._connector.requestData) {
      requestHeaders = await this._connector.requestData(id, "requestHeaders");
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
        this._connector.getLongString
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

  async buildResponse(networkEvent) {
    // When using HarAutomation, HarCollector will automatically fetch responseHeaders
    // and responseCookies, but when we use it from netmonitor, FirefoxDataProvider
    // should fetch it itself lazily, via requestData.

    let { id, responseCookies, responseHeaders } = networkEvent;
    if (!responseHeaders && this._connector.requestData) {
      responseHeaders = await this._connector.requestData(
        id,
        "responseHeaders"
      );
    }

    if (!responseCookies && this._connector.requestData) {
      responseCookies = await this._connector.requestData(
        id,
        "responseCookies"
      );
    }

    const response = {
      status: 0,
    };

    // Arbitrary value if it's aborted to make sure status has a number
    if (networkEvent.status) {
      response.status = parseInt(networkEvent.status, 10);
    }
    response.statusText = networkEvent.statusText || "";
    response.httpVersion = networkEvent.httpVersion || "";

    response.headers = this.buildHeaders(responseHeaders);
    response.cookies = this.buildCookies(responseCookies);
    response.content = await this.buildContent(networkEvent);

    const headers = responseHeaders ? responseHeaders.headers : null;
    const headersSize = responseHeaders ? responseHeaders.headersSize : -1;

    response.redirectURL = findValue(headers, "Location");
    response.headersSize = headersSize;

    // 'bodySize' is size of the received response body in bytes.
    // Set to zero in case of responses coming from the cache (304).
    // Set to -1 if the info is not available.
    if (typeof networkEvent.transferredSize != "number") {
      response.bodySize = response.status == 304 ? 0 : -1;
    } else {
      response.bodySize = networkEvent.transferredSize;
    }

    return response;
  },

  async buildContent(networkEvent) {
    const content = {
      mimeType: networkEvent.mimeType,
      size: -1,
    };

    // When using HarAutomation, HarCollector will automatically fetch responseContent,
    // but when we use it from netmonitor, FirefoxDataProvider should fetch it itself
    // lazily, via requestData.
    let { responseContent } = networkEvent;
    if (!responseContent && this._connector.requestData) {
      responseContent = await this._connector.requestData(
        networkEvent.id,
        "responseContent"
      );
    }
    if (responseContent?.content) {
      content.size = responseContent.content.size;
      content.encoding = responseContent.content.encoding;
    }

    const includeBodies = this._includeResponseBodies;
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

  async buildCache(networkEvent) {
    const cache = {};

    // if resource has changed, return early
    if (networkEvent.status != "304") {
      return cache;
    }

    if (networkEvent.responseCacheAvailable && this._connector.requestData) {
      const responseCache = await this._connector.requestData(
        networkEvent.id,
        "responseCache"
      );
      if (responseCache.cache) {
        cache.afterRequest = this.buildCacheEntry(responseCache.cache);
      }
    } else if (networkEvent.responseCache?.cache) {
      cache.afterRequest = this.buildCacheEntry(
        networkEvent.responseCache.cache
      );
    } else {
      cache.afterRequest = null;
    }

    return cache;
  },

  buildCacheEntry(cacheEntry) {
    const cache = {};

    if (typeof cacheEntry !== "undefined") {
      cache.expires = findKeys(cacheEntry, ["expirationTime", "expires"]);
      cache.lastFetched = findKeys(cacheEntry, ["lastFetched"]);

      // TODO: eTag support
      // Har format expects cache entries to provide information about eTag,
      // however this is not currently exposed on nsICacheEntry.
      // This should be stored under cache.eTag. See Bug 1799844.

      cache.fetchCount = findKeys(cacheEntry, ["fetchCount"]);

      // har-importer.js, along with other files, use buildCacheEntry
      // initial value comes from properties without underscores.
      // this checks for both in appropriate order.
      cache._dataSize = findKeys(cacheEntry, ["storageDataSize", "_dataSize"]);
      cache._lastModified = findKeys(cacheEntry, [
        "lastModified",
        "_lastModified",
      ]);
      cache._device = findKeys(cacheEntry, ["deviceID", "_device"]);
    }

    return cache;
  },

  // RDP Helpers

  fetchData(string) {
    const promise = this._connector.getLongString(string).then(value => {
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
function dateToHarString(date) {
  function f(n, c) {
    if (!c) {
      c = 2;
    }
    let s = String(n);
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
