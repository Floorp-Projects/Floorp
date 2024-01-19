/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TP_PREF = "privacy.trackingprotection.enabled";
const TPC_PREF = "network.cookie.cookieBehavior";
const TRACKING_PAGE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";
const COOKIE_PAGE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://tracking.example.com/browser/browser/base/content/test/protectionsUI/cookiePage.html";

async function waitAndAssertPreferencesShown(_spotlight) {
  await BrowserTestUtils.waitForEvent(
    gProtectionsHandler._protectionsPopup,
    "popuphidden"
  );
  await TestUtils.waitForCondition(
    () => gBrowser.currentURI.spec == "about:preferences#privacy",
    "Should open about:preferences."
  );

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [_spotlight],
    async spotlight => {
      let doc = content.document;
      let section = await ContentTaskUtils.waitForCondition(
        () => doc.querySelector(".spotlight"),
        "The spotlight should appear."
      );
      Assert.equal(
        section.getAttribute("data-subcategory"),
        spotlight,
        "The correct section is spotlighted."
      );
    }
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_setup(async function () {
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

  let promise = BrowserTestUtils.openNewForegroundTab({
    url: TRACKING_PAGE,
    gBrowser,
  });

  // Wait for 2 content blocking events - one for the load and one for the tracker.
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent(2)]);

  await openProtectionsPanel();

  let categoryItem = document.getElementById(
    "protections-popup-category-trackers"
  );

  // Explicitly waiting for the category item becoming visible.
  await TestUtils.waitForCondition(() => {
    return BrowserTestUtils.isVisible(categoryItem);
  });

  ok(BrowserTestUtils.isVisible(categoryItem), "TP category item is visible");
  let trackersView = document.getElementById("protections-popup-trackersView");
  let viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Trackers view was shown");

  let preferencesButton = document.getElementById(
    "protections-popup-trackersView-settings-button"
  );

  ok(
    BrowserTestUtils.isVisible(preferencesButton),
    "The preferences button is shown."
  );

  let shown = waitAndAssertPreferencesShown("trackingprotection");
  preferencesButton.click();
  await shown;
  BrowserTestUtils.removeTab(tab);

  Services.prefs.clearUserPref(TP_PREF);
});

// Tests that pressing the preferences button in the cookies subview
// links to about:preferences
add_task(async function testOpenPreferencesFromCookiesSubview() {
  Services.prefs.setIntPref(
    TPC_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );

  let promise = BrowserTestUtils.openNewForegroundTab({
    url: COOKIE_PAGE,
    gBrowser,
  });

  // Wait for 2 content blocking events - one for the load and one for the tracker.
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent(2)]);

  await openProtectionsPanel();

  let categoryItem = document.getElementById(
    "protections-popup-category-cookies"
  );

  // Explicitly waiting for the category item becoming visible.
  await TestUtils.waitForCondition(() => {
    return BrowserTestUtils.isVisible(categoryItem);
  });

  ok(BrowserTestUtils.isVisible(categoryItem), "TP category item is visible");
  let cookiesView = document.getElementById("protections-popup-cookiesView");
  let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Cookies view was shown");

  let preferencesButton = document.getElementById(
    "protections-popup-cookiesView-settings-button"
  );

  ok(
    BrowserTestUtils.isVisible(preferencesButton),
    "The preferences button is shown."
  );

  let shown = waitAndAssertPreferencesShown("trackingprotection");
  preferencesButton.click();
  await shown;
  BrowserTestUtils.removeTab(tab);

  Services.prefs.clearUserPref(TPC_PREF);
});
