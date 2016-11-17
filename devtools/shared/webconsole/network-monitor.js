/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cm, Cu, Cr, components} = require("chrome");
const Services = require("Services");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");

loader.lazyRequireGetter(this, "NetworkHelper",
                         "devtools/shared/webconsole/network-helper");
loader.lazyRequireGetter(this, "DevToolsUtils",
                         "devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "flags",
                         "devtools/shared/flags");
loader.lazyRequireGetter(this, "DebuggerServer",
                         "devtools/server/main", true);
loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");
loader.lazyServiceGetter(this, "gActivityDistributor",
                         "@mozilla.org/network/http-activity-distributor;1",
                         "nsIHttpActivityDistributor");
const {NetworkThrottleManager} = require("devtools/shared/webconsole/throttle");

// Network logging

// The maximum uint32 value.
const PR_UINT32_MAX = 4294967295;

// HTTP status codes.
const HTTP_MOVED_PERMANENTLY = 301;
const HTTP_FOUND = 302;
const HTTP_SEE_OTHER = 303;
const HTTP_TEMPORARY_REDIRECT = 307;

// The maximum number of bytes a NetworkResponseListener can hold: 1 MB
const RESPONSE_BODY_LIMIT = 1048576;

/**
 * Check if a given network request should be logged by a network monitor
 * based on the specified filters.
 *
 * @param nsIHttpChannel channel
 *        Request to check.
 * @param filters
 *        NetworkMonitor filters to match against.
 * @return boolean
 *         True if the network request should be logged, false otherwise.
 */
function matchRequest(channel, filters) {
  // Log everything if no filter is specified
  if (!filters.outerWindowID && !filters.window && !filters.appId) {
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
    let topFrame = NetworkHelper.getTopFrameForRequest(channel);
    // topFrame is typically null for some chrome requests like favicons
    if (topFrame) {
      try {
        if (topFrame.outerWindowID == filters.outerWindowID) {
          return true;
        }
      } catch (e) {
        // outerWindowID getter from browser.xml (non-remote <xul:browser>) may
        // throw when closing a tab while resources are still loading.
      }
    }
  }

  if (filters.appId) {
    let appId = NetworkHelper.getAppIdForRequest(channel);
    if (appId && appId == filters.appId) {
      return true;
    }
  }

  return false;
}

/**
 * This is a nsIChannelEventSink implementation that monitors channel redirects and
 * informs the registered StackTraceCollector about the old and new channels.
 */
const SINK_CLASS_DESCRIPTION = "NetworkMonitor Channel Event Sink";
const SINK_CLASS_ID = components.ID("{e89fa076-c845-48a8-8c45-2604729eba1d}");
const SINK_CONTRACT_ID = "@mozilla.org/network/monitor/channeleventsink;1";
const SINK_CATEGORY_NAME = "net-channel-event-sinks";

function ChannelEventSink() {
  this.wrappedJSObject = this;
  this.collectors = new Set();
}

ChannelEventSink.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannelEventSink]),

  registerCollector(collector) {
    this.collectors.add(collector);
  },

  unregisterCollector(collector) {
    this.collectors.delete(collector);

    if (this.collectors.size == 0) {
      ChannelEventSinkFactory.unregister();
    }
  },

  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
    for (let collector of this.collectors) {
      try {
        collector.onChannelRedirect(oldChannel, newChannel, flags);
      } catch (ex) {
        console.error("StackTraceCollector.onChannelRedirect threw an exception", ex);
      }
    }
    callback.onRedirectVerifyCallback(Cr.NS_OK);
  }
};

const ChannelEventSinkFactory = XPCOMUtils.generateSingletonFactory(ChannelEventSink);

ChannelEventSinkFactory.register = function () {
  const registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  if (registrar.isCIDRegistered(SINK_CLASS_ID)) {
    return;
  }

  registrar.registerFactory(SINK_CLASS_ID,
                            SINK_CLASS_DESCRIPTION,
                            SINK_CONTRACT_ID,
                            ChannelEventSinkFactory);

  XPCOMUtils.categoryManager.addCategoryEntry(SINK_CATEGORY_NAME, SINK_CONTRACT_ID,
    SINK_CONTRACT_ID, false, true);
};

ChannelEventSinkFactory.unregister = function () {
  const registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.unregisterFactory(SINK_CLASS_ID, ChannelEventSinkFactory);

  XPCOMUtils.categoryManager.deleteCategoryEntry(SINK_CATEGORY_NAME, SINK_CONTRACT_ID,
    false);
};

ChannelEventSinkFactory.getService = function () {
  // Make sure the ChannelEventSink service is registered before accessing it
  ChannelEventSinkFactory.register();

  return Cc[SINK_CONTRACT_ID].getService(Ci.nsIChannelEventSink).wrappedJSObject;
};

function StackTraceCollector(filters) {
  this.filters = filters;
  this.stacktracesById = new Map();
}

StackTraceCollector.prototype = {
  init() {
    Services.obs.addObserver(this, "http-on-opening-request", false);
    ChannelEventSinkFactory.getService().registerCollector(this);
  },

  destroy() {
    Services.obs.removeObserver(this, "http-on-opening-request");
    ChannelEventSinkFactory.getService().unregisterCollector(this);
  },

  _saveStackTrace(channel, stacktrace) {
    this.stacktracesById.set(channel.channelId, stacktrace);
  },

  observe(subject) {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (!matchRequest(channel, this.filters)) {
      return;
    }

    // Convert the nsIStackFrame XPCOM objects to a nice JSON that can be
    // passed around through message managers etc.
    let frame = components.stack;
    let stacktrace = [];
    if (frame && frame.caller) {
      frame = frame.caller;
      while (frame) {
        stacktrace.push({
          filename: frame.filename,
          lineNumber: frame.lineNumber,
          columnNumber: frame.columnNumber,
          functionName: frame.name,
          asyncCause: frame.asyncCause,
        });
        frame = frame.caller || frame.asyncCaller;
      }
    }

    this._saveStackTrace(channel, stacktrace);
  },

  onChannelRedirect(oldChannel, newChannel, flags) {
    // We can be called with any nsIChannel, but are interested only in HTTP channels
    try {
      oldChannel.QueryInterface(Ci.nsIHttpChannel);
      newChannel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      return;
    }

    let oldId = oldChannel.channelId;
    let stacktrace = this.stacktracesById.get(oldId);
    if (stacktrace) {
      this.stacktracesById.delete(oldId);
      this._saveStackTrace(newChannel, stacktrace);
    }
  },

  getStackTrace(channelId) {
    let trace = this.stacktracesById.get(channelId);
    this.stacktracesById.delete(channelId);
    return trace;
  }
};

