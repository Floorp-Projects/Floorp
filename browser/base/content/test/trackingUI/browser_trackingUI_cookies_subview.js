/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const COOKIE_PAGE = "http://not-tracking.example.com/browser/browser/base/content/test/trackingUI/cookiePage.html";

const TPC_PREF = "network.cookie.cookieBehavior";

add_task(async function setup() {
  // Avoid the content blocking tour interfering with our tests by popping up.
  await SpecialPowers.pushPrefEnv({set: [[ContentBlocking.prefIntroCount, ContentBlocking.MAX_INTROS]]});
  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(() => {
    UrlClassifierTestUtils.cleanupTestTrackers();
  });
});

async function assertSitesListed(trackersBlocked, thirdPartyBlocked, firstPartyBlocked) {
  await BrowserTestUtils.withNewTab(COOKIE_PAGE, async function(browser) {
    await openIdentityPopup();

    let categoryItem =
      document.getElementById("identity-popup-content-blocking-category-cookies");
    ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
    let cookiesView = document.getElementById("identity-popup-cookiesView");
    let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Cookies view was shown");

    let listHeaders = cookiesView.querySelectorAll(".identity-popup-cookiesView-list-header");
    is(listHeaders.length, 3, "We have 3 list headers");

    let emptyLabels = cookiesView.querySelectorAll(".identity-popup-content-blocking-empty-label");
    is(emptyLabels.length, 2, "We have 2 empty labels");

    let listItems = cookiesView.querySelectorAll(".identity-popup-content-blocking-list-item");
    is(listItems.length, 1, "We have 1 cookie in the list");

    let listItem = listItems[0];
    let label = listItem.querySelector(".identity-popup-content-blocking-list-host-label");
    is(label.value, "http://trackertest.org", "Has an item for trackertest.org");
    ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
    is(listItem.classList.contains("allowed"), !trackersBlocked,
      "Indicates whether the cookie was blocked or allowed");

    let mainView = document.getElementById("identity-popup-mainView");
    viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
    let backButton = cookiesView.querySelector(".subviewbutton-back");
    backButton.click();
    await viewShown;

    ok(true, "Main view was shown");

    let change = waitForSecurityChange();
    let timeoutPromise = new Promise(resolve => setTimeout(resolve, 1000));

    await ContentTask.spawn(browser, {}, function() {
      content.postMessage("third-party-cookie", "*");
    });

    let result = await Promise.race([change, timeoutPromise]);
    is(result, undefined, "No securityChange events should be received");

    viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Cookies view was shown");

    emptyLabels = cookiesView.querySelectorAll(".identity-popup-content-blocking-empty-label");
    is(emptyLabels.length, 1, "We have 1 empty label");

    listItems = cookiesView.querySelectorAll(".identity-popup-content-blocking-list-item");
    is(listItems.length, 2, "We have 2 cookies in the list");

    listItem = listItems[1];
    label = listItem.querySelector(".identity-popup-content-blocking-list-host-label");
    is(label.value, "https://test1.example.org", "Has an item for test1.example.org");
    ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
    is(listItem.classList.contains("allowed"), !thirdPartyBlocked,
      "Indicates whether the cookie was blocked or allowed");

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

    emptyLabels = cookiesView.querySelectorAll(".identity-popup-content-blocking-empty-label");
    is(emptyLabels.length, 0, "We have 0 empty label");

    listItems = cookiesView.querySelectorAll(".identity-popup-content-blocking-list-item");
    is(listItems.length, 3, "We have 2 cookies in the list");

    listItem = listItems[0];
    label = listItem.querySelector(".identity-popup-content-blocking-list-host-label");
    is(label.value, "http://not-tracking.example.com", "Has an item for the first party");
    ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
    is(listItem.classList.contains("allowed"), !firstPartyBlocked,
      "Indicates whether the cookie was blocked or allowed");
  });
}

add_task(async function testCookiesSubView() {
  info("Testing cookies subview with reject tracking cookies.");
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);
  await assertSitesListed(true, false, false);
  info("Testing cookies subview with reject third party cookies.");
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN);
  await assertSitesListed(true, true, false);
  info("Testing cookies subview with reject all cookies.");
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT);
  await assertSitesListed(true, true, true);
  info("Testing cookies subview with accept all cookies.");
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);
  await assertSitesListed(false, false, false);

  Services.prefs.clearUserPref(TPC_PREF);
});

