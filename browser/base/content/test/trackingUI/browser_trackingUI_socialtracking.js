/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE =
  "http://example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
const STP_PROTECTION_PREF = "privacy.trackingprotection.socialtracking.enabled";
const STP_ANNOTATION_PREF =
  "privacy.trackingprotection.socialtracking.annotate.enabled";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "urlclassifier.features.socialtracking.blacklistHosts",
        "socialtracking.example.com",
      ],
      [
        "urlclassifier.features.socialtracking.annotate.blacklistHosts",
        "socialtracking.example.com",
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.annotate_channels", false],
      ["privacy.trackingprotection.cryptomining.enabled", false],
      ["privacy.trackingprotection.cryptomining.annotate.enabled", false],
      ["privacy.trackingprotection.fingerprinting.enabled", false],
      ["privacy.trackingprotection.fingerprinting.annotate.enabled", false],
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

  ok(
    !gProtectionsHandler._protectionsPopup.hasAttribute("detected"),
    "socialtrackings are not detected"
  );

  ok(
    BrowserTestUtils.is_visible(gProtectionsHandler.iconBox),
    "icon box is visible regardless the exception"
  );

  promise = waitForContentBlockingEvent();

  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    content.postMessage("socialtracking", "*");
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
  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    content.postMessage("socialtracking", "*");
  });
  await promise;

  await openProtectionsPopup();

  let categoryItem = document.getElementById(
    "protections-popup-category-socialblock"
  );
  ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
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
    "socialtracking.example.com",
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

add_task(async function test() {
  Services.prefs.setBoolPref(STP_PROTECTION_PREF, true);
  Services.prefs.setBoolPref(STP_ANNOTATION_PREF, true);

  await testIdentityState(false);
  await testIdentityState(true);
  await testSubview(false);
  await testSubview(true);

  Services.prefs.clearUserPref(STP_PROTECTION_PREF);
  Services.prefs.clearUserPref(STP_ANNOTATION_PREF);
});
