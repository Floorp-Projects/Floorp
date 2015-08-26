/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu, Ci, Cc } = require("chrome");
const { defer, all } = require("promise");
const { setTimeout, clearTimeout } = require("sdk/timers");
const { makeInfallible } = require("devtools/toolkit/DevToolsUtils.js");

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Helper tracer. Should be generic sharable by other modules (bug 1171927)
const trace = {
  log: function(...args) {
  }
}

/**
 * This object is responsible for collecting data related to all
 * HTTP requests executed by the page (including inner iframes).
 */
function HarCollector(options) {
  this.webConsoleClient = options.webConsoleClient;
  this.debuggerClient = options.debuggerClient;

  this.onNetworkEvent = this.onNetworkEvent.bind(this);
  this.onNetworkEventUpdate = this.onNetworkEventUpdate.bind(this);
  this.onRequestHeaders = this.onRequestHeaders.bind(this);
  this.onRequestCookies = this.onRequestCookies.bind(this);
  this.onRequestPostData = this.onRequestPostData.bind(this);
  this.onResponseHeaders = this.onResponseHeaders.bind(this);
  this.onResponseCookies = this.onResponseCookies.bind(this);
  this.onResponseContent = this.onResponseContent.bind(this);
  this.onEventTimings = this.onEventTimings.bind(this);

  this.onPageLoadTimeout = this.onPageLoadTimeout.bind(this);

  this.clear();
}

