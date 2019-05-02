/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cr} = require("chrome");
const Services = require("Services");
const flags = require("devtools/shared/flags");

loader.lazyRequireGetter(this, "NetworkHelper",
  "devtools/shared/webconsole/network-helper");
loader.lazyRequireGetter(this, "DevToolsUtils",
  "devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "NetworkThrottleManager",
  "devtools/shared/webconsole/throttle", true);
loader.lazyServiceGetter(this, "gActivityDistributor",
  "@mozilla.org/network/http-activity-distributor;1",
  "nsIHttpActivityDistributor");
loader.lazyRequireGetter(this, "NetworkResponseListener",
  "devtools/server/actors/network-monitor/network-response-listener", true);

// Network logging

// The maximum uint32 value.
const PR_UINT32_MAX = 4294967295;

// HTTP status codes.
const HTTP_MOVED_PERMANENTLY = 301;
const HTTP_FOUND = 302;
const HTTP_SEE_OTHER = 303;
const HTTP_TEMPORARY_REDIRECT = 307;

/**
 * Check if a given network request should be logged by a network monitor
 * based on the specified filters.
 *
 * @param nsIHttpChannel channel
 *        Request to check.
 * @param filters
 *        NetworkObserver filters to match against.
 * @return boolean
 *         True if the network request should be logged, false otherwise.
 */
function matchRequest(channel, filters) {
  // Log everything if no filter is specified
  if (!filters.outerWindowID && !filters.window) {
    return true;
  }

  // Ignore requests from chrome or add-on code when we are monitoring
  // content.
  // TODO: one particular test (browser_styleeditor_fetch-from-cache.js) needs
  // the flags.testing check. We will move to a better way to serve
  // its needs in bug 1167188, where this check should be removed.
  if (!flags.testing && channel.loadInfo &&
      channel.loadInfo.loadingDocument === null &&
      channel.loadInfo.loadingPrincipal ===
      Services.scriptSecurityManager.getSystemPrincipal()) {
    return false;
  }

  if (filters.window) {
    // Since frames support, this.window may not be the top level content
    // frame, so that we can't only compare with win.top.
    let win = NetworkHelper.getWindowForRequest(channel);
    while (win) {
      if (win == filters.window) {
        return true;
      }
      if (win.parent == win) {
        break;
      }
      win = win.parent;
    }
  }

  if (filters.outerWindowID) {
    const topFrame = NetworkHelper.getTopFrameForRequest(channel);
    // topFrame is typically null for some chrome requests like favicons
    if (topFrame) {
      try {
        if (topFrame.outerWindowID == filters.outerWindowID) {
          return true;
        }
      } catch (e) {
        // outerWindowID getter from browser.js (non-remote <xul:browser>) may
        // throw when closing a tab while resources are still loading.
      }
    }
  }

  return false;
}
exports.matchRequest = matchRequest;

/**
 * The network monitor uses the nsIHttpActivityDistributor to monitor network
 * requests. The nsIObserverService is also used for monitoring
 * http-on-examine-response notifications. All network request information is
 * routed to the remote Web Console.
 *
 * @constructor
 * @param object filters
 *        Object with the filters to use for network requests:
 *        - window (nsIDOMWindow): filter network requests by the associated
 *          window object.
 *        - outerWindowID (number): filter requests by their top frame's outerWindowID.
 *        Filters are optional. If any of these filters match the request is
 *        logged (OR is applied). If no filter is provided then all requests are
 *        logged.
 * @param object owner
 *        The network observer owner. This object needs to hold:
 *        - onNetworkEvent(requestInfo)
 *          This method is invoked once for every new network request and it is
 *          given the initial network request information as an argument.
 *          onNetworkEvent() must return an object which holds several add*()
 *          methods which are used to add further network request/response information.
 */
function NetworkObserver(filters, owner) {
  this.filters = filters;
  this.owner = owner;

  this.openRequests = new Map();
  this.openResponses = new Map();

  this.blockedURLs = new Set();

  this._httpResponseExaminer =
    DevToolsUtils.makeInfallible(this._httpResponseExaminer).bind(this);
  this._httpModifyExaminer =
    DevToolsUtils.makeInfallible(this._httpModifyExaminer).bind(this);
  this._httpFailedOpening =
    DevToolsUtils.makeInfallible(this._httpFailedOpening).bind(this);
  this._serviceWorkerRequest = this._serviceWorkerRequest.bind(this);

  this._throttleData = null;
  this._throttler = null;
}

exports.NetworkObserver = NetworkObserver;

