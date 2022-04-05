/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../head.js */

"use strict";
var updateService = Cc["@mozilla.org/updates/update-service;1"].getService(
  Ci.nsIApplicationUpdateService
);

add_task(async function test_updates_post_policy() {
  is(
    Services.policies.isAllowed("devtools"),
    false,
    "devtools should be disabled by policy."
  );

  is(
    Services.prefs.getBoolPref("devtools.policy.disabled"),
    true,
    "devtools dedicated disabled pref is set to true"
  );

  Services.prefs.setBoolPref("devtools.policy.disabled", false);

  is(
    Services.prefs.getBoolPref("devtools.policy.disabled"),
    true,
    "devtools dedicated disabled pref can not be updated"
  );

  await testPageBlockedByPolicy("about:devtools-toolbox");
  await testPageBlockedByPolicy("about:debugging");
  await testPageBlockedByPolicy("about:profiling");

  let testURL = "data:text/html;charset=utf-8,test";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    testURL,
    false
  );

  let menuButton = document.getElementById("PanelUI-menu-button");
  menuButton.click();
  await BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "ViewShown");
  let moreToolsButtonId = "appMenu-more-button2";
  document.getElementById(moreToolsButtonId).click();
  await BrowserTestUtils.waitForEvent(
    document.getElementById("appmenu-moreTools"),
    "ViewShown"
  );
  is(
    document.getElementById("appmenu-developer-tools-view").children.length,
    2,
    "The developer tools are properly populated"
  );
  window.PanelUI.hide();

  BrowserTestUtils.removeTab(tab);
});
