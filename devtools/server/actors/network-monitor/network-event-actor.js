/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  networkEventSpec,
} = require("resource://devtools/shared/specs/network-event.js");

const {
  TYPES: { NETWORK_EVENT },
} = require("resource://devtools/server/actors/resources/index.js");
const {
  LongStringActor,
} = require("resource://devtools/server/actors/string.js");

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NetworkUtils:
    "resource://devtools/shared/network-observer/NetworkUtils.sys.mjs",
});

const CONTENT_TYPE_REGEXP = /^content-type/i;

/**
 * Creates an actor for a network event.
 *
 * @constructor
 * @param {DevToolsServerConnection} conn
 *        The connection into which this Actor will be added.
 * @param {Object} sessionContext
 *        The Session Context to help know what is debugged.
 *        See devtools/server/actors/watcher/session-context.js
 * @param {Object} options
 *        Dictionary object with the following attributes:
 *        - onNetworkEventUpdate: optional function
 *          Callback for updates for the network event
 *        - onNetworkEventDestroy: optional function
 *          Callback for the destruction of the network event
 * @param {Object} networkEventOptions
 *        Object describing the network event or the configuration of the
 *        network observer, and which cannot be easily inferred from the raw
 *        channel.
 *        - blockingExtension: optional string
 *          id of the blocking webextension if any
 *        - blockedReason: optional number or string
 *        - discardRequestBody: boolean
 *        - discardResponseBody: boolean
 *        - fromCache: boolean
 *        - fromServiceWorker: boolean
 *        - rawHeaders: string
 *        - timestamp: number
 * @param {nsIChannel} channel
 *        The channel related to this network event
 */
class NetworkEventActor extends Actor {
  constructor(
    conn,
    sessionContext,
    { onNetworkEventUpdate, onNetworkEventDestroy },
    networkEventOptions,
    channel
  ) {
    super(conn, networkEventSpec);

    this._sessionContext = sessionContext;
    this._onNetworkEventUpdate = onNetworkEventUpdate;
    this._onNetworkEventDestroy = onNetworkEventDestroy;

    // Store the channelId which will act as resource id.
    this._channelId = channel.channelId;

    // innerWindowId and isNavigationRequest are used to check if the actor
    // should be destroyed when a window is destroyed. See network-events.js.
    this._innerWindowId = lazy.NetworkUtils.getChannelInnerWindowId(channel);
    this._isNavigationRequest = lazy.NetworkUtils.isNavigationRequest(channel);

    // Retrieve cookies and headers from the channel
    const { cookies, headers } =
      lazy.NetworkUtils.fetchRequestHeadersAndCookies(channel);

    this._request = {
      cookies,
      headers,
      postData: {},
      rawHeaders: networkEventOptions.rawHeaders,
    };

    this._response = {
      headers: [],
      cookies: [],
      content: {},
    };

    this._timings = {};
    this._serverTimings = [];

    this._discardRequestBody = !!networkEventOptions.discardRequestBody;
    this._discardResponseBody = !!networkEventOptions.discardResponseBody;

    this._resource = this._createResource(networkEventOptions, channel);
  }

  /**
   * Return the network event actor as a resource, and add the actorID which is
   * not available in the constructor yet.
   */
  asResource() {
    return {
      actor: this.actorID,
      ...this._resource,
    };
  }

