/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cr, components: Components } = require("chrome");

loader.lazyRequireGetter(
  this,
  "NetworkHelper",
  "devtools/shared/webconsole/network-helper"
);
loader.lazyRequireGetter(
  this,
  "CacheEntry",
  "devtools/shared/platform/cache-entry",
  true
);
loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");

// Network logging

/**
 * The network response listener implements the nsIStreamListener and
 * nsIRequestObserver interfaces. This is used within the NetworkObserver feature
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
 *        HttpActivity object associated with this request. See NetworkObserver
 *        for more information.
 * @param Map decodedCertificateCache
 *        A Map of certificate fingerprints to decoded certificates, to avoid
 *        repeatedly decoding previously-seen certificates.
 */
function NetworkResponseListener(owner, httpActivity, decodedCertificateCache) {
  this.owner = owner;
  this.receivedData = "";
  this.httpActivity = httpActivity;
  this.bodySize = 0;
  // Indicates if the response had a size greater than response body limit.
  this.truncated = false;
  // Note that this is really only needed for the non-e10s case.
  // See bug 1309523.
  const channel = this.httpActivity.channel;
  this._wrappedNotificationCallbacks = channel.notificationCallbacks;
  channel.notificationCallbacks = this;
  this._decodedCertificateCache = decodedCertificateCache;
}

exports.NetworkResponseListener = NetworkResponseListener;

