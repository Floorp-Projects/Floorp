/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TIMING_KEYS } = require("../constants");

var guid = 0;

/**
 * This object is responsible for importing HAR file. See HAR spec:
 * https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/HAR/Overview.html
 * http://www.softwareishard.com/blog/har-12-spec/
 */
var HarImporter = function(actions) {
  this.actions = actions;
};

HarImporter.prototype = {
  /**
   * This is the main method used to import HAR data.
   */
  import: function(har) {
    const json = JSON.parse(har);
    this.doImport(json);
  },

  doImport: function(har) {
    this.actions.clearRequests();

    // Helper map for pages.
    const pages = new Map();
    har.log.pages.forEach(page => {
      pages.set(page.id, page);
    });

    // Iterate all entries/requests and generate state.
    har.log.entries.forEach(entry => {
      const requestId = String(++guid);
      const startedMillis = Date.parse(entry.startedDateTime);

      // Add request
      this.actions.addRequest(requestId, {
        startedMillis: startedMillis,
        method: entry.request.method,
        url: entry.request.url,
        isXHR: false,
        cause: {
          loadingDocumentUri: "",
          stackTraceAvailable: false,
          type: "",
        },
        fromCache: false,
        fromServiceWorker: false,
      }, false);

      // Update request
      this.actions.updateRequest(requestId, {
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
          return (time != -1) ? (sum + time) : sum;
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
      }, false);

      // Page timing markers
      const pageTimings = pages.get(entry.pageref).pageTimings;
      let onContentLoad = pageTimings.onContentLoad || 0;
      let onLoad = pageTimings.onLoad || 0;

      // Set 0 as the default value
      onContentLoad = (onContentLoad != -1) ? onContentLoad : 0;
      onLoad = (onLoad != -1) ? onLoad : 0;

      // Add timing markers
      if (onContentLoad > 0) {
        this.actions.addTimingMarker({
          name: "dom-interactive",
          time: startedMillis + onContentLoad,
        });
      }

      if (onLoad > 0) {
        this.actions.addTimingMarker({
          name: "dom-complete",
          time: startedMillis + onLoad,
        });
      }
    });
  },
};

// Exports from this module
exports.HarImporter = HarImporter;
