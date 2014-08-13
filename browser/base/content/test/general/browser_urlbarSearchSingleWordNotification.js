/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let notificationObserver;
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.fixup.domainwhitelist.localhost");
  if (notificationObserver) {
    notificationObserver.disconnect();
  }
});

function promiseNotificationForTab(aBrowser, value, expected, tab=aBrowser.selectedTab) {
  let deferred = Promise.defer();
  let notificationBox = aBrowser.getNotificationBox(tab.linkedBrowser);
  if (expected) {
    let checkForNotification = function() {
      if (notificationBox.getNotificationWithValue(value)) {
        notificationObserver.disconnect();
        notificationObserver = null;
        deferred.resolve();
      }
    }
    if (notificationObserver) {
      notificationObserver.disconnect();
    }
    notificationObserver = new MutationObserver(checkForNotification);
    notificationObserver.observe(notificationBox, {childList: true});
  } else {
    setTimeout(() => {
      is(notificationBox.getNotificationWithValue(value), null, "We are expecting to not get a notification");
      deferred.resolve();
    }, 1000);
  }
  return deferred.promise;
}

function* runURLBarSearchTest(valueToOpen, expectSearch, expectNotification, aWindow=window) {
  aWindow.gURLBar.value = valueToOpen;
  let expectedURI;
  if (!expectSearch) {
    expectedURI = "http://" + valueToOpen + "/";
  } else {
    yield new Promise(resolve => {
      Services.search.init(resolve);
    });
    expectedURI = Services.search.defaultEngine.getSubmission(valueToOpen, null, "keyword").uri.spec;
  }
  aWindow.gURLBar.focus();
  let docLoadPromise = waitForDocLoadAndStopIt(expectedURI, aWindow.gBrowser);
  EventUtils.synthesizeKey("VK_RETURN", {}, aWindow);

  yield docLoadPromise;

  yield promiseNotificationForTab(aWindow.gBrowser, "keyword-uri-fixup", expectNotification);
}

add_task(function* test_navigate_full_domain() {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield* runURLBarSearchTest("www.mozilla.org", false, false);
  gBrowser.removeTab(tab);
});

function get_test_function_for_localhost_with_hostname(hostName, isPrivate) {
  return function* test_navigate_single_host() {
    const pref = "browser.fixup.domainwhitelist.localhost";
    let win;
    if (isPrivate) {
      win = yield promiseOpenAndLoadWindow({private: true}, true);
      let deferredOpenFocus = Promise.defer();
      waitForFocus(deferredOpenFocus.resolve, win);
      yield deferredOpenFocus.promise;
    } else {
      win = window;
    }
    let browser = win.gBrowser;
    let tab = browser.selectedTab = browser.addTab();

    Services.prefs.setBoolPref(pref, false);
    yield* runURLBarSearchTest(hostName, true, true, win);

    let notificationBox = browser.getNotificationBox(tab.linkedBrowser);
    let notification = notificationBox.getNotificationWithValue("keyword-uri-fixup");
    let docLoadPromise = waitForDocLoadAndStopIt("http://" + hostName + "/", browser);
    notification.querySelector(".notification-button-default").click();

    // check pref value
    let prefValue = Services.prefs.getBoolPref(pref);
    is(prefValue, !isPrivate, "Pref should have the correct state.");

    yield docLoadPromise;
    browser.removeTab(tab);

    // Now try again with the pref set.
    let tab = browser.selectedTab = browser.addTab();
    // In a private window, the notification should appear again.
    yield* runURLBarSearchTest(hostName, isPrivate, isPrivate, win);
    browser.removeTab(tab);
    if (isPrivate) {
      info("Waiting for private window to close");
      yield promiseWindowClosed(win);
      let deferredFocus = Promise.defer();
      info("Waiting for focus");
      waitForFocus(deferredFocus.resolve, window);
      yield deferredFocus.promise;
    }
  }
}

add_task(get_test_function_for_localhost_with_hostname("localhost"));
add_task(get_test_function_for_localhost_with_hostname("localhost."));
add_task(get_test_function_for_localhost_with_hostname("localhost", true));

add_task(function* test_navigate_invalid_url() {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield* runURLBarSearchTest("mozilla is awesome", true, false);
  gBrowser.removeTab(tab);
});
