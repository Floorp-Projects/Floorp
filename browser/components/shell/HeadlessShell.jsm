/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["HeadlessShell"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");

// Refrences to the progress listeners to keep them from being gc'ed
// before they are called.
const progressListeners = new Map();

function loadContentWindow(webNavigation, uri) {
  return new Promise((resolve, reject) => {
    webNavigation.loadURI(uri, Ci.nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
    let docShell = webNavigation.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDocShell);
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    let progressListener = {
      onLocationChange(progress, request, location, flags) {
        // Ignore inner-frame events
        if (progress != webProgress) {
          return;
        }
        // Ignore events that don't change the document
        if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
          return;
        }
        let contentWindow = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                    .getInterface(Ci.nsIDOMWindow);
        progressListeners.delete(progressListener);
        webProgress.removeProgressListener(progressListener);
        contentWindow.addEventListener("load", (event) => {
          resolve(contentWindow);
        }, { once: true });
      },
      QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener",
                                              "nsISupportsWeakReference"])
    };
    progressListeners.set(progressListener, progressListener);
    webProgress.addProgressListener(progressListener,
                                    Ci.nsIWebProgress.NOTIFY_LOCATION);
  });
}

async function takeScreenshot(fullWidth, fullHeight, contentWidth, contentHeight, path, url) {
  try {
    let windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
    var webNavigation = windowlessBrowser.QueryInterface(Ci.nsIWebNavigation);
    let contentWindow = await loadContentWindow(webNavigation, url);
    contentWindow.resizeTo(contentWidth, contentHeight);

    let canvas = contentWindow.document.createElementNS("http://www.w3.org/1999/xhtml", "html:canvas");
    let context = canvas.getContext("2d");
    let width = fullWidth ? contentWindow.innerWidth + contentWindow.scrollMaxX - contentWindow.scrollMinX
                          : contentWindow.innerWidth;
    let height = fullHeight ? contentWindow.innerHeight + contentWindow.scrollMaxY - contentWindow.scrollMinY
                            : contentWindow.innerHeight;
    canvas.width = width;
    canvas.height = height;
    context.drawWindow(contentWindow, 0, 0, width, height, "rgb(255, 255, 255)");

    function getBlob() {
      return new Promise(resolve => canvas.toBlob(resolve));
    }

    function readBlob(blob) {
      return new Promise(resolve => {
        let reader = new FileReader();
        reader.onloadend = () => resolve(reader);
        reader.readAsArrayBuffer(blob);
      });
    }

    let blob = await getBlob();
    let reader = await readBlob(blob);
    await OS.File.writeAtomic(path, new Uint8Array(reader.result), {flush: true});
    dump("Screenshot saved to: " + path + "\n");
  } catch (e) {
    dump("Failure taking screenshot: " + e + "\n");
  } finally {
    if (webNavigation) {
      webNavigation.close();
    }
  }
}

let HeadlessShell = {
  async handleCmdLineArgs(cmdLine, URLlist) {
    try {
      // Don't quit even though we don't create a window
      Services.startup.enterLastWindowClosingSurvivalArea();

      // Default options
      let fullWidth = true;
      let fullHeight = true;
      // Most common screen resolution of Firefox users
      let contentWidth = 1366;
      let contentHeight = 768;

      // Parse `window-size`
      try {
        var dimensionsStr = cmdLine.handleFlagWithParam("window-size", true);
      } catch (e) {
        dump("expected format: --window-size width[,height]\n");
        return;
      }
      if (dimensionsStr) {
        let success;
        let dimensions = dimensionsStr.split(",", 2);
        if (dimensions.length == 1) {
          success = dimensions[0] > 0;
          if (success) {
            fullWidth = false;
            fullHeight = true;
            contentWidth = dimensions[0];
          }
        } else {
          success = dimensions[0] > 0 && dimensions[1] > 0;
          if (success) {
            fullWidth = false;
            fullHeight = false;
            contentWidth = dimensions[0];
            contentHeight = dimensions[1];
          }
        }

        if (!success) {
          dump("expected format: --window-size width[,height]\n");
          return;
        }
      }

      // Only command line argument left should be `screenshot`
      // There could still be URLs however
      try {
        var path = cmdLine.handleFlagWithParam("screenshot", true);
        if (!cmdLine.length && !URLlist.length) {
          URLlist.push(path); // Assume the user wanted to specify a URL
          path = OS.Path.join(cmdLine.workingDirectory.path, "screenshot.png");
        }
      } catch (e) {
        path = OS.Path.join(cmdLine.workingDirectory.path, "screenshot.png");
        cmdLine.handleFlag("screenshot", true); // Remove `screenshot`
      }

      for (let i = 0; i < cmdLine.length; ++i) {
        URLlist.push(cmdLine.getArgument(i)); // Assume that all remaining arguments are URLs
      }

      if (URLlist.length == 1) {
        await takeScreenshot(fullWidth, fullHeight, contentWidth, contentHeight, path, URLlist[0]);
      } else {
        dump("expected exactly one URL when using `screenshot`\n");
      }
    } finally {
      Services.startup.exitLastWindowClosingSurvivalArea();
      Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
    }
  }
};