NetworkResponseListener.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIStreamListener",
    "nsIInputStreamCallback",
    "nsIRequestObserver",
    "nsIInterfaceRequestor",
  ]),

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
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
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
      const impl = this._wrappedNotificationCallbacks.getInterface(iid);
      impl[method].apply(impl, args);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_NO_INTERFACE) {
        throw e;
      }
    }
  },

  /**
   * This NetworkResponseListener tracks the NetworkObserver.openResponses object
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
   * A Map of certificate fingerprints to decoded certificates, to avoid repeatedly
   * decoding previously-seen certificates.
   */
  _decodedCertificateCache: null,

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
   * Response size on the wire, potentially compressed / encoded.
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
  setAsyncListener(stream, listener) {
    // Asynchronously wait for the stream to be readable or closed.
    stream.asyncWait(listener, 0, 0, Services.tm.mainThread);
  },

  /**
   * Stores the received data, if request/response body logging is enabled. It
   * also does limit the number of stored bytes, based on the
   * `devtools.netmonitor.responseBodyLimit` pref.
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
  onDataAvailable(request, inputStream, offset, count) {
    this._findOpenResponse();
    const data = NetUtil.readInputStreamToString(inputStream, count);

    this.bodySize += count;

    if (!this.httpActivity.discardResponseBody) {
      const limit = Services.prefs.getIntPref(
        "devtools.netmonitor.responseBodyLimit"
      );
      if (this.receivedData.length <= limit || limit == 0) {
        this.receivedData += NetworkHelper.convertToUnicode(
          data,
          request.contentCharset
        );
      }
      if (this.receivedData.length > limit && limit > 0) {
        this.receivedData = this.receivedData.substr(0, limit);
        this.truncated = true;
      }
    }
  },

  /**
   * See documentation at
   * https://developer.mozilla.org/En/NsIRequestObserver
   *
   * @param nsIRequest request
   * @param nsISupports context
   */
  onStartRequest(request) {
    request = request.QueryInterface(Ci.nsIChannel);
    // Converter will call this again, we should just ignore that.
    if (this.request) {
      return;
    }

    this.request = request;
    this._onSecurityInfo = this._getSecurityInfo();
    this._findOpenResponse();
    // We need to track the offset for the onDataAvailable calls where
    // we pass the data from our pipe to the converter.
    this.offset = 0;

    const channel = this.request;

    // Bug 1372115 - We should load bytecode cached requests from cache as the actual
    // channel content is going to be optimized data that reflects platform internals
    // instead of the content user expects (i.e. content served by HTTP server)
    // Note that bytecode cached is one example, there may be wasm or other usecase in
    // future.
    let isOptimizedContent = false;
    try {
      if (channel instanceof Ci.nsICacheInfoChannel) {
        isOptimizedContent = channel.alternativeDataType;
      }
    } catch (e) {
      // Accessing `alternativeDataType` for some SW requests throws.
    }
    if (isOptimizedContent) {
      let charset;
      try {
        charset = this.request.contentCharset;
      } catch (e) {
        // Accessing the charset sometimes throws NS_ERROR_NOT_AVAILABLE when
        // reloading the page
      }
      if (!charset) {
        charset = this.httpActivity.charset;
      }
      NetworkHelper.loadFromCache(
        this.httpActivity.url,
        charset,
        this._onComplete.bind(this)
      );
      return;
    }

    // In the multi-process mode, the conversion happens on the child
    // side while we can only monitor the channel on the parent
    // side. If the content is gzipped, we have to unzip it
    // ourself. For that we use the stream converter services.  Do not
    // do that for Service workers as they are run in the child
    // process.
    if (
      !this.httpActivity.fromServiceWorker &&
      channel instanceof Ci.nsIEncodedChannel &&
      channel.contentEncodings &&
      !channel.applyConversion
    ) {
      const encodingHeader = channel.getResponseHeader("Content-Encoding");
      const scs = Cc["@mozilla.org/streamConverters;1"].getService(
        Ci.nsIStreamConverterService
      );
      const encodings = encodingHeader.split(/\s*\t*,\s*\t*/);
      let nextListener = this;
      const acceptedEncodings = [
        "gzip",
        "deflate",
        "br",
        "x-gzip",
        "x-deflate",
      ];
      for (const i in encodings) {
        // There can be multiple conversions applied
        const enc = encodings[i].toLowerCase();
        if (acceptedEncodings.indexOf(enc) > -1) {
          this.converter = scs.asyncConvertData(
            enc,
            "uncompressed",
            nextListener,
            null
          );
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
  async _getSecurityInfo() {
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
    const secinfo = this.httpActivity.channel.securityInfo;
    if (secinfo) {
      secinfo.QueryInterface(Ci.nsITransportSecurityInfo);
    }
    const info = await NetworkHelper.parseSecurityInfo(
      secinfo,
      this.request.loadInfo.originAttributes,
      this.httpActivity,
      this._decodedCertificateCache
    );
    let isRacing = false;
    try {
      const channel = this.httpActivity.channel;
      if (channel instanceof Ci.nsICacheInfoChannel) {
        isRacing = channel.isRacing();
      }
    } catch (err) {
      // See the following bug for more details:
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1582589
    }

    this.httpActivity.owner.addSecurityInfo(info, isRacing);
  },

  /**
   * Fetches cache information from CacheEntry
   * @private
   */
  _fetchCacheInformation() {
    const httpActivity = this.httpActivity;
    CacheEntry.getCacheEntry(this.request, descriptor => {
      httpActivity.owner.addResponseCache({
        responseCache: descriptor,
      });
    });
  },

  /**
   * Handle the onStopRequest by closing the sink output stream.
   *
   * For more documentation about nsIRequestObserver go to:
   * https://developer.mozilla.org/En/NsIRequestObserver
   */
  onStopRequest() {
    // Bug 1429365: onStopRequest may be called after onComplete for resources loaded
    // from bytecode cache.
    if (!this.httpActivity) {
      return;
    }
    this._findOpenResponse();
    this.sink.outputStream.close();
  },

  // nsIProgressEventSink implementation

  /**
   * Handle progress event as data is transferred.  This is used to record the
   * size on the wire, which may be compressed / encoded.
   */
  onProgress(request, progress, progressMax) {
    this.transferredSize = progress;
    // Need to forward as well to keep things like Download Manager's progress
    // bar working properly.
    this._forwardNotification(Ci.nsIProgressEventSink, "onProgress", arguments);
  },

  onStatus() {
    this._forwardNotification(Ci.nsIProgressEventSink, "onStatus", arguments);
  },

  /**
   * Find the open response object associated to the current request. The
   * NetworkObserver._httpResponseExaminer() method saves the response headers in
   * NetworkObserver.openResponses. This method takes the data from the open
   * response object and puts it into the HTTP activity object, then sends it to
   * the remote Web Console instance.
   *
   * @private
   */
  _findOpenResponse() {
    if (!this.owner || this._foundOpenResponse) {
      return;
    }

    const channel = this.httpActivity.channel;
    const openResponse = this.owner.openResponses.getChannelById(
      channel.channelId
    );

    if (!openResponse) {
      return;
    }

    this._foundOpenResponse = true;
    this.owner.openResponses.delete(channel);

    this.httpActivity.owner.addResponseHeaders(openResponse.headers);
    this.httpActivity.owner.addResponseCookies(openResponse.cookies);
  },

  /**
   * Clean up the response listener once the response input stream is closed.
   * This is called from onStopRequest() or from onInputStreamReady() when the
   * stream is closed.
   * @return void
   */
  onStreamClose() {
    if (!this.httpActivity) {
      return;
    }
    // Remove our listener from the request input stream.
    this.setAsyncListener(this.sink.inputStream, null);

    this._findOpenResponse();
    if (this.request.fromCache || this.httpActivity.responseStatus == 304) {
      this._fetchCacheInformation();
    }

    if (!this.httpActivity.discardResponseBody && this.receivedData.length) {
      this._onComplete(this.receivedData);
    } else if (
      !this.httpActivity.discardResponseBody &&
      this.httpActivity.responseStatus == 304
    ) {
      // Response is cached, so we load it from cache.
      let charset;
      try {
        charset = this.request.contentCharset;
      } catch (e) {
        // Accessing the charset sometimes throws NS_ERROR_NOT_AVAILABLE when
        // reloading the page
      }
      if (!charset) {
        charset = this.httpActivity.charset;
      }
      NetworkHelper.loadFromCache(
        this.httpActivity.url,
        charset,
        this._onComplete.bind(this)
      );
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
  _onComplete(data) {
    // Make sure all the security and response content info are sent
    this._getResponseContent(data);
    this._onSecurityInfo.then(() => this._destroy());
  },

  /**
   * Create the response object and send it to the client.
   */
  _getResponseContent(data) {
    const response = {
      mimeType: "",
      text: data || "",
    };

    response.size = this.bodySize;
    response.transferredSize =
      this.transferredSize + this.httpActivity.headersSize;

    try {
      response.mimeType = this.request.contentType;
    } catch (ex) {
      // Ignore.
    }

    if (
      !response.mimeType ||
      !NetworkHelper.isTextMimeType(response.mimeType)
    ) {
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

    let id;
    let reason;

    try {
      const properties = this.request.QueryInterface(Ci.nsIPropertyBag);
      reason = this.request.loadInfo.requestBlockingReason;
      id = properties.getProperty("cancelledByExtension");

      // WebExtensionPolicy is not available for workers
      if (typeof WebExtensionPolicy !== "undefined") {
        id = WebExtensionPolicy.getByID(id).name;
      }
    } catch (err) {
      // "cancelledByExtension" doesn't have to be available.
    }

    this.httpActivity.owner.addResponseContent(response, {
      discardResponseBody: this.httpActivity.discardResponseBody,
      truncated: this.truncated,
      blockedReason: reason,
      blockingExtension: id,
    });
  },

  _destroy() {
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
  onInputStreamReady(stream) {
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
          this.converter.onDataAvailable(
            this.request,
            stream,
            this.offset,
            available
          );
        } else {
          this.onDataAvailable(this.request, stream, this.offset, available);
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