  /**
   * Create the resource corresponding to this actor.
   */
  _createResource(networkEventOptions, channel) {
    channel = channel.QueryInterface(Ci.nsIHttpChannel);
    const wsChannel = lazy.NetworkUtils.getWebSocketChannel(channel);

    // Use the WebSocket channel URL for websockets.
    const url = wsChannel ? wsChannel.URI.spec : channel.URI.spec;

    let browsingContextID =
      lazy.NetworkUtils.getChannelBrowsingContextID(channel);

    // Ensure that we have a browsing context ID for all requests.
    // Only privileged requests debugged via the Browser Toolbox (sessionContext.type == "all") can be unrelated to any browsing context.
    if (!browsingContextID && this._sessionContext.type != "all") {
      throw new Error(`Got a request ${url} without a browsingContextID set`);
    }

    // The browsingContextID is used by the ResourceCommand on the client
    // to find the related Target Front.
    //
    // For now in the browser and web extension toolboxes, requests
    // do not relate to any specific WindowGlobalTargetActor
    // as we are still using a unique target (ParentProcessTargetActor) for everything.
    if (
      this._sessionContext.type == "all" ||
      this._sessionContext.type == "webextension"
    ) {
      browsingContextID = -1;
    }

    const cause = lazy.NetworkUtils.getCauseDetails(channel);
    // Both xhr and fetch are flagged as XHR in DevTools.
    const isXHR = cause.type == "xhr" || cause.type == "fetch";

    // For websocket requests the serial is used instead of the channel id.
    const stacktraceResourceId =
      cause.type == "websocket" ? wsChannel.serial : channel.channelId;

    // If a timestamp was provided, it is a high resolution timestamp
    // corresponding to ACTIVITY_SUBTYPE_REQUEST_HEADER. Fallback to Date.now().
    const timeStamp = networkEventOptions.timestamp
      ? networkEventOptions.timestamp / 1000
      : Date.now();

    let blockedReason = networkEventOptions.blockedReason;

    // Check if blockedReason was set to a falsy value, meaning the blocked did
    // not give an explicit blocked reason.
    if (
      blockedReason === 0 ||
      blockedReason === false ||
      blockedReason === null ||
      blockedReason === ""
    ) {
      blockedReason = "unknown";
    }

    const resource = {
      resourceId: channel.channelId,
      resourceType: NETWORK_EVENT,
      blockedReason,
      blockingExtension: networkEventOptions.blockingExtension,
      browsingContextID,
      cause,
      // This is used specifically in the browser toolbox console to distinguish privileged
      // resources from the parent process from those from the contet
      chromeContext: lazy.NetworkUtils.isChannelFromSystemPrincipal(channel),
      fromCache: networkEventOptions.fromCache,
      fromServiceWorker: networkEventOptions.fromServiceWorker,
      innerWindowId: this._innerWindowId,
      isNavigationRequest: this._isNavigationRequest,
      isThirdPartyTrackingResource:
        lazy.NetworkUtils.isThirdPartyTrackingResource(channel),
      isXHR,
      method: channel.requestMethod,
      priority: lazy.NetworkUtils.getChannelPriority(channel),
      private: lazy.NetworkUtils.isChannelPrivate(channel),
      referrerPolicy: lazy.NetworkUtils.getReferrerPolicy(channel),
      stacktraceResourceId,
      startedDateTime: new Date(timeStamp).toISOString(),
      timeStamp,
      timings: {},
      url,
    };

    return resource;
  }

  /**
   * Releases this actor from the pool.
   */
  destroy(conn) {
    if (!this._channelId) {
      return;
    }

    if (this._onNetworkEventDestroy) {
      this._onNetworkEventDestroy(this._channelId);
    }

    this._channelId = null;
    super.destroy(conn);
  }

  release() {
    // Per spec, destroy is automatically going to be called after this request
  }

  getInnerWindowId() {
    return this._innerWindowId;
  }

  isNavigationRequest() {
    return this._isNavigationRequest;
  }

  /**
   * The "getRequestHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network request headers.
   */
  getRequestHeaders() {
    let rawHeaders;
    let headersSize = 0;
    if (this._request.rawHeaders) {
      headersSize = this._request.rawHeaders.length;
      rawHeaders = this._createLongStringActor(this._request.rawHeaders);
    }

    return {
      headers: this._request.headers.map(header => ({
        name: header.name,
        value: this._createLongStringActor(header.value),
      })),
      headersSize,
      rawHeaders,
    };
  }

  /**
   * The "getRequestCookies" packet type handler.
   *
   * @return object
   *         The response packet - network request cookies.
   */
  getRequestCookies() {
    return {
      cookies: this._request.cookies.map(cookie => ({
        name: cookie.name,
        value: this._createLongStringActor(cookie.value),
      })),
    };
  }

  /**
   * The "getRequestPostData" packet type handler.
   *
   * @return object
   *         The response packet - network POST data.
   */
  getRequestPostData() {
    let postDataText;
    if (this._request.postData.text) {
      // Create a long string actor for the postData text if needed.
      postDataText = this._createLongStringActor(this._request.postData.text);
    }

    return {
      postData: {
        size: this._request.postData.size,
        text: postDataText,
      },
      postDataDiscarded: this._discardRequestBody,
    };
  }

