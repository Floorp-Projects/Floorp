/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF = "privacy.trackingprotection.enabled";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";

async function waitAndAssertPreferencesShown() {
  await BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popuphidden");
  await TestUtils.waitForCondition(() => gBrowser.currentURI.spec == "about:preferences#privacy",
    "Should open about:preferences.");

  await ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    let doc = content.document;
    let section = await ContentTaskUtils.waitForCondition(
      () => doc.querySelector(".spotlight"), "The spotlight should appear.");
    is(section.getAttribute("data-subcategory"), "trackingprotection",
      "The trackingprotection section is spotlighted.");
  });

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function setup() {
  await UrlClassifierTestUtils.addTestTrackers();
});

// Tests that pressing the preferences icon in the identity popup
// links to about:preferences
add_task(async function testOpenPreferencesFromPrefsButton() {
  await BrowserTestUtils.withNewTab("https://example.com", async function() {
    let promisePanelOpen = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
    gIdentityHandler._identityBox.click();
    await promisePanelOpen;

    let preferencesButton = document.getElementById("tracking-protection-preferences-button");

    ok(!BrowserTestUtils.is_hidden(preferencesButton), "The enable tracking protection button is shown.");

    let shown = waitAndAssertPreferencesShown();
    preferencesButton.click();
    await shown;
  });
});

add_task(async function cleanup() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});
