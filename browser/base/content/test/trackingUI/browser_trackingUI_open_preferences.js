/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CB_PREF = "browser.contentblocking.enabled";
const CB_UI_PREF = "browser.contentblocking.ui.enabled";
const TP_PREF = "privacy.trackingprotection.enabled";
const FB_PREF = "browser.fastblock.enabled";
const TPC_PREF = "network.cookie.cookieBehavior";
const FB_UI_PREF = "browser.contentblocking.fastblock.control-center.ui.enabled";
const TP_UI_PREF = "browser.contentblocking.trackingprotection.control-center.ui.enabled";
const RT_UI_PREF = "browser.contentblocking.rejecttrackers.control-center.ui.enabled";
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
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });

  Services.telemetry.clearEvents();
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

    let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true).parent;
    let clickEvents = events.filter(
      e => e[1] == "security.ui.identitypopup" && e[2] == "click" && e[3] == "cb_prefs_button");
    is(clickEvents.length, 1, "recorded telemetry for the click");
  });
});

// Tests that clicking the contentblocking category items "add blocking" labels
// links to about:preferences
add_task(async function testOpenPreferencesFromAddBlockingButtons() {
  SpecialPowers.pushPrefEnv({set: [
    [CB_PREF, true],
    [CB_UI_PREF, true],
    [FB_PREF, false],
    [TP_PREF, false],
    [TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT],
    [FB_UI_PREF, true],
    [TP_UI_PREF, true],
    [RT_UI_PREF, true],
  ]});

  await BrowserTestUtils.withNewTab(TRACKING_PAGE, async function() {
    let addBlockingButtons = document.querySelectorAll(".identity-popup-content-blocking-category-add-blocking");
    for (let button of addBlockingButtons) {
      let promisePanelOpen = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
      gIdentityHandler._identityBox.click();
      await promisePanelOpen;

      ok(BrowserTestUtils.is_visible(button), "Button is shown.");
      let shown = waitAndAssertPreferencesShown();
      button.click();
      await shown;

      let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true).parent;
      let clickEvents = events.filter(
        e => e[1] == "security.ui.identitypopup" && e[2] == "click" && e[3].endsWith("_add_blocking"));
      is(clickEvents.length, 1, "recorded telemetry for the click");
    }
  });
});


add_task(async function cleanup() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});