  /**
   * The "getSecurityInfo" packet type handler.
   *
   * @return object
   *         The response packet - connection security information.
   */
  getSecurityInfo() {
    return {
      securityInfo: this._securityInfo,
    };
  }

  /**
   * The "getResponseHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network response headers.
   */
  getResponseHeaders() {
    let rawHeaders;
    let headersSize = 0;
    if (this._response.rawHeaders) {
      headersSize = this._response.rawHeaders.length;
      rawHeaders = this._createLongStringActor(this._response.rawHeaders);
    }

    return {
      headers: this._response.headers.map(header => ({
        name: header.name,
        value: this._createLongStringActor(header.value),
      })),
      headersSize,
      rawHeaders,
    };
  }

  /**
   * The "getResponseCache" packet type handler.
   *
   * @return object
   *         The cache packet - network cache information.
   */
  getResponseCache() {
    return {
      cache: this._response.responseCache,
    };
  }

  /**
   * The "getResponseCookies" packet type handler.
   *
   * @return object
   *         The response packet - network response cookies.
   */
  getResponseCookies() {
    // As opposed to request cookies, response cookies can come with additional
    // properties.
    const cookieOptionalProperties = [
      "domain",
      "expires",
      "httpOnly",
      "path",
      "samesite",
      "secure",
    ];

    return {
      cookies: this._response.cookies.map(cookie => {
        const cookieResponse = {
          name: cookie.name,
          value: this._createLongStringActor(cookie.value),
        };

        for (const prop of cookieOptionalProperties) {
          if (prop in cookie) {
            cookieResponse[prop] = cookie[prop];
          }
        }
        return cookieResponse;
      }),
    };
  }

  /**
   * The "getResponseContent" packet type handler.
   *
   * @return object
   *         The response packet - network response content.
   */
  getResponseContent() {
    return {
      content: this._response.content,
      contentDiscarded: this._discardResponseBody,
    };
  }

  /**
   * The "getEventTimings" packet type handler.
   *
   * @return object
   *         The response packet - network event timings.
   */
  getEventTimings() {
    return {
      timings: this._timings,
      totalTime: this._totalTime,
      offsets: this._offsets,
      serverTimings: this._serverTimings,
      serviceWorkerTimings: this._serviceWorkerTimings,
    };
  }

  /** ****************************************************************
   * Listeners for new network event data coming from NetworkMonitor.
   ******************************************************************/

