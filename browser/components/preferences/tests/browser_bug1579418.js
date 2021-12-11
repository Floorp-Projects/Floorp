/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function default_homepage_test() {
  let oldHomepagePref = Services.prefs.getCharPref("browser.startup.homepage");
  let oldStartpagePref = Services.prefs.getIntPref("browser.startup.page");

  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });

  let doc = gBrowser.contentDocument;
  let homeMode = doc.getElementById("homeMode");
  let customSettings = doc.getElementById("customSettings");

  // HOME_MODE_FIREFOX_HOME
  homeMode.value = 0;

  homeMode.dispatchEvent(new Event("command"));

  // HOME_MODE_BLANK
  homeMode.value = 1;

  homeMode.dispatchEvent(new Event("command"));

  await TestUtils.waitForCondition(
    () => customSettings.hidden === true,
    "Wait for customSettings to be hidden."
  );

  // HOME_MODE_CUSTOM
  homeMode.value = 2;

  homeMode.dispatchEvent(new Event("command"));

  await TestUtils.waitForCondition(
    () => customSettings.hidden === false,
    "Wait for customSettings to be shown."
  );

  is(customSettings.hidden, false, "homePageURL should be visible");

  registerCleanupFunction(async () => {
    Services.prefs.setCharPref("browser.startup.homepage", oldHomepagePref);
    Services.prefs.setIntPref("browser.startup.page", oldStartpagePref);
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});
