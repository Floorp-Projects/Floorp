/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.fixup.domainwhitelist.localhost");
});

function promiseNotificationForTab(value, expected, tab=gBrowser.selectedTab) {
  let deferred = Promise.defer();
  let notificationBox = gBrowser.getNotificationBox(tab.linkedBrowser);
  if (expected) {
    waitForCondition(() => notificationBox.getNotificationWithValue(value) !== null,
                     deferred.resolve, "Were expecting to get a notification");
  } else {
    setTimeout(() => {
      is(notificationBox.getNotificationWithValue(value), null, "We are expecting to not get a notification");
      deferred.resolve();
    }, 1000);
  }
  return deferred.promise;
}

function* runURLBarSearchTest(valueToOpen, expectSearch, expectNotification) {
  gURLBar.value = valueToOpen;
  let expectedURI;
  if (!expectSearch) {
    expectedURI = "http://" + valueToOpen + "/";
  } else {
    yield new Promise(resolve => {
      Services.search.init(resolve)
    });
    expectedURI = Services.search.defaultEngine.getSubmission(valueToOpen, null, "keyword").uri.spec;
  }
  gURLBar.focus();
  let docLoadPromise = waitForDocLoadAndStopIt(expectedURI);
  EventUtils.synthesizeKey("VK_RETURN", {});

  yield docLoadPromise;

  yield promiseNotificationForTab("keyword-uri-fixup", expectNotification);
}

add_task(function* test_navigate_full_domain() {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield* runURLBarSearchTest("www.mozilla.org", false, false);
  gBrowser.removeTab(tab);
});

add_task(function* test_navigate_single_host() {
  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.localhost", false);
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield* runURLBarSearchTest("localhost", true, true);

  let notificationBox = gBrowser.getNotificationBox(tab.linkedBrowser);
  let notification = notificationBox.getNotificationWithValue("keyword-uri-fixup");
  let docLoadPromise = waitForDocLoadAndStopIt("http://localhost/");
  notification.querySelector(".notification-button-default").click();

  // check pref value
  let pref = "browser.fixup.domainwhitelist.localhost";
  let prefValue = Services.prefs.getBoolPref(pref);
  ok(prefValue, "Pref should have been toggled");

  yield docLoadPromise;
  gBrowser.removeTab(tab);

  // Now try again with the pref set.
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield* runURLBarSearchTest("localhost", false, false);
  gBrowser.removeTab(tab);
});

add_task(function* test_navigate_invalid_url() {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield* runURLBarSearchTest("mozilla is awesome", true, false);
  gBrowser.removeTab(tab);
});

