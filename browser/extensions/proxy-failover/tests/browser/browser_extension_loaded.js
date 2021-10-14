/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

add_task(async function test_proxyfailover_isActive() {
  let addon = await AddonManager.getAddonByID("proxy-failover@mozilla.com");

  ok(addon, "Add-on exists");
  ok(addon.isActive, "Add-on is active");
});