NetworkObserver.prototype = {
  filters: null,

  httpTransactionCodes: {
    0x5001: "REQUEST_HEADER",
    0x5002: "REQUEST_BODY_SENT",
    0x5003: "RESPONSE_START",
    0x5004: "RESPONSE_HEADER",
    0x5005: "RESPONSE_COMPLETE",
    0x5006: "TRANSACTION_CLOSE",

    0x804b0003: "STATUS_RESOLVING",
    0x804b000b: "STATUS_RESOLVED",
    0x804b0007: "STATUS_CONNECTING_TO",
    0x804b0004: "STATUS_CONNECTED_TO",
    0x804b0005: "STATUS_SENDING_TO",
    0x804b000a: "STATUS_WAITING_FOR",
    0x804b0006: "STATUS_RECEIVING_FROM",
    0x804b000c: "STATUS_TLS_STARTING",
    0x804b000d: "STATUS_TLS_ENDING",
  },

  httpDownloadActivities: [
    gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_START,
    gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_HEADER,
    gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_COMPLETE,
    gActivityDistributor.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE,
  ],

  // Network response bodies are piped through a buffer of the given size (in
  // bytes).
  responsePipeSegmentSize: null,

  owner: null,

  /**
   * Whether to save the bodies of network requests and responses.
   * @type boolean
   */
  saveRequestAndResponseBodies: true,

  /**
   * Object that holds the HTTP activity objects for ongoing requests.
   */
  openRequests: null,

  /**
   * Object that holds response headers coming from this._httpResponseExaminer.
   */
  openResponses: null,

  /**
   * The network monitor initializer.
   */
  init: function() {
    this.responsePipeSegmentSize = Services.prefs
                                   .getIntPref("network.buffer.cache.size");
    this.interceptedChannels = new Set();

    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      gActivityDistributor.addObserver(this);
      Services.obs.addObserver(this._httpResponseExaminer,
                               "http-on-examine-response");
      Services.obs.addObserver(this._httpResponseExaminer,
                               "http-on-examine-cached-response");
      Services.obs.addObserver(this._httpModifyExaminer,
                               "http-on-modify-request");
    } else {
      Services.obs.addObserver(this._httpFailedOpening,
                               "http-on-failed-opening-request");
    }
    // In child processes, only watch for service worker requests
    // everything else only happens in the parent process
    Services.obs.addObserver(this._serviceWorkerRequest,
                             "service-worker-synthesized-response");
  },

  get throttleData() {
    return this._throttleData;
  },

  set throttleData(value) {
    this._throttleData = value;
    // Clear out any existing throttlers
    this._throttler = null;
  },

  _getThrottler: function() {
    if (this.throttleData !== null && this._throttler === null) {
      this._throttler = new NetworkThrottleManager(this.throttleData);
    }
    return this._throttler;
  },

  _serviceWorkerRequest: function(subject, topic, data) {
    const channel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (!matchRequest(channel, this.filters)) {
      return;
    }

    this.interceptedChannels.add(subject);

    // Service workers never fire http-on-examine-cached-response, so fake one.
    this._httpResponseExaminer(channel, "http-on-examine-cached-response");
  },

  /**
   * Observes for http-on-failed-opening-request notification to catch any
   * channels for which asyncOpen has synchronously failed.  This is the only
   * place to catch early security check failures.
   */
  _httpFailedOpening: function(subject, topic) {
    if (!this.owner ||
        (topic != "http-on-failed-opening-request") ||
        !(subject instanceof Ci.nsIHttpChannel)) {
      return;
    }

    const channel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (!matchRequest(channel, this.filters)) {
      return;
    }

    "add your handling code here";
  },

  /**
   * Observe notifications for the http-on-examine-response topic, coming from
   * the nsIObserverService.
   *
   * @private
   * @param nsIHttpChannel subject
   * @param string topic
   * @returns void
   */
  _httpResponseExaminer: function(subject, topic) {
    // The httpResponseExaminer is used to retrieve the uncached response
    // headers. The data retrieved is stored in openResponses. The
    // NetworkResponseListener is responsible with updating the httpActivity
    // object with the data from the new object in openResponses.

    if (!this.owner ||
        (topic != "http-on-examine-response" &&
         topic != "http-on-examine-cached-response") ||
        !(subject instanceof Ci.nsIHttpChannel)) {
      return;
    }

    const channel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (!matchRequest(channel, this.filters)) {
      return;
    }

    const response = {
      id: gSequenceId(),
      channel: channel,
      headers: [],
      cookies: [],
    };

    const setCookieHeaders = [];

    channel.visitOriginalResponseHeaders({
      visitHeader: function(name, value) {
        const lowerName = name.toLowerCase();
        if (lowerName == "set-cookie") {
          setCookieHeaders.push(value);
        }
        response.headers.push({ name: name, value: value });
      },
    });

    if (!response.headers.length) {
      // No need to continue.
      return;
    }

    if (setCookieHeaders.length) {
      response.cookies = setCookieHeaders.reduce((result, header) => {
        const cookies = NetworkHelper.parseSetCookieHeader(header);
        return result.concat(cookies);
      }, []);
    }

    // Determine the HTTP version.
    const httpVersionMaj = {};
    const httpVersionMin = {};

    channel.QueryInterface(Ci.nsIHttpChannelInternal);
    channel.getResponseVersion(httpVersionMaj, httpVersionMin);

    response.status = channel.responseStatus;
    response.statusText = channel.responseStatusText;
    response.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                     httpVersionMin.value;

    this.openResponses.set(channel, response);

    if (topic === "http-on-examine-cached-response") {
      // Service worker requests emits cached-response notification on non-e10s,
      // and we fake one on e10s.
      const fromServiceWorker = this.interceptedChannels.has(channel);
      this.interceptedChannels.delete(channel);

      // If this is a cached response, there never was a request event
      // so we need to construct one here so the frontend gets all the
      // expected events.
      const httpActivity = this._createNetworkEvent(channel, {
        fromCache: !fromServiceWorker,
        fromServiceWorker: fromServiceWorker,
      });
      httpActivity.owner.addResponseStart({
        httpVersion: response.httpVersion,
        remoteAddress: "",
        remotePort: "",
        status: response.status,
        statusText: response.statusText,
        headersSize: 0,
      }, "", true);

      // There also is never any timing events, so we can fire this
      // event with zeroed out values.
      const timings = this._setupHarTimings(httpActivity, true);
      httpActivity.owner.addEventTimings(timings.total, timings.timings,
                                         timings.offsets);
    }
  },

  /**
   * Observe notifications for the http-on-modify-request topic, coming from
   * the nsIObserverService.
   *
   * @private
   * @param nsIHttpChannel aSubject
   * @returns void
   */
  _httpModifyExaminer: function(subject) {
    const throttler = this._getThrottler();
    if (throttler) {
      const channel = subject.QueryInterface(Ci.nsIHttpChannel);
      if (matchRequest(channel, this.filters)) {
        // Read any request body here, before it is throttled.
        const httpActivity = this.createOrGetActivityObject(channel);
        this._onRequestBodySent(httpActivity);
        throttler.manageUpload(channel);
      }
    }
  },

  /**
   * A helper function for observeActivity.  This does whatever work
   * is required by a particular http activity event.  Arguments are
   * the same as for observeActivity.
   */
  _dispatchActivity: function(httpActivity, channel, activityType,
                               activitySubtype, timestamp, extraSizeData,
                               extraStringData) {
    const transCodes = this.httpTransactionCodes;

    // Store the time information for this activity subtype.
    if (activitySubtype in transCodes) {
      const stage = transCodes[activitySubtype];
      if (stage in httpActivity.timings) {
        httpActivity.timings[stage].last = timestamp;
      } else {
        httpActivity.timings[stage] = {
          first: timestamp,
          last: timestamp,
        };
      }
    }

    switch (activitySubtype) {
      case gActivityDistributor.ACTIVITY_SUBTYPE_REQUEST_BODY_SENT:
        this._onRequestBodySent(httpActivity);
        if (httpActivity.sentBody !== null) {
          const limit = Services.prefs.getIntPref("devtools.netmonitor.requestBodyLimit");
          const size = httpActivity.sentBody.length;
          if (size > limit && limit > 0) {
            httpActivity.sentBody = httpActivity.sentBody.substr(0, limit);
          }
          httpActivity.owner.addRequestPostData({
            text: httpActivity.sentBody,
            size: size,
          });
          httpActivity.sentBody = null;
        }
        break;
      case gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_HEADER:
        this._onResponseHeader(httpActivity, extraStringData);
        break;
      case gActivityDistributor.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE:
        this._onTransactionClose(httpActivity);
        break;
      default:
        break;
    }
  },

  /**
   * Begin observing HTTP traffic that originates inside the current tab.
   *
   * @see https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIHttpActivityObserver
   *
   * @param nsIHttpChannel channel
   * @param number activityType
   * @param number activitySubtype
   * @param number timestamp
   * @param number extraSizeData
   * @param string extraStringData
   */
  observeActivity:
  DevToolsUtils.makeInfallible(function(channel, activityType, activitySubtype,
                                        timestamp, extraSizeData,
                                        extraStringData) {
    if (!this.owner ||
        activityType != gActivityDistributor.ACTIVITY_TYPE_HTTP_TRANSACTION &&
        activityType != gActivityDistributor.ACTIVITY_TYPE_SOCKET_TRANSPORT) {
      return;
    }

    if (!(channel instanceof Ci.nsIHttpChannel)) {
      return;
    }

    channel = channel.QueryInterface(Ci.nsIHttpChannel);

    if (activitySubtype ==
        gActivityDistributor.ACTIVITY_SUBTYPE_REQUEST_HEADER) {
      this._onRequestHeader(channel, timestamp, extraStringData);
      return;
    }

    // Iterate over all currently ongoing requests. If channel can't
    // be found within them, then exit this function.
    const httpActivity = this._findActivityObject(channel);
    if (!httpActivity) {
      return;
    }

    // If we're throttling, we must not report events as they arrive
    // from platform, but instead let the throttler emit the events
    // after some time has elapsed.
    if (httpActivity.downloadThrottle &&
        this.httpDownloadActivities.includes(activitySubtype)) {
      const callback = this._dispatchActivity.bind(this);
      httpActivity.downloadThrottle
        .addActivityCallback(callback, httpActivity, channel, activityType,
                             activitySubtype, timestamp, extraSizeData,
                             extraStringData);
    } else {
      this._dispatchActivity(httpActivity, channel, activityType,
                             activitySubtype, timestamp, extraSizeData,
                             extraStringData);
    }
  }),

  /**
   *
   */
  _createNetworkEvent: function(channel, { timestamp, extraStringData,
                                           fromCache, fromServiceWorker }) {
    const httpActivity = this.createOrGetActivityObject(channel);

    channel.QueryInterface(Ci.nsIPrivateBrowsingChannel);
    httpActivity.private = channel.isChannelPrivate;

    if (timestamp) {
      httpActivity.timings.REQUEST_HEADER = {
        first: timestamp,
        last: timestamp,
      };
    }

    const event = {};
    event.method = channel.requestMethod;
    event.channelId = channel.channelId;
    event.url = channel.URI.spec;
    event.private = httpActivity.private;
    event.headersSize = 0;
    event.startedDateTime =
      (timestamp ? new Date(Math.round(timestamp / 1000)) : new Date())
      .toISOString();
    event.fromCache = fromCache;
    event.fromServiceWorker = fromServiceWorker;
    event.isThirdPartyTrackingResource = channel.isThirdPartyTrackingResource();
    const referrerInfo = channel.referrerInfo;
    event.referrerPolicy =
      Services.netUtils.getReferrerPolicyString(referrerInfo ?
          referrerInfo.referrerPolicy : Ci.nsIHttpChannel.REFERRER_POLICY_UNSET);
    httpActivity.fromServiceWorker = fromServiceWorker;

    if (extraStringData) {
      event.headersSize = extraStringData.length;
    }

    // Determine the cause and if this is an XHR request.
    let causeType = Ci.nsIContentPolicy.TYPE_OTHER;
    let causeUri = null;
    let stacktrace;

    if (channel.loadInfo) {
      causeType = channel.loadInfo.externalContentPolicyType;
      const { loadingPrincipal } = channel.loadInfo;
      if (loadingPrincipal && loadingPrincipal.URI) {
        causeUri = loadingPrincipal.URI.spec;
      }
    }

    event.cause = {
      type: causeTypeToString(causeType),
      loadingDocumentUri: causeUri,
      stacktrace,
    };

    httpActivity.isXHR = event.isXHR =
        (causeType === Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST ||
         causeType === Ci.nsIContentPolicy.TYPE_FETCH);

    // Determine the HTTP version.
    const httpVersionMaj = {};
    const httpVersionMin = {};
    channel.QueryInterface(Ci.nsIHttpChannelInternal);
    channel.getRequestVersion(httpVersionMaj, httpVersionMin);

    event.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                  httpVersionMin.value;

    event.discardRequestBody = !this.saveRequestAndResponseBodies;
    event.discardResponseBody = !this.saveRequestAndResponseBodies;

    const headers = [];
    let cookies = [];
    let cookieHeader = null;

    // Copy the request header data.
    channel.visitRequestHeaders({
      visitHeader: function(name, value) {
        if (name == "Cookie") {
          cookieHeader = value;
        }
        headers.push({ name: name, value: value });
      },
    });

    if (cookieHeader) {
      cookies = NetworkHelper.parseCookieHeader(cookieHeader);
    }

    // Check the request URL with ones manually blocked by the user in DevTools.
    // If it's meant to be blocked, we cancel the request and annotate the event.
    if (this.blockedURLs.has(httpActivity.url)) {
      channel.cancel(Cr.NS_BINDING_ABORTED);
      event.blockedReason = "DevTools";
    }

    httpActivity.owner = this.owner.onNetworkEvent(event);

    if (!event.blockedReason) {
      this._setupResponseListener(httpActivity, fromCache);
    }

    httpActivity.owner.addRequestHeaders(headers, extraStringData);
    httpActivity.owner.addRequestCookies(cookies);

    return httpActivity;
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_REQUEST_HEADER. When a request starts the
   * headers are sent to the server. This method creates the |httpActivity|
   * object where we store the request and response information that is
   * collected through its lifetime.
   *
   * @private
   * @param nsIHttpChannel channel
   * @param number timestamp
   * @param string extraStringData
   * @return void
   */
  _onRequestHeader: function(channel, timestamp, extraStringData) {
    if (!matchRequest(channel, this.filters)) {
      return;
    }

    this._createNetworkEvent(channel, { timestamp, extraStringData });
  },

  /**
   * Find an HTTP activity object for the channel.
   *
   * @param nsIHttpChannel channel
   *        The HTTP channel whose activity object we want to find.
   * @return object
   *        The HTTP activity object, or null if it is not found.
   */
  _findActivityObject: function(channel) {
    return this.openRequests.get(channel) || null;
  },

  /**
   * Find an existing HTTP activity object, or create a new one. This
   * object is used for storing all the request and response
   * information.
   *
   * This is a HAR-like object. Conformance to the spec is not guaranteed at
   * this point.
   *
   * @see http://www.softwareishard.com/blog/har-12-spec
   * @param nsIHttpChannel channel
   *        The HTTP channel for which the HTTP activity object is created.
   * @return object
   *         The new HTTP activity object.
   */
  createOrGetActivityObject: function(channel) {
    let httpActivity = this._findActivityObject(channel);
    if (!httpActivity) {
      const win = NetworkHelper.getWindowForRequest(channel);
      const charset = win ? win.document.characterSet : null;

      httpActivity = {
        id: gSequenceId(),
        channel: channel,
        // see _onRequestBodySent()
        charset: charset,
        sentBody: null,
        url: channel.URI.spec,
        headersSize: null,
        // needed for host specific security info
        hostname: channel.URI.host,
        discardRequestBody: !this.saveRequestAndResponseBodies,
        discardResponseBody: !this.saveRequestAndResponseBodies,
        // internal timing information, see observeActivity()
        timings: {},
        // see _onResponseHeader()
        responseStatus: null,
        // the activity owner which is notified when changes happen
        owner: null,
      };

      this.openRequests.set(channel, httpActivity);
    }

    return httpActivity;
  },

  /**
   * Block a request based on certain filtering options.
   *
   * Currently, an exact URL match is the only supported filter type.
   */
  blockRequest(filter) {
    if (!filter || !filter.url) {
      // In the future, there may be other types of filters, such as domain.
      // For now, ignore anything other than URL.
      return;
    }

    this.blockedURLs.add(filter.url);
  },

  /**
   * Unblock a request based on certain filtering options.
   *
   * Currently, an exact URL match is the only supported filter type.
   */
  unblockRequest(filter) {
    if (!filter || !filter.url) {
      // In the future, there may be other types of filters, such as domain.
      // For now, ignore anything other than URL.
      return;
    }

    this.blockedURLs.delete(filter.url);
  },

  /**
   * Setup the network response listener for the given HTTP activity. The
   * NetworkResponseListener is responsible for storing the response body.
   *
   * @private
   * @param object httpActivity
   *        The HTTP activity object we are tracking.
   */
  _setupResponseListener: function(httpActivity, fromCache) {
    const channel = httpActivity.channel;
    channel.QueryInterface(Ci.nsITraceableChannel);

    if (!fromCache) {
      const throttler = this._getThrottler();
      if (throttler) {
        httpActivity.downloadThrottle = throttler.manage(channel);
      }
    }

    // The response will be written into the outputStream of this pipe.
    // This allows us to buffer the data we are receiving and read it
    // asynchronously.
    // Both ends of the pipe must be blocking.
    const sink = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);

    // The streams need to be blocking because this is required by the
    // stream tee.
    sink.init(false, false, this.responsePipeSegmentSize, PR_UINT32_MAX, null);

    // Add listener for the response body.
    const newListener = new NetworkResponseListener(this, httpActivity);

    // Remember the input stream, so it isn't released by GC.
    newListener.inputStream = sink.inputStream;
    newListener.sink = sink;

    const tee = Cc["@mozilla.org/network/stream-listener-tee;1"]
              .createInstance(Ci.nsIStreamListenerTee);

    const originalListener = channel.setNewListener(tee);

    tee.init(originalListener, sink.outputStream, newListener);
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_REQUEST_BODY_SENT. The request body is logged
   * here.
   *
   * @private
   * @param object httpActivity
   *        The HTTP activity object we are working with.
   */
  _onRequestBodySent: function(httpActivity) {
    // Return early if we don't need the request body, or if we've
    // already found it.
    if (httpActivity.discardRequestBody || httpActivity.sentBody !== null) {
      return;
    }

    let sentBody = NetworkHelper.readPostTextFromRequest(httpActivity.channel,
                                                         httpActivity.charset);

    if (sentBody !== null && this.window &&
        httpActivity.url == this.window.location.href) {
      // If the request URL is the same as the current page URL, then
      // we can try to get the posted text from the page directly.
      // This check is necessary as otherwise the
      //   NetworkHelper.readPostTextFromPageViaWebNav()
      // function is called for image requests as well but these
      // are not web pages and as such don't store the posted text
      // in the cache of the webpage.
      const webNav = this.window.docShell.QueryInterface(Ci.nsIWebNavigation);
      sentBody = NetworkHelper
                 .readPostTextFromPageViaWebNav(webNav, httpActivity.charset);
    }

    if (sentBody !== null) {
      httpActivity.sentBody = sentBody;
    }
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_RESPONSE_HEADER. This method stores
   * information about the response headers.
   *
   * @private
   * @param object httpActivity
   *        The HTTP activity object we are working with.
   * @param string extraStringData
   *        The uncached response headers.
   */
  _onResponseHeader: function(httpActivity, extraStringData) {
    // extraStringData contains the uncached response headers. The first line
    // contains the response status (e.g. HTTP/1.1 200 OK).
    //
    // Note: The response header is not saved here. Calling the
    // channel.visitResponseHeaders() method at this point sometimes causes an
    // NS_ERROR_NOT_AVAILABLE exception.
    //
    // We could parse extraStringData to get the headers and their values, but
    // that is not trivial to do in an accurate manner. Hence, we save the
    // response headers in this._httpResponseExaminer().

    const headers = extraStringData.split(/\r\n|\n|\r/);
    const statusLine = headers.shift();
    const statusLineArray = statusLine.split(" ");

    const response = {};
    response.httpVersion = statusLineArray.shift();
    response.remoteAddress = httpActivity.channel.remoteAddress;
    response.remotePort = httpActivity.channel.remotePort;
    response.status = statusLineArray.shift();
    response.statusText = statusLineArray.join(" ");
    response.headersSize = extraStringData.length;

    httpActivity.responseStatus = response.status;
    httpActivity.headersSize = response.headersSize;

    // Discard the response body for known response statuses.
    switch (parseInt(response.status, 10)) {
      case HTTP_MOVED_PERMANENTLY:
      case HTTP_FOUND:
      case HTTP_SEE_OTHER:
      case HTTP_TEMPORARY_REDIRECT:
        httpActivity.discardResponseBody = true;
        break;
    }

    response.discardResponseBody = httpActivity.discardResponseBody;

    httpActivity.owner.addResponseStart(response, extraStringData);
  },

  /**
   * Handler for ACTIVITY_SUBTYPE_TRANSACTION_CLOSE. This method updates the HAR
   * timing information on the HTTP activity object and clears the request
   * from the list of known open requests.
   *
   * @private
   * @param object httpActivity
   *        The HTTP activity object we work with.
   */
  _onTransactionClose: function(httpActivity) {
    const result = this._setupHarTimings(httpActivity);
    httpActivity.owner.addEventTimings(result.total, result.timings,
                                       result.offsets);
    this.openRequests.delete(httpActivity.channel);
  },

  /**
   * Update the HTTP activity object to include timing information as in the HAR
   * spec. The HTTP activity object holds the raw timing information in
   * |timings| - these are timings stored for each activity notification. The
   * HAR timing information is constructed based on these lower level
   * data.
   *
   * @param object httpActivity
   *        The HTTP activity object we are working with.
   * @param boolean fromCache
   *        Indicates that the result was returned from the browser cache
   * @return object
   *         This object holds two properties:
   *         - total - the total time for all of the request and response.
   *         - timings - the HAR timings object.
   */
  _setupHarTimings: function(httpActivity, fromCache) {
    if (fromCache) {
      // If it came from the browser cache, we have no timing
      // information and these should all be 0
      return {
        total: 0,
        timings: {
          blocked: 0,
          dns: 0,
          ssl: 0,
          connect: 0,
          send: 0,
          wait: 0,
          receive: 0,
        },
        offsets: {
          blocked: 0,
          dns: 0,
          ssl: 0,
          connect: 0,
          send: 0,
          wait: 0,
          receive: 0,
        },
      };
    }

    const timings = httpActivity.timings;
    const harTimings = {};
    // If the TCP Fast Open option or tls1.3 0RTT is used tls and data can
    // be dispatched in SYN packet and not after tcp socket is connected.
    // To demostrate this properly we will calculated TLS and send start time
    // relative to CONNECTING_TO.
    // Similary if 0RTT is used, data can be sent as soon as a TLS handshake
    // starts.
    let secureConnectionStartTime = 0;
    let secureConnectionStartTimeRelative = false;
    let startSendingTime = 0;
    let startSendingTimeRelative = false;

    if (timings.STATUS_RESOLVING && timings.STATUS_CONNECTING_TO) {
      harTimings.blocked = timings.STATUS_RESOLVING.first -
                           timings.REQUEST_HEADER.first;
    } else if (timings.STATUS_SENDING_TO) {
      harTimings.blocked = timings.STATUS_SENDING_TO.first -
                           timings.REQUEST_HEADER.first;
    } else {
      harTimings.blocked = -1;
    }

    // DNS timing information is available only in when the DNS record is not
    // cached.
    harTimings.dns = timings.STATUS_RESOLVING && timings.STATUS_RESOLVED ?
                     timings.STATUS_RESOLVED.last -
                     timings.STATUS_RESOLVING.first : -1;

    if (timings.STATUS_CONNECTING_TO && timings.STATUS_CONNECTED_TO) {
      harTimings.connect = timings.STATUS_CONNECTED_TO.last -
                           timings.STATUS_CONNECTING_TO.first;
    } else {
      harTimings.connect = -1;
    }

    if (timings.STATUS_TLS_STARTING && timings.STATUS_TLS_ENDING) {
      harTimings.ssl = timings.STATUS_TLS_ENDING.last -
                           timings.STATUS_TLS_STARTING.first;
      if (timings.STATUS_CONNECTING_TO) {
        secureConnectionStartTime =
          timings.STATUS_TLS_STARTING.first - timings.STATUS_CONNECTING_TO.first;
      }
      if (secureConnectionStartTime < 0) {
        secureConnectionStartTime = 0;
      }
      secureConnectionStartTimeRelative = true;
    } else {
      harTimings.ssl = -1;
    }

    // sometimes the connection information events are attached to a speculative
    // channel instead of this one, but necko might glue them back together in the
    // nsITimedChannel interface used by Resource and Navigation Timing
    const timedChannel = httpActivity.channel.QueryInterface(Ci.nsITimedChannel);

    let tcTcpConnectEndTime = 0;
    let tcConnectStartTime = 0;
    let tcConnectEndTime = 0;
    let tcSecureConnectionStartTime = 0;
    let tcDomainLookupEndTime = 0;
    let tcDomainLookupStartTime = 0;

    if (timedChannel) {
      tcTcpConnectEndTime = timedChannel.tcpConnectEndTime;
      tcConnectStartTime = timedChannel.connectStartTime;
      tcConnectEndTime = timedChannel.connectEndTime;
      tcSecureConnectionStartTime = timedChannel.secureConnectionStartTime;
      tcDomainLookupEndTime = timedChannel.domainLookupEndTime;
      tcDomainLookupStartTime = timedChannel.domainLookupStartTime;
    }

    // Make sure the above values are at least timedChannel.asyncOpenTime.
    if (timedChannel && timedChannel.asyncOpenTime) {
      if ((tcTcpConnectEndTime != 0) &&
          (tcTcpConnectEndTime < timedChannel.asyncOpenTime)) {
        tcTcpConnectEndTime = 0;
      }
      if ((tcConnectStartTime != 0) &&
          (tcConnectStartTime < timedChannel.asyncOpenTime)) {
        tcConnectStartTime = 0;
      }
      if ((tcConnectEndTime != 0) &&
          (tcConnectEndTime < timedChannel.asyncOpenTime)) {
        tcConnectEndTime = 0;
      }
      if ((tcSecureConnectionStartTime != 0) &&
          (tcSecureConnectionStartTime < timedChannel.asyncOpenTime)) {
        tcSecureConnectionStartTime = 0;
      }
      if ((tcDomainLookupEndTime != 0) &&
          (tcDomainLookupEndTime < timedChannel.asyncOpenTime)) {
        tcDomainLookupEndTime = 0;
      }
      if ((tcDomainLookupStartTime != 0) &&
          (tcDomainLookupStartTime < timedChannel.asyncOpenTime)) {
        tcDomainLookupStartTime = 0;
      }
    }

    if ((harTimings.connect <= 0) && timedChannel &&
        (tcTcpConnectEndTime != 0) && (tcConnectStartTime != 0)) {
      harTimings.connect = tcTcpConnectEndTime - tcConnectStartTime;
      if (tcSecureConnectionStartTime != 0) {
        harTimings.ssl = tcConnectEndTime - tcSecureConnectionStartTime;
        secureConnectionStartTime =
          tcSecureConnectionStartTime - tcConnectStartTime;
        secureConnectionStartTimeRelative = true;
      } else {
        harTimings.ssl = -1;
      }
    } else if (timedChannel && timings.STATUS_TLS_STARTING &&
               (tcSecureConnectionStartTime != 0)) {
      // It can happen that TCP Fast Open actually have not sent any data and
      // timings.STATUS_TLS_STARTING.first value will be corrected in
      // timedChannel.secureConnectionStartTime
      if (tcSecureConnectionStartTime > timings.STATUS_TLS_STARTING.first) {
        // TCP Fast Open actually did not sent any data.
        harTimings.ssl =
          tcConnectEndTime - tcSecureConnectionStartTime;
        secureConnectionStartTimeRelative = false;
      }
    }

    if ((harTimings.dns <= 0) && timedChannel &&
        (tcDomainLookupEndTime != 0) && (tcDomainLookupStartTime != 0)) {
      harTimings.dns = tcDomainLookupEndTime - tcDomainLookupStartTime;
    }

    if (timings.STATUS_SENDING_TO) {
      harTimings.send =
        timings.STATUS_SENDING_TO.last - timings.STATUS_SENDING_TO.first;
      if (timings.STATUS_CONNECTING_TO) {
        startSendingTime =
          timings.STATUS_SENDING_TO.first - timings.STATUS_CONNECTING_TO.first;
        startSendingTimeRelative = true;
      } else if (tcConnectStartTime != 0) {
        startSendingTime = timings.STATUS_SENDING_TO.first - tcConnectStartTime;
        startSendingTimeRelative = true;
      }
      if (startSendingTime < 0) {
        startSendingTime = 0;
      }
    } else if (timings.REQUEST_HEADER && timings.REQUEST_BODY_SENT) {
      harTimings.send = timings.REQUEST_BODY_SENT.last - timings.REQUEST_HEADER.first;
    } else {
      harTimings.send = -1;
    }

    if (timings.RESPONSE_START) {
      harTimings.wait = timings.RESPONSE_START.first -
                        (timings.REQUEST_BODY_SENT ||
                         timings.STATUS_SENDING_TO).last;
    } else {
      harTimings.wait = -1;
    }

    if (timings.RESPONSE_START && timings.RESPONSE_COMPLETE) {
      harTimings.receive = timings.RESPONSE_COMPLETE.last -
                           timings.RESPONSE_START.first;
    } else {
      harTimings.receive = -1;
    }

    if (secureConnectionStartTimeRelative) {
      const time = Math.max(Math.round(secureConnectionStartTime / 1000), -1);
      secureConnectionStartTime = time;
    }
    if (startSendingTimeRelative) {
      const time = Math.max(Math.round(startSendingTime / 1000), -1);
      startSendingTime = time;
    }

    const ot = this._calculateOffsetAndTotalTime(harTimings,
                                               secureConnectionStartTime,
                                               startSendingTimeRelative,
                                               secureConnectionStartTimeRelative,
                                               startSendingTime);
    return {
      total: ot.total,
      timings: harTimings,
      offsets: ot.offsets,
    };
  },

  _calculateOffsetAndTotalTime: function(harTimings,
                                          secureConnectionStartTime,
                                          startSendingTimeRelative,
                                          secureConnectionStartTimeRelative,
                                          startSendingTime) {
    let totalTime = 0;
    for (const timing in harTimings) {
      const time = Math.max(Math.round(harTimings[timing] / 1000), -1);
      harTimings[timing] = time;
      if ((time > -1) && (timing != "connect") && (timing != "ssl")) {
        totalTime += time;
      }
    }

    // connect, ssl and send times can be overlapped.
    if (startSendingTimeRelative) {
      totalTime += startSendingTime;
    } else if (secureConnectionStartTimeRelative) {
      totalTime += secureConnectionStartTime;
      totalTime += harTimings.ssl;
    }

    const offsets = {};
    offsets.blocked = 0;
    offsets.dns = harTimings.blocked;
    offsets.connect = offsets.dns + harTimings.dns;
    if (secureConnectionStartTimeRelative) {
      offsets.ssl = offsets.connect + secureConnectionStartTime;
    } else {
      offsets.ssl = offsets.connect + harTimings.connect;
    }
    if (startSendingTimeRelative) {
      offsets.send = offsets.connect + startSendingTime;
      if (!secureConnectionStartTimeRelative) {
        offsets.ssl = offsets.send - harTimings.ssl;
      }
    } else {
      offsets.send = offsets.ssl + harTimings.ssl;
    }
    offsets.wait = offsets.send + harTimings.send;
    offsets.receive = offsets.wait + harTimings.wait;

    return {
      total: totalTime,
      offsets: offsets,
    };
  },

  /**
   * Suspend observer activity. This is called when the Network monitor actor stops
   * listening.
   */
  destroy: function() {
    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      gActivityDistributor.removeObserver(this);
      Services.obs.removeObserver(this._httpResponseExaminer,
                                  "http-on-examine-response");
      Services.obs.removeObserver(this._httpResponseExaminer,
                                  "http-on-examine-cached-response");
      Services.obs.removeObserver(this._httpModifyExaminer,
                                  "http-on-modify-request");
    } else {
      Services.obs.removeObserver(this._httpFailedOpening,
                                  "http-on-failed-opening-request");
    }

    Services.obs.removeObserver(this._serviceWorkerRequest,
                                "service-worker-synthesized-response");

    this.interceptedChannels.clear();
    this.openRequests.clear();
    this.openResponses.clear();
    this.owner = null;
    this.filters = null;
    this._throttler = null;
  },
};

