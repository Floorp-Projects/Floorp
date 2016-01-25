/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {AddonWatcher} = Cu.import("resource://gre/modules/AddonWatcher.jsm", {});

function setup() {
  requestLongerTimeout(10);

  info("Checking for mozscreenshots extension");
  return new Promise((resolve) => {
    AddonManager.getAddonByID("mozscreenshots@mozilla.org", function(aAddon) {
      isnot(aAddon, null, "The mozscreenshots extension should be installed");
      AddonWatcher.ignoreAddonPermanently(aAddon.id);
      resolve();
    });
  });
}

function shouldCapture() {
  // Automation isn't able to schedule test jobs to only run on nightlies so we handle it here
  // (see also: bug 1116275). Try pushes and local builds should also capture.
  let capture = AppConstants.MOZ_UPDATE_CHANNEL == "nightly" ||
                AppConstants.SOURCE_REVISION_URL.includes("/try/rev/") ||
                AppConstants.SOURCE_REVISION_URL == "";
  if (!capture) {
    ok(true, "Capturing is disabled for this MOZ_UPDATE_CHANNEL or REPO");
  }
  return capture;
}

add_task(setup);
