/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);

// Test that the "Tracking Protection" button in the app menu loads about:preferences
add_task(async function testPreferencesButton() {
  let cuiTestUtils = new CustomizableUITestUtils(window);

  await BrowserTestUtils.withNewTab(gBrowser, async function(browser) {
    await cuiTestUtils.openMainMenu();

    let loaded = TestUtils.waitForCondition(() => gBrowser.currentURI.spec == "about:preferences#privacy",
      "Should open about:preferences.");
    document.getElementById("appMenu-tp-label").click();
    await loaded;

    await ContentTask.spawn(browser, {}, async function() {
      let doc = content.document;
      let section = await ContentTaskUtils.waitForCondition(
        () => doc.querySelector(".spotlight"), "The spotlight should appear.");
      is(section.getAttribute("data-subcategory"), "trackingprotection",
        "The trackingprotection section is spotlighted.");
    });
  });
});

// Test that the app menu toggle correctly flips the TP pref in
// normal windows and private windows.
add_task(async function testGlobalToggle() {
  async function runTest(privateWindow) {
    let win = await BrowserTestUtils.openNewBrowserWindow({private: privateWindow});
    let cuiTestUtils = new CustomizableUITestUtils(win);

    let prefName = privateWindow ? "privacy.trackingprotection.pbmode.enabled" :
      "privacy.trackingprotection.enabled";

    await cuiTestUtils.openMainMenu();

    let toggle = win.document.getElementById("appMenu-tp-toggle");

    Services.prefs.setBoolPref(prefName, false);
    BrowserTestUtils.waitForAttribute("enabled", toggle, "false");

    Services.prefs.setBoolPref(prefName, true);
    BrowserTestUtils.waitForAttribute("enabled", toggle, "true");

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
