/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["Screenshots"];

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BackgroundPageThumbs",
  "resource://gre/modules/BackgroundPageThumbs.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageThumbs",
  "resource://gre/modules/PageThumbs.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
    "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MIMEService",
  "@mozilla.org/mime;1", "nsIMIMEService");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm");

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

  async getScreenshotForURL(url) {
    let screenshot = null;
    try {
      await BackgroundPageThumbs.captureIfMissing(url, {backgroundColor: GREY_10});
      const imgPath = PageThumbs.getThumbnailPath(url);

      // OS.File object used to easily read off-thread
      const file = await OS.File.open(imgPath, {read: true, existing: true});

      // nsIFile object needed for MIMEService
      const nsFile = FileUtils.File(imgPath);

      const contentType = MIMEService.getTypeFromFile(nsFile);
      const bytes = await file.read();
      const encodedData = btoa(this._bytesToString(bytes));
      file.close();
      screenshot = `data:${contentType};base64,${encodedData}`;
    } catch (err) {
      Cu.reportError(`getScreenshot error: ${err}`);
    }
    return screenshot;
  },

  /**
   * Conditionally get a screenshot for a link if there's no existing pending
   * screenshot. Updates the link object's desired property with the result.
   *
   * @param link {object} Link object to update
   * @param url {string} Url to get a screenshot of
   * @param property {string} Name of property on object to set
   @ @param onScreenshot {function} Callback for when the screenshot loads
   */
  async maybeGetAndSetScreenshot(link, url, property, onScreenshot) {
    // Make a link copy so we can stash internal properties to cache
    const updateCache = link.__updateCache ? link.__updateCache.bind(link) :
      () => {};

    // Request a screenshot if we don't already have one pending
    if (!link.__fetchingScreenshot) {
      link.__fetchingScreenshot = this.getScreenshotForURL(url);
      updateCache("__fetchingScreenshot");

      // Trigger this callback only when first requesting
      link.__fetchingScreenshot.then(onScreenshot).catch();
    }

    // Clean up now that we got the screenshot
    const screenshot = await link.__fetchingScreenshot;
    delete link.__fetchingScreenshot;
    updateCache("__fetchingScreenshot");

    // Update the link so the screenshot is in the cache
    if (screenshot) {
      link[property] = screenshot;
      updateCache(property);
    }
  }
};
