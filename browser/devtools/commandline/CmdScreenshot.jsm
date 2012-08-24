/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

let EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LayoutHelpers",
                                  "resource:///modules/devtools/LayoutHelpers.jsm");

/**
 * 'screenshot' command
 */
gcli.addCommand({
  name: "screenshot",
  description: gcli.lookup("screenshotDesc"),
  manual: gcli.lookup("screenshotManual"),
  returnType: "string",
  params: [
    {
      name: "filename",
      type: "string",
      description: gcli.lookup("screenshotFilenameDesc"),
      manual: gcli.lookup("screenshotFilenameManual")
    },
    {
      name: "delay",
      type: { name: "number", min: 0 },
      defaultValue: 0,
      description: gcli.lookup("screenshotDelayDesc"),
      manual: gcli.lookup("screenshotDelayManual")
    },
    {
      name: "fullpage",
      type: "boolean",
      description: gcli.lookup("screenshotFullPageDesc"),
      manual: gcli.lookup("screenshotFullPageManual")
    },
    {
      name: "node",
      type: "node",
      defaultValue: null,
      description: gcli.lookup("inspectNodeDesc"),
      manual: gcli.lookup("inspectNodeManual")
    }
  ],
  exec: function Command_screenshot(args, context) {
    var document = context.environment.contentDocument;
    if (args.delay > 0) {
      var promise = context.createPromise();
      document.defaultView.setTimeout(function Command_screenshotDelay() {
        let reply = this.grabScreen(document, args.filename);
        promise.resolve(reply);
      }.bind(this), args.delay * 1000);
      return promise;
    }
    else {
      return this.grabScreen(document, args.filename, args.fullpage, args.node);
    }
  },
  grabScreen:
  function Command_screenshotGrabScreen(document, filename, fullpage, node) {
    let window = document.defaultView;
    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    let left = 0;
    let top = 0;
    let width;
    let height;

    if (!fullpage) {
      if (!node) {
        left = window.scrollX;
        top = window.scrollY;
        width = window.innerWidth;
        height = window.innerHeight;
      } else {
        let rect = LayoutHelpers.getRect(node, window);
        top = rect.top;
        left = rect.left;
        width = rect.width;
        height = rect.height;
      }
    } else {
      width = window.innerWidth + window.scrollMaxX;
      height = window.innerHeight + window.scrollMaxY;
    }
    canvas.width = width;
    canvas.height = height;

    let ctx = canvas.getContext("2d");
    ctx.drawWindow(window, left, top, width, height, "#fff");

    let data = canvas.toDataURL("image/png", "");
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);

    // Check there is a .png extension to filename
    if (!filename.match(/.png$/i)) {
      filename += ".png";
    }

    // If the filename is relative, tack it onto the download directory
    if (!filename.match(/[\\\/]/)) {
      let downloadMgr = Cc["@mozilla.org/download-manager;1"]
        .getService(Ci.nsIDownloadManager);
      let tempfile = downloadMgr.userDownloadsDirectory;
      tempfile.append(filename);
      filename = tempfile.path;
    }

    try {
      file.initWithPath(filename);
    } catch (ex) {
      return "Error saving to " + filename;
    }

    let ioService = Cc["@mozilla.org/network/io-service;1"]
      .getService(Ci.nsIIOService);

    let Persist = Ci.nsIWebBrowserPersist;
    let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
      .createInstance(Persist);
    persist.persistFlags = Persist.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                           Persist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

    let source = ioService.newURI(data, "UTF8", null);
    persist.saveURI(source, null, null, null, null, file);

    return "Saved to " + filename;
  }
});
