/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);

// Test that the "Tracking Protection" button in the app menu loads about:preferences
add_task(async function testPreferencesButton() {
  await SpecialPowers.pushPrefEnv({set: [["privacy.trackingprotection.appMenuToggle.enabled", true]]});

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
