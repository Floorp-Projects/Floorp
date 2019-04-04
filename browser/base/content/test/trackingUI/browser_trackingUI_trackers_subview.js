/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";

const TP_PREF = "privacy.trackingprotection.enabled";

add_task(async function setup() {
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Avoid the content blocking tour interfering with our tests by popping up.
  await SpecialPowers.pushPrefEnv({set: [[ContentBlocking.prefIntroCount, ContentBlocking.MAX_INTROS]]});
  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
    UrlClassifierTestUtils.cleanupTestTrackers();
  });
});

async function assertSitesListed(blocked) {
  await BrowserTestUtils.withNewTab(TRACKING_PAGE, async function(browser) {
    await openIdentityPopup();

    Services.telemetry.clearEvents();

    let categoryItem =
      document.getElementById("identity-popup-content-blocking-category-tracking-protection");
    ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
    let trackersView = document.getElementById("identity-popup-trackersView");
    let viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Trackers view was shown");

    let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS).parent;
    let buttonEvents = events.filter(
      e => e[1] == "security.ui.identitypopup" && e[2] == "click" && e[3] == "trackers_subview_btn");
    is(buttonEvents.length, 1, "recorded telemetry for the button click");

    let listItems = trackersView.querySelectorAll(".identity-popup-content-blocking-list-item");
    is(listItems.length, 1, "We have 1 tracker in the list");

    let strictInfo = document.getElementById("identity-popup-trackersView-strict-info");
    is(BrowserTestUtils.is_hidden(strictInfo), Services.prefs.getBoolPref(TP_PREF),
      "Strict info is hidden if TP is enabled.");

    let mainView = document.getElementById("identity-popup-mainView");
    viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
    let backButton = trackersView.querySelector(".subviewbutton-back");
    backButton.click();
    await viewShown;

    ok(true, "Main view was shown");

    let change = waitForSecurityChange(1);
    let timeoutPromise = new Promise(resolve => setTimeout(resolve, 1000));

    await ContentTask.spawn(browser, {}, function() {
      content.postMessage("more-tracking", "*");
    });

    let result = await Promise.race([change, timeoutPromise]);
    is(result, undefined, "No securityChange events should be received");

    viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Trackers view was shown");

    listItems = Array.from(trackersView.querySelectorAll(".identity-popup-content-blocking-list-item"));
    is(listItems.length, 2, "We have 2 trackers in the list");

    let listItem = listItems.find(item => item.querySelector("label").value == "trackertest.org");
    ok(listItem, "Has an item for trackertest.org");
    ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
    is(listItem.classList.contains("allowed"), !blocked,
      "Indicates whether the tracker was blocked or allowed");

    listItem = listItems.find(item => item.querySelector("label").value == "itisatracker.org");
    ok(listItem, "Has an item for itisatracker.org");
    ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
    is(listItem.classList.contains("allowed"), !blocked,
      "Indicates whether the tracker was blocked or allowed");
  });
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
  Services.perms.add(uri, "trackingprotection", Services.perms.ALLOW_ACTION);
  await assertSitesListed(false);
  info("Testing trackers subview with TP enabled and a CB exception removed.");
  Services.perms.remove(uri, "trackingprotection");
  await assertSitesListed(true);

  Services.prefs.clearUserPref(TP_PREF);
});