exports.StackTraceCollector = StackTraceCollector;

/**
 * The network response listener implements the nsIStreamListener and
 * nsIRequestObserver interfaces. This is used within the NetworkMonitor feature
 * to get the response body of the request.
 *
 * The code is mostly based on code listings from:
 *
 *   http://www.softwareishard.com/blog/firebug/
 *      nsitraceablechannel-intercept-http-traffic/
 *
 * @constructor
 * @param object owner
 *        The response listener owner. This object needs to hold the
 *        |openResponses| object.
 * @param object httpActivity
 *        HttpActivity object associated with this request. See NetworkMonitor
 *        for more information.
 */
function NetworkResponseListener(owner, httpActivity) {
  this.owner = owner;
  this.receivedData = "";
  this.httpActivity = httpActivity;
  this.bodySize = 0;
  // Note that this is really only needed for the non-e10s case.
  // See bug 1309523.
  let channel = this.httpActivity.channel;
  this._wrappedNotificationCallbacks = channel.notificationCallbacks;
  channel.notificationCallbacks = this;
}

NetworkResponseListener.prototype = {
  QueryInterface:
    XPCOMUtils.generateQI([Ci.nsIStreamListener, Ci.nsIInputStreamCallback,
                           Ci.nsIRequestObserver, Ci.nsIInterfaceRequestor,
                           Ci.nsISupports]),

  // nsIInterfaceRequestor implementation

  /**
   * This object implements nsIProgressEventSink, but also needs to forward
   * interface requests to the notification callbacks of other objects.
   */
  getInterface(iid) {
    if (iid.equals(Ci.nsIProgressEventSink)) {
      return this;
    }
    if (this._wrappedNotificationCallbacks) {
      return this._wrappedNotificationCallbacks.getInterface(iid);
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  /**
   * Forward notifications for interfaces this object implements, in case other
   * objects also implemented them.
   */
  _forwardNotification(iid, method, args) {
    if (!this._wrappedNotificationCallbacks) {
      return;
    }
    try {
      let impl = this._wrappedNotificationCallbacks.getInterface(iid);
      impl[method].apply(impl, args);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_NO_INTERFACE) {
        throw e;
      }
    }
  },

  /**
   * This NetworkResponseListener tracks the NetworkMonitor.openResponses object
   * to find the associated uncached headers.
   * @private
   */
  _foundOpenResponse: false,

  /**
   * If the channel already had notificationCallbacks, hold them here internally
   * so that we can forward getInterface requests to that object.
   */
  _wrappedNotificationCallbacks: null,

  /**
   * The response listener owner.
   */
  owner: null,

  /**
   * The response will be written into the outputStream of this nsIPipe.
   * Both ends of the pipe must be blocking.
   */
  sink: null,

  /**
   * The HttpActivity object associated with this response.
   */
  httpActivity: null,

  /**
   * Stores the received data as a string.
   */
  receivedData: null,

  /**
   * The uncompressed, decoded response body size.
   */
  bodySize: null,

  /**
   * Response body size on the wire, potentially compressed / encoded.
   */
  transferredSize: null,

  /**
   * The nsIRequest we are started for.
   */
  request: null,

  /**
   * Set the async listener for the given nsIAsyncInputStream. This allows us to
   * wait asynchronously for any data coming from the stream.
   *
   * @param nsIAsyncInputStream stream
   *        The input stream from where we are waiting for data to come in.
   * @param nsIInputStreamCallback listener
   *        The input stream callback you want. This is an object that must have
   *        the onInputStreamReady() method. If the argument is null, then the
   *        current callback is removed.
   * @return void
   */
  setAsyncListener: function (stream, listener) {
    // Asynchronously wait for the stream to be readable or closed.
    stream.asyncWait(listener, 0, 0, Services.tm.mainThread);
  },

  /**
   * Stores the received data, if request/response body logging is enabled. It
   * also does limit the number of stored bytes, based on the
   * RESPONSE_BODY_LIMIT constant.
   *
   * Learn more about nsIStreamListener at:
   * https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIStreamListener
   *
   * @param nsIRequest request
   * @param nsISupports context
   * @param nsIInputStream inputStream
   * @param unsigned long offset
   * @param unsigned long count
   */
  onDataAvailable: function (request, context, inputStream, offset, count) {
    this._findOpenResponse();
    let data = NetUtil.readInputStreamToString(inputStream, count);

    this.bodySize += count;

    if (!this.httpActivity.discardResponseBody &&
        this.receivedData.length < RESPONSE_BODY_LIMIT) {
      this.receivedData +=
        NetworkHelper.convertToUnicode(data, request.contentCharset);
    }
  },

  /**
   * See documentation at
   * https://developer.mozilla.org/En/NsIRequestObserver
   *
   * @param nsIRequest request
   * @param nsISupports context
   */
  onStartRequest: function (request) {
    // Converter will call this again, we should just ignore that.
    if (this.request) {
      return;
    }

    this.request = request;
    this._getSecurityInfo();
    this._findOpenResponse();
    // We need to track the offset for the onDataAvailable calls where
    // we pass the data from our pipe to the converter.
    this.offset = 0;

    // In the multi-process mode, the conversion happens on the child
    // side while we can only monitor the channel on the parent
    // side. If the content is gzipped, we have to unzip it
    // ourself. For that we use the stream converter services.  Do not
    // do that for Service workers as they are run in the child
    // process.
    let channel = this.request;
    if (!this.httpActivity.fromServiceWorker &&
        channel instanceof Ci.nsIEncodedChannel &&
        channel.contentEncodings &&
        !channel.applyConversion) {
      let encodingHeader = channel.getResponseHeader("Content-Encoding");
      let scs = Cc["@mozilla.org/streamConverters;1"]
        .getService(Ci.nsIStreamConverterService);
      let encodings = encodingHeader.split(/\s*\t*,\s*\t*/);
      let nextListener = this;
      let acceptedEncodings = ["gzip", "deflate", "br", "x-gzip", "x-deflate"];
      for (let i in encodings) {
        // There can be multiple conversions applied
        let enc = encodings[i].toLowerCase();
        if (acceptedEncodings.indexOf(enc) > -1) {
          this.converter = scs.asyncConvertData(enc, "uncompressed",
                                                nextListener, null);
          nextListener = this.converter;
        }
      }
      if (this.converter) {
        this.converter.onStartRequest(this.request, null);
      }
    }
    // Asynchronously wait for the data coming from the request.
    this.setAsyncListener(this.sink.inputStream, this);
  },

  /**
   * Parse security state of this request and report it to the client.
   */
  _getSecurityInfo: DevToolsUtils.makeInfallible(function () {
    // Many properties of the securityInfo (e.g., the server certificate or HPKP
    // status) are not available in the content process and can't be even touched safely,
    // because their C++ getters trigger assertions. This function is called in content
    // process for synthesized responses from service workers, in the parent otherwise.
    if (Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      return;
    }

    // Take the security information from the original nsIHTTPChannel instead of
    // the nsIRequest received in onStartRequest. If response to this request
    // was a redirect from http to https, the request object seems to contain
    // security info for the https request after redirect.
    let secinfo = this.httpActivity.channel.securityInfo;
    let info = NetworkHelper.parseSecurityInfo(secinfo, this.httpActivity);

    this.httpActivity.owner.addSecurityInfo(info);
  }),

  /**
   * Handle the onStopRequest by closing the sink output stream.
   *
   * For more documentation about nsIRequestObserver go to:
   * https://developer.mozilla.org/En/NsIRequestObserver
   */
  onStopRequest: function () {
    this._findOpenResponse();
    this.sink.outputStream.close();
  },

  // nsIProgressEventSink implementation

  /**
   * Handle progress event as data is transferred.  This is used to record the
   * size on the wire, which may be compressed / encoded.
   */
  onProgress: function (request, context, progress, progressMax) {
    this.transferredSize = progress;
    // Need to forward as well to keep things like Download Manager's progress
    // bar working properly.
    this._forwardNotification(Ci.nsIProgressEventSink, "onProgress", arguments);
  },

  onStatus: function () {
    this._forwardNotification(Ci.nsIProgressEventSink, "onStatus", arguments);
  },

  /**
   * Find the open response object associated to the current request. The
   * NetworkMonitor._httpResponseExaminer() method saves the response headers in
   * NetworkMonitor.openResponses. This method takes the data from the open
   * response object and puts it into the HTTP activity object, then sends it to
   * the remote Web Console instance.
   *
   * @private
   */
  _findOpenResponse: function () {
    if (!this.owner || this._foundOpenResponse) {
      return;
    }

    let openResponse = null;

    for (let id in this.owner.openResponses) {
      let item = this.owner.openResponses[id];
      if (item.channel === this.httpActivity.channel) {
        openResponse = item;
        break;
      }
    }

    if (!openResponse) {
      return;
    }
    this._foundOpenResponse = true;

    delete this.owner.openResponses[openResponse.id];

    this.httpActivity.owner.addResponseHeaders(openResponse.headers);
    this.httpActivity.owner.addResponseCookies(openResponse.cookies);
  },

  /**
   * Clean up the response listener once the response input stream is closed.
   * This is called from onStopRequest() or from onInputStreamReady() when the
   * stream is closed.
   * @return void
   */
  onStreamClose: function () {
    if (!this.httpActivity) {
      return;
    }
    // Remove our listener from the request input stream.
    this.setAsyncListener(this.sink.inputStream, null);

    this._findOpenResponse();

    if (!this.httpActivity.discardResponseBody && this.receivedData.length) {
      this._onComplete(this.receivedData);
    } else if (!this.httpActivity.discardResponseBody &&
               this.httpActivity.responseStatus == 304) {
      // Response is cached, so we load it from cache.
      let charset = this.request.contentCharset || this.httpActivity.charset;
      NetworkHelper.loadFromCache(this.httpActivity.url, charset,
                                  this._onComplete.bind(this));
    } else {
      this._onComplete();
    }
  },

  /**
   * Handler for when the response completes. This function cleans up the
   * response listener.
   *
   * @param string [data]
   *        Optional, the received data coming from the response listener or
   *        from the cache.
   */
  _onComplete: function (data) {
    let response = {
      mimeType: "",
      text: data || "",
    };

    response.size = response.text.length;
    response.transferredSize = this.transferredSize;

    try {
      response.mimeType = this.request.contentType;
    } catch (ex) {
      // Ignore.
    }

    if (!response.mimeType ||
        !NetworkHelper.isTextMimeType(response.mimeType)) {
      response.encoding = "base64";
      try {
        response.text = btoa(response.text);
      } catch (err) {
        // Ignore.
      }
    }

    if (response.mimeType && this.request.contentCharset) {
      response.mimeType += "; charset=" + this.request.contentCharset;
    }

    this.receivedData = "";

    this.httpActivity.owner.addResponseContent(
      response,
      this.httpActivity.discardResponseBody
    );

    this._wrappedNotificationCallbacks = null;
    this.httpActivity = null;
    this.sink = null;
    this.inputStream = null;
    this.converter = null;
    this.request = null;
    this.owner = null;
  },

  /**
   * The nsIInputStreamCallback for when the request input stream is ready -
   * either it has more data or it is closed.
   *
   * @param nsIAsyncInputStream stream
   *        The sink input stream from which data is coming.
   * @returns void
   */
  onInputStreamReady: function (stream) {
    if (!(stream instanceof Ci.nsIAsyncInputStream) || !this.httpActivity) {
      return;
    }

    let available = -1;
    try {
      // This may throw if the stream is closed normally or due to an error.
      available = stream.available();
    } catch (ex) {
      // Ignore.
    }

    if (available != -1) {
      if (available != 0) {
        if (this.converter) {
          this.converter.onDataAvailable(this.request, null, stream,
                                         this.offset, available);
        } else {
          this.onDataAvailable(this.request, null, stream, this.offset,
                               available);
        }
      }
      this.offset += available;
      this.setAsyncListener(stream, this);
    } else {
      this.onStreamClose();
      this.offset = 0;
    }
  },
};

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
 *        - appId (number): filter requests by the appId.
 *        - outerWindowID (number): filter requests by their top frame's outerWindowID.
 *        Filters are optional. If any of these filters match the request is
 *        logged (OR is applied). If no filter is provided then all requests are
 *        logged.
 * @param object owner
 *        The network monitor owner. This object needs to hold:
 *        - onNetworkEvent(requestInfo)
 *          This method is invoked once for every new network request and it is
 *          given the initial network request information as an argument.
 *          onNetworkEvent() must return an object which holds several add*()
 *          methods which are used to add further network request/response information.
 *        - stackTraceCollector
 *          If the owner has this optional property, it will be used as a
 *          StackTraceCollector by the NetworkMonitor.
 */
function NetworkMonitor(filters, owner) {
  this.filters = filters;
  this.owner = owner;
  this.openRequests = {};
  this.openResponses = {};
  this._httpResponseExaminer =
    DevToolsUtils.makeInfallible(this._httpResponseExaminer).bind(this);
  this._httpModifyExaminer =
    DevToolsUtils.makeInfallible(this._httpModifyExaminer).bind(this);
  this._serviceWorkerRequest = this._serviceWorkerRequest.bind(this);
  this._throttleData = null;
  this._throttler = null;
}

exports.NetworkMonitor = NetworkMonitor;

NetworkMonitor.prototype = {
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
    0x804b0006: "STATUS_RECEIVING_FROM"
  },

  httpDownloadActivities: [
    gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_START,
    gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_HEADER,
    gActivityDistributor.ACTIVITY_SUBTYPE_RESPONSE_COMPLETE,
    gActivityDistributor.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE
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
  init: function () {
    this.responsePipeSegmentSize = Services.prefs
                                   .getIntPref("network.buffer.cache.size");
    this.interceptedChannels = new Set();

    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      gActivityDistributor.addObserver(this);
      Services.obs.addObserver(this._httpResponseExaminer,
                               "http-on-examine-response", false);
      Services.obs.addObserver(this._httpResponseExaminer,
                               "http-on-examine-cached-response", false);
      Services.obs.addObserver(this._httpModifyExaminer,
                               "http-on-modify-request", false);
    }
    // In child processes, only watch for service worker requests
    // everything else only happens in the parent process
    Services.obs.addObserver(this._serviceWorkerRequest,
                             "service-worker-synthesized-response", false);
  },

  get throttleData() {
    return this._throttleData;
  },

  set throttleData(value) {
    this._throttleData = value;
    // Clear out any existing throttlers
    this._throttler = null;
  },

  _getThrottler: function () {
    if (this.throttleData !== null && this._throttler === null) {
      this._throttler = new NetworkThrottleManager(this.throttleData);
    }
    return this._throttler;
  },

  _serviceWorkerRequest: function (subject, topic, data) {
    let channel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (!matchRequest(channel, this.filters)) {
      return;
    }

    this.interceptedChannels.add(subject);

    // On e10s, we never receive http-on-examine-cached-response, so fake one.
    if (Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      this._httpResponseExaminer(channel, "http-on-examine-cached-response");
    }
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
  _httpResponseExaminer: function (subject, topic) {
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

    let channel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (!matchRequest(channel, this.filters)) {
      return;
    }

    let response = {
      id: gSequenceId(),
      channel: channel,
      headers: [],
      cookies: [],
    };

    let setCookieHeader = null;

    channel.visitResponseHeaders({
      visitHeader: function (name, value) {
        let lowerName = name.toLowerCase();
        if (lowerName == "set-cookie") {
          setCookieHeader = value;
        }
        response.headers.push({ name: name, value: value });
      }
    });

    if (!response.headers.length) {
      // No need to continue.
      return;
    }

    if (setCookieHeader) {
      response.cookies = NetworkHelper.parseSetCookieHeader(setCookieHeader);
    }

    // Determine the HTTP version.
    let httpVersionMaj = {};
    let httpVersionMin = {};

    channel.QueryInterface(Ci.nsIHttpChannelInternal);
    channel.getResponseVersion(httpVersionMaj, httpVersionMin);

    response.status = channel.responseStatus;
    response.statusText = channel.responseStatusText;
    response.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                     httpVersionMin.value;

    this.openResponses[response.id] = response;

    if (topic === "http-on-examine-cached-response") {
      // Service worker requests emits cached-reponse notification on non-e10s,
      // and we fake one on e10s.
      let fromServiceWorker = this.interceptedChannels.has(channel);
      this.interceptedChannels.delete(channel);

      // If this is a cached response, there never was a request event
      // so we need to construct one here so the frontend gets all the
      // expected events.
      let httpActivity = this._createNetworkEvent(channel, {
        fromCache: !fromServiceWorker,
        fromServiceWorker: fromServiceWorker
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
      let timings = this._setupHarTimings(httpActivity, true);
      httpActivity.owner.addEventTimings(timings.total, timings.timings);
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
  _httpModifyExaminer: function (subject) {
    let throttler = this._getThrottler();
    if (throttler) {
      let channel = subject.QueryInterface(Ci.nsIHttpChannel);
      if (matchRequest(channel, this.filters)) {
        // Read any request body here, before it is throttled.
        let httpActivity = this.createOrGetActivityObject(channel);
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
  _dispatchActivity: function (httpActivity, channel, activityType,
                               activitySubtype, timestamp, extraSizeData,
                               extraStringData) {
    let transCodes = this.httpTransactionCodes;

    // Store the time information for this activity subtype.
    if (activitySubtype in transCodes) {
      let stage = transCodes[activitySubtype];
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
          httpActivity.owner.addRequestPostData({ text: httpActivity.sentBody });
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
  DevToolsUtils.makeInfallible(function (channel, activityType, activitySubtype,
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
    let httpActivity = this._findActivityObject(channel);
    if (!httpActivity) {
      return;
    }

    // If we're throttling, we must not report events as they arrive
    // from platform, but instead let the throttler emit the events
    // after some time has elapsed.
    if (httpActivity.downloadThrottle &&
        this.httpDownloadActivities.indexOf(activitySubtype) >= 0) {
      let callback = this._dispatchActivity.bind(this);
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
  _createNetworkEvent: function (channel, { timestamp, extraStringData,
                                           fromCache, fromServiceWorker }) {
    let httpActivity = this.createOrGetActivityObject(channel);

    channel.QueryInterface(Ci.nsIPrivateBrowsingChannel);
    httpActivity.private = channel.isChannelPrivate;

    if (timestamp) {
      httpActivity.timings.REQUEST_HEADER = {
        first: timestamp,
        last: timestamp
      };
    }

    let event = {};
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
    httpActivity.fromServiceWorker = fromServiceWorker;

    if (extraStringData) {
      event.headersSize = extraStringData.length;
    }

    // Determine the cause and if this is an XHR request.
    let causeType = channel.loadInfo.externalContentPolicyType;
    let loadingPrincipal = channel.loadInfo.loadingPrincipal;
    let causeUri = loadingPrincipal ? loadingPrincipal.URI : null;
    let stacktrace;
    // If this is the parent process, there is no stackTraceCollector - the stack
    // trace will be added in NetworkMonitorChild._onNewEvent.
    if (this.owner.stackTraceCollector) {
      stacktrace = this.owner.stackTraceCollector.getStackTrace(event.channelId);
    }

    event.cause = {
      type: causeType,
      loadingDocumentUri: causeUri ? causeUri.spec : null,
      stacktrace
    };

    httpActivity.isXHR = event.isXHR =
        (causeType === Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST ||
         causeType === Ci.nsIContentPolicy.TYPE_FETCH);

    // Determine the HTTP version.
    let httpVersionMaj = {};
    let httpVersionMin = {};
    channel.QueryInterface(Ci.nsIHttpChannelInternal);
    channel.getRequestVersion(httpVersionMaj, httpVersionMin);

    event.httpVersion = "HTTP/" + httpVersionMaj.value + "." +
                                  httpVersionMin.value;

    event.discardRequestBody = !this.saveRequestAndResponseBodies;
    event.discardResponseBody = !this.saveRequestAndResponseBodies;

    let headers = [];
    let cookies = [];
    let cookieHeader = null;

    // Copy the request header data.
    channel.visitRequestHeaders({
      visitHeader: function (name, value) {
        if (name == "Cookie") {
          cookieHeader = value;
        }
        headers.push({ name: name, value: value });
      }
    });

    if (cookieHeader) {
      cookies = NetworkHelper.parseCookieHeader(cookieHeader);
    }

    httpActivity.owner = this.owner.onNetworkEvent(event);

    this._setupResponseListener(httpActivity, fromCache);

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
  _onRequestHeader: function (channel, timestamp, extraStringData) {
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
  _findActivityObject: function (channel) {
    for (let id in this.openRequests) {
      let item = this.openRequests[id];
      if (item.channel === channel) {
        return item;
      }
    }
    return null;
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
  createOrGetActivityObject: function (channel) {
    let httpActivity = this._findActivityObject(channel);
    if (!httpActivity) {
      let win = NetworkHelper.getWindowForRequest(channel);
      let charset = win ? win.document.characterSet : null;

      httpActivity = {
        id: gSequenceId(),
        channel: channel,
        // see _onRequestBodySent()
        charset: charset,
        sentBody: null,
        url: channel.URI.spec,
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

      this.openRequests[httpActivity.id] = httpActivity;
    }

    return httpActivity;
  },

  /**
   * Setup the network response listener for the given HTTP activity. The
   * NetworkResponseListener is responsible for storing the response body.
   *
   * @private
   * @param object httpActivity
   *        The HTTP activity object we are tracking.
   */
  _setupResponseListener: function (httpActivity, fromCache) {
    let channel = httpActivity.channel;
    channel.QueryInterface(Ci.nsITraceableChannel);

    if (!fromCache) {
      let throttler = this._getThrottler();
      if (throttler) {
        httpActivity.downloadThrottle = throttler.manage(channel);
      }
    }

    // The response will be written into the outputStream of this pipe.
    // This allows us to buffer the data we are receiving and read it
    // asynchronously.
    // Both ends of the pipe must be blocking.
    let sink = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);

    // The streams need to be blocking because this is required by the
    // stream tee.
    sink.init(false, false, this.responsePipeSegmentSize, PR_UINT32_MAX, null);

    // Add listener for the response body.
    let newListener = new NetworkResponseListener(this, httpActivity);

    // Remember the input stream, so it isn't released by GC.
    newListener.inputStream = sink.inputStream;
    newListener.sink = sink;

    let tee = Cc["@mozilla.org/network/stream-listener-tee;1"]
              .createInstance(Ci.nsIStreamListenerTee);

    let originalListener = channel.setNewListener(tee);

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
  _onRequestBodySent: function (httpActivity) {
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
      let webNav = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation);
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
  _onResponseHeader: function (httpActivity, extraStringData) {
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

    let headers = extraStringData.split(/\r\n|\n|\r/);
    let statusLine = headers.shift();
    let statusLineArray = statusLine.split(" ");

    let response = {};
    response.httpVersion = statusLineArray.shift();
    response.remoteAddress = httpActivity.channel.remoteAddress;
    response.remotePort = httpActivity.channel.remotePort;
    response.status = statusLineArray.shift();
    response.statusText = statusLineArray.join(" ");
    response.headersSize = extraStringData.length;

    httpActivity.responseStatus = response.status;

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
  _onTransactionClose: function (httpActivity) {
    let result = this._setupHarTimings(httpActivity);
    httpActivity.owner.addEventTimings(result.total, result.timings);
    delete this.openRequests[httpActivity.id];
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
  _setupHarTimings: function (httpActivity, fromCache) {
    if (fromCache) {
      // If it came from the browser cache, we have no timing
      // information and these should all be 0
      return {
        total: 0,
        timings: {
          blocked: 0,
          dns: 0,
          connect: 0,
          send: 0,
          wait: 0,
          receive: 0
        }
      };
    }

    let timings = httpActivity.timings;
    let harTimings = {};

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

    if (timings.STATUS_SENDING_TO) {
      harTimings.send = timings.STATUS_SENDING_TO.last - timings.STATUS_SENDING_TO.first;
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

    let totalTime = 0;
    for (let timing in harTimings) {
      let time = Math.max(Math.round(harTimings[timing] / 1000), -1);
      harTimings[timing] = time;
      if (time > -1) {
        totalTime += time;
      }
    }

    return {
      total: totalTime,
      timings: harTimings,
    };
  },

  /**
   * Suspend Web Console activity. This is called when all Web Consoles are
   * closed.
   */
  destroy: function () {
    if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      gActivityDistributor.removeObserver(this);
      Services.obs.removeObserver(this._httpResponseExaminer,
                                  "http-on-examine-response");
      Services.obs.removeObserver(this._httpResponseExaminer,
                                  "http-on-examine-cached-response");
      Services.obs.removeObserver(this._httpModifyExaminer,
                                  "http-on-modify-request", false);
    }

    Services.obs.removeObserver(this._serviceWorkerRequest,
                                "service-worker-synthesized-response");

    this.interceptedChannels.clear();
    this.openRequests = {};
    this.openResponses = {};
    this.owner = null;
    this.filters = null;
    this._throttler = null;
  },
};

/**
 * The NetworkMonitorChild is used to proxy all of the network activity of the
 * child app process from the main process. The child WebConsoleActor creates an
 * instance of this object.
 *
 * Network requests for apps happen in the main process. As such,
 * a NetworkMonitor instance is used by the WebappsActor in the main process to
 * log the network requests for this child process.
 *
 * The main process creates NetworkEventActorProxy instances per request. These
 * send the data to this object using the nsIMessageManager. Here we proxy the
 * data to the WebConsoleActor or to a NetworkEventActor.
 *
 * @constructor
 * @param number outerWindowID
 *        The outerWindowID of the TabActor's main window.
 * @param nsIMessageManager messageManager
 *        The nsIMessageManager to use to communicate with the parent process.
 * @param object DebuggerServerConnection
 *        The RDP connection to the client.
 * @param object owner
 *        The WebConsoleActor that is listening for the network requests.
 */
function NetworkMonitorChild(outerWindowID, messageManager, conn, owner) {
  this.outerWindowID = outerWindowID;
  this.conn = conn;
  this.owner = owner;
  this._messageManager = messageManager;
  this._onNewEvent = this._onNewEvent.bind(this);
  this._onUpdateEvent = this._onUpdateEvent.bind(this);
  this._netEvents = new Map();
}

exports.NetworkMonitorChild = NetworkMonitorChild;

NetworkMonitorChild.prototype = {
  owner: null,
  _netEvents: null,
  _saveRequestAndResponseBodies: true,
  _throttleData: null,

  get saveRequestAndResponseBodies() {
    return this._saveRequestAndResponseBodies;
  },

  set saveRequestAndResponseBodies(val) {
    this._saveRequestAndResponseBodies = val;

    this._messageManager.sendAsyncMessage("debug:netmonitor", {
      action: "setPreferences",
      preferences: {
        saveRequestAndResponseBodies: this._saveRequestAndResponseBodies,
      },
    });
  },

  get throttleData() {
    return this._throttleData;
  },

  set throttleData(val) {
    this._throttleData = val;

    this._messageManager.sendAsyncMessage("debug:netmonitor", {
      action: "setPreferences",
      preferences: {
        throttleData: this._throttleData,
      },
    });
  },

  init: function () {
    this.conn.setupInParent({
      module: "devtools/shared/webconsole/network-monitor",
      setupParent: "setupParentProcess"
    });

    let mm = this._messageManager;
    mm.addMessageListener("debug:netmonitor:newEvent",
                          this._onNewEvent);
    mm.addMessageListener("debug:netmonitor:updateEvent",
                          this._onUpdateEvent);
    mm.sendAsyncMessage("debug:netmonitor", {
      outerWindowID: this.outerWindowID,
      action: "start",
    });
  },

  _onNewEvent: DevToolsUtils.makeInfallible(function _onNewEvent(msg) {
    let {id, event} = msg.data;

    // Try to add stack trace to the event data received from parent
    if (this.owner.stackTraceCollector) {
      event.cause.stacktrace =
        this.owner.stackTraceCollector.getStackTrace(event.channelId);
    }

    let actor = this.owner.onNetworkEvent(event);
    this._netEvents.set(id, Cu.getWeakReference(actor));
  }),

  _onUpdateEvent: DevToolsUtils.makeInfallible(function _onUpdateEvent(msg) {
    let {id, method, args} = msg.data;
    let weakActor = this._netEvents.get(id);
    let actor = weakActor ? weakActor.get() : null;
    if (!actor) {
      console.error("Received debug:netmonitor:updateEvent for unknown " +
                    "event ID: " + id);
      return;
    }
    if (!(method in actor)) {
      console.error("Received debug:netmonitor:updateEvent unsupported " +
                    "method: " + method);
      return;
    }
    actor[method].apply(actor, args);
  }),

  destroy: function () {
    let mm = this._messageManager;
    try {
      mm.removeMessageListener("debug:netmonitor:newEvent",
                               this._onNewEvent);
      mm.removeMessageListener("debug:netmonitor:updateEvent",
                               this._onUpdateEvent);
    } catch (e) {
      // On b2g, when registered to a new root docshell,
      // all message manager functions throw when trying to call them during
      // message-manager-disconnect event.
      // As there is no attribute/method on message manager to know
      // if they are still usable or not, we can only catch the exception...
    }
    this._netEvents.clear();
    this._messageManager = null;
    this.conn = null;
    this.owner = null;
  },
};

/**
 * The NetworkEventActorProxy is used to send network request information from
 * the main process to the child app process. One proxy is used per request.
 * Similarly, one NetworkEventActor in the child app process is used per
 * request. The client receives all network logs from the child actors.
 *
 * The child process has a NetworkMonitorChild instance that is listening for
 * all network logging from the main process. The net monitor shim is used to
 * proxy the data to the WebConsoleActor instance of the child process.
 *
 * @constructor
 * @param nsIMessageManager messageManager
 *        The message manager for the child app process. This is used for
 *        communication with the NetworkMonitorChild instance of the process.
 */
function NetworkEventActorProxy(messageManager) {
  this.id = gSequenceId();
  this.messageManager = messageManager;
}
exports.NetworkEventActorProxy = NetworkEventActorProxy;

NetworkEventActorProxy.methodFactory = function (method) {
  return DevToolsUtils.makeInfallible(function () {
    let args = Array.slice(arguments);
    let mm = this.messageManager;
    mm.sendAsyncMessage("debug:netmonitor:updateEvent", {
      id: this.id,
      method: method,
      args: args,
    });
  }, "NetworkEventActorProxy." + method);
};

NetworkEventActorProxy.prototype = {
  /**
   * Initialize the network event. This method sends the network request event
   * to the content process.
   *
   * @param object event
   *        Object describing the network request.
   * @return object
   *         This object.
   */
  init: DevToolsUtils.makeInfallible(function (event) {
    let mm = this.messageManager;
    mm.sendAsyncMessage("debug:netmonitor:newEvent", {
      id: this.id,
      event: event,
    });
    return this;
  }),
};

(function () {
  // Listeners for new network event data coming from the NetworkMonitor.
  let methods = ["addRequestHeaders", "addRequestCookies", "addRequestPostData",
                 "addResponseStart", "addSecurityInfo", "addResponseHeaders",
                 "addResponseCookies", "addResponseContent", "addEventTimings"];
  let factory = NetworkEventActorProxy.methodFactory;
  for (let method of methods) {
    NetworkEventActorProxy.prototype[method] = factory(method);
  }
})();

/**
 * This is triggered by the child calling `setupInParent` when the child's network monitor
 * is starting up.  This initializes the parent process side of the monitoring.
 */
function setupParentProcess({ mm, prefix }) {
  let networkMonitor = new NetworkMonitorParent(mm, prefix);
  return {
    onBrowserSwap: newMM => networkMonitor.setMessageManager(newMM),
    onDisconnected: () => {
      networkMonitor.destroy();
      networkMonitor = null;
    }
  };
}

exports.setupParentProcess = setupParentProcess;

/**
 * The NetworkMonitorParent runs in the parent process and uses the message manager to
 * listen for requests from the child process to start/stop the network monitor.  Most
 * request data is only available from the parent process, so that's why the network
 * monitor needs to run there when debugging tabs that are in the child.
 *
 * @param nsIMessageManager mm
 *        The message manager for the browser we're filtering on.
 * @param string prefix
 *        The RDP connection prefix that uniquely identifies the connection.
 */
function NetworkMonitorParent(mm, prefix) {
  this.onNetMonitorMessage = this.onNetMonitorMessage.bind(this);
  this.onNetworkEvent = this.onNetworkEvent.bind(this);
  this.setMessageManager(mm);
}

NetworkMonitorParent.prototype = {
  netMonitor: null,
  messageManager: null,

  setMessageManager(mm) {
    if (this.messageManager) {
      let oldMM = this.messageManager;
      oldMM.removeMessageListener("debug:netmonitor", this.onNetMonitorMessage);
    }
    this.messageManager = mm;
    if (mm) {
      mm.addMessageListener("debug:netmonitor", this.onNetMonitorMessage);
    }
  },

  /**
   * Handler for "debug:netmonitor" messages received through the message manager
   * from the content process.
   *
   * @param object msg
   *        Message from the content.
   */
  onNetMonitorMessage: DevToolsUtils.makeInfallible(function (msg) {
    let {action} = msg.json;
    // Pipe network monitor data from parent to child via the message manager.
    switch (action) {
      case "start":
        if (!this.netMonitor) {
          let {appId, outerWindowID} = msg.json;
          this.netMonitor = new NetworkMonitor({
            outerWindowID,
            appId,
          }, this);
          this.netMonitor.init();
        }
        break;
      case "setPreferences": {
        let {preferences} = msg.json;
        for (let key of Object.keys(preferences)) {
          if ((key == "saveRequestAndResponseBodies" ||
               key == "throttleData") && this.netMonitor) {
            this.netMonitor[key] = preferences[key];
          }
        }
        break;
      }

      case "stop":
        if (this.netMonitor) {
          this.netMonitor.destroy();
          this.netMonitor = null;
        }
        break;

      case "disconnect":
        this.destroy();
        break;
    }
  }),

  /**
   * Handler for new network requests. This method is invoked by the current
   * NetworkMonitor instance.
   *
   * @param object event
   *        Object describing the network request.
   * @return object
   *         A NetworkEventActorProxy instance which is notified when further
   *         data about the request is available.
   */
  onNetworkEvent: DevToolsUtils.makeInfallible(function (event) {
    return new NetworkEventActorProxy(this.messageManager).init(event);
  }),

  destroy: function () {
    this.setMessageManager(null);

    if (this.netMonitor) {
      this.netMonitor.destroy();
      this.netMonitor = null;
    }
  },
};

/**
 * A WebProgressListener that listens for location changes.
 *
 * This progress listener is used to track file loads and other kinds of
 * location changes.
 *
 * @constructor
 * @param object window
 *        The window for which we need to track location changes.
 * @param object owner
 *        The listener owner which needs to implement two methods:
 *        - onFileActivity(aFileURI)
 *        - onLocationChange(aState, aTabURI, aPageTitle)
 */
function ConsoleProgressListener(window, owner) {
  this.window = window;
  this.owner = owner;
}
exports.ConsoleProgressListener = ConsoleProgressListener;

ConsoleProgressListener.prototype = {
  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor file loads.
   */
  MONITOR_FILE_ACTIVITY: 1,

  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor page location changes.
   */
  MONITOR_LOCATION_CHANGE: 2,

  /**
   * Tells if you want to monitor file activity.
   * @private
   * @type boolean
   */
  _fileActivity: false,

  /**
   * Tells if you want to monitor location changes.
   * @private
   * @type boolean
   */
  _locationChange: false,

  /**
   * Tells if the console progress listener is initialized or not.
   * @private
   * @type boolean
   */
  _initialized: false,

  _webProgress: null,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  /**
   * Initialize the ConsoleProgressListener.
   * @private
   */
  _init: function () {
    if (this._initialized) {
      return;
    }

    this._webProgress = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIWebProgress);
    this._webProgress.addProgressListener(this,
                                          Ci.nsIWebProgress.NOTIFY_STATE_ALL);

    this._initialized = true;
  },

  /**
   * Start a monitor/tracker related to the current nsIWebProgressListener
   * instance.
   *
   * @param number monitor
   *        Tells what you want to track. Available constants:
   *        - this.MONITOR_FILE_ACTIVITY
   *          Track file loads.
   *        - this.MONITOR_LOCATION_CHANGE
   *          Track location changes for the top window.
   */
  startMonitor: function (monitor) {
    switch (monitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = true;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = true;
        break;
      default:
        throw new Error("ConsoleProgressListener: unknown monitor type " +
                        monitor + "!");
    }
    this._init();
  },

  /**
   * Stop a monitor.
   *
   * @param number monitor
   *        Tells what you want to stop tracking. See this.startMonitor() for
   *        the list of constants.
   */
  stopMonitor: function (monitor) {
    switch (monitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = false;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = false;
        break;
      default:
        throw new Error("ConsoleProgressListener: unknown monitor type " +
                        monitor + "!");
    }

    if (!this._fileActivity && !this._locationChange) {
      this.destroy();
    }
  },

  onStateChange: function (progress, request, state, status) {
    if (!this.owner) {
      return;
    }

    if (this._fileActivity) {
      this._checkFileActivity(progress, request, state, status);
    }

    if (this._locationChange) {
      this._checkLocationChange(progress, request, state, status);
    }
  },

  /**
   * Check if there is any file load, given the arguments of
   * nsIWebProgressListener.onStateChange. If the state change tells that a file
   * URI has been loaded, then the remote Web Console instance is notified.
   * @private
   */
  _checkFileActivity: function (progress, request, state, status) {
    if (!(state & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    let uri = null;
    if (request instanceof Ci.imgIRequest) {
      let imgIRequest = request.QueryInterface(Ci.imgIRequest);
      uri = imgIRequest.URI;
    } else if (request instanceof Ci.nsIChannel) {
      let nsIChannel = request.QueryInterface(Ci.nsIChannel);
      uri = nsIChannel.URI;
    }

    if (!uri || !uri.schemeIs("file") && !uri.schemeIs("ftp")) {
      return;
    }

    this.owner.onFileActivity(uri.spec);
  },

  /**
   * Check if the current window.top location is changing, given the arguments
   * of nsIWebProgressListener.onStateChange. If that is the case, the remote
   * Web Console instance is notified.
   * @private
   */
  _checkLocationChange: function (progress, request, state) {
    let isStart = state & Ci.nsIWebProgressListener.STATE_START;
    let isStop = state & Ci.nsIWebProgressListener.STATE_STOP;
    let isNetwork = state & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
    let isWindow = state & Ci.nsIWebProgressListener.STATE_IS_WINDOW;

    // Skip non-interesting states.
    if (!isNetwork || !isWindow || progress.DOMWindow != this.window) {
      return;
    }

    if (isStart && request instanceof Ci.nsIChannel) {
      this.owner.onLocationChange("start", request.URI.spec, "");
    } else if (isStop) {
      this.owner.onLocationChange("stop", this.window.location.href,
                                  this.window.document.title);
    }
  },

  onLocationChange: function () {},
  onStatusChange: function () {},
  onProgressChange: function () {},
  onSecurityChange: function () {},

  /**
   * Destroy the ConsoleProgressListener.
   */
  destroy: function () {
    if (!this._initialized) {
      return;
    }

    this._initialized = false;
    this._fileActivity = false;
    this._locationChange = false;

    try {
      this._webProgress.removeProgressListener(this);
    } catch (ex) {
      // This can throw during browser shutdown.
    }

    this._webProgress = null;
    this.window = null;
    this.owner = null;
  },
};

function gSequenceId() {
  return gSequenceId.n++;
}
gSequenceId.n = 1;
