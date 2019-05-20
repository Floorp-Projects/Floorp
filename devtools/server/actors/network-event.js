/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { networkEventSpec } = require("devtools/shared/specs/network-event");
const { LongStringActor } = require("devtools/server/actors/string");

/**
 * Creates an actor for a network event.
 *
 * @constructor
 * @param object netMonitorActor
 *        The parent NetworkMonitorActor instance for this object.
 */
const NetworkEventActor = protocol.ActorClassWithSpec(networkEventSpec, {
  initialize(netMonitorActor) {
    // Necessary to get the events to work
    protocol.Actor.prototype.initialize.call(this, netMonitorActor.conn);

    this.netMonitorActor = netMonitorActor;
    this.conn = this.netMonitorActor.conn;

    this._request = {
      method: null,
      url: null,
      httpVersion: null,
      headers: [],
      cookies: [],
      headersSize: null,
      postData: {},
    };

    this._response = {
      headers: [],
      cookies: [],
      content: {},
    };

    this._timings = {};
    this._stackTrace = {};

    this._discardRequestBody = false;
    this._discardResponseBody = false;
  },

  _request: null,
  _response: null,
  _timings: null,

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  form() {
    return {
      actor: this.actorID,
      startedDateTime: this._startedDateTime,
      timeStamp: Date.parse(this._startedDateTime),
      url: this._request.url,
      method: this._request.method,
      isXHR: this._isXHR,
      cause: this._cause,
      fromCache: this._fromCache,
      fromServiceWorker: this._fromServiceWorker,
      private: this._private,
      isThirdPartyTrackingResource: this._isThirdPartyTrackingResource,
      referrerPolicy: this._referrerPolicy,
      blockedReason: this._blockedReason,
    };
  },

  /**
   * Releases this actor from the pool.
   */
  destroy(conn) {
    if (!this.netMonitorActor) {
      return;
    }
    if (this._request.url) {
      this.netMonitorActor._networkEventActorsByURL.delete(this._request.url);
    }
    if (this.channel) {
      this.netMonitorActor._netEvents.delete(this.channel);
    }

    this.netMonitorActor = null;
    protocol.Actor.prototype.destroy.call(this, conn);
  },

  release() {
    // Per spec, destroy is automatically going to be called after this request
  },

  /**
   * Set the properties of this actor based on it's corresponding
   * network event.
   *
   * @param object networkEvent
   *        The network event associated with this actor.
   */
  init(networkEvent) {
    this._startedDateTime = networkEvent.startedDateTime;
    this._isXHR = networkEvent.isXHR;
    this._cause = networkEvent.cause;
    this._fromCache = networkEvent.fromCache;
    this._fromServiceWorker = networkEvent.fromServiceWorker;
    this._isThirdPartyTrackingResource = networkEvent.isThirdPartyTrackingResource;
    this._referrerPolicy = networkEvent.referrerPolicy;
    this._channelId = networkEvent.channelId;

    // Stack trace info isn't sent automatically. The client
    // needs to request it explicitly using getStackTrace
    // packet. NetmonitorActor may pass just a boolean instead of the stack
    // when the actor is in parent process and stack is in the content process.
    this._stackTrace = networkEvent.cause.stacktrace;
    delete networkEvent.cause.stacktrace;
    networkEvent.cause.stacktraceAvailable =
      !!(this._stackTrace &&
         (typeof this._stackTrace == "boolean" || this._stackTrace.length));

    for (const prop of ["method", "url", "httpVersion", "headersSize"]) {
      this._request[prop] = networkEvent[prop];
    }

    // Consider as not discarded if networkEvent.discard*Body is undefined
    this._discardRequestBody = !!networkEvent.discardRequestBody;
    this._discardResponseBody = !!networkEvent.discardResponseBody;

    this._blockedReason = networkEvent.blockedReason;

    this._truncated = false;
    this._private = networkEvent.private;
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
    };
  },

  /**
   * The "getStackTrace" packet type handler.
   *
   * @return object
   *         The response packet - stack trace.
   */
  async getStackTrace() {
    let stacktrace = this._stackTrace;
    // If _stackTrace was "true", it means we are in parent process
    // and the stack is available from the content process.
    // Fetch it lazily from here via the message manager.
    if (stacktrace && typeof stacktrace == "boolean") {
      let id;
      if (this._cause.type == "websocket") {
        // Convert to a websocket URL, as in onNetworkEvent.
        id = this._request.url.replace(/^http/, "ws");
      } else {
        id = this._channelId;
      }

      const messageManager = this.netMonitorActor.messageManager;
      stacktrace = await new Promise(resolve => {
        const onMessage = ({ data }) => {
          const { channelId, stack } = data;
          if (channelId == id) {
            messageManager.removeMessageListener("debug:request-stack:response",
              onMessage);
            resolve(stack);
          }
        };
        messageManager.addMessageListener("debug:request-stack:response", onMessage);
        messageManager.sendAsyncMessage("debug:request-stack:request", id);
      });
      this._stackTrace = stacktrace;
    }

    return {
      stacktrace,
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
    if (!this.actorID) {
      return;
    }

    this._request.headers = headers;
    this._prepareHeaders(headers);

    if (rawHeaders) {
      rawHeaders = new LongStringActor(this.conn, rawHeaders);
      // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
      // protocol.js performance issue
      this.manage(rawHeaders);
      rawHeaders = rawHeaders.form();
    }
    this._request.rawHeaders = rawHeaders;

    this.emit("network-event-update:headers", "requestHeaders", {
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
    if (!this.actorID) {
      return;
    }

    this._request.cookies = cookies;
    this._prepareHeaders(cookies);

    this.emit("network-event-update:cookies", "requestCookies", {
      cookies: cookies.length,
    });
  },

  /**
   * Add network request POST data.
   *
   * @param object postData
   *        The request POST data.
   */
  addRequestPostData(postData) {
    // Ignore calls when this actor is already destroyed
    if (!this.actorID) {
      return;
    }

    this._request.postData = postData;
    postData.text = new LongStringActor(this.conn, postData.text);
    // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
    // protocol.js performance issue
    this.manage(postData.text);
    const dataSize = postData.size;
    postData.text = postData.text.form();

    this.emit("network-event-update:post-data", "requestPostData", {
      dataSize,
      discardRequestBody: this._discardRequestBody,
    });
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
    if (!this.actorID) {
      return;
    }

    rawHeaders = new LongStringActor(this.conn, rawHeaders);
    // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
    // protocol.js performance issue
    this.manage(rawHeaders);
    this._response.rawHeaders = rawHeaders.form();

    this._response.httpVersion = info.httpVersion;
    this._response.status = info.status;
    this._response.statusText = info.statusText;
    this._response.headersSize = info.headersSize;
    // Consider as not discarded if info.discardResponseBody is undefined
    this._discardResponseBody = !!info.discardResponseBody;

    this.emit("network-event-update:response-start", "responseStart", {
      response: info,
    });
  },

  /**
   * Add connection security information.
   *
   * @param object info
   *        The object containing security information.
   */
  addSecurityInfo(info, isRacing) {
    // Ignore calls when this actor is already destroyed
    if (!this.actorID) {
      return;
    }

    this._securityInfo = info;
    this.emit("network-event-update:security-info", "securityInfo", {
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
    if (!this.actorID) {
      return;
    }

    this._response.headers = headers;
    this._prepareHeaders(headers);

    this.emit("network-event-update:headers", "responseHeaders", {
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
    if (!this.actorID) {
      return;
    }

    this._response.cookies = cookies;
    this._prepareHeaders(cookies);

    this.emit("network-event-update:cookies", "responseCookies", {
      cookies: cookies.length,
    });
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
  addResponseContent(content, {discardResponseBody, truncated}) {
    // Ignore calls when this actor is already destroyed
    if (!this.actorID) {
      return;
    }

    this._truncated = truncated;
    this._response.content = content;
    content.text = new LongStringActor(this.conn, content.text);
    // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
    // protocol.js performance issue
    this.manage(content.text);
    content.text = content.text.form();

    this.emit("network-event-update:response-content", "responseContent", {
      mimeType: content.mimeType,
      contentSize: content.size,
      encoding: content.encoding,
      transferredSize: content.transferredSize,
      discardResponseBody,
    });
  },

  addResponseCache: function(content) {
    // Ignore calls when this actor is already destroyed
    if (!this.actorID) {
      return;
    }

    this._response.responseCache = content.responseCache;
    this.emit("network-event-update:response-cache", "responseCache");
  },

  /**
   * Add network event timing information.
   *
   * @param number total
   *        The total time of the network event.
   * @param object timings
   *        Timing details about the network event.
   */
  addEventTimings(total, timings, offsets) {
    // Ignore calls when this actor is already destroyed
    if (!this.actorID) {
      return;
    }

    this._totalTime = total;
    this._timings = timings;
    this._offsets = offsets;

    this.emit("network-event-update:event-timings", "eventTimings", {
      totalTime: total,
    });
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
      header.value = new LongStringActor(this.conn, header.value);
      // bug 1462561 - Use "json" type and manually manage/marshall actors to woraround
      // protocol.js performance issue
      this.manage(header.value);
      header.value = header.value.form();
    }
  },
});

exports.NetworkEventActor = NetworkEventActor;
