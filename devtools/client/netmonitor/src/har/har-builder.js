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
} = require("../utils/request-utils");

const L10N = new LocalizationHelper("devtools/client/locales/har.properties");
const HAR_VERSION = "1.1";

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
var HarBuilder = function (options) {
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
  build: function () {
    this.promises = [];

    // Build basic structure for data.
    let log = this.buildLog();

    // Build entries.
    for (let file of this._options.items) {
      log.entries.push(this.buildEntry(log, file));
    }

    // Some data needs to be fetched from the backend during the
    // build process, so wait till all is done.
    return Promise.all(this.promises).then(() => ({ log }));
  },

  // Helpers

  buildLog: function () {
    return {
      version: HAR_VERSION,
      creator: {
        name: appInfo.name,
        version: appInfo.version
      },
      browser: {
        name: appInfo.name,
        version: appInfo.version
      },
      pages: [],
      entries: [],
    };
  },

  buildPage: function (file) {
    let page = {};

    // Page start time is set when the first request is processed
    // (see buildEntry)
    page.startedDateTime = 0;
    page.id = "page_" + this._options.id;
    page.title = this._options.title;

    return page;
  },

  getPage: function (log, file) {
    let id = this._options.id;
    let page = this._pageMap[id];
    if (page) {
      return page;
    }

    this._pageMap[id] = page = this.buildPage(file);
    log.pages.push(page);

    return page;
  },

  buildEntry: function (log, file) {
    let page = this.getPage(log, file);

    let entry = {};
    entry.pageref = page.id;
    entry.startedDateTime = dateToJSON(new Date(file.startedMillis));
    entry.time = file.endedMillis - file.startedMillis;

    entry.request = this.buildRequest(file);
    entry.response = this.buildResponse(file);
    entry.cache = this.buildCache(file);
    entry.timings = file.eventTimings ? file.eventTimings.timings : {};

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

  buildPageTimings: function (page, file) {
    // Event timing info isn't available
    let timings = {
      onContentLoad: -1,
      onLoad: -1
    };

    return timings;
  },

  buildRequest: function (file) {
    let request = {
      bodySize: 0
    };

    request.method = file.method;
    request.url = file.url;
    request.httpVersion = file.httpVersion || "";

    request.headers = this.buildHeaders(file.requestHeaders);
    request.headers = this.appendHeadersPostData(request.headers, file);
    request.cookies = this.buildCookies(file.requestCookies);

    request.queryString = parseQueryString(getUrlQuery(file.url)) || [];

    request.postData = this.buildPostData(file);

    request.headersSize = file.requestHeaders.headersSize;

    // Set request body size, but make sure the body is fetched
    // from the backend.
    if (file.requestPostData) {
      this.fetchData(file.requestPostData.postData.text).then(value => {
        request.bodySize = value.length;
      });
    }

    return request;
  },

  /**
   * Fetch all header values from the backend (if necessary) and
   * build the result HAR structure.
   *
   * @param {Object} input Request or response header object.
   */
  buildHeaders: function (input) {
    if (!input) {
      return [];
    }

    return this.buildNameValuePairs(input.headers);
  },

  appendHeadersPostData: function (input = [], file) {
    if (!file.requestPostData) {
      return input;
    }

    this.fetchData(file.requestPostData.postData.text).then(value => {
      let multipartHeaders = CurlUtils.getHeadersFromMultipartText(value);
      for (let header of multipartHeaders) {
        input.push(header);
      }
    });

    return input;
  },

  buildCookies: function (input) {
    if (!input) {
      return [];
    }

    return this.buildNameValuePairs(input.cookies);
  },

  buildNameValuePairs: function (entries) {
    let result = [];

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
          value: value
        });
      });
    });

    return result;
  },

  buildPostData: function (file) {
    let postData = {
      mimeType: findValue(file.requestHeaders.headers, "content-type"),
      params: [],
      text: ""
    };

    if (!file.requestPostData) {
      return postData;
    }

    if (file.requestPostData.postDataDiscarded) {
      postData.comment = L10N.getStr("har.requestBodyNotIncluded");
      return postData;
    }

    // Load request body from the backend.
    this.fetchData(file.requestPostData.postData.text).then(postDataText => {
      postData.text = postDataText;

      // If we are dealing with URL encoded body, parse parameters.
      let { headers } = file.requestHeaders;
      if (CurlUtils.isUrlEncodedRequest({ headers, postDataText })) {
        postData.mimeType = "application/x-www-form-urlencoded";

        // Extract form parameters and produce nice HAR array.
        getFormDataSections(
          file.requestHeaders,
          file.requestHeadersFromUploadStream,
          file.requestPostData,
          this._options.getString,
        ).then(formDataSections => {
          formDataSections.forEach(section => {
            let paramsArray = parseQueryString(section);
            if (paramsArray) {
              postData.params = [...postData.params, ...paramsArray];
            }
          });
        });
      }
    });

    return postData;
  },

  buildResponse: function (file) {
    let response = {
      status: 0
    };

    // Arbitrary value if it's aborted to make sure status has a number
    if (file.status) {
      response.status = parseInt(file.status, 10);
    }

    let responseHeaders = file.responseHeaders;

    response.statusText = file.statusText || "";
    response.httpVersion = file.httpVersion || "";

    response.headers = this.buildHeaders(responseHeaders);
    response.cookies = this.buildCookies(file.responseCookies);
    response.content = this.buildContent(file);

    let headers = responseHeaders ? responseHeaders.headers : null;
    let headersSize = responseHeaders ? responseHeaders.headersSize : -1;

    response.redirectURL = findValue(headers, "Location");
    response.headersSize = headersSize;

    // 'bodySize' is size of the received response body in bytes.
    // Set to zero in case of responses coming from the cache (304).
    // Set to -1 if the info is not available.
    if (typeof file.transferredSize != "number") {
      response.bodySize = (response.status == 304) ? 0 : -1;
    } else {
      response.bodySize = file.transferredSize;
    }

    return response;
  },

  buildContent: function (file) {
    let content = {
      mimeType: file.mimeType,
      size: -1
    };

    let responseContent = file.responseContent;
    if (responseContent && responseContent.content) {
      content.size = responseContent.content.size;
      content.encoding = responseContent.content.encoding;
    }

    let includeBodies = this._options.includeResponseBodies;
    let contentDiscarded = responseContent ?
      responseContent.contentDiscarded : false;

    // The comment is appended only if the response content
    // is explicitly discarded.
    if (!includeBodies || contentDiscarded) {
      content.comment = L10N.getStr("har.responseBodyNotIncluded");
      return content;
    }

    if (responseContent) {
      let text = responseContent.content.text;
      this.fetchData(text).then(value => {
        content.text = value;
      });
    }

    return content;
  },

  buildCache: function (file) {
    let cache = {};

    if (!file.fromCache) {
      return cache;
    }

    // There is no such info yet in the Net panel.
    // cache.beforeRequest = {};

    if (file.cacheEntry) {
      cache.afterRequest = this.buildCacheEntry(file.cacheEntry);
    } else {
      cache.afterRequest = null;
    }

    return cache;
  },

  buildCacheEntry: function (cacheEntry) {
    let cache = {};

    cache.expires = findValue(cacheEntry, "Expires");
    cache.lastAccess = findValue(cacheEntry, "Last Fetched");
    cache.eTag = "";
    cache.hitCount = findValue(cacheEntry, "Fetch Count");

    return cache;
  },

  getBlockingEndTime: function (file) {
    if (file.resolveStarted && file.connectStarted) {
      return file.resolvingTime;
    }

    if (file.connectStarted) {
      return file.connectingTime;
    }

    if (file.sendStarted) {
      return file.sendingTime;
    }

    return (file.sendingTime > file.startTime) ?
      file.sendingTime : file.waitingForTime;
  },

  // RDP Helpers

  fetchData: function (string) {
    let promise = this._options.getString(string).then(value => {
      return value;
    });

    // Building HAR is asynchronous and not done till all
    // collected promises are resolved.
    this.promises.push(promise);

    return promise;
  }
};

// Helpers

/**
 * Find specified value within an array of name-value pairs
 * (used for headers, cookies and cache entries)
 */
function findValue(arr, name) {
  if (!arr) {
    return "";
  }

  name = name.toLowerCase();
  let result = arr.find(entry => entry.name.toLowerCase() == name);
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

  let result = date.getFullYear() + "-" +
    f(date.getMonth() + 1) + "-" +
    f(date.getDate()) + "T" +
    f(date.getHours()) + ":" +
    f(date.getMinutes()) + ":" +
    f(date.getSeconds()) + "." +
    f(date.getMilliseconds(), 3);

  let offset = date.getTimezoneOffset();
  let positive = offset > 0;

  // Convert to positive number before using Math.floor (see issue 5512)
  offset = Math.abs(offset);
  let offsetHours = Math.floor(offset / 60);
  let offsetMinutes = Math.floor(offset % 60);
  let prettyOffset = (positive > 0 ? "-" : "+") + f(offsetHours) +
    ":" + f(offsetMinutes);

  return result + prettyOffset;
}

// Exports from this module
exports.HarBuilder = HarBuilder;
