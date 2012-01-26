/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * PageThumbsProtocol.js
 *
 * This file implements the moz-page-thumb:// protocol and the corresponding
 * channel delivering cached thumbnails.
 *
 * URL structure:
 *
 * moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.org%2F
 *
 * This URL requests an image for 'http://www.mozilla.org/'.
 */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Cr = Components.results;
const Ci = Components.interfaces;

Cu.import("resource:///modules/PageThumbs.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

/**
 * Implements the thumbnail protocol handler responsible for moz-page-thumb: URIs.
 */
function Protocol() {
}

Protocol.prototype = {
  /**
   * The scheme used by this protocol.
   */
  get scheme() PageThumbs.scheme,

  /**
   * The default port for this protocol (we don't support ports).
   */
  get defaultPort() -1,

  /**
   * The flags specific to this protocol implementation.
   */
  get protocolFlags() {
    return Ci.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD |
           Ci.nsIProtocolHandler.URI_NORELATIVE |
           Ci.nsIProtocolHandler.URI_NOAUTH;
  },

  /**
   * Creates a new URI object that is suitable for loading by this protocol.
   * @param aSpec The URI string in UTF8 encoding.
   * @param aOriginCharset The charset of the document from which the URI originated.
   * @return The newly created URI.
   */
  newURI: function Proto_newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  /**
   * Constructs a new channel from the given URI for this protocol handler.
   * @param aURI The URI for which to construct a channel.
   * @return The newly created channel.
   */
  newChannel: function Proto_newChannel(aURI) {
    return new Channel(aURI);
  },

  /**
   * Decides whether to allow a blacklisted port.
   * @return Always false, we'll never allow ports.
   */
  allowPort: function () false,

  classID: Components.ID("{5a4ae9b5-f475-48ae-9dce-0b4c1d347884}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler])
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([Protocol]);

/**
 * A channel implementation responsible for delivering cached thumbnails.
 */
function Channel(aURI) {
  this._uri = aURI;

  // nsIChannel
  this.originalURI = aURI;

  // nsIHttpChannel
  this._responseHeaders = {"content-type": PageThumbs.contentType};
}

Channel.prototype = {
  /**
   * Tracks if the channel has been opened, yet.
   */
  _wasOpened: false,

  /**
   * Opens this channel asynchronously.
   * @param aListener The listener that receives the channel data when available.
   * @param aContext A custom context passed to the listener's methods.
   */
  asyncOpen: function Channel_asyncOpen(aListener, aContext) {
    if (this._wasOpened)
      throw Cr.NS_ERROR_ALREADY_OPENED;

    if (this.canceled)
      return;

    this._listener = aListener;
    this._context = aContext;

    this._isPending = true;
    this._wasOpened = true;

    // Try to read the data from the thumbnail cache.
    this._readCache(function (aData) {
      // Update response if there's no data.
      if (!aData) {
        this._responseStatus = 404;
        this._responseText = "Not Found";
      }

      this._startRequest();

      if (!this.canceled) {
        this._addToLoadGroup();

        if (aData)
          this._serveData(aData);

        if (!this.canceled)
          this._stopRequest();
      }
    }.bind(this));
  },

  /**
   * Reads a data stream from the cache entry.
   * @param aCallback The callback the data is passed to.
   */
  _readCache: function Channel_readCache(aCallback) {
    let {url} = parseURI(this._uri);

    // Return early if there's no valid URL given.
    if (!url) {
      aCallback(null);
      return;
    }

    // Try to get a cache entry.
    PageThumbsCache.getReadEntry(url, function (aEntry) {
      let inputStream = aEntry && aEntry.openInputStream(0);

      function closeEntryAndFinish(aData) {
        if (aEntry) {
          aEntry.close();
        }
        aCallback(aData);
      }

      // Check if we have a valid entry and if it has any data.
      if (!inputStream || !inputStream.available()) {
        closeEntryAndFinish();
        return;
      }

      try {
        // Read the cache entry's data.
        NetUtil.asyncFetch(inputStream, function (aData, aStatus) {
          // We might have been canceled while waiting.
          if (this.canceled)
            return;

          // Check if we have a valid data stream.
          if (!Components.isSuccessCode(aStatus) || !aData.available())
            aData = null;

          closeEntryAndFinish(aData);
        }.bind(this));
      } catch (e) {
        closeEntryAndFinish();
      }
    }.bind(this));
  },

  /**
   * Calls onStartRequest on the channel listener.
   */
  _startRequest: function Channel_startRequest() {
    try {
      this._listener.onStartRequest(this, this._context);
    } catch (e) {
      // The listener might throw if the request has been canceled.
      this.cancel(Cr.NS_BINDING_ABORTED);
    }
  },

  /**
   * Calls onDataAvailable on the channel listener and passes the data stream.
   * @param aData The data to be delivered.
   */
  _serveData: function Channel_serveData(aData) {
    try {
      let available = aData.available();
      this._listener.onDataAvailable(this, this._context, aData, 0, available);
    } catch (e) {
      // The listener might throw if the request has been canceled.
      this.cancel(Cr.NS_BINDING_ABORTED);
    }
  },

  /**
   * Calls onStopRequest on the channel listener.
   */
  _stopRequest: function Channel_stopRequest() {
    try {
      this._listener.onStopRequest(this, this._context, this.status);
    } catch (e) {
      // This might throw but is generally ignored.
    }

    // The request has finished, clean up after ourselves.
    this._cleanup();
  },

  /**
   * Adds this request to the load group, if any.
   */
  _addToLoadGroup: function Channel_addToLoadGroup() {
    if (this.loadGroup)
      this.loadGroup.addRequest(this, this._context);
  },

  /**
   * Removes this request from its load group, if any.
   */
  _removeFromLoadGroup: function Channel_removeFromLoadGroup() {
    if (!this.loadGroup)
      return;

    try {
      this.loadGroup.removeRequest(this, this._context, this.status);
    } catch (e) {
      // This might throw but is ignored.
    }
  },

  /**
   * Cleans up the channel when the request has finished.
   */
  _cleanup: function Channel_cleanup() {
    this._removeFromLoadGroup();
    this.loadGroup = null;

    this._isPending = false;

    delete this._listener;
    delete this._context;
  },

  /* :::::::: nsIChannel ::::::::::::::: */

  contentType: PageThumbs.contentType,
  contentLength: -1,
  owner: null,
  contentCharset: null,
  notificationCallbacks: null,

  get URI() this._uri,
  get securityInfo() null,

  /**
   * Opens this channel synchronously. Not supported.
   */
  open: function Channel_open() {
    // Synchronous data delivery is not implemented.
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /* :::::::: nsIHttpChannel ::::::::::::::: */

  redirectionLimit: 10,
  requestMethod: "GET",
  allowPipelining: true,
  referrer: null,

  get requestSucceeded() true,

  _responseStatus: 200,
  get responseStatus() this._responseStatus,

  _responseText: "OK",
  get responseStatusText() this._responseText,

  /**
   * Checks if the server sent the equivalent of a "Cache-control: no-cache"
   * response header.
   * @return Always false.
   */
  isNoCacheResponse: function () false,

  /**
   * Checks if the server sent the equivalent of a "Cache-control: no-cache"
   * response header.
   * @return Always false.
   */
  isNoStoreResponse: function () false,

  /**
   * Returns the value of a particular request header. Not implemented.
   */
  getRequestHeader: function Channel_getRequestHeader() {
    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  /**
   * This method is called to set the value of a particular request header.
   * Not implemented.
   */
  setRequestHeader: function Channel_setRequestHeader() {
    if (this._wasOpened)
      throw Cr.NS_ERROR_IN_PROGRESS;
  },

  /**
   * Call this method to visit all request headers. Not implemented.
   */
  visitRequestHeaders: function () {},

  /**
   * Gets the value of a particular response header.
   * @param aHeader The case-insensitive name of the response header to query.
   * @return The header value.
   */
  getResponseHeader: function Channel_getResponseHeader(aHeader) {
    let name = aHeader.toLowerCase();
    if (name in this._responseHeaders)
      return this._responseHeaders[name];

    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  /**
   * This method is called to set the value of a particular response header.
   * @param aHeader The case-insensitive name of the response header to query.
   * @param aValue The response header value to set.
   */
  setResponseHeader: function Channel_setResponseHeader(aHeader, aValue, aMerge) {
    let name = aHeader.toLowerCase();
    if (!aValue && !aMerge)
      delete this._responseHeaders[name];
    else
      this._responseHeaders[name] = aValue;
  },

  /**
   * Call this method to visit all response headers.
   * @param aVisitor The header visitor.
   */
  visitResponseHeaders: function Channel_visitResponseHeaders(aVisitor) {
    for (let name in this._responseHeaders) {
      let value = this._responseHeaders[name];

      try {
        aVisitor.visitHeader(name, value);
      } catch (e) {
        // The visitor can throw to stop the iteration.
        return;
      }
    }
  },

  /* :::::::: nsIRequest ::::::::::::::: */

  loadFlags: Ci.nsIRequest.LOAD_NORMAL,
  loadGroup: null,

  get name() this._uri.spec,

  _status: Cr.NS_OK,
  get status() this._status,

  _isPending: false,
  isPending: function () this._isPending,

  resume: function () {},
  suspend: function () {},

  /**
   * Cancels this request.
   * @param aStatus The reason for cancelling.
   */
  cancel: function Channel_cancel(aStatus) {
    if (this.canceled)
      return;

    this._isCanceled = true;
    this._status = aStatus;

    this._cleanup();
  },

  /* :::::::: nsIHttpChannelInternal ::::::::::::::: */

  documentURI: null,

  _isCanceled: false,
  get canceled() this._isCanceled,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannel,
                                         Ci.nsIHttpChannel,
                                         Ci.nsIHttpChannelInternal,
                                         Ci.nsIRequest])
};

/**
 * Parses a given URI and extracts all parameters relevant to this protocol.
 * @param aURI The URI to parse.
 * @return The parsed parameters.
 */
function parseURI(aURI) {
  let {scheme, staticHost} = PageThumbs;
  let re = new RegExp("^" + scheme + "://" + staticHost + ".*?\\?");
  let query = aURI.spec.replace(re, "");
  let params = {};

  query.split("&").forEach(function (aParam) {
    let [key, value] = aParam.split("=").map(decodeURIComponent);
    params[key.toLowerCase()] = value;
  });

  return params;
}
