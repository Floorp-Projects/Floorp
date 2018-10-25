/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CB_PREF = "browser.contentblocking.enabled";
const TOGGLE_PREF = "browser.contentblocking.global-toggle.enabled";

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);

// Test that the app menu toggle correctly flips the TP pref in
// normal windows and private windows.
add_task(async function testGlobalToggle() {
  await SpecialPowers.pushPrefEnv({set: [
    [TOGGLE_PREF, true],
  ]});

  let panelUIButton = await TestUtils.waitForCondition(() => document.getElementById("PanelUI-menu-button"));

  info("Opening main menu");

  let promiseShown = BrowserTestUtils.waitForEvent(PanelUI.mainView, "ViewShown");
  panelUIButton.click();
  await promiseShown;

  info("Opened main menu");

  let toggle = document.getElementById("appMenu-tp-toggle");

  Services.prefs.setBoolPref(CB_PREF, false);
  await TestUtils.waitForCondition(() => toggle.getAttribute("enabled") == "false");

  Services.prefs.setBoolPref(CB_PREF, true);
  await TestUtils.waitForCondition(() => toggle.getAttribute("enabled") == "true");

  toggle.click();
  is(Services.prefs.getBoolPref(CB_PREF), false);

  toggle.click();
  is(Services.prefs.getBoolPref(CB_PREF), true);

  Services.prefs.clearUserPref(CB_PREF);
});
