/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE = "http://example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
const CM_PROTECTION_PREF = "privacy.trackingprotection.cryptomining.enabled";
const CM_ANNOTATION_PREF = "privacy.trackingprotection.cryptomining.annotate.enabled";
let cmHistogram;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({set: [
    [ ContentBlocking.prefIntroCount, ContentBlocking.MAX_INTROS ],
    [ "urlclassifier.features.cryptomining.blacklistHosts", "cryptomining.example.com" ],
    [ "urlclassifier.features.cryptomining.annotate.blacklistHosts", "cryptomining.example.com" ],
    [ "privacy.trackingprotection.enabled", false ],
    [ "privacy.trackingprotection.annotate_channels", false ],
    [ "privacy.trackingprotection.fingerprinting.enabled", false ],
    [ "privacy.trackingprotection.fingerprinting.annotate.enabled", false ],
  ]});
  cmHistogram = Services.telemetry.getHistogramById("CRYPTOMINERS_BLOCKED_COUNT");
  registerCleanupFunction(() => {
    cmHistogram.clear();
  });
});

async function testIdentityState(hasException) {
  cmHistogram.clear();
  let promise = BrowserTestUtils.openNewForegroundTab({url: TRACKING_PAGE, gBrowser});
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent()]);

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, TRACKING_PAGE);
    ContentBlocking.disableForCurrentPage();
    await loaded;
  }

  ok(!ContentBlocking.content.hasAttribute("detected"), "cryptominers are not detected");
  ok(BrowserTestUtils.is_hidden(ContentBlocking.iconBox), "icon box is not visible");

  promise = waitForContentBlockingEvent();

  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    content.postMessage("cryptomining", "*");
  });

  await promise;

  ok(ContentBlocking.content.hasAttribute("detected"), "trackers are detected");
  ok(BrowserTestUtils.is_visible(ContentBlocking.iconBox), "icon box is visible");
  is(ContentBlocking.iconBox.hasAttribute("hasException"), hasException,
    "Shows an exception when appropriate");

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, TRACKING_PAGE);
    ContentBlocking.enableForCurrentPage();
    await loaded;
  }

  let loads = hasException ? 3 : 1;
  testTelemetry(loads, 1, hasException);

  BrowserTestUtils.removeTab(tab);
}

async function testSubview(hasException) {
  cmHistogram.clear();
  let promise = BrowserTestUtils.openNewForegroundTab({url: TRACKING_PAGE, gBrowser});
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent()]);

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, TRACKING_PAGE);
    ContentBlocking.disableForCurrentPage();
    await loaded;
  }

  promise = waitForContentBlockingEvent();
  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    content.postMessage("cryptomining", "*");
  });
  await promise;

  await openIdentityPopup();

  let categoryItem =
    document.getElementById("identity-popup-content-blocking-category-cryptominers");
  ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
  let subview = document.getElementById("identity-popup-cryptominersView");
  let viewShown = BrowserTestUtils.waitForEvent(subview, "ViewShown");
  categoryItem.click();
  await viewShown;

  let listItems = subview.querySelectorAll(".identity-popup-content-blocking-list-item");
  is(listItems.length, 1, "We have 1 item in the list");
  let listItem = listItems[0];
  ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
  is(listItem.querySelector("label").value, "cryptomining.example.com", "Has the correct host");
  is(listItem.classList.contains("allowed"), hasException,
    "Indicates the miner was blocked or allowed");

  let mainView = document.getElementById("identity-popup-mainView");
  viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  let backButton = subview.querySelector(".subviewbutton-back");
  backButton.click();
  await viewShown;

  ok(true, "Main view was shown");

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, TRACKING_PAGE);
    ContentBlocking.enableForCurrentPage();
    await loaded;
  }

  let loads = hasException ? 3 : 1;
  testTelemetry(loads, 1, hasException);

  BrowserTestUtils.removeTab(tab);
}

function testTelemetry(pagesVisited, pagesWithBlockableContent, hasException) {
  let results = cmHistogram.snapshot();
  Assert.equal(results.values[0], pagesVisited, "The correct number of page loads have been recorded");
  let expectedValue = hasException ? 2 : 1;
  Assert.equal(results.values[expectedValue], pagesWithBlockableContent, "The correct number of cryptominers have been recorded as blocked or allowed.");
}

add_task(async function test() {
  Services.prefs.setBoolPref(CM_PROTECTION_PREF, true);
  Services.prefs.setBoolPref(CM_ANNOTATION_PREF, true);

  await testIdentityState(false);
  await testIdentityState(true);

  await testSubview(false);
  await testSubview(true);

  Services.prefs.clearUserPref(CM_PROTECTION_PREF);
  Services.prefs.clearUserPref(CM_ANNOTATION_PREF);
});

