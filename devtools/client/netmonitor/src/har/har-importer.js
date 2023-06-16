/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TIMING_KEYS,
} = require("resource://devtools/client/netmonitor/src/constants.js");
const {
  getUrlDetails,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

var guid = 0;

/**
 * This object is responsible for importing HAR file. See HAR spec:
 * https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/HAR/Overview.html
 * http://www.softwareishard.com/blog/har-12-spec/
 */
var HarImporter = function (actions) {
  this.actions = actions;
};

HarImporter.prototype = {
  /**
   * This is the main method used to import HAR data.
   */
  import(har) {
    const json = JSON.parse(har);
    this.doImport(json);
  },

  doImport(har) {
    this.actions.clearRequests();

    // Helper map for pages.
    const pages = new Map();
    har.log.pages.forEach(page => {
      pages.set(page.id, page);
    });

    // Iterate all entries/requests and generate state.
    har.log.entries.forEach(entry => {
      const requestId = String(++guid);
      const startedMs = Date.parse(entry.startedDateTime);

      // Add request
      this.actions.addRequest(
        requestId,
        {
          startedMs,
          method: entry.request.method,
          url: entry.request.url,
          urlDetails: getUrlDetails(entry.request.url),
          isXHR: false,
          cause: {
            loadingDocumentUri: "",
            stackTraceAvailable: false,
            type: "",
          },
          fromCache: false,
          fromServiceWorker: false,
        },
        false
      );

      // Update request
      const data = {
        requestHeaders: {
          headers: entry.request.headers,
          headersSize: entry.request.headersSize,
          rawHeaders: "",
        },
        responseHeaders: {
          headers: entry.response.headers,
          headersSize: entry.response.headersSize,
          rawHeaders: "",
        },
        requestCookies: entry.request.cookies,
        responseCookies: entry.response.cookies,
        requestPostData: {
          postData: entry.request.postData || {},
          postDataDiscarded: false,
        },
        responseContent: {
          content: entry.response.content,
          contentDiscarded: false,
        },
        eventTimings: {
          timings: entry.timings,
        },
        totalTime: TIMING_KEYS.reduce((sum, type) => {
          const time = entry.timings[type];
          return typeof time != "undefined" && time != -1 ? sum + time : sum;
        }, 0),

        httpVersion: entry.request.httpVersion,
        contentSize: entry.response.content.size,
        mimeType: entry.response.content.mimeType,
        remoteAddress: entry.serverIPAddress,
        remotePort: entry.connection,
        status: entry.response.status,
        statusText: entry.response.statusText,
        transferredSize: entry.response.bodySize,
        securityState: entry._securityState,

        // Avoid auto-fetching data from the backend
        eventTimingsAvailable: false,
        requestCookiesAvailable: false,
        requestHeadersAvailable: false,
        responseContentAvailable: false,
        responseStartAvailable: false,
        responseCookiesAvailable: false,
        responseHeadersAvailable: false,
        securityInfoAvailable: false,
        requestPostDataAvailable: false,
      };

      if (entry.cache.afterRequest) {
        const { afterRequest } = entry.cache;
        data.responseCache = {
          cache: {
            expires: afterRequest.expires,
            fetchCount: afterRequest.fetchCount,
            lastFetched: afterRequest.lastFetched,
            // TODO: eTag support, see Bug 1799844.
            // eTag: afterRequest.eTag,
            _dataSize: afterRequest._dataSize,
            _lastModified: afterRequest._lastModified,
            _device: afterRequest._device,
          },
        };
      }

      this.actions.updateRequest(requestId, data, false);

      // Page timing markers
      const pageTimings = pages.get(entry.pageref)?.pageTimings;
      let onContentLoad = (pageTimings && pageTimings.onContentLoad) || 0;
      let onLoad = (pageTimings && pageTimings.onLoad) || 0;

      // Set 0 as the default value
      onContentLoad = onContentLoad != -1 ? onContentLoad : 0;
      onLoad = onLoad != -1 ? onLoad : 0;

      // Add timing markers
      if (onContentLoad > 0) {
        this.actions.addTimingMarker({
          name: "dom-interactive",
          time: startedMs + onContentLoad,
        });
      }

      if (onLoad > 0) {
        this.actions.addTimingMarker({
          name: "dom-complete",
          time: startedMs + onLoad,
        });
      }
    });
  },
};

// Exports from this module
exports.HarImporter = HarImporter;
