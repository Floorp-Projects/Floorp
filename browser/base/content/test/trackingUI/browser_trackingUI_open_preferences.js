/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TP_PREF = "privacy.trackingprotection.enabled";
const TPC_PREF = "network.cookie.cookieBehavior";
const TRACKING_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";

async function waitAndAssertPreferencesShown(_spotlight, identityPopup) {
  await BrowserTestUtils.waitForEvent(
    identityPopup
      ? gIdentityHandler._identityPopup
      : gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );
  await TestUtils.waitForCondition(
    () => gBrowser.currentURI.spec == "about:preferences#privacy",
    "Should open about:preferences."
  );

  await ContentTask.spawn(
    gBrowser.selectedBrowser,
    { spotlight: _spotlight },
    async function({ spotlight }) {
      let doc = content.document;
      let section = await ContentTaskUtils.waitForCondition(
        () => doc.querySelector(".spotlight"),
        "The spotlight should appear."
      );
      is(
        section.getAttribute("data-subcategory"),
        spotlight,
        "The correct section is spotlighted."
      );
    }
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function setup() {
  await UrlClassifierTestUtils.addTestTrackers();
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
    UrlClassifierTestUtils.cleanupTestTrackers();
  });
});

// Tests that pressing the preferences button in the trackers subview
// links to about:preferences
add_task(async function testOpenPreferencesFromTrackersSubview() {
  Services.prefs.setBoolPref(TP_PREF, true);

  await BrowserTestUtils.withNewTab(TRACKING_PAGE, async function() {
    await openProtectionsPopup();

    let categoryItem = document.getElementById(
      "protections-popup-category-tracking-protection"
    );
    ok(
      BrowserTestUtils.is_visible(categoryItem),
      "TP category item is visible"
    );
    let trackersView = document.getElementById(
      "protections-popup-trackersView"
    );
    let viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Trackers view was shown");

    let preferencesButton = document.getElementById(
      "protections-popup-trackersView-settings-button"
    );

    ok(
      BrowserTestUtils.is_visible(preferencesButton),
      "The preferences button is shown."
    );

    let shown = waitAndAssertPreferencesShown("trackingprotection");
    preferencesButton.click();
    await shown;
  });

  Services.prefs.clearUserPref(TP_PREF);
});

// Tests that pressing the preferences button in the cookies subview
// links to about:preferences
add_task(async function testOpenPreferencesFromCookiesSubview() {
  Services.prefs.setIntPref(
    TPC_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );

  await BrowserTestUtils.withNewTab(TRACKING_PAGE, async function() {
    await openProtectionsPopup();

    let categoryItem = document.getElementById(
      "protections-popup-category-cookies"
    );
    ok(
      BrowserTestUtils.is_visible(categoryItem),
      "TP category item is visible"
    );
    let cookiesView = document.getElementById("protections-popup-cookiesView");
    let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Cookies view was shown");

    let preferencesButton = document.getElementById(
      "protections-popup-cookiesView-settings-button"
    );

    ok(
      BrowserTestUtils.is_visible(preferencesButton),
      "The preferences button is shown."
    );

    let shown = waitAndAssertPreferencesShown("trackingprotection");
    preferencesButton.click();
    await shown;
  });

  Services.prefs.clearUserPref(TPC_PREF);
});
