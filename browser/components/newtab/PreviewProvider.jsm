/* global XPCOMUtils, BackgroundPageThumbs, FileUtils, PageThumbsStorage, Task, MIMEService */
/* exported PreviewProvider */

"use strict";

this.EXPORTED_SYMBOLS = ["PreviewProvider"];

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/PageThumbs.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
const {OS} = Cu.import("resource://gre/modules/osfile.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "BackgroundPageThumbs",
  "resource://gre/modules/BackgroundPageThumbs.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MIMEService",
  "@mozilla.org/mime;1", "nsIMIMEService");

let PreviewProvider = {
  /**
   * Returns a thumbnail as a data URI for a url, creating it if necessary
   *
   * @param {String} url
   *        a url to obtain a thumbnail for
   * @return {Promise} A Promise that resolves with a base64 encoded thumbnail
   */
  getThumbnail: Task.async(function* PreviewProvider_getThumbnail(url) {
    try {
      yield BackgroundPageThumbs.captureIfMissing(url);
      let imgPath = PageThumbsStorage.getFilePathForURL(url);

      // OS.File object used to easily read off-thread
      let file = yield OS.File.open(imgPath, {read: true, existing: true});

      // nsIFile object needed for MIMEService
      let nsFile = FileUtils.File(imgPath);

      let contentType = MIMEService.getTypeFromFile(nsFile);
      let bytes = yield file.read();
      let encodedData = btoa(String.fromCharCode.apply(null, bytes));
      file.close();
      return `data:${contentType};base64,${encodedData}`;
    } catch (err) {
      Cu.reportError(`PreviewProvider_getThumbnail error: ${err}`);
      throw err;
    }
  })
};
