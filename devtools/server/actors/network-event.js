/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Creates an actor for a network event.
 *
 * @constructor
 * @param object webConsoleActor
 *        The parent WebConsoleActor instance for this object.
 */
function NetworkEventActor(webConsoleActor) {
  this.parent = webConsoleActor;
  this.conn = this.parent.conn;

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

  // Keep track of LongStringActors owned by this NetworkEventActor.
  this._longStringActors = new Set();
}

NetworkEventActor.prototype =
{
  _request: null,
  _response: null,
  _timings: null,
  _longStringActors: null,

  actorPrefix: "netEvent",

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function() {
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
    };
  },

  /**
   * Releases this actor from the pool.
   */
  release: function() {
    for (let grip of this._longStringActors) {
      let actor = this.parent.getActorByID(grip.actor);
      if (actor) {
        this.parent.releaseActor(actor);
      }
    }
    this._longStringActors = new Set();

    if (this._request.url) {
      this.parent._networkEventActorsByURL.delete(this._request.url);
    }
    if (this.channel) {
      this.parent._netEvents.delete(this.channel);
    }
    this.parent.releaseActor(this);
  },

  /**
   * Handle a protocol request to release a grip.
   */
  onRelease: function() {
    this.release();
    return {};
  },

  /**
   * Set the properties of this actor based on it's corresponding
   * network event.
   *
   * @param object networkEvent
   *        The network event associated with this actor.
   */
  init: function(networkEvent) {
    this._startedDateTime = networkEvent.startedDateTime;
    this._isXHR = networkEvent.isXHR;
    this._cause = networkEvent.cause;
    this._fromCache = networkEvent.fromCache;
    this._fromServiceWorker = networkEvent.fromServiceWorker;

    // Stack trace info isn't sent automatically. The client
    // needs to request it explicitly using getStackTrace
    // packet.
    this._stackTrace = networkEvent.cause.stacktrace;
    delete networkEvent.cause.stacktrace;
    networkEvent.cause.stacktraceAvailable =
      !!(this._stackTrace && this._stackTrace.length);

    for (let prop of ["method", "url", "httpVersion", "headersSize"]) {
      this._request[prop] = networkEvent[prop];
    }

    this._discardRequestBody = networkEvent.discardRequestBody;
    this._discardResponseBody = networkEvent.discardResponseBody;
    this._truncated = false;
    this._private = networkEvent.private;
  },

  /**
   * The "getRequestHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network request headers.
   */
  onGetRequestHeaders: function() {
    return {
      from: this.actorID,
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
  onGetRequestCookies: function() {
    return {
      from: this.actorID,
      cookies: this._request.cookies,
    };
  },

  /**
   * The "getRequestPostData" packet type handler.
   *
   * @return object
   *         The response packet - network POST data.
   */
  onGetRequestPostData: function() {
    return {
      from: this.actorID,
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
  onGetSecurityInfo: function() {
    return {
      from: this.actorID,
      securityInfo: this._securityInfo,
    };
  },

  /**
   * The "getResponseHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network response headers.
   */
  onGetResponseHeaders: function() {
    return {
      from: this.actorID,
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
  onGetResponseCache: function() {
    return {
      from: this.actorID,
      cache: this._response.responseCache,
    };
  },

  /**
   * The "getResponseCookies" packet type handler.
   *
   * @return object
   *         The response packet - network response cookies.
   */
  onGetResponseCookies: function() {
    return {
      from: this.actorID,
      cookies: this._response.cookies,
    };
  },

  /**
   * The "getResponseContent" packet type handler.
   *
   * @return object
   *         The response packet - network response content.
   */
  onGetResponseContent: function() {
    return {
      from: this.actorID,
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
  onGetEventTimings: function() {
    return {
      from: this.actorID,
      timings: this._timings,
      totalTime: this._totalTime,
      offsets: this._offsets
    };
  },

  /**
   * The "getStackTrace" packet type handler.
   *
   * @return object
   *         The response packet - stack trace.
   */
  onGetStackTrace: function() {
    return {
      from: this.actorID,
      stacktrace: this._stackTrace,
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
  addRequestHeaders: function(headers, rawHeaders) {
    this._request.headers = headers;
    this._prepareHeaders(headers);

    rawHeaders = this.parent._createStringGrip(rawHeaders);
    if (typeof rawHeaders == "object") {
      this._longStringActors.add(rawHeaders);
    }
    this._request.rawHeaders = rawHeaders;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "requestHeaders",
      headers: headers.length,
      headersSize: this._request.headersSize,
    };

    this.conn.send(packet);
  },

  /**
   * Add network request cookies.
   *
   * @param array cookies
   *        The request cookies array.
   */
  addRequestCookies: function(cookies) {
    this._request.cookies = cookies;
    this._prepareHeaders(cookies);

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "requestCookies",
      cookies: cookies.length,
    };

    this.conn.send(packet);
  },

  /**
   * Add network request POST data.
   *
   * @param object postData
   *        The request POST data.
   */
  addRequestPostData: function(postData) {
    this._request.postData = postData;
    postData.text = this.parent._createStringGrip(postData.text);
    if (typeof postData.text == "object") {
      this._longStringActors.add(postData.text);
    }

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "requestPostData",
      dataSize: postData.text.length,
      discardRequestBody: this._discardRequestBody,
    };

    this.conn.send(packet);
  },

  /**
   * Add the initial network response information.
   *
   * @param object info
   *        The response information.
   * @param string rawHeaders
   *        The raw headers source.
   */
  addResponseStart: function(info, rawHeaders) {
    rawHeaders = this.parent._createStringGrip(rawHeaders);
    if (typeof rawHeaders == "object") {
      this._longStringActors.add(rawHeaders);
    }
    this._response.rawHeaders = rawHeaders;

    this._response.httpVersion = info.httpVersion;
    this._response.status = info.status;
    this._response.statusText = info.statusText;
    this._response.headersSize = info.headersSize;
    this._discardResponseBody = info.discardResponseBody;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseStart",
      response: info
    };

    this.conn.send(packet);
  },

  /**
   * Add connection security information.
   *
   * @param object info
   *        The object containing security information.
   */
  addSecurityInfo: function(info) {
    this._securityInfo = info;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "securityInfo",
      state: info.state,
    };

    this.conn.send(packet);
  },

  /**
   * Add network response headers.
   *
   * @param array headers
   *        The response headers array.
   */
  addResponseHeaders: function(headers) {
    this._response.headers = headers;
    this._prepareHeaders(headers);

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseHeaders",
      headers: headers.length,
      headersSize: this._response.headersSize,
    };

    this.conn.send(packet);
  },

  /**
   * Add network response cookies.
   *
   * @param array cookies
   *        The response cookies array.
   */
  addResponseCookies: function(cookies) {
    this._response.cookies = cookies;
    this._prepareHeaders(cookies);

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseCookies",
      cookies: cookies.length,
    };

    this.conn.send(packet);
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
  addResponseContent: function(content, {discardResponseBody, truncated}) {
    this._truncated = truncated;
    this._response.content = content;
    content.text = this.parent._createStringGrip(content.text);
    if (typeof content.text == "object") {
      this._longStringActors.add(content.text);
    }

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseContent",
      mimeType: content.mimeType,
      contentSize: content.size,
      encoding: content.encoding,
      transferredSize: content.transferredSize,
      discardResponseBody,
    };

    this.conn.send(packet);
  },

  addResponseCache: function(content) {
    this._response.responseCache = content.responseCache;
    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseCache",
    };
    this.conn.send(packet);
  },

  /**
   * Add network event timing information.
   *
   * @param number total
   *        The total time of the network event.
   * @param object timings
   *        Timing details about the network event.
   */
  addEventTimings: function(total, timings, offsets) {
    this._totalTime = total;
    this._timings = timings;
    this._offsets = offsets;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "eventTimings",
      totalTime: total
    };

    this.conn.send(packet);
  },

  /**
   * Prepare the headers array to be sent to the client by using the
   * LongStringActor for the header values, when needed.
   *
   * @private
   * @param array aHeaders
   */
  _prepareHeaders: function(headers) {
    for (let header of headers) {
      header.value = this.parent._createStringGrip(header.value);
      if (typeof header.value == "object") {
        this._longStringActors.add(header.value);
      }
    }
  },
};

NetworkEventActor.prototype.requestTypes =
{
  "release": NetworkEventActor.prototype.onRelease,
  "getRequestHeaders": NetworkEventActor.prototype.onGetRequestHeaders,
  "getRequestCookies": NetworkEventActor.prototype.onGetRequestCookies,
  "getRequestPostData": NetworkEventActor.prototype.onGetRequestPostData,
  "getResponseHeaders": NetworkEventActor.prototype.onGetResponseHeaders,
  "getResponseCookies": NetworkEventActor.prototype.onGetResponseCookies,
  "getResponseCache": NetworkEventActor.prototype.onGetResponseCache,
  "getResponseContent": NetworkEventActor.prototype.onGetResponseContent,
  "getEventTimings": NetworkEventActor.prototype.onGetEventTimings,
  "getSecurityInfo": NetworkEventActor.prototype.onGetSecurityInfo,
  "getStackTrace": NetworkEventActor.prototype.onGetStackTrace,
};

exports.NetworkEventActor = NetworkEventActor;
