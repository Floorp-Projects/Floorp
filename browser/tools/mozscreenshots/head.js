/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported TestRunner, shouldCapture */

"use strict";

const {AddonWatcher} = Cu.import("resource://gre/modules/AddonWatcher.jsm", {});
const chromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);
const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
const EXTENSION_DIR = "chrome://mochitests/content/extensions/mozscreenshots/browser/";

let TestRunner;

function* setup() {
  requestLongerTimeout(10);

  info("installing extension temporarily");
  let chromeURL = Services.io.newURI(EXTENSION_DIR);
  let dir = chromeRegistry.convertChromeURL(chromeURL).QueryInterface(Ci.nsIFileURL).file;
  yield AddonManager.installTemporaryAddon(dir);

  info("Checking for mozscreenshots extension");
  return new Promise((resolve) => {
    AddonManager.getAddonByID("mozscreenshots@mozilla.org", function(aAddon) {
      isnot(aAddon, null, "The mozscreenshots extension should be installed");
      AddonWatcher.ignoreAddonPermanently(aAddon.id);
      TestRunner = Cu.import("chrome://mozscreenshots/content/TestRunner.jsm", {}).TestRunner;
      resolve();
    });
  });
}

function shouldCapture() {
  // Try pushes only capture in browser_screenshots.js with MOZSCREENSHOTS_SETS.
  if (env.get("MOZSCREENSHOTS_SETS")) {
    ok(true, "MOZSCREENSHOTS_SETS was specified so only capture what was " +
       "requested (in browser_screenshots.js)");
    return false;
  }

  // Automation isn't able to schedule test jobs to only run on nightlies so we handle it here
  // (see also: bug 1116275).
  let capture = AppConstants.MOZ_UPDATE_CHANNEL == "nightly" ||
                AppConstants.SOURCE_REVISION_URL == "";
  if (!capture) {
    ok(true, "Capturing is disabled for this MOZ_UPDATE_CHANNEL or REPO");
  }
  return capture;
}

add_task(setup);
