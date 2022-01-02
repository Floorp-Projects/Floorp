/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);
var updateService = Cc["@mozilla.org/updates/update-service;1"].getService(
  Ci.nsIApplicationUpdateService
);

// This test is intended to ensure that nsIUpdateService::canCheckForUpdates
// is true before the "DisableAppUpdate" policy is applied. Testing that
// nsIUpdateService::canCheckForUpdates is false after the "DisableAppUpdate"
// policy is applied needs to occur in a different test since the policy does
// not properly take effect unless it is applied during application startup.
add_task(async function test_updates_pre_policy() {
  // Turn off automatic update before we set app.update.disabledForTesting to
  // false so that we don't cause an actual update.
  let originalUpdateAutoValue = await UpdateUtils.getAppUpdateAutoEnabled();
  await UpdateUtils.setAppUpdateAutoEnabled(false);
  registerCleanupFunction(async () => {
    await UpdateUtils.setAppUpdateAutoEnabled(originalUpdateAutoValue);
  });

  await SpecialPowers.pushPrefEnv({
    set: [["app.update.disabledForTesting", false]],
  });

  is(
    Services.policies.isAllowed("appUpdate"),
    true,
    "Since no policies have been set, appUpdate should be allowed by default"
  );

  is(
    updateService.canCheckForUpdates,
    true,
    "Should be able to check for updates before any policies are in effect."
  );
});
