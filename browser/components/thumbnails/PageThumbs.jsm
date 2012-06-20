/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let EXPORTED_SYMBOLS = ["PageThumbs", "PageThumbsStorage", "PageThumbsCache"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const HTML_NAMESPACE = "http://www.w3.org/1999/xhtml";
const PREF_STORAGE_VERSION = "browser.pagethumbnails.storage_version";
const LATEST_STORAGE_VERSION = 1;

/**
 * Name of the directory in the profile that contains the thumbnails.
 */
const THUMBNAIL_DIRECTORY = "thumbnails";

/**
 * The default background color for page thumbnails.
 */
const THUMBNAIL_BG_COLOR = "#fff";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "gCryptoHash", function () {
  return Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
});

XPCOMUtils.defineLazyGetter(this, "gUnicodeConverter", function () {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = 'utf8';
  return converter;
});

/**
 * Singleton providing functionality for capturing web page thumbnails and for
 * accessing them if already cached.
 */
let PageThumbs = {
  _initialized: false,

  /**
   * The calculated width and height of the thumbnails.
   */
  _thumbnailWidth : 0,
  _thumbnailHeight : 0,

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

  init: function PageThumbs_init() {
    if (!this._initialized) {
      this._initialized = true;
      PlacesUtils.history.addObserver(PageThumbsHistoryObserver, false);

      // Migrate the underlying storage, if needed.
      PageThumbsStorageMigrator.migrate();
    }
  },

  uninit: function PageThumbs_uninit() {
    if (this._initialized) {
      this._initialized = false;
      PlacesUtils.history.removeObserver(PageThumbsHistoryObserver);
    }
  },

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
   * Captures a thumbnail for the given window.
   * @param aWindow The DOM window to capture a thumbnail from.
   * @param aCallback The function to be called when the thumbnail has been
   *                  captured. The first argument will be the data stream
   *                  containing the image data.
   */
  capture: function PageThumbs_capture(aWindow, aCallback) {
    if (!this._prefEnabled()) {
      return;
    }

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

    let telemetry = Services.telemetry;
    telemetry.getHistogramById("FX_THUMBNAILS_CAPTURE_TIME_MS")
      .add(new Date() - telemetryCaptureTime);

    canvas.mozFetchAsStream(aCallback, this.contentType);
  },

  /**
   * Captures a thumbnail for the given browser and stores it to the cache.
   * @param aBrowser The browser to capture a thumbnail for.
   * @param aCallback The function to be called when finished (optional).
   */
  captureAndStore: function PageThumbs_captureAndStore(aBrowser, aCallback) {
    if (!this._prefEnabled()) {
      return;
    }

    let url = aBrowser.currentURI.spec;
    let channel = aBrowser.docShell.currentDocumentChannel;
    let originalURL = channel.originalURI.spec;

    this.capture(aBrowser.contentWindow, function (aInputStream) {
      let telemetryStoreTime = new Date();

      function finish(aSuccessful) {
        if (aSuccessful) {
          Services.telemetry.getHistogramById("FX_THUMBNAILS_STORE_TIME_MS")
            .add(new Date() - telemetryStoreTime);

          // We've been redirected. Create a copy of the current thumbnail for
          // the redirect source. We need to do this because:
          //
          // 1) Users can drag any kind of links onto the newtab page. If those
          //    links redirect to a different URL then we want to be able to
          //    provide thumbnails for both of them.
          //
          // 2) The newtab page should actually display redirect targets, only.
          //    Because of bug 559175 this information can get lost when using
          //    Sync and therefore also redirect sources appear on the newtab
          //    page. We also want thumbnails for those.
          if (url != originalURL)
            PageThumbsStorage.copy(url, originalURL);
        }

        if (aCallback)
          aCallback(aSuccessful);
      }

      PageThumbsStorage.write(url, aInputStream, finish);
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

    let [thumbnailWidth, thumbnailHeight] = this._getThumbnailSize();
    let scale = Math.max(thumbnailWidth / sw, thumbnailHeight / sh);
    let scaledWidth = sw * scale;
    let scaledHeight = sh * scale;

    if (scaledHeight > thumbnailHeight)
      sh -= Math.floor(Math.abs(scaledHeight - thumbnailHeight) * scale);

    if (scaledWidth > thumbnailWidth)
      sw -= Math.floor(Math.abs(scaledWidth - thumbnailWidth) * scale);

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
    let [thumbnailWidth, thumbnailHeight] = this._getThumbnailSize();
    canvas.width = thumbnailWidth;
    canvas.height = thumbnailHeight;
    return canvas;
  },

  /**
   * Calculates the thumbnail size based on current desktop's dimensions.
   * @return The calculated thumbnail size or a default if unable to calculate.
   */
  _getThumbnailSize: function PageThumbs_getThumbnailSize() {
    if (!this._thumbnailWidth || !this._thumbnailHeight) {
      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"]
                            .getService(Ci.nsIScreenManager);
      let left = {}, top = {}, width = {}, height = {};
      screenManager.primaryScreen.GetRect(left, top, width, height);
      this._thumbnailWidth = Math.round(width.value / 3);
      this._thumbnailHeight = Math.round(height.value / 3);
    }
    return [this._thumbnailWidth, this._thumbnailHeight];
  },

  _prefEnabled: function PageThumbs_prefEnabled() {
    try {
      return Services.prefs.getBoolPref("browser.pageThumbs.enabled");
    }
    catch (e) {
      return true;
    }
  },
};

let PageThumbsStorage = {
  getFileForURL: function Storage_getFileForURL(aURL, aOptions) {
    let hash = this._calculateMD5Hash(aURL);
    let parts = [THUMBNAIL_DIRECTORY, hash[0], hash[1]];
    let file = FileUtils.getDir("ProfLD", parts, aOptions && aOptions.createPath);
    file.append(hash.slice(2) + ".png");
    return file;
  },

  write: function Storage_write(aURL, aDataStream, aCallback) {
    let file = this.getFileForURL(aURL, {createPath: true});
    let fos = FileUtils.openSafeFileOutputStream(file);

    NetUtil.asyncCopy(aDataStream, fos, function (aResult) {
      FileUtils.closeSafeFileOutputStream(fos);
      aCallback(Components.isSuccessCode(aResult));
    });
  },

  copy: function Storage_copy(aSourceURL, aTargetURL) {
    let sourceFile = this.getFileForURL(aSourceURL);
    let targetFile = this.getFileForURL(aTargetURL);

    try {
      sourceFile.copyTo(targetFile.parent, targetFile.leafName);
    } catch (e) {
      /* We might not be permitted to write to the file. */
    }
  },

  remove: function Storage_remove(aURL) {
    try {
      this.getFileForURL(aURL).remove(false);
    } catch (e) {
      /* The file might not exist or we're not permitted to remove it. */
    }
  },

  wipe: function Storage_wipe() {
    try {
      FileUtils.getDir("ProfLD", [THUMBNAIL_DIRECTORY]).remove(true);
    } catch (e) {
      /* The file might not exist or we're not permitted to remove it. */
    }
  },

  _calculateMD5Hash: function Storage_calculateMD5Hash(aValue) {
    let hash = gCryptoHash;
    let value = gUnicodeConverter.convertToByteArray(aValue);

    hash.init(hash.MD5);
    hash.update(value, value.length);
    return this._convertToHexString(hash.finish(false));
  },

  _convertToHexString: function Storage_convertToHexString(aData) {
    let hex = "";
    for (let i = 0; i < aData.length; i++)
      hex += ("0" + aData.charCodeAt(i).toString(16)).slice(-2);
    return hex;
  },

};

let PageThumbsStorageMigrator = {
  get currentVersion() {
    try {
      return Services.prefs.getIntPref(PREF_STORAGE_VERSION);
    } catch (e) {
      // The pref doesn't exist, yet. Return version 0.
      return 0;
    }
  },

  set currentVersion(aVersion) {
    Services.prefs.setIntPref(PREF_STORAGE_VERSION, aVersion);
  },

  migrate: function Migrator_migrate() {
    let version = this.currentVersion;

    if (version < 1)
      this.removeThumbnailsFromRoamingProfile();

    this.currentVersion = LATEST_STORAGE_VERSION;
  },

  removeThumbnailsFromRoamingProfile:
  function Migrator_removeThumbnailsFromRoamingProfile() {
    let local = FileUtils.getDir("ProfLD", [THUMBNAIL_DIRECTORY]);
    let roaming = FileUtils.getDir("ProfD", [THUMBNAIL_DIRECTORY]);

    if (!roaming.equals(local) && roaming.exists()) {
      try {
        roaming.remove(true);
      } catch (e) {
        // The directory might not exist or we're not permitted to remove it.
      }
    }
  }
};

let PageThumbsHistoryObserver = {
  onDeleteURI: function Thumbnails_onDeleteURI(aURI, aGUID) {
    PageThumbsStorage.remove(aURI.spec);
  },

  onClearHistory: function Thumbnails_onClearHistory() {
    PageThumbsStorage.wipe();
  },

  onTitleChanged: function () {},
  onBeginUpdateBatch: function () {},
  onEndUpdateBatch: function () {},
  onVisit: function () {},
  onBeforeDeleteURI: function () {},
  onPageChanged: function () {},
  onDeleteVisits: function () {},

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
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
