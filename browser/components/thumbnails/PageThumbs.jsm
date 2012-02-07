/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let EXPORTED_SYMBOLS = ["PageThumbs", "PageThumbsCache"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const HTML_NAMESPACE = "http://www.w3.org/1999/xhtml";

/**
 * The default width for page thumbnails.
 *
 * Hint: This is the default value because the 'New Tab Page' is the only
 *       client for now.
 */
const THUMBNAIL_WIDTH = 201;

/**
 * The default height for page thumbnails.
 *
 * Hint: This is the default value because the 'New Tab Page' is the only
 *       client for now.
 */
const THUMBNAIL_HEIGHT = 127;

/**
 * The default background color for page thumbnails.
 */
const THUMBNAIL_BG_COLOR = "#fff";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

/**
 * Singleton providing functionality for capturing web page thumbnails and for
 * accessing them if already cached.
 */
let PageThumbs = {
  /**
   * The scheme to use for thumbnail urls.
   */
  get scheme() "moz-page-thumb",

  /**
   * The static host to use for thumbnail urls.
   */
  get staticHost() "thumbnail",

  /**
   * The thumbnails' image type.
   */
  get contentType() "image/png",

  /**
   * Gets the thumbnail image's url for a given web page's url.
   * @param aUrl The web page's url that is depicted in the thumbnail.
   * @return The thumbnail image's url.
   */
  getThumbnailURL: function PageThumbs_getThumbnailURL(aUrl) {
    return this.scheme + "://" + this.staticHost +
           "?url=" + encodeURIComponent(aUrl);
  },

  /**
   * Creates a canvas containing a thumbnail depicting the given window.
   * @param aWindow The DOM window to capture a thumbnail from.
   * @return The newly created canvas containing the image data.
   */
  capture: function PageThumbs_capture(aWindow) {
    let telemetryCaptureTime = new Date();
    let [sw, sh, scale] = this._determineCropSize(aWindow);

    let canvas = this._createCanvas();
    let ctx = canvas.getContext("2d");

    // Scale the canvas accordingly.
    ctx.scale(scale, scale);

    try {
      // Draw the window contents to the canvas.
      ctx.drawWindow(aWindow, 0, 0, sw, sh, THUMBNAIL_BG_COLOR,
                     ctx.DRAWWINDOW_DO_NOT_FLUSH);
    } catch (e) {
      // We couldn't draw to the canvas for some reason.
    }

    Services.telemetry.getHistogramById("FX_THUMBNAILS_CAPTURE_TIME_MS")
      .add(new Date() - telemetryCaptureTime);

    return canvas;
  },

  /**
   * Stores the image data contained in the given canvas to the underlying
   * storage.
   * @param aKey The key to use for the storage.
   * @param aCanvas The canvas containing the thumbnail's image data.
   * @param aCallback The function to be called when the canvas data has been
   *                  stored (optional).
   */
  store: function PageThumbs_store(aKey, aCanvas, aCallback) {
    let telemetryStoreTime = new Date();

    function finish(aSuccessful) {
      if (aSuccessful) {
        Services.telemetry.getHistogramById("FX_THUMBNAILS_STORE_TIME_MS")
          .add(new Date() - telemetryStoreTime);
      }

      if (aCallback)
        aCallback(aSuccessful);
    }

    let self = this;

    // Get a writeable cache entry.
    PageThumbsCache.getWriteEntry(aKey, function (aEntry) {
      if (!aEntry) {
        finish(false);
        return;
      }

      // Extract image data from the canvas.
      self._readImageData(aCanvas, function (aData) {
        let outputStream = aEntry.openOutputStream(0);

        // Write the image data to the cache entry.
        NetUtil.asyncCopy(aData, outputStream, function (aResult) {
          let success = Components.isSuccessCode(aResult);
          if (success)
            aEntry.markValid();

          aEntry.close();
          finish(success);
        });
      });
    });
  },

  /**
   * Reads the image data from a given canvas and passes it to the callback.
   * @param aCanvas The canvas to read the image data from.
   * @param aCallback The function that the image data is passed to.
   */
  _readImageData: function PageThumbs_readImageData(aCanvas, aCallback) {
    let dataUri = aCanvas.toDataURL(PageThumbs.contentType, "");
    let uri = Services.io.newURI(dataUri, "UTF8", null);

    NetUtil.asyncFetch(uri, function (aData, aResult) {
      if (Components.isSuccessCode(aResult) && aData && aData.available())
        aCallback(aData);
    });
  },

  /**
   * Determines the crop size for a given content window.
   * @param aWindow The content window.
   * @return An array containing width, height and scale.
   */
  _determineCropSize: function PageThumbs_determineCropSize(aWindow) {
    let sw = aWindow.innerWidth;
    let sh = aWindow.innerHeight;

    let scale = Math.max(THUMBNAIL_WIDTH / sw, THUMBNAIL_HEIGHT / sh);
    let scaledWidth = sw * scale;
    let scaledHeight = sh * scale;

    if (scaledHeight > THUMBNAIL_HEIGHT)
      sh -= Math.floor(Math.abs(scaledHeight - THUMBNAIL_HEIGHT) * scale);

    if (scaledWidth > THUMBNAIL_WIDTH)
      sw -= Math.floor(Math.abs(scaledWidth - THUMBNAIL_WIDTH) * scale);

    return [sw, sh, scale];
  },

  /**
   * Creates a new hidden canvas element.
   * @return The newly created canvas.
   */
  _createCanvas: function PageThumbs_createCanvas() {
    let doc = Services.appShell.hiddenDOMWindow.document;
    let canvas = doc.createElementNS(HTML_NAMESPACE, "canvas");
    canvas.mozOpaque = true;
    canvas.mozImageSmoothingEnabled = true;
    canvas.width = THUMBNAIL_WIDTH;
    canvas.height = THUMBNAIL_HEIGHT;
    return canvas;
  }
};

/**
 * A singleton handling the storage of page thumbnails.
 */
let PageThumbsCache = {
  /**
   * Calls the given callback with a cache entry opened for reading.
   * @param aKey The key identifying the desired cache entry.
   * @param aCallback The callback that is called when the cache entry is ready.
   */
  getReadEntry: function Cache_getReadEntry(aKey, aCallback) {
    // Try to open the desired cache entry.
    this._openCacheEntry(aKey, Ci.nsICache.ACCESS_READ, aCallback);
  },

  /**
   * Calls the given callback with a cache entry opened for writing.
   * @param aKey The key identifying the desired cache entry.
   * @param aCallback The callback that is called when the cache entry is ready.
   */
  getWriteEntry: function Cache_getWriteEntry(aKey, aCallback) {
    // Try to open the desired cache entry.
    this._openCacheEntry(aKey, Ci.nsICache.ACCESS_WRITE, aCallback);
  },

  /**
   * Opens the cache entry identified by the given key.
   * @param aKey The key identifying the desired cache entry.
   * @param aAccess The desired access mode (see nsICache.ACCESS_* constants).
   * @param aCallback The function to be called when the cache entry was opened.
   */
  _openCacheEntry: function Cache_openCacheEntry(aKey, aAccess, aCallback) {
    function onCacheEntryAvailable(aEntry, aAccessGranted, aStatus) {
      let validAccess = aAccess == aAccessGranted;
      let validStatus = Components.isSuccessCode(aStatus);

      // Check if a valid entry was passed and if the
      // access we requested was actually granted.
      if (aEntry && !(validAccess && validStatus)) {
        aEntry.close();
        aEntry = null;
      }

      aCallback(aEntry);
    }

    let listener = this._createCacheListener(onCacheEntryAvailable);
    this._cacheSession.asyncOpenCacheEntry(aKey, aAccess, listener);
  },

  /**
   * Returns a cache listener implementing the nsICacheListener interface.
   * @param aCallback The callback to be called when the cache entry is available.
   * @return The new cache listener.
   */
  _createCacheListener: function Cache_createCacheListener(aCallback) {
    return {
      onCacheEntryAvailable: aCallback,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICacheListener])
    };
  }
};

/**
 * Define a lazy getter for the cache session.
 */
XPCOMUtils.defineLazyGetter(PageThumbsCache, "_cacheSession", function () {
  return Services.cache.createSession(PageThumbs.scheme,
                                     Ci.nsICache.STORE_ON_DISK, true);
});
