/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { NETWORK_EVENT },
} = require("devtools/server/actors/resources/index");

const protocol = require("devtools/shared/protocol");
const { networkEventSpec } = require("devtools/shared/specs/network-event");
const { LongStringActor } = require("devtools/server/actors/string");

/**
 * Creates an actor for a network event.
 *
 * @constructor
 * @param object networkEventWatcher
 *        The parent NetworkEventWatcher instance for this object.
 * @param object options
 *        Dictionary object with the following attributes:
 *        - onNetworkEventUpdate: optional function
 *          Listener for updates for the network event
 */
const NetworkEventActor = protocol.ActorClassWithSpec(networkEventSpec, {
  initialize(
    networkEventWatcher,
    { onNetworkEventUpdate, onNetworkEventDestroy },
    networkEvent
  ) {
    this._networkEventWatcher = networkEventWatcher;
    this._conn = networkEventWatcher.conn;
    this._onNetworkEventUpdate = onNetworkEventUpdate;
    this._onNetworkEventDestroy = onNetworkEventDestroy;

    this.asResource = this.asResource.bind(this);

    // Necessary to get the events to work
    protocol.Actor.prototype.initialize.call(this, this._conn);

    this._request = {
      method: networkEvent.method || null,
      url: networkEvent.url || null,
      httpVersion: networkEvent.httpVersion || null,
      headers: [],
      cookies: [],
      headersSize: networkEvent.headersSize || null,
      postData: {},
    };

    this._response = {
      headers: [],
      cookies: [],
      content: {},
    };

    this._timings = {};
    this._serverTimings = [];
    // Stack trace info isn't sent automatically. The client
    // needs to request it explicitly using getStackTrace
    // packet. NetmonitorActor may pass just a boolean instead of the stack
    // when the actor is in parent process and stack is in the content process.
    this._stackTrace = false;

    this._discardRequestBody = !!networkEvent.discardRequestBody;
    this._discardResponseBody = !!networkEvent.discardResponseBody;

    this._startedDateTime = networkEvent.startedDateTime;
    this._isXHR = networkEvent.isXHR;

    this._cause = networkEvent.cause;
    // Lets remove the last frame here as
    // it is passed from the the server by the NETWORK_EVENT_STACKTRACE
    // resource type. This done here for backward compatibility.
    if (this._cause.lastFrame) {
      delete this._cause.lastFrame;
    }

    this._fromCache = networkEvent.fromCache;
    this._fromServiceWorker = networkEvent.fromServiceWorker;
    this._isThirdPartyTrackingResource =
      networkEvent.isThirdPartyTrackingResource;
    this._referrerPolicy = networkEvent.referrerPolicy;
    this._channelId = networkEvent.channelId;
    this._browsingContextID = networkEvent.browsingContextID;
    this._serial = networkEvent.serial;
    this._blockedReason = networkEvent.blockedReason;
    this._blockingExtension = networkEvent.blockingExtension;

    this._truncated = false;
    this._private = networkEvent.private;
  },

  /**
   * Returns a grip for this actor.
   */
  asResource() {
    // The browsingContextID is used by the ResourceWatcher on the client
    // to find the related Target Front.
    const browsingContextID = this._browsingContextID
      ? this._browsingContextID
      : -1;

    // Ensure that we have a browsing context ID for all requests when debugging a tab (=`browserId` is defined).
    // Only privileged requests debugged via the Browser Toolbox (=`browserId` null) can be unrelated to any browsing context.
    if (!this._browsingContextID && this._networkEventWatcher.browserId) {
      throw new Error(
        `Got a request ${this._request.url} without a browsingContextID set`
      );
    }
    return {
      resourceType: NETWORK_EVENT,
      browsingContextID,
      resourceId: this._channelId,
      actor: this.actorID,
      startedDateTime: this._startedDateTime,
      timeStamp: Date.parse(this._startedDateTime),
      url: this._request.url,
      method: this._request.method,
      isXHR: this._isXHR,
      cause: this._cause,
      timings: {},
      fromCache: this._fromCache,
      fromServiceWorker: this._fromServiceWorker,
      private: this._private,
      isThirdPartyTrackingResource: this._isThirdPartyTrackingResource,
      referrerPolicy: this._referrerPolicy,
      blockedReason: this._blockedReason,
      blockingExtension: this._blockingExtension,
      // For websocket requests the serial is used instead of the channel id.
      stacktraceResourceId:
        this._cause.type == "websocket" ? this._serial : this._channelId,
    };
  },

  /**
   * Releases this actor from the pool.
   */
  destroy(conn) {
    if (!this._networkEventWatcher) {
      return;
    }
    if (this._channelId) {
      this._onNetworkEventDestroy(this._channelId);
    }
    this._networkEventWatcher = null;
    protocol.Actor.prototype.destroy.call(this, conn);
  },

  release() {
    // Per spec, destroy is automatically going to be called after this request
  },

  /**
   * The "getRequestHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network request headers.
   */
  getRequestHeaders() {
    return {
      headers: this._request.headers,
      headersSize: this._request.headersSize,
      rawHeaders: this._request.rawHeaders,
    };
  },

  /**
   * The "getRequestCookies" packet type handler.
   *
   * @return object
   *         The response packet - network request cookies.
   */
  getRequestCookies() {
    return {
      cookies: this._request.cookies,
    };
  },

  /**
   * The "getRequestPostData" packet type handler.
   *
   * @return object
   *         The response packet - network POST data.
   */
  getRequestPostData() {
    return {
      postData: this._request.postData,
      postDataDiscarded: this._discardRequestBody,
    };
  },

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
  },

  /**
   * The "getResponseHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network response headers.
   */
  getResponseHeaders() {
    return {
      headers: this._response.headers,
      headersSize: this._response.headersSize,
      rawHeaders: this._response.rawHeaders,
    };
  },

  /**
   * The "getResponseCache" packet type handler.
   *
   * @return object
   *         The cache packet - network cache information.
   */
  getResponseCache: function() {
    return {
      cache: this._response.responseCache,
    };
  },

  /**
   * The "getResponseCookies" packet type handler.
   *
   * @return object
   *         The response packet - network response cookies.
   */
  getResponseCookies() {
    return {
      cookies: this._response.cookies,
    };
  },

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
  },

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
    };
  },

  /** ****************************************************************
   * Listeners for new network event data coming from NetworkMonitor.
   ******************************************************************/

  /**
   * Add network request headers.
   *
   * @param array headers
   *        The request headers array.
   * @param string rawHeaders
   *        The raw headers source.
   */
  addRequestHeaders(headers, rawHeaders) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._request.headers = headers;
    this._prepareHeaders(headers);

    if (rawHeaders) {
      rawHeaders = new LongStringActor(this._conn, rawHeaders);
      // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
      // protocol.js performance issue
      this.manage(rawHeaders);
      rawHeaders = rawHeaders.form();
    }
    this._request.rawHeaders = rawHeaders;

    this._onEventUpdate("requestHeaders", {
      headers: headers.length,
      headersSize: this._request.headersSize,
    });
  },

  /**
   * Add network request cookies.
   *
   * @param array cookies
   *        The request cookies array.
   */
  addRequestCookies(cookies) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._request.cookies = cookies;
    this._prepareHeaders(cookies);

    this._onEventUpdate("requestCookies", { cookies: cookies.length });
  },

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
    postData.text = new LongStringActor(this._conn, postData.text);
    // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
    // protocol.js performance issue
    this.manage(postData.text);
    postData.text = postData.text.form();

    this._onEventUpdate("requestPostData", {});
  },

  /**
   * Add the initial network response information.
   *
   * @param object info
   *        The response information.
   * @param string rawHeaders
   *        The raw headers source.
   */
  addResponseStart(info, rawHeaders) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    rawHeaders = new LongStringActor(this._conn, rawHeaders);
    // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
    // protocol.js performance issue
    this.manage(rawHeaders);
    this._response.rawHeaders = rawHeaders.form();

    this._response.httpVersion = info.httpVersion;
    this._response.status = info.status;
    this._response.statusText = info.statusText;
    this._response.headersSize = info.headersSize;
    this._response.waitingTime = info.waitingTime;
    // Consider as not discarded if info.discardResponseBody is undefined
    this._discardResponseBody = !!info.discardResponseBody;

    this._onEventUpdate("responseStart", { ...info });
  },

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
      isRacing: isRacing,
    });
  },

  /**
   * Add network response headers.
   *
   * @param array headers
   *        The response headers array.
   */
  addResponseHeaders(headers) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._response.headers = headers;
    this._prepareHeaders(headers);

    this._onEventUpdate("responseHeaders", {
      headers: headers.length,
      headersSize: this._response.headersSize,
    });
  },

  /**
   * Add network response cookies.
   *
   * @param array cookies
   *        The response cookies array.
   */
  addResponseCookies(cookies) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._response.cookies = cookies;
    this._prepareHeaders(cookies);

    this._onEventUpdate("responseCookies", { cookies: cookies.length });
  },

  /**
   * Add network response content.
   *
   * @param object content
   *        The response content.
   * @param object
   *        - boolean discardedResponseBody
   *          Tells if the response content was recorded or not.
   *        - boolean truncated
   *          Tells if the some of the response content is missing.
   */
  addResponseContent(
    content,
    { discardResponseBody, truncated, blockedReason, blockingExtension }
  ) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._truncated = truncated;
    this._response.content = content;
    content.text = new LongStringActor(this._conn, content.text);
    // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
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
  },

  addResponseCache: function(content) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }
    this._response.responseCache = content.responseCache;
    this._onEventUpdate("responseCache", {});
  },

  /**
   * Add network event timing information.
   *
   * @param number total
   *        The total time of the network event.
   * @param object timings
   *        Timing details about the network event.
   * @param object offsets
   * @param object serverTimings
   *        Timing details extracted from the Server-Timing header.
   */
  addEventTimings(total, timings, offsets, serverTimings) {
    // Ignore calls when this actor is already destroyed
    if (this.isDestroyed()) {
      return;
    }

    this._totalTime = total;
    this._timings = timings;
    this._offsets = offsets;

    if (serverTimings) {
      this._serverTimings = serverTimings;
    }

    this._onEventUpdate("eventTimings", { totalTime: total });
  },

  /**
   * Store server timing information. They will be merged together
   * with network event timing data when they are available and
   * notification sent to the client.
   * See `addEventTimnings`` above for more information.
   *
   * @param object serverTimings
   *        Timing details extracted from the Server-Timing header.
   */
  addServerTimings(serverTimings) {
    if (serverTimings) {
      this._serverTimings = serverTimings;
    }
  },

  /**
   * Prepare the headers array to be sent to the client by using the
   * LongStringActor for the header values, when needed.
   *
   * @private
   * @param array aHeaders
   */
  _prepareHeaders(headers) {
    for (const header of headers) {
      header.value = new LongStringActor(this._conn, header.value);
      // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
      // protocol.js performance issue
      this.manage(header.value);
      header.value = header.value.form();
    }
  },
  /**
   * Sends the updated event data to the client
   *
   * @private
   * @param string updateType
   * @param object data
   *        The properties that have changed for the event
   */
  _onEventUpdate(updateType, data) {
    this._onNetworkEventUpdate({
      resourceId: this._channelId,
      updateType,
      ...data,
    });
  },
});

exports.NetworkEventActor = NetworkEventActor;
