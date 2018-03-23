/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["Screenshots"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "BackgroundPageThumbs",
  "resource://gre/modules/BackgroundPageThumbs.jsm");
ChromeUtils.defineModuleGetter(this, "PageThumbs",
  "resource://gre/modules/PageThumbs.jsm");
ChromeUtils.defineModuleGetter(this, "FileUtils",
    "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MIMEService",
  "@mozilla.org/mime;1", "nsIMIMEService");
ChromeUtils.defineModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

const GREY_10 = "#F9F9FA";

this.Screenshots = {
  /**
   * Convert bytes to a string using extremely fast String.fromCharCode without
   * exceeding the max number of arguments that can be provided to a function.
   */
  _bytesToString(bytes) {
    // NB: This comes from js/src/vm/ArgumentsObject.h ARGS_LENGTH_MAX
    const ARGS_LENGTH_MAX = 500 * 1000;
    let i = 0;
    let str = "";
    let {length} = bytes;
    while (i < length) {
      const start = i;
      i += ARGS_LENGTH_MAX;
      str += String.fromCharCode.apply(null, bytes.slice(start, i));
    }
    return str;
  },

  /**
   * Get a screenshot / thumbnail for a url. Either returns the disk cached
   * image or initiates a background request for the url.
   *
   * @param url {string} The url to get a thumbnail
   * @return {Promise} Resolves a data uri string or null if failed
   */
  async getScreenshotForURL(url) {
    try {
      await BackgroundPageThumbs.captureIfMissing(url, {backgroundColor: GREY_10});
      const imgPath = PageThumbs.getThumbnailPath(url);

      // OS.File object used to easily read off-thread
      const file = await OS.File.open(imgPath, {read: true, existing: true});

      // Check if the file is empty, which indicates there isn't actually a
      // thumbnail, so callers can show a failure state.
      const bytes = await file.read();
      if (bytes.length === 0) {
        return null;
      }

      // nsIFile object needed for MIMEService
      const nsFile = FileUtils.File(imgPath);
      const contentType = MIMEService.getTypeFromFile(nsFile);

      const encodedData = btoa(this._bytesToString(bytes));
      file.close();
      return `data:${contentType};base64,${encodedData}`;
    } catch (err) {
      Cu.reportError(`getScreenshot(${url}) failed: ${err}`);
    }

    // We must have failed to get the screenshot, so persist the failure by
    // storing an empty file. Future calls will then skip requesting and return
    // failure, so do the same thing here. The empty file should not expire with
    // the usual filtering process to avoid repeated background requests, which
    // can cause unwanted high CPU, network and memory usage - Bug 1384094
    try {
      await PageThumbs._store(url, url, null, true);
    } catch (err) {
      // Probably failed to create the empty file, but not much more we can do.
    }
    return null;
  },

  /**
   * Checks if all the open windows are private browsing windows. If so, we do not
   * want to collect screenshots. If there exists at least 1 non-private window,
   * we are ok to collect screenshots.
   */
  _shouldGetScreenshots() {
    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      if (!PrivateBrowsingUtils.isWindowPrivate(windows.getNext())) {
        // As soon as we encounter 1 non-private window, screenshots are fair game.
        return true;
      }
    }
    return false;
  },

  /**
   * Conditionally get a screenshot for a link if there's no existing pending
   * screenshot. Updates the cached link's desired property with the result.
   *
   * @param link {object} Link object to update
   * @param url {string} Url to get a screenshot of
   * @param property {string} Name of property on object to set
   @ @param onScreenshot {function} Callback for when the screenshot loads
   */
  async maybeCacheScreenshot(link, url, property, onScreenshot) {
    // If there are only private windows open, do not collect screenshots
    if (!this._shouldGetScreenshots()) {
      return;
    }
    // Nothing to do if we already have a pending screenshot or
    // if a previous request failed and returned null.
    const cache = link.__sharedCache;
    if (cache.fetchingScreenshot || link[property] !== undefined) {
      return;
    }

    // Save the promise to the cache so other links get it immediately
    cache.fetchingScreenshot = this.getScreenshotForURL(url);

    // Clean up now that we got the screenshot
    const screenshot = await cache.fetchingScreenshot;
    delete cache.fetchingScreenshot;

    // Update the cache for future links and call back for existing content
    cache.updateLink(property, screenshot);
    onScreenshot(screenshot);
  }
};
