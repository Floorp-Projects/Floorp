/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["Screenshots"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

ChromeUtils.defineModuleGetter(
  this,
  "BackgroundPageThumbs",
  "resource://gre/modules/BackgroundPageThumbs.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PageThumbs",
  "resource://gre/modules/PageThumbs.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

const GREY_10 = "#F9F9FA";

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gPrivilegedAboutProcessEnabled",
  "browser.tabs.remote.separatePrivilegedContentProcess",
  false
);

this.Screenshots = {
  /**
   * Get a screenshot / thumbnail for a url. Either returns the disk cached
   * image or initiates a background request for the url.
   *
   * @param url {string} The url to get a thumbnail
   * @return {Promise} Resolves a custom object or null if failed
   */
  async getScreenshotForURL(url) {
    try {
      await BackgroundPageThumbs.captureIfMissing(url, {
        backgroundColor: GREY_10,
      });

      // The privileged about content process is able to use the moz-page-thumb
      // protocol, so if it's enabled, send that down.
      if (gPrivilegedAboutProcessEnabled) {
        return PageThumbs.getThumbnailURL(url);
      }

      // Otherwise, for normal content processes, we fallback to using
      // Blob URIs for the screenshots.
      const imgPath = PageThumbs.getThumbnailPath(url);

      const filePathResponse = await fetch(`file://${imgPath}`);
      const fileContents = await filePathResponse.blob();

      // Check if the file is empty, which indicates there isn't actually a
      // thumbnail, so callers can show a failure state.
      if (fileContents.size === 0) {
        return null;
      }

      return { path: imgPath, data: fileContents };
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
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      if (!PrivateBrowsingUtils.isWindowPrivate(win)) {
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
  },
};
