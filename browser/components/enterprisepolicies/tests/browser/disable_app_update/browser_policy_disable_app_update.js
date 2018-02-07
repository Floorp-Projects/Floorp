/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
var updateService = Cc["@mozilla.org/updates/update-service;1"].
                    getService(Ci.nsIApplicationUpdateService);

add_task(async function test_updates_post_policy() {
  is(Services.policies.isAllowed("appUpdate"), false,
     "appUpdate should be disabled by policy.");

  is(updateService.canCheckForUpdates, false,
     "Should not be able to check for updates with DisableAppUpdate enabled.");
});
