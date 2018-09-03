/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function setup() {
  await setupPolicyEngineWithJson({
    "policies": {
      "DisableSafeMode": true,
    },
  });
});

add_task(async function test_help_menu() {
  buildHelpMenu();
  let safeModeMenu = document.getElementById("helpSafeMode");
  is(safeModeMenu.getAttribute("disabled"), "true",
     "The `Restart with Add-ons Disabled...` item should be disabled");
});

add_task(async function test_safemode_from_about_support() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:support");

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let button = content.document.getElementById("restart-in-safe-mode-button");
    is(button.getAttribute("disabled"), "true",
       "The `Restart with Add-ons Disabled...` button should be disabled");
  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_safemode_from_about_profiles() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:profiles");

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let button = content.document.getElementById("restart-in-safe-mode-button");
    is(button.getAttribute("disabled"), "true",
       "The `Restart with Add-ons Disabled...` button should be disabled");
  });

  await BrowserTestUtils.removeTab(tab);
});
