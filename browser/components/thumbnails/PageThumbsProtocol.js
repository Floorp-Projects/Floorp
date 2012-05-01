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
    let {url} = parseURI(aURI);
    let file = PageThumbsStorage.getFileForURL(url);

    if (file.exists()) {
      let fileuri = Services.io.newFileURI(file);
      return Services.io.newChannelFromURI(fileuri);
    }

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
  _uri: null,
  _referrer: null,
  _canceled: false,
  _status: Cr.NS_OK,
  _isPending: false,
  _wasOpened: false,
  _responseText: "OK",
  _responseStatus: 200,
  _responseHeaders: null,
  _requestMethod: "GET",
  _requestStarted: false,
  _allowPipelining: true,
  _requestSucceeded: true,

  /* :::::::: nsIChannel ::::::::::::::: */

  get URI() this._uri,
  owner: null,
  notificationCallbacks: null,
  get securityInfo() null,

  contentType: PageThumbs.contentType,
  contentCharset: null,
  contentLength: -1,

  get contentDisposition() {
    throw (Components.returnCode = Cr.NS_ERROR_NOT_AVAILABLE);
  },

  get contentDispositionFilename() {
    throw (Components.returnCode = Cr.NS_ERROR_NOT_AVAILABLE);
  },

  get contentDispositionHeader() {
    throw (Components.returnCode = Cr.NS_ERROR_NOT_AVAILABLE);
  },

  open: function Channel_open() {
    throw (Components.returnCode = Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  asyncOpen: function Channel_asyncOpen(aListener, aContext) {
    if (this._isPending)
      throw (Components.returnCode = Cr.NS_ERROR_IN_PROGRESS);

    if (this._wasOpened)
      throw (Components.returnCode = Cr.NS_ERROR_ALREADY_OPENED);

    if (this._canceled)
      return (Components.returnCode = this._status);

    this._isPending = true;
    this._wasOpened = true;

    this._listener = aListener;
    this._context = aContext;

    if (this.loadGroup)
      this.loadGroup.addRequest(this, null);

    if (this._canceled)
      return;

    let {url} = parseURI(this._uri);
    if (!url) {
      this._serveThumbnailNotFound();
      return;
    }

    PageThumbsCache.getReadEntry(url, function (aEntry) {
      let inputStream = aEntry && aEntry.openInputStream(0);
      if (!inputStream || !inputStream.available()) {
        if (aEntry)
          aEntry.close();
        this._serveThumbnailNotFound();
        return;
      }

      this._entry = aEntry;
      this._pump = Cc["@mozilla.org/network/input-stream-pump;1"].
                   createInstance(Ci.nsIInputStreamPump);

      this._pump.init(inputStream, -1, -1, 0, 0, true);
      this._pump.asyncRead(this, null);

      this._trackThumbnailHitOrMiss(true);
    }.bind(this));
  },

  /**
   * Serves a "404 Not Found" if we didn't find the requested thumbnail.
   */
  _serveThumbnailNotFound: function Channel_serveThumbnailNotFound() {
    this._responseStatus = 404;
    this._responseText = "Not Found";
    this._requestSucceeded = false;

    this.onStartRequest(this, null);
    this.onStopRequest(this, null, Cr.NS_OK);

    this._trackThumbnailHitOrMiss(false);
  },

  /**
   * Implements telemetry tracking for thumbnail cache hits and misses.
   * @param aFound Whether the thumbnail was found.
   */
  _trackThumbnailHitOrMiss: function Channel_trackThumbnailHitOrMiss(aFound) {
    Services.telemetry.getHistogramById("FX_THUMBNAILS_HIT_OR_MISS")
      .add(aFound);
  },

  /* :::::::: nsIStreamListener ::::::::::::::: */

  onStartRequest: function Channel_onStartRequest(aRequest, aContext) {
    if (!this.canceled && Components.isSuccessCode(this._status))
      this._status = aRequest.status;

    this._requestStarted = true;
    this._listener.onStartRequest(this, this._context);
  },

  onDataAvailable: function Channel_onDataAvailable(aRequest, aContext,
                                                    aInStream, aOffset, aCount) {
    this._listener.onDataAvailable(this, this._context, aInStream, aOffset, aCount);
  },

  onStopRequest: function Channel_onStopRequest(aRequest, aContext, aStatus) {
    this._isPending = false;
    this._status = aStatus;

    this._listener.onStopRequest(this, this._context, aStatus);
    this._listener = null;
    this._context = null;

    if (this._entry)
      this._entry.close();

    if (this.loadGroup)
      this.loadGroup.removeRequest(this, null, aStatus);
  },

  /* :::::::: nsIRequest ::::::::::::::: */

  get status() this._status,
  get name() this._uri.spec,
  isPending: function Channel_isPending() this._isPending,

  loadFlags: Ci.nsIRequest.LOAD_NORMAL,
  loadGroup: null,

  cancel: function Channel_cancel(aStatus) {
    if (this._canceled)
      return;

    this._canceled = true;
    this._status = aStatus;

    if (this._pump)
      this._pump.cancel(aStatus);
  },

  suspend: function Channel_suspend() {
    if (this._pump)
      this._pump.suspend();
  },

  resume: function Channel_resume() {
    if (this._pump)
      this._pump.resume();
  },

  /* :::::::: nsIHttpChannel ::::::::::::::: */

  get referrer() this._referrer,

  set referrer(aReferrer) {
    if (this._wasOpened)
      throw (Components.returnCode = Cr.NS_ERROR_IN_PROGRESS);

    this._referrer = aReferrer;
  },

  get requestMethod() this._requestMethod,

  set requestMethod(aMethod) {
    if (this._wasOpened)
      throw (Components.returnCode = Cr.NS_ERROR_IN_PROGRESS);

    this._requestMethod = aMethod.toUpperCase();
  },

  get allowPipelining() this._allowPipelining,

  set allowPipelining(aAllow) {
    if (this._wasOpened)
      throw (Components.returnCode = Cr.NS_ERROR_FAILURE);

    this._allowPipelining = aAllow;
  },

  redirectionLimit: 10,

  get responseStatus() {
    if (this._requestStarted)
      throw (Components.returnCode = Cr.NS_ERROR_NOT_AVAILABLE);

    return this._responseStatus;
  },

  get responseStatusText() {
    if (this._requestStarted)
      throw (Components.returnCode = Cr.NS_ERROR_NOT_AVAILABLE);

    return this._responseText;
  },

  get requestSucceeded() {
    if (this._requestStarted)
      throw (Components.returnCode = Cr.NS_ERROR_NOT_AVAILABLE);

    return this._requestSucceeded;
  },

  isNoCacheResponse: function Channel_isNoCacheResponse() false,
  isNoStoreResponse: function Channel_isNoStoreResponse() false,

  getRequestHeader: function Channel_getRequestHeader() {
    throw (Components.returnCode = Cr.NS_ERROR_NOT_AVAILABLE);
  },

  setRequestHeader: function Channel_setRequestHeader() {
    if (this._wasOpened)
      throw (Components.returnCode = Cr.NS_ERROR_IN_PROGRESS);
  },

  visitRequestHeaders: function Channel_visitRequestHeaders() {},

  getResponseHeader: function Channel_getResponseHeader(aHeader) {
    let name = aHeader.toLowerCase();
    if (name in this._responseHeaders)
      return this._responseHeaders[name];

    throw (Components.returnCode = Cr.NS_ERROR_NOT_AVAILABLE);
  },

  setResponseHeader: function Channel_setResponseHeader(aHeader, aValue, aMerge) {
    let name = aHeader.toLowerCase();
    if (!aValue && !aMerge)
      delete this._responseHeaders[name];
    else
      this._responseHeaders[name] = aValue;
  },

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

  /* :::::::: nsIHttpChannelInternal ::::::::::::::: */

  documentURI: null,
  get canceled() this._canceled,
  allowSpdy: false,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannel,
                                         Ci.nsIHttpChannel,
                                         Ci.nsIHttpChannelInternal,
                                         Ci.nsIRequest])
}

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
