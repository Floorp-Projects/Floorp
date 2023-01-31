/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_searchDetection_isActive() {
  let addon = await AddonManager.getAddonByID(
    "addons-search-detection@mozilla.com"
  );

  ok(addon, "Add-on exists");
  ok(addon.isActive, "Add-on is active");
  ok(addon.isBuiltin, "Add-on is built-in");
  ok(addon.hidden, "Add-on is hidden");
});
