/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE =
  "http://example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";
const FP_PROTECTION_PREF = "privacy.trackingprotection.fingerprinting.enabled";
let fpHistogram;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "urlclassifier.features.fingerprinting.blacklistHosts",
        "fingerprinting.example.com",
      ],
      [
        "urlclassifier.features.fingerprinting.annotate.blacklistHosts",
        "fingerprinting.example.com",
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.annotate_channels", false],
      ["privacy.trackingprotection.cryptomining.enabled", false],
      ["urlclassifier.features.cryptomining.annotate.blacklistHosts", ""],
      ["urlclassifier.features.cryptomining.annotate.blacklistTables", ""],
    ],
  });
  fpHistogram = Services.telemetry.getHistogramById(
    "FINGERPRINTERS_BLOCKED_COUNT"
  );
  registerCleanupFunction(() => {
    fpHistogram.clear();
  });
});

async function testIdentityState(hasException) {
  fpHistogram.clear();
  let promise = BrowserTestUtils.openNewForegroundTab({
    url: TRACKING_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent()]);

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      TRACKING_PAGE
    );
    gProtectionsHandler.disableForCurrentPage();
    await loaded;
  }

  ok(
    !gProtectionsHandler._protectionsPopup.hasAttribute("detected"),
    "fingerprinters are not detected"
  );
  ok(
    !BrowserTestUtils.is_hidden(gProtectionsHandler.iconBox),
    "icon box is visible regardless the exception"
  );

  promise = waitForContentBlockingEvent();

  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    content.postMessage("fingerprinting", "*");
  });

  await promise;

  ok(
    gProtectionsHandler._protectionsPopup.hasAttribute("detected"),
    "trackers are detected"
  );
  ok(
    BrowserTestUtils.is_visible(gProtectionsHandler.iconBox),
    "icon box is visible"
  );
  is(
    gProtectionsHandler.iconBox.hasAttribute("hasException"),
    hasException,
    "Shows an exception when appropriate"
  );

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      TRACKING_PAGE
    );
    gProtectionsHandler.enableForCurrentPage();
    await loaded;
  }

  let loads = hasException ? 3 : 1;
  testTelemetry(loads, 1, hasException);

  BrowserTestUtils.removeTab(tab);
}

async function testCategoryItem() {
  Services.prefs.setBoolPref(FP_PROTECTION_PREF, false);
  let promise = BrowserTestUtils.openNewForegroundTab({
    url: TRACKING_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent()]);

  let categoryItem = document.getElementById(
    "protections-popup-category-fingerprinters"
  );

  ok(
    !categoryItem.classList.contains("blocked"),
    "Category not marked as blocked"
  );
  ok(
    categoryItem.classList.contains("notFound"),
    "Category marked as not found"
  );
  Services.prefs.setBoolPref(FP_PROTECTION_PREF, true);
  ok(categoryItem.classList.contains("blocked"), "Category marked as blocked");
  ok(
    categoryItem.classList.contains("notFound"),
    "Category marked as not found"
  );
  Services.prefs.setBoolPref(FP_PROTECTION_PREF, false);
  ok(
    !categoryItem.classList.contains("blocked"),
    "Category not marked as blocked"
  );
  ok(
    categoryItem.classList.contains("notFound"),
    "Category marked as not found"
  );

  promise = waitForContentBlockingEvent();

  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    content.postMessage("fingerprinting", "*");
  });

  await promise;

  ok(
    !categoryItem.classList.contains("blocked"),
    "Category not marked as blocked"
  );
  ok(
    !categoryItem.classList.contains("notFound"),
    "Category not marked as not found"
  );
  Services.prefs.setBoolPref(FP_PROTECTION_PREF, true);
  ok(categoryItem.classList.contains("blocked"), "Category marked as blocked");
  ok(
    !categoryItem.classList.contains("notFound"),
    "Category not marked as not found"
  );
  Services.prefs.setBoolPref(FP_PROTECTION_PREF, false);
  ok(
    !categoryItem.classList.contains("blocked"),
    "Category not marked as blocked"
  );
  ok(
    !categoryItem.classList.contains("notFound"),
    "Category not marked as not found"
  );

  BrowserTestUtils.removeTab(tab);
}

async function testSubview(hasException) {
  fpHistogram.clear();
  let promise = BrowserTestUtils.openNewForegroundTab({
    url: TRACKING_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent()]);

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      TRACKING_PAGE
    );
    gProtectionsHandler.disableForCurrentPage();
    await loaded;
  }

  promise = waitForContentBlockingEvent();
  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    content.postMessage("fingerprinting", "*");
  });
  await promise;

  await openProtectionsPanel();

  let categoryItem = document.getElementById(
    "protections-popup-category-fingerprinters"
  );

  // Explicitly waiting for the category item becoming visible.
  await TestUtils.waitForCondition(() => {
    return BrowserTestUtils.is_visible(categoryItem);
  });

  ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");

  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  // We have to wait until the ContentBlockingLog gets updated in the content.
  // Unfortunately, we need to use the setTimeout here since we don't have an
  // easy to know whether the log is updated in the content. This should be
  // removed after the log been removed in the content (Bug 1599046).
  await new Promise(resolve => {
    setTimeout(resolve, 500);
  });
  /* eslint-enable mozilla/no-arbitrary-setTimeout */

  let subview = document.getElementById("protections-popup-fingerprintersView");
  let viewShown = BrowserTestUtils.waitForEvent(subview, "ViewShown");
  categoryItem.click();
  await viewShown;

  let listItems = subview.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 1, "We have 1 item in the list");
  let listItem = listItems[0];
  ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
  is(
    listItem.querySelector("label").value,
    "https://fingerprinting.example.com",
    "Has the correct host"
  );
  is(
    listItem.classList.contains("allowed"),
    hasException,
    "Indicates the fingerprinter was blocked or allowed"
  );

  let mainView = document.getElementById("protections-popup-mainView");
  viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  let backButton = subview.querySelector(".subviewbutton-back");
  backButton.click();
  await viewShown;

  ok(true, "Main view was shown");

  if (hasException) {
    let loaded = BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      TRACKING_PAGE
    );
    gProtectionsHandler.enableForCurrentPage();
    await loaded;
  }

  let loads = hasException ? 3 : 1;
  testTelemetry(loads, 1, hasException);

  BrowserTestUtils.removeTab(tab);
}

function testTelemetry(pagesVisited, pagesWithBlockableContent, hasException) {
  let results = fpHistogram.snapshot();
  Assert.equal(
    results.values[0],
    pagesVisited,
    "The correct number of page loads have been recorded"
  );
  let expectedValue = hasException ? 2 : 1;
  Assert.equal(
    results.values[expectedValue],
    pagesWithBlockableContent,
    "The correct number of fingerprinters have been recorded as blocked or allowed."
  );
}

add_task(async function test() {
  Services.prefs.setBoolPref(FP_PROTECTION_PREF, true);

  await testIdentityState(false);
  await testIdentityState(true);

  await testSubview(false);
  await testSubview(true);

  await testCategoryItem();

  Services.prefs.clearUserPref(FP_PROTECTION_PREF);
});
