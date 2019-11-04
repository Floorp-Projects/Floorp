/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE =
  "http://example.com/browser/browser/base/content/test/trackingUI/trackingPage.html";

const ST_PROTECTION_PREF = "privacy.trackingprotection.socialtracking.enabled";
const ST_BLOCK_COOKIES_PREF = "privacy.socialtracking.block_cookies.enabled";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [ST_BLOCK_COOKIES_PREF, true],
      [
        "urlclassifier.features.socialtracking.blacklistHosts",
        "social-tracking.example.org",
      ],
      [
        "urlclassifier.features.socialtracking.annotate.blacklistHosts",
        "social-tracking.example.org",
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
    ],
  });
});

async function testIdentityState(hasException) {
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

  let categoryItem = document.getElementById(
    "protections-popup-category-socialblock"
  );

  ok(
    categoryItem.classList.contains("notFound"),
    "socialtrackings are not detected"
  );

  ok(
    BrowserTestUtils.is_visible(gProtectionsHandler.iconBox),
    "icon box is visible regardless the exception"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    content.postMessage("socialtracking", "*");
  });

  await TestUtils.waitForCondition(() => {
    return !categoryItem.classList.contains("notFound");
  });

  ok(
    gProtectionsHandler._protectionsPopup.hasAttribute("detected"),
    "trackers are detected"
  );
  ok(
    !categoryItem.classList.contains("notFound"),
    "social trackers are detected"
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

  BrowserTestUtils.removeTab(tab);
}

async function testSubview(hasException) {
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
    content.postMessage("socialtracking", "*");
  });
  await promise;

  await openProtectionsPopup();

  let categoryItem = document.getElementById(
    "protections-popup-category-socialblock"
  );
  ok(BrowserTestUtils.is_visible(categoryItem), "STP category item is visible");
  ok(
    categoryItem.classList.contains("blocked"),
    "STP category item is blocked"
  );
  let subview = document.getElementById("protections-popup-socialblockView");
  let viewShown = BrowserTestUtils.waitForEvent(subview, "ViewShown");
  categoryItem.click();
  await viewShown;

  let listItems = subview.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 1, "We have 1 item in the list");
  let listItem = listItems[0];
  ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
  is(
    listItem.querySelector("label").value,
    "social-tracking.example.org",
    "Has the correct host"
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

  BrowserTestUtils.removeTab(tab);
}

async function testCategoryItem(blockLoads) {
  if (blockLoads) {
    Services.prefs.setBoolPref(ST_PROTECTION_PREF, true);
  }

  Services.prefs.setBoolPref(ST_BLOCK_COOKIES_PREF, false);

  let promise = BrowserTestUtils.openNewForegroundTab({
    url: TRACKING_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent()]);

  let categoryItem = document.getElementById(
    "protections-popup-category-socialblock"
  );

  ok(
    !categoryItem.classList.contains("blocked"),
    "Category not marked as blocked"
  );
  ok(
    categoryItem.classList.contains("notFound"),
    "Category marked as not found"
  );

  Services.prefs.setBoolPref(ST_BLOCK_COOKIES_PREF, true);

  ok(categoryItem.classList.contains("blocked"), "Category marked as blocked");
  ok(
    categoryItem.classList.contains("notFound"),
    "Category marked as not found"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
    content.postMessage("socialtracking", "*");
  });

  await TestUtils.waitForCondition(() => {
    return !categoryItem.classList.contains("notFound");
  });

  ok(categoryItem.classList.contains("blocked"), "Category marked as blocked");

  ok(
    !categoryItem.classList.contains("notFound"),
    "Category not marked as not found"
  );

  BrowserTestUtils.removeTab(tab);

  Services.prefs.clearUserPref(ST_PROTECTION_PREF);
}

add_task(async function testIdentityUI() {
  requestLongerTimeout(2);

  await testIdentityState(false);
  await testIdentityState(true);

  await testSubview(false);
  await testSubview(true);

  await testCategoryItem(false);
  await testCategoryItem(true);
});
