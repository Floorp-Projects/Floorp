/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported TestRunner, shouldCapture */

"use strict";

const chromeRegistry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);
const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
const EXTENSION_DIR = "chrome://mochitests/content/browser/browser/tools/mozscreenshots/mozscreenshots/extension/mozscreenshots/browser/";

let TestRunner;

async function setup() {
  // This timeout doesn't actually end the job even if it is hit - the buildbot timeout will
  // handle things for us if the test actually hangs.
  requestLongerTimeout(100);

  // Generate output so mozprocess knows we're still alive for the long session.
  SimpleTest.requestCompleteLog();

  info("installing extension temporarily");
  let chromeURL = Services.io.newURI(EXTENSION_DIR);
  let dir = chromeRegistry.convertChromeURL(chromeURL).QueryInterface(Ci.nsIFileURL).file;
  await AddonManager.installTemporaryAddon(dir);

  info("Checking for mozscreenshots extension");
  return new Promise(async (resolve) => {
    let aAddon = await AddonManager.getAddonByID("mozscreenshots@mozilla.org");
    isnot(aAddon, null, "The mozscreenshots extension should be installed");
    TestRunner = ChromeUtils.import("chrome://mozscreenshots/content/TestRunner.jsm", {}).TestRunner;
    TestRunner.initTest(this);
    resolve();
  });
}

/**
 * Used by pre-defined sets of configurations to decide whether to run for a build.
 * @note This is not used by browser_screenshots.js which handles when MOZSCREENSHOTS_SETS is set.
 * @return {bool} whether to capture screenshots.
 */
function shouldCapture() {
  if (env.get("MOZSCREENSHOTS_SETS")) {
    ok(true, "MOZSCREENSHOTS_SETS was specified so only capture what was " +
       "requested (in browser_screenshots.js)");
    return false;
  }

  if (AppConstants.MOZ_UPDATE_CHANNEL == "nightly") {
    ok(true, "Screenshots aren't captured on Nightlies");
    return false;
  }

  return true;
}

add_task(setup);
