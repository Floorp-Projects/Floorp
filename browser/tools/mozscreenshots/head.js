/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {AddonWatcher} = Cu.import("resource://gre/modules/AddonWatcher.jsm", {});
let TestRunner;

function setup() {
  requestLongerTimeout(10);

  info("Checking for mozscreenshots extension");
  AddonManager.getAddonByID("mozscreenshots@mozilla.org", function(aAddon) {
    isnot(aAddon, null, "The mozscreenshots extension should be installed");
    AddonWatcher.ignoreAddonPermanently(aAddon.id);
  });
}

add_task(setup);
