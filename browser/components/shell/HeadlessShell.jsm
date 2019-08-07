/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["HeadlessShell", "ScreenshotParent"];

const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);
const { HiddenFrame } = ChromeUtils.import(
  "resource://gre/modules/HiddenFrame.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// Refrences to the progress listeners to keep them from being gc'ed
// before they are called.
const progressListeners = new Set();

class ScreenshotParent extends JSWindowActorParent {
  takeScreenshot(params) {
    return this.sendQuery("TakeScreenshot", params);
  }
}

ChromeUtils.registerWindowActor("Screenshot", {
  parent: {
    moduleURI: "resource:///modules/HeadlessShell.jsm",
  },
  child: {
    moduleURI: "resource:///modules/ScreenshotChild.jsm",
    messages: ["TakeScreenshot"],
  },
});

function loadContentWindow(browser, url) {
  let uri;
  try {
    uri = Services.io.newURI(url);
  } catch (e) {
    let msg = `Invalid URL passed to loadContentWindow(): ${url}`;
    Cu.reportError(msg);
    return Promise.reject(new Error(msg));
  }

  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  return new Promise((resolve, reject) => {
    let loadURIOptions = {
      triggeringPrincipal: principal,
      remoteType: E10SUtils.getRemoteTypeForURI(url, true, false),
    };
    browser.loadURI(uri.spec, loadURIOptions);
    let { webProgress } = browser;

    let progressListener = {
      onLocationChange(progress, request, location, flags) {
        // Ignore inner-frame events
        if (!progress.isTopLevel) {
          return;
        }
        // Ignore events that don't change the document
        if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
          return;
        }
        // Ignore the initial about:blank, unless about:blank is requested
        if (location.spec == "about:blank" && uri.spec != "about:blank") {
          return;
        }

        progressListeners.delete(progressListener);
        webProgress.removeProgressListener(progressListener);
        resolve();
      },
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };
    progressListeners.add(progressListener);
    webProgress.addProgressListener(
      progressListener,
      Ci.nsIWebProgress.NOTIFY_LOCATION
    );
  });
}

async function takeScreenshot(
  fullWidth,
  fullHeight,
  contentWidth,
  contentHeight,
  path,
  url
) {
  let frame;
  try {
    frame = new HiddenFrame();
    let windowlessBrowser = await frame.get();

    let doc = windowlessBrowser.document;
    let browser = doc.createXULElement("browser");
    browser.setAttribute("remote", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute(
      "style",
      `width: ${contentWidth}px; min-width: ${contentWidth}px; height: ${contentHeight}px; min-height: ${contentHeight}px;`
    );
    doc.documentElement.appendChild(browser);

    await loadContentWindow(browser, url);

    let actor = browser.browsingContext.currentWindowGlobal.getActor(
      "Screenshot"
    );
    let blob = await actor.takeScreenshot({
      fullWidth,
      fullHeight,
    });

    let reader = await new Promise(resolve => {
      let fr = new FileReader();
      fr.onloadend = () => resolve(fr);
      fr.readAsArrayBuffer(blob);
    });

    await OS.File.writeAtomic(path, new Uint8Array(reader.result), {
      flush: true,
    });
    dump("Screenshot saved to: " + path + "\n");
  } catch (e) {
    dump("Failure taking screenshot: " + e + "\n");
  } finally {
    if (frame) {
      frame.destroy();
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
        await takeScreenshot(
          fullWidth,
          fullHeight,
          contentWidth,
          contentHeight,
          path,
          URLlist[0]
        );
      } else {
        dump("expected exactly one URL when using `screenshot`\n");
      }
    } finally {
      Services.startup.exitLastWindowClosingSurvivalArea();
      Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
    }
  },
};