add_task(async function testCookiesSubViewAllowed() {
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);
  let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("http://trackertest.org/");
  Services.perms.addFromPrincipal(principal, "cookie", Services.perms.ALLOW_ACTION);

  await BrowserTestUtils.withNewTab(COOKIE_PAGE, async function(browser) {
    await openIdentityPopup();

    let categoryItem =
      document.getElementById("identity-popup-content-blocking-category-cookies");
    ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
    let cookiesView = document.getElementById("identity-popup-cookiesView");
    let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Cookies view was shown");

    let listItems = cookiesView.querySelectorAll(".identity-popup-content-blocking-list-item");
    is(listItems.length, 1, "We have 1 cookie in the list");

    let listItem = listItems[0];
    let label = listItem.querySelector(".identity-popup-content-blocking-list-host-label");
    is(label.value, "http://trackertest.org", "Has an item for trackertest.org");
    ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
    ok(listItem.classList.contains("allowed"), "Indicates whether the cookie was blocked or allowed");

    let button = listItem.querySelector(".identity-popup-permission-remove-button");
    ok(BrowserTestUtils.is_visible(button), "Permission remove button is visible");
    button.click();
    is(Services.perms.testExactPermissionFromPrincipal(principal, "cookie"), Services.perms.UNKNOWN_ACTION, "Button click should remove cookie pref.");
    ok(!listItem.classList.contains("allowed"), "Has removed the allowed class");
  });

  Services.prefs.clearUserPref(TPC_PREF);
});

add_task(async function testCookiesSubViewAllowedHeuristic() {
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);
  let principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("http://not-tracking.example.com/");

  // Pretend that the tracker has already been interacted with
  let trackerPrincipal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin("http://trackertest.org/");
  Services.perms.addFromPrincipal(trackerPrincipal, "storageAccessAPI", Services.perms.ALLOW_ACTION);

  await BrowserTestUtils.withNewTab(COOKIE_PAGE, async function(browser) {
    let popup;
    let windowCreated = TestUtils.topicObserved("chrome-document-global-created", (subject, data) => {
      popup = subject;
      return true;
    });
    let permChanged = TestUtils.topicObserved("perm-changed",
      (subject, data) => {
        return subject &&
               subject.QueryInterface(Ci.nsIPermission)
                      .type == "3rdPartyStorage^http://trackertest.org" &&
               subject.principal.origin == principal.origin &&
               data == "added";
      });

    await ContentTask.spawn(browser, {}, function() {
      content.postMessage("window-open", "*");
    });
    await Promise.all([windowCreated, permChanged]);

    await new Promise(resolve => waitForFocus(resolve, popup));
    await new Promise(resolve => waitForFocus(resolve, window));

    await openIdentityPopup();

    let categoryItem =
      document.getElementById("identity-popup-content-blocking-category-cookies");
    ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
    let cookiesView = document.getElementById("identity-popup-cookiesView");
    let viewShown = BrowserTestUtils.waitForEvent(cookiesView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Cookies view was shown");

    let listItems = cookiesView.querySelectorAll(".identity-popup-content-blocking-list-item");
    is(listItems.length, 1, "We have 1 cookie in the list");

    let listItem = listItems[0];
    let label = listItem.querySelector(".identity-popup-content-blocking-list-host-label");
    is(label.value, "http://trackertest.org", "Has an item for trackertest.org");
    ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
    ok(listItem.classList.contains("allowed"), "Indicates whether the cookie was blocked or allowed");

    let button = listItem.querySelector(".identity-popup-permission-remove-button");
    ok(BrowserTestUtils.is_visible(button), "Permission remove button is visible");
    button.click();
    is(Services.perms.testExactPermissionFromPrincipal(principal, "3rdPartyStorage^http://trackertest.org"), Services.perms.UNKNOWN_ACTION, "Button click should remove the storage pref.");
    ok(!listItem.classList.contains("allowed"), "Has removed the allowed class");

    await ContentTask.spawn(browser, {}, function() {
      content.postMessage("window-close", "*");
    });
  });

  Services.prefs.clearUserPref(TPC_PREF);
});
