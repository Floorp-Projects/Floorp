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

add_task(async function test_update_preferences_ui() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let updateRadioGroup = content.document.getElementById("updateRadioGroup");
    is(updateRadioGroup.hidden, true,
       "Update choices should be diabled when app update is locked by policy");
  });

  BrowserTestUtils.removeTab(tab);
});
