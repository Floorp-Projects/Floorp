/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Bug 1819662 - Testing the tracking category of the protection panel shows the
 *               email tracker domain if the email tracking protection is
 *               enabled
 */

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const TEST_PAGE =
  "https://www.example.com/browser/browser/base/content/test/protectionsUI/emailTrackingPage.html";
const TEST_TRACKER_PAGE = "https://itisatracker.org/";

const TP_PREF = "privacy.trackingprotection.enabled";
const EMAIL_TP_PREF = "privacy.trackingprotection.emailtracking.enabled";

/**
 * A helper function to check whether or not an element has "notFound" class.
 *
 * @param {String} id The id of the testing element.
 * @returns {Boolean} true when the element has "notFound" class.
 */
function notFound(id) {
  return document.getElementById(id).classList.contains("notFound");
}

/**
 * A helper function to test the protection UI tracker category.
 *
 * @param {Boolean} blocked - true if the email tracking protection is enabled.
 */
async function assertSitesListed(blocked) {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    url: TEST_PAGE,
    gBrowser,
  });

  await openProtectionsPanel();

  let categoryItem = document.getElementById(
    "protections-popup-category-trackers"
  );

  if (!blocked) {
    // The tracker category should have the 'notFound' class to indicate that
    // no tracker was blocked in the page.
    ok(
      notFound("protections-popup-category-trackers"),
      "Tracker category is not found"
    );

    ok(
      !BrowserTestUtils.isVisible(categoryItem),
      "TP category item is not visible"
    );
    BrowserTestUtils.removeTab(tab);

    return;
  }

  // Testing if the tracker category is visible.

  // Explicitly waiting for the category item becoming visible.
  await BrowserTestUtils.waitForMutationCondition(categoryItem, {}, () =>
    BrowserTestUtils.isVisible(categoryItem)
  );

  ok(BrowserTestUtils.isVisible(categoryItem), "TP category item is visible");

  // Click the tracker category and wait until the tracker view is shown.
  let trackersView = document.getElementById("protections-popup-trackersView");
  let viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Trackers view was shown");

  // Ensure the email tracker is listed on the tracker list.
  let listItems = Array.from(
    trackersView.querySelectorAll(".protections-popup-list-item")
  );
  is(listItems.length, 1, "We have 1 trackers in the list");

  let listItem = listItems.find(
    item =>
      item.querySelector("label").value == "https://email-tracking.example.org"
  );
  ok(listItem, "Has an item for email-tracking.example.org");
  ok(BrowserTestUtils.isVisible(listItem), "List item is visible");

  // Back to the popup main view.
  let mainView = document.getElementById("protections-popup-mainView");
  viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  let backButton = trackersView.querySelector(".subviewbutton-back");
  backButton.click();
  await viewShown;

  ok(true, "Main view was shown");

  // Add an iframe to a tracker domain and wait until the content event files.
  let contentBlockingEventPromise = waitForContentBlockingEvent(1);
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_TRACKER_PAGE],
    test_url => {
      let ifr = content.document.createElement("iframe");

      content.document.body.appendChild(ifr);
      ifr.src = test_url;
    }
  );
  await contentBlockingEventPromise;

  // Click the tracker category again.
  viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
  categoryItem.click();
  await viewShown;

  // Ensure both the email tracker and the tracker are listed on the tracker
  // list.
  listItems = Array.from(
    trackersView.querySelectorAll(".protections-popup-list-item")
  );
  is(listItems.length, 2, "We have 2 trackers in the list");

  listItem = listItems.find(
    item =>
      item.querySelector("label").value == "https://email-tracking.example.org"
  );
  ok(listItem, "Has an item for email-tracking.example.org");
  ok(BrowserTestUtils.isVisible(listItem), "List item is visible");

  listItem = listItems.find(
    item => item.querySelector("label").value == "https://itisatracker.org"
  );
  ok(listItem, "Has an item for itisatracker.org");
  ok(BrowserTestUtils.isVisible(listItem), "List item is visible");

  BrowserTestUtils.removeTab(tab);
}

add_setup(async function () {
  Services.prefs.setBoolPref(TP_PREF, true);

  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(TP_PREF);
    UrlClassifierTestUtils.cleanupTestTrackers();
  });
});

add_task(async function testTrackersSubView() {
  info("Testing trackers subview with TP disabled.");
  Services.prefs.setBoolPref(EMAIL_TP_PREF, false);
  await assertSitesListed(false);
  info("Testing trackers subview with TP enabled.");
  Services.prefs.setBoolPref(EMAIL_TP_PREF, true);
  await assertSitesListed(true);
  info("Testing trackers subview with TP enabled and a CB exception.");
  let uri = Services.io.newURI("https://www.example.com");
  PermissionTestUtils.add(
    uri,
    "trackingprotection",
    Services.perms.ALLOW_ACTION
  );
  await assertSitesListed(false);
  info("Testing trackers subview with TP enabled and a CB exception removed.");
  PermissionTestUtils.remove(uri, "trackingprotection");
  await assertSitesListed(true);

  Services.prefs.clearUserPref(EMAIL_TP_PREF);
});
