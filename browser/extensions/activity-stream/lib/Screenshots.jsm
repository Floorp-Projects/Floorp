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

this.Screenshots = {
  async getScreenshotForURL(url) {
    let screenshot = null;
    try {
      await BackgroundPageThumbs.captureIfMissing(url);
      const imgPath = PageThumbs.getThumbnailPath(url);

      // OS.File object used to easily read off-thread
      const file = await OS.File.open(imgPath, {read: true, existing: true});

      // nsIFile object needed for MIMEService
      const nsFile = FileUtils.File(imgPath);

      const contentType = MIMEService.getTypeFromFile(nsFile);
      const bytes = await file.read();
      const encodedData = btoa(String.fromCharCode.apply(null, bytes));
      file.close();
      screenshot = `data:${contentType};base64,${encodedData}`;
    } catch (err) {
      Cu.reportError(`getScreenshot error: ${err}`);
    }
    return screenshot;
  }
};
