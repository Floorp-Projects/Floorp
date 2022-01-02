/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const TRACKING_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";

const TP_PREF = "privacy.trackingprotection.enabled";

add_task(async function setup() {
  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(() => {
    UrlClassifierTestUtils.cleanupTestTrackers();
  });
});

async function assertSitesListed(blocked) {
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
    return BrowserTestUtils.is_visible(categoryItem);
  });

  ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
  let trackersView = document.getElementById("protections-popup-trackersView");
  let viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Trackers view was shown");

  let trackersViewShimHint = document.getElementById(
    "protections-popup-trackersView-shim-allow-hint"
  );
  ok(trackersViewShimHint.hidden, "Shim hint is hidden");
  let listItems = trackersView.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 1, "We have 1 tracker in the list");

  let mainView = document.getElementById("protections-popup-mainView");
  viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  let backButton = trackersView.querySelector(".subviewbutton-back");
  backButton.click();
  await viewShown;

  ok(true, "Main view was shown");

  let change = waitForSecurityChange(1);
  let timeoutPromise = new Promise(resolve => setTimeout(resolve, 1000));

  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    content.postMessage("more-tracking", "*");
  });

  let result = await Promise.race([change, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Trackers view was shown");

  listItems = Array.from(
    trackersView.querySelectorAll(".protections-popup-list-item")
  );
  is(listItems.length, 2, "We have 2 trackers in the list");

  let listItem = listItems.find(
    item => item.querySelector("label").value == "http://trackertest.org"
  );
  ok(listItem, "Has an item for trackertest.org");
  ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
  is(
    listItem.classList.contains("allowed"),
    !blocked,
    "Indicates whether the tracker was blocked or allowed"
  );

  listItem = listItems.find(
    item => item.querySelector("label").value == "https://itisatracker.org"
  );
  ok(listItem, "Has an item for itisatracker.org");
  ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
  is(
    listItem.classList.contains("allowed"),
    !blocked,
    "Indicates whether the tracker was blocked or allowed"
  );
  BrowserTestUtils.removeTab(tab);
}

add_task(async function testTrackersSubView() {
  info("Testing trackers subview with TP disabled.");
  Services.prefs.setBoolPref(TP_PREF, false);
  await assertSitesListed(false);
  info("Testing trackers subview with TP enabled.");
  Services.prefs.setBoolPref(TP_PREF, true);
  await assertSitesListed(true);
  info("Testing trackers subview with TP enabled and a CB exception.");
  let uri = Services.io.newURI("https://tracking.example.org");
  PermissionTestUtils.add(
    uri,
    "trackingprotection",
    Services.perms.ALLOW_ACTION
  );
  await assertSitesListed(false);
  info("Testing trackers subview with TP enabled and a CB exception removed.");
  PermissionTestUtils.remove(uri, "trackingprotection");
  await assertSitesListed(true);

  Services.prefs.clearUserPref(TP_PREF);
});