  /**
   * Add network request POST data.
   *
   * @param object postData
   *        The request POST data.
   */
  addRequestPostData(postData) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._request.postData = postData;
    this._onEventUpdate("requestPostData", {});
  }

  /**
   * Add the initial network response information.
   *
   * @param {object} options
   * @param {nsIChannel} options.channel
   * @param {boolean} options.fromCache
   * @param {string} options.rawHeaders
   * @param {string} options.proxyResponseRawHeaders
   */
  addResponseStart({
    channel,
    fromCache,
    rawHeaders = "",
    proxyResponseRawHeaders,
  }) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    fromCache = fromCache || lazy.NetworkUtils.isFromCache(channel);

    // Read response headers and cookies.
    let responseHeaders = [];
    let responseCookies = [];
    if (!this._blockedReason) {
      const { cookies, headers } =
        lazy.NetworkUtils.fetchResponseHeadersAndCookies(channel);
      responseCookies = cookies;
      responseHeaders = headers;
    }

    // Handle response headers
    this._response.rawHeaders = rawHeaders;
    this._response.headers = responseHeaders;
    this._response.cookies = responseCookies;

    // Handle the rest of the response start metadata.
    this._response.headersSize = rawHeaders ? rawHeaders.length : 0;

    // Discard the response body for known response statuses.
    if (lazy.NetworkUtils.isRedirectedChannel(channel)) {
      this._discardResponseBody = true;
    }

    // Mime type needs to be sent on response start for identifying an sse channel.
    const contentTypeHeader = responseHeaders.find(header =>
      CONTENT_TYPE_REGEXP.test(header.name)
    );

    let mimeType = "";
    if (contentTypeHeader) {
      mimeType = contentTypeHeader.value;
    }

    const timedChannel = channel.QueryInterface(Ci.nsITimedChannel);
    const waitingTime = Math.round(
      (timedChannel.responseStartTime - timedChannel.requestStartTime) / 1000
    );

    let proxyInfo = [];
    if (proxyResponseRawHeaders) {
      // The typical format for proxy raw headers is `HTTP/2 200 Connected\r\nConnection: keep-alive`
      // The content is parsed and split into http version (HTTP/2), status(200) and status text (Connected)
      proxyInfo = proxyResponseRawHeaders.split("\r\n")[0].split(" ");
    }

    this._onEventUpdate("responseStart", {
      httpVersion: lazy.NetworkUtils.getHttpVersion(channel),
      mimeType,
      remoteAddress: fromCache ? "" : channel.remoteAddress,
      remotePort: fromCache ? "" : channel.remotePort,
      status: channel.responseStatus + "",
      statusText: channel.responseStatusText,
      waitingTime,
      isResolvedByTRR: channel.isResolvedByTRR,
      proxyHttpVersion: proxyInfo[0],
      proxyStatus: proxyInfo[1],
      proxyStatusText: proxyInfo[2],
    });
  }

  /**
   * Add connection security information.
   *
   * @param object info
   *        The object containing security information.
   */
  addSecurityInfo(info, isRacing) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._securityInfo = info;

    this._onEventUpdate("securityInfo", {
      state: info.state,
      isRacing,
    });
  }

  /**
   * Add network response content.
   *
   * @param object content
   *        The response content.
   * @param object
   *        - boolean discardedResponseBody
   *          Tells if the response content was recorded or not.
   */
  addResponseContent(
    content,
    { discardResponseBody, blockedReason, blockingExtension }
  ) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._response.content = content;
    content.text = new LongStringActor(this.conn, content.text);
    // bug 1462561 - Use "json" type and manually manage/marshall actors to workaround
    // protocol.js performance issue
    this.manage(content.text);
    content.text = content.text.form();

    this._onEventUpdate("responseContent", {
      mimeType: content.mimeType,
      contentSize: content.size,
      transferredSize: content.transferredSize,
      blockedReason,
      blockingExtension,
    });
  }

  addResponseCache(content) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }
    this._response.responseCache = content.responseCache;
    this._onEventUpdate("responseCache", {});
  }

  /**
   * Add network event timing information.
   *
   * @param number total
   *        The total time of the network event.
   * @param object timings
   *        Timing details about the network event.
   * @param object offsets
   */
  addEventTimings(total, timings, offsets) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._totalTime = total;
    this._timings = timings;
    this._offsets = offsets;

    this._onEventUpdate("eventTimings", { totalTime: total });
  }

  /**
   * Store server timing information. They are merged together
   * with network event timing data when they are available and
   * notification sent to the client.
   * See `addEventTimings` above for more information.
   *
   * @param object serverTimings
   *        Timing details extracted from the Server-Timing header.
   */
  addServerTimings(serverTimings) {
    if (!serverTimings || this.isDestroyed()) {
      return;
    }
    this._serverTimings = serverTimings;
  }

  /**
   * Store service worker timing information. They are merged together
   * with network event timing data when they are available and
   * notification sent to the client.
   * See `addEventTimnings`` above for more information.
   *
   * @param object serviceWorkerTimings
   *        Timing details extracted from the Timed Channel.
   */
  addServiceWorkerTimings(serviceWorkerTimings) {
    if (!serviceWorkerTimings || this.isDestroyed()) {
      return;
    }
    this._serviceWorkerTimings = serviceWorkerTimings;
  }

  _createLongStringActor(string) {
    if (string?.actorID) {
      return string;
    }

    const longStringActor = new LongStringActor(this.conn, string);
    // bug 1462561 - Use "json" type and manually manage/marshall actors to workaround
    // protocol.js performance issue
    this.manage(longStringActor);
    return longStringActor.form();
  }

  /**
   * Sends the updated event data to the client
   *
   * @private
   * @param string updateType
   * @param object data
   *        The properties that have changed for the event
   */
  _onEventUpdate(updateType, data) {
    if (this._onNetworkEventUpdate) {
      this._onNetworkEventUpdate({
        resourceId: this._channelId,
        updateType,
        ...data,
      });
    }
  }
}

exports.NetworkEventActor = NetworkEventActor;