function gSequenceId() {
  return gSequenceId.n++;
}
gSequenceId.n = 1;

/**
 * Convert a nsIContentPolicy constant to a display string
 */
const LOAD_CAUSE_STRINGS = {
  [Ci.nsIContentPolicy.TYPE_INVALID]: "invalid",
  [Ci.nsIContentPolicy.TYPE_OTHER]: "other",
  [Ci.nsIContentPolicy.TYPE_SCRIPT]: "script",
  [Ci.nsIContentPolicy.TYPE_IMAGE]: "img",
  [Ci.nsIContentPolicy.TYPE_STYLESHEET]: "stylesheet",
  [Ci.nsIContentPolicy.TYPE_OBJECT]: "object",
  [Ci.nsIContentPolicy.TYPE_DOCUMENT]: "document",
  [Ci.nsIContentPolicy.TYPE_SUBDOCUMENT]: "subdocument",
  [Ci.nsIContentPolicy.TYPE_REFRESH]: "refresh",
  [Ci.nsIContentPolicy.TYPE_XBL]: "xbl",
  [Ci.nsIContentPolicy.TYPE_PING]: "ping",
  [Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST]: "xhr",
  [Ci.nsIContentPolicy.TYPE_OBJECT_SUBREQUEST]: "objectSubdoc",
  [Ci.nsIContentPolicy.TYPE_DTD]: "dtd",
  [Ci.nsIContentPolicy.TYPE_FONT]: "font",
  [Ci.nsIContentPolicy.TYPE_MEDIA]: "media",
  [Ci.nsIContentPolicy.TYPE_WEBSOCKET]: "websocket",
  [Ci.nsIContentPolicy.TYPE_CSP_REPORT]: "csp",
  [Ci.nsIContentPolicy.TYPE_XSLT]: "xslt",
  [Ci.nsIContentPolicy.TYPE_BEACON]: "beacon",
  [Ci.nsIContentPolicy.TYPE_FETCH]: "fetch",
  [Ci.nsIContentPolicy.TYPE_IMAGESET]: "imageset",
  [Ci.nsIContentPolicy.TYPE_WEB_MANIFEST]: "webManifest",
};

function causeTypeToString(causeType) {
  return LOAD_CAUSE_STRINGS[causeType] || "unknown";
}

function stringToCauseType(value) {
  return Object.keys(LOAD_CAUSE_STRINGS)
  .find(key => LOAD_CAUSE_STRINGS[key] === value);
}
exports.stringToCauseType = stringToCauseType;