HarCollector.prototype = {
  // Connection

  start: function() {
    this.debuggerClient.addListener("networkEvent", this.onNetworkEvent);
    this.debuggerClient.addListener("networkEventUpdate", this.onNetworkEventUpdate);
  },

  stop: function() {
    this.debuggerClient.removeListener("networkEvent", this.onNetworkEvent);
    this.debuggerClient.removeListener("networkEventUpdate", this.onNetworkEventUpdate);
  },

  clear: function() {
    // Any pending requests events will be ignored (they turn
    // into zombies, since not present in the files array).
    this.files = new Map();
    this.items = [];
    this.firstRequestStart = -1;
    this.lastRequestStart = -1;
    this.requests = [];
  },

  waitForHarLoad: function() {
    // There should be yet another timeout 'devtools.netmonitor.har.pageLoadTimeout'
    // that should force export even if page isn't fully loaded.
    let deferred = defer();
    this.waitForResponses().then(() => {
      trace.log("HarCollector.waitForHarLoad; DONE HAR loaded!");
      deferred.resolve(this);
    });

    return deferred.promise;
  },

  waitForResponses: function() {
    trace.log("HarCollector.waitForResponses; " + this.requests.length);

    // All requests for additional data must be received to have complete
    // HTTP info to generate the result HAR file. So, wait for all current
    // promises. Note that new promises (requests) can be generated during the
    // process of HTTP data collection.
    return waitForAll(this.requests).then(() => {
      // All responses are received from the backend now. We yet need to
      // wait for a little while to see if a new request appears. If yes,
      // lets's start gathering HTTP data again. If no, we can declare
      // the page loaded.
      // If some new requests appears in the meantime the promise will
      // be rejected and we need to wait for responses all over again.
      return this.waitForTimeout().then(() => {
        // Page loaded!
      }, () => {
        trace.log("HarCollector.waitForResponses; NEW requests " +
          "appeared during page timeout!");

        // New requests executed, let's wait again.
        return this.waitForResponses();
      })
    });
  },

  // Page Loaded Timeout

  /**
   * The page is loaded when there are no new requests within given period
   * of time. The time is set in preferences:
   * 'devtools.netmonitor.har.pageLoadedTimeout'
   */
  waitForTimeout: function() {
    // The auto-export is not done if the timeout is set to zero (or less).
    // This is useful in cases where the export is done manually through
    // API exposed to the content.
    let timeout = Services.prefs.getIntPref(
      "devtools.netmonitor.har.pageLoadedTimeout");

    trace.log("HarCollector.waitForTimeout; " + timeout);

    this.pageLoadDeferred = defer();

    if (timeout <= 0) {
      this.pageLoadDeferred.resolve();
      return this.pageLoadDeferred.promise;
    }

    this.pageLoadTimeout = setTimeout(this.onPageLoadTimeout, timeout);

    return this.pageLoadDeferred.promise;
  },

  onPageLoadTimeout: function() {
    trace.log("HarCollector.onPageLoadTimeout;");

    // Ha, page has been loaded. Resolve the final timeout promise.
    this.pageLoadDeferred.resolve();
  },

  resetPageLoadTimeout: function() {
    // Remove the current timeout.
    if (this.pageLoadTimeout) {
      trace.log("HarCollector.resetPageLoadTimeout;");

      clearTimeout(this.pageLoadTimeout);
      this.pageLoadTimeout = null;
    }

    // Reject the current page load promise
    if (this.pageLoadDeferred) {
      this.pageLoadDeferred.reject();
      this.pageLoadDeferred = null;
    }
  },

  // Collected Data

  getFile: function(actorId) {
    return this.files.get(actorId);
  },

  getItems: function() {
    return this.items;
  },

  // Event Handlers

  onNetworkEvent: function(type, packet) {
    // Skip events from different console actors.
    if (packet.from != this.webConsoleClient.actor) {
      return;
    }

    trace.log("HarCollector.onNetworkEvent; " + type, packet);

    let { actor, startedDateTime, method, url, isXHR } = packet.eventActor;
    let startTime = Date.parse(startedDateTime);

    if (this.firstRequestStart == -1) {
      this.firstRequestStart = startTime;
    }

    if (this.lastRequestEnd < startTime) {
      this.lastRequestEnd = startTime;
    }

    let file = this.getFile(actor);
    if (file) {
      Cu.reportError("HarCollector.onNetworkEvent; ERROR " +
        "existing file conflict!");
      return;
    }

    file = {
      startedDeltaMillis: startTime - this.firstRequestStart,
      startedMillis: startTime,
      method: method,
      url: url,
      isXHR: isXHR
    };

    this.files.set(actor, file);

    // Mimic the Net panel data structure
    this.items.push({
      attachment: file
    });
  },

  onNetworkEventUpdate: function(type, packet) {
    let actor = packet.from;

    // Skip events from unknown actors (not in the list).
    // There could also be zombie requests received after the target is closed.
    let file = this.getFile(packet.from);
    if (!file) {
      Cu.reportError("HarCollector.onNetworkEventUpdate; ERROR " +
        "Unknown event actor: " + type, packet);
      return;
    }

    trace.log("HarCollector.onNetworkEventUpdate; " +
      packet.updateType, packet);

    let includeResponseBodies = Services.prefs.getBoolPref(
      "devtools.netmonitor.har.includeResponseBodies");

    let request;
    switch (packet.updateType) {
      case "requestHeaders":
        request = this.getData(actor, "getRequestHeaders", this.onRequestHeaders);
        break;
      case "requestCookies":
        request = this.getData(actor, "getRequestCookies", this.onRequestCookies);
        break;
      case "requestPostData":
        request = this.getData(actor, "getRequestPostData", this.onRequestPostData);
        break;
      case "responseHeaders":
        request = this.getData(actor, "getResponseHeaders", this.onResponseHeaders);
        break;
      case "responseCookies":
        request = this.getData(actor, "getResponseCookies", this.onResponseCookies);
        break;
      case "responseStart":
        file.httpVersion = packet.response.httpVersion;
        file.status = packet.response.status;
        file.statusText = packet.response.statusText;
        break;
      case "responseContent":
        file.contentSize = packet.contentSize;
        file.mimeType = packet.mimeType;
        file.transferredSize = packet.transferredSize;

        if (includeResponseBodies) {
          request = this.getData(actor, "getResponseContent", this.onResponseContent);
        }
        break;
      case "eventTimings":
        request = this.getData(actor, "getEventTimings", this.onEventTimings);
        break;
    }

    if (request) {
      this.requests.push(request);
    }

    this.resetPageLoadTimeout();
  },

  getData: function(actor, method, callback) {
    let deferred = defer();

    if (!this.webConsoleClient[method]) {
      Cu.reportError("HarCollector.getData; ERROR " +
        "Unknown method!");
      return;
    }

    let file = this.getFile(actor);

    trace.log("HarCollector.getData; REQUEST " + method +
      ", " + file.url, file);

    this.webConsoleClient[method](actor, response => {
      trace.log("HarCollector.getData; RESPONSE " + method +
        ", " + file.url, response);

      callback(response);
      deferred.resolve(response);
    });

    return deferred.promise;
  },

  /**
   * Handles additional information received for a "requestHeaders" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  onRequestHeaders: function(response) {
    let file = this.getFile(response.from);
    file.requestHeaders = response;

    this.getLongHeaders(response.headers);
  },

  /**
   * Handles additional information received for a "requestCookies" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  onRequestCookies: function(response) {
    let file = this.getFile(response.from);
    file.requestCookies = response;

    this.getLongHeaders(response.cookies);
  },

  /**
   * Handles additional information received for a "requestPostData" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  onRequestPostData: function(response) {
    trace.log("HarCollector.onRequestPostData;", response);

    let file = this.getFile(response.from);
    file.requestPostData = response;

    // Resolve long string
    let text = response.postData.text;
    if (typeof text == "object") {
      this.getString(text).then(value => {
          response.postData.text = value;
      })
    }
  },

  /**
   * Handles additional information received for a "responseHeaders" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  onResponseHeaders: function(response) {
    let file = this.getFile(response.from);
    file.responseHeaders = response;

    this.getLongHeaders(response.headers);
  },

  /**
   * Handles additional information received for a "responseCookies" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  onResponseCookies: function(response) {
    let file = this.getFile(response.from);
    file.responseCookies = response;

    this.getLongHeaders(response.cookies);
  },

  /**
   * Handles additional information received for a "responseContent" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  onResponseContent: function(response) {
    let file = this.getFile(response.from);
    file.mimeType = "text/plain";
    file.responseContent = response;

    // Resolve long string
    let text = response.content.text;
    if (typeof text == "object") {
      this.getString(text).then(value => {
        response.content.text = value;
      })
    }
  },

  /**
   * Handles additional information received for a "eventTimings" packet.
   *
   * @param object response
   *        The message received from the server.
   */
  onEventTimings: function(response) {
    let file = this.getFile(response.from);
    file.eventTimings = response;

    let totalTime = response.totalTime;
    file.totalTime = totalTime;
    file.endedMillis = file.startedMillis + totalTime;
  },

  // Helpers

  getLongHeaders: makeInfallible(function(headers) {
    for (let header of headers) {
      if (typeof header.value == "object") {
        this.getString(header.value).then(value => {
          header.value = value;
        });
      }
    }
  }),

  /**
   * Fetches the full text of a string.
   *
   * @param object | string stringGrip
   *        The long string grip containing the corresponding actor.
   *        If you pass in a plain string (by accident or because you're lazy),
   *        then a promise of the same string is simply returned.
   * @return object Promise
   *         A promise that is resolved when the full string contents
   *         are available, or rejected if something goes wrong.
   */
  getString: function(stringGrip) {
    let promise = this.webConsoleClient.getString(stringGrip);
    this.requests.push(promise);
    return promise;
  }
};

// Helpers

/**
 * Helper function that allows to wait for array of promises. It is
 * possible to dynamically add new promises in the provided array.
 * The function will wait even for the newly added promises.
 * (this isn't possible with the default Promise.all);
 */
function waitForAll(promises) {
  // Remove all from the original array and get clone of it.
  let clone = promises.splice(0, promises.length);

  // Wait for all promises in the given array.
  return all(clone).then(() => {
    // If there are new promises (in the original array)
    // to wait for - chain them!
    if (promises.length) {
      return waitForAll(promises);
    }
  });
}

// Exports from this module
exports.HarCollector = HarCollector;
