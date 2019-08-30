/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const COOKIE_PAGE =
  "http://not-tracking.example.com/browser/browser/base/content/test/trackingUI/cookiePage.html";
const CONTAINER_PAGE =
  "http://not-tracking.example.com/browser/browser/base/content/test/trackingUI/containerPage.html";

const TPC_PREF = "network.cookie.cookieBehavior";

add_task(async function setup() {
  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(() => {
    UrlClassifierTestUtils.cleanupTestTrackers();
  });
});

/*
 * Accepts an array containing 6 elements that identify the testcase:
 * [0] - boolean indicating whether trackers are blocked.
 * [1] - boolean indicating whether third party cookies are blocked.
 * [2] - boolean indicating whether first party cookies are blocked.
 * [3] - integer indicating number of expected content blocking events.
 * [4] - integer indicating number of expected subview list headers.
 * [5] - integer indicating number of expected cookie list items.
 * [6] - integer indicating number of expected cookie list items
 *       after loading a cookie-setting third party URL in an iframe
 * [7] - integer indicating number of expected cookie list items
 *       after loading a cookie-setting first party URL in an iframe
 */
async function assertSitesListed(testCase) {
  let sitesListedTestCases = [
    [true, false, false, 4, 1, 1, 1, 1],
    [true, true, false, 5, 1, 1, 2, 2],
    [true, true, true, 6, 2, 2, 3, 3],
    [false, false, false, 3, 1, 1, 1, 1],
  ];
  let [
    trackersBlocked,
    thirdPartyBlocked,
    firstPartyBlocked,
    contentBlockingEventCount,
    listHeaderCount,
    cookieItemsCount1,
    cookieItemsCount2,
    cookieItemsCount3,
  ] = sitesListedTestCases[testCase];
  let promise = BrowserTestUtils.openNewForegroundTab({
    url: COOKIE_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([
    promise,
    waitForContentBlockingEvent(contentBlockingEventCount),
  ]);
  let browser = tab.linkedBrowser;

  await openProtectionsPopup();

  let categoryItem = document.getElementById(
    "protections-popup-category-cookies"
  );
  ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
  let cookiesView = document.getElementById("protections-popup-cookiesView");
  let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Cookies view was shown");

  let listHeaders = cookiesView.querySelectorAll(
    ".protections-popup-cookiesView-list-header"
  );
  is(
    listHeaders.length,
    listHeaderCount,
    `We have ${listHeaderCount} list headers.`
  );
  if (listHeaderCount == 1) {
    ok(
      !BrowserTestUtils.is_visible(listHeaders[0]),
      "Only one header, should be hidden"
    );
  } else {
    for (let header of listHeaders) {
      ok(
        BrowserTestUtils.is_visible(header),
        "Multiple list headers - all should be visible."
      );
    }
  }

  let emptyLabels = cookiesView.querySelectorAll(
    ".protections-popup-empty-label"
  );
  is(emptyLabels.length, 0, `We have no empty labels`);

  let listItems = cookiesView.querySelectorAll(".protections-popup-list-item");
  is(
    listItems.length,
    cookieItemsCount1,
    `We have ${cookieItemsCount1} cookies in the list`
  );

  if (trackersBlocked) {
    let trackerTestItem;
    for (let item of listItems) {
      let label = item.querySelector(".protections-popup-list-host-label");
      if (label.value == "http://trackertest.org") {
        trackerTestItem = item;
        break;
      }
    }
    ok(trackerTestItem, "Has an item for trackertest.org");
    ok(BrowserTestUtils.is_visible(trackerTestItem), "List item is visible");
  }

  if (firstPartyBlocked) {
    let notTrackingExampleItem;
    for (let item of listItems) {
      let label = item.querySelector(".protections-popup-list-host-label");
      if (label.value == "http://not-tracking.example.com") {
        notTrackingExampleItem = item;
        break;
      }
    }
    ok(notTrackingExampleItem, "Has an item for not-tracking.example.com");
    ok(
      BrowserTestUtils.is_visible(notTrackingExampleItem),
      "List item is visible"
    );
  }
  let mainView = document.getElementById("protections-popup-mainView");
  viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  let backButton = cookiesView.querySelector(".subviewbutton-back");
  backButton.click();
  await viewShown;

  ok(true, "Main view was shown");

  let change = waitForContentBlockingEvent();
  let timeoutPromise = new Promise(resolve => setTimeout(resolve, 1000));

  await ContentTask.spawn(browser, {}, function() {
    content.postMessage("third-party-cookie", "*");
  });

  let result = await Promise.race([change, timeoutPromise]);
  is(result, undefined, "No contentBlockingEvent events should be received");

  viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Cookies view was shown");

  emptyLabels = cookiesView.querySelectorAll(".protections-popup-empty-label");
  is(emptyLabels.length, 0, `We have no empty labels`);

  listItems = cookiesView.querySelectorAll(".protections-popup-list-item");
  is(
    listItems.length,
    cookieItemsCount2,
    `We have ${cookieItemsCount2} cookies in the list`
  );

  if (thirdPartyBlocked) {
    let test1ExampleItem;
    for (let item of listItems) {
      let label = item.querySelector(".protections-popup-list-host-label");
      if (label.value == "https://test1.example.org") {
        test1ExampleItem = item;
        break;
      }
    }
    ok(test1ExampleItem, "Has an item for test1.example.org");
    ok(BrowserTestUtils.is_visible(test1ExampleItem), "List item is visible");
  }

  if (trackersBlocked || thirdPartyBlocked || firstPartyBlocked) {
    let trackerTestItem;
    for (let item of listItems) {
      let label = item.querySelector(".protections-popup-list-host-label");
      if (label.value == "http://trackertest.org") {
        trackerTestItem = item;
        break;
      }
    }
    ok(trackerTestItem, "List item should exist for http://trackertest.org");
    ok(BrowserTestUtils.is_visible(trackerTestItem), "List item is visible");
  }

  viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  backButton.click();
  await viewShown;

  ok(true, "Main view was shown");

  change = waitForSecurityChange();
  timeoutPromise = new Promise(resolve => setTimeout(resolve, 1000));

  await ContentTask.spawn(browser, {}, function() {
    content.postMessage("first-party-cookie", "*");
  });

  result = await Promise.race([change, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Cookies view was shown");

  emptyLabels = cookiesView.querySelectorAll(".protections-popup-empty-label");
  is(emptyLabels.length, 0, "We have no empty labels");

  listItems = cookiesView.querySelectorAll(".protections-popup-list-item");
  is(
    listItems.length,
    cookieItemsCount3,
    `We have ${cookieItemsCount3} cookies in the list`
  );

  if (firstPartyBlocked) {
    let notTrackingExampleItem;
    for (let item of listItems) {
      let label = item.querySelector(".protections-popup-list-host-label");
      if (label.value == "http://not-tracking.example.com") {
        notTrackingExampleItem = item;
        break;
      }
    }
    ok(notTrackingExampleItem, "Has an item for not-tracking.example.com");
    ok(
      BrowserTestUtils.is_visible(notTrackingExampleItem),
      "List item is visible"
    );
  }

  BrowserTestUtils.removeTab(tab);
}

add_task(async function testCookiesSubView() {
  info("Testing cookies subview with reject tracking cookies.");
  Services.prefs.setIntPref(
    TPC_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  let testCaseIndex = 0;
  await assertSitesListed(testCaseIndex++);
  info("Testing cookies subview with reject third party cookies.");
  Services.prefs.setIntPref(
    TPC_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN
  );
  await assertSitesListed(testCaseIndex++);
  info("Testing cookies subview with reject all cookies.");
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT);
  await assertSitesListed(testCaseIndex++);
  info("Testing cookies subview with accept all cookies.");
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);
  await assertSitesListed(testCaseIndex++);

  Services.prefs.clearUserPref(TPC_PREF);
});

add_task(async function testCookiesSubViewAllowed() {
  Services.prefs.setIntPref(
    TPC_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://trackertest.org/"
  );
  Services.perms.addFromPrincipal(
    principal,
    "cookie",
    Services.perms.ALLOW_ACTION
  );

  let promise = BrowserTestUtils.openNewForegroundTab({
    url: COOKIE_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent(3)]);

  await openProtectionsPopup();

  let categoryItem = document.getElementById(
    "protections-popup-category-cookies"
  );
  ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
  let cookiesView = document.getElementById("protections-popup-cookiesView");
  let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Cookies view was shown");

  let listItems = cookiesView.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 1, "We have 1 cookie in the list");

  let listItem = listItems[0];
  let label = listItem.querySelector(".protections-popup-list-host-label");
  is(label.value, "http://trackertest.org", "has an item for trackertest.org");
  ok(BrowserTestUtils.is_visible(listItem), "list item is visible");
  ok(
    listItem.classList.contains("allowed"),
    "indicates whether the cookie was blocked or allowed"
  );

  let button = listItem.querySelector(
    ".identity-popup-permission-remove-button"
  );
  ok(
    BrowserTestUtils.is_visible(button),
    "Permission remove button is visible"
  );
  button.click();
  is(
    Services.perms.testExactPermissionFromPrincipal(principal, "cookie"),
    Services.perms.UNKNOWN_ACTION,
    "Button click should remove cookie pref."
  );
  ok(!listItem.classList.contains("allowed"), "Has removed the allowed class");

  BrowserTestUtils.removeTab(tab);

  Services.prefs.clearUserPref(TPC_PREF);
});

add_task(async function testCookiesSubViewAllowedHeuristic() {
  Services.prefs.setIntPref(
    TPC_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://not-tracking.example.com/"
  );

  // Pretend that the tracker has already been interacted with
  let trackerPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://trackertest.org/"
  );
  Services.perms.addFromPrincipal(
    trackerPrincipal,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  let promise = BrowserTestUtils.openNewForegroundTab({
    url: COOKIE_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent(5)]);
  let browser = tab.linkedBrowser;

  let popup;
  let windowCreated = TestUtils.topicObserved(
    "chrome-document-global-created",
    (subject, data) => {
      popup = subject;
      return true;
    }
  );
  let permChanged = TestUtils.topicObserved("perm-changed", (subject, data) => {
    return (
      subject &&
      subject.QueryInterface(Ci.nsIPermission).type ==
        "3rdPartyStorage^http://trackertest.org" &&
      subject.principal.origin == principal.origin &&
      data == "added"
    );
  });

  await ContentTask.spawn(browser, {}, function() {
    content.postMessage("window-open", "*");
  });
  await Promise.all([windowCreated, permChanged]);

  await new Promise(resolve => waitForFocus(resolve, popup));
  await new Promise(resolve => waitForFocus(resolve, window));

  await openProtectionsPopup();

  let categoryItem = document.getElementById(
    "protections-popup-category-cookies"
  );
  ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
  let cookiesView = document.getElementById("protections-popup-cookiesView");
  let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Cookies view was shown");

  let listItems = cookiesView.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 1, "We have 1 cookie in the list");

  let listItem = listItems[0];
  let label = listItem.querySelector(".protections-popup-list-host-label");
  is(label.value, "http://trackertest.org", "has an item for trackertest.org");
  ok(BrowserTestUtils.is_visible(listItem), "list item is visible");
  ok(
    listItem.classList.contains("allowed"),
    "indicates whether the cookie was blocked or allowed"
  );

  let button = listItem.querySelector(
    ".identity-popup-permission-remove-button"
  );
  ok(
    BrowserTestUtils.is_visible(button),
    "Permission remove button is visible"
  );
  button.click();
  is(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "3rdPartyStorage^http://trackertest.org"
    ),
    Services.perms.UNKNOWN_ACTION,
    "Button click should remove the storage pref."
  );
  ok(!listItem.classList.contains("allowed"), "Has removed the allowed class");

  await ContentTask.spawn(browser, {}, function() {
    content.postMessage("window-close", "*");
  });

  BrowserTestUtils.removeTab(tab);

  Services.prefs.clearUserPref(TPC_PREF);
});

