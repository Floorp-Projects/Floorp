/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);

// Test that the app menu toggle correctly flips the TP pref in
// normal windows and private windows.
add_task(async function testGlobalToggle() {
  await SpecialPowers.pushPrefEnv({set: [["privacy.trackingprotection.appMenuToggle.enabled", true]]});

  async function runTest(privateWindow) {
    let win = await BrowserTestUtils.openNewBrowserWindow({private: privateWindow});

    let panelUIButton = await TestUtils.waitForCondition(() => win.document.getElementById("PanelUI-menu-button"));

    let prefName = privateWindow ? "privacy.trackingprotection.pbmode.enabled" :
      "privacy.trackingprotection.enabled";

    info("Opening main menu");

    let promiseShown = BrowserTestUtils.waitForEvent(win.PanelUI.mainView, "ViewShown");
    panelUIButton.click();
    await promiseShown;

    info("Opened main menu");

    let toggle = win.document.getElementById("appMenu-tp-toggle");

    Services.prefs.setBoolPref(prefName, false);
    await TestUtils.waitForCondition(() => toggle.getAttribute("enabled") == "false");

    Services.prefs.setBoolPref(prefName, true);
    await TestUtils.waitForCondition(() => toggle.getAttribute("enabled") == "true");

    toggle.click();
    is(Services.prefs.getBoolPref(prefName), false);

    toggle.click();
    is(Services.prefs.getBoolPref(prefName), true);

    Services.prefs.clearUserPref(prefName);

    await BrowserTestUtils.closeWindow(win);
  }

  // Run once in private and once in normal window.
  await runTest(true);
  await runTest(false);
});