add_task(async function testCookiesSubViewBlockedDoublyNested() {
  Services.prefs.setIntPref(
    TPC_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );

  let promise = BrowserTestUtils.openNewForegroundTab({
    url: CONTAINER_PAGE,
    gBrowser,
  });
  let [tab] = await Promise.all([promise, waitForContentBlockingEvent(3)]);

  await openProtectionsPopup();

  let categoryItem = document.getElementById(
    "protections-popup-category-cookies"
  );
  ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
  let cookiesView = document.getElementById("protections-popup-cookiesView");
  let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
  categoryItem.click();
  await viewShown;

  ok(true, "Cookies view was shown");

  let listItems = cookiesView.querySelectorAll(".protections-popup-list-item");
  is(listItems.length, 1, "We have 1 cookie in the list");

  let listItem = listItems[0];
  let label = listItem.querySelector(".protections-popup-list-host-label");
  is(label.value, "http://trackertest.org", "has an item for trackertest.org");
  ok(BrowserTestUtils.is_visible(listItem), "list item is visible");
  ok(
    !listItem.classList.contains("allowed"),
    "indicates whether the cookie was blocked or allowed"
  );

  let button = listItem.querySelector(
    ".identity-popup-permission-remove-button"
  );
  ok(!button, "Permission remove button doesn't exist");

  BrowserTestUtils.removeTab(tab);

  Services.prefs.clearUserPref(TPC_PREF);
});
