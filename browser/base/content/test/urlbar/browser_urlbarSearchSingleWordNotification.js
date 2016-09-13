/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var notificationObserver;
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.fixup.domainwhitelist.localhost");
  if (notificationObserver) {
    notificationObserver.disconnect();
  }
});

function promiseNotification(aBrowser, value, expected, input) {
  let deferred = Promise.defer();
  let notificationBox = aBrowser.getNotificationBox(aBrowser.selectedBrowser);
  if (expected) {
    info("Waiting for " + value + " notification");
    let checkForNotification = function() {
      if (notificationBox.getNotificationWithValue(value)) {
        info("Saw the notification");
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
      is(notificationBox.getNotificationWithValue(value), null,
         `We are expecting to not get a notification for ${input}`);
      deferred.resolve();
    }, 1000);
  }
  return deferred.promise;
}

function* runURLBarSearchTest({valueToOpen, expectSearch, expectNotification, aWindow=window}) {
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
  let docLoadPromise = waitForDocLoadAndStopIt(expectedURI, aWindow.gBrowser.selectedBrowser);
  EventUtils.synthesizeKey("VK_RETURN", {}, aWindow);

  yield Promise.all([
    docLoadPromise,
    promiseNotification(aWindow.gBrowser, "keyword-uri-fixup", expectNotification, valueToOpen)
  ]);
}

add_task(function* test_navigate_full_domain() {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield* runURLBarSearchTest({
    valueToOpen: "www.mozilla.org",
    expectSearch: false,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(function* test_navigate_decimal_ip() {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield* runURLBarSearchTest({
    valueToOpen: "1234",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(function* test_navigate_decimal_ip_with_path() {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield* runURLBarSearchTest({
    valueToOpen: "1234/12",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(function* test_navigate_large_number() {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield* runURLBarSearchTest({
    valueToOpen: "123456789012345",
    expectSearch: true,
    expectNotification: false
  });
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
    let tab = browser.selectedTab = browser.addTab("about:blank");
    yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    Services.prefs.setBoolPref(pref, false);
    yield* runURLBarSearchTest({
      valueToOpen: hostName,
      expectSearch: true,
      expectNotification: true,
      aWindow: win,
    });

    let notificationBox = browser.getNotificationBox(tab.linkedBrowser);
    let notification = notificationBox.getNotificationWithValue("keyword-uri-fixup");
    let docLoadPromise = waitForDocLoadAndStopIt("http://" + hostName + "/", tab.linkedBrowser);
    notification.querySelector(".notification-button-default").click();

    // check pref value
    let prefValue = Services.prefs.getBoolPref(pref);
    is(prefValue, !isPrivate, "Pref should have the correct state.");

    yield docLoadPromise;
    browser.removeTab(tab);

    // Now try again with the pref set.
    tab = browser.selectedTab = browser.addTab("about:blank");
    yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    // In a private window, the notification should appear again.
    yield* runURLBarSearchTest({
      valueToOpen: hostName,
      expectSearch: isPrivate,
      expectNotification: isPrivate,
      aWindow: win,
    });
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
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield* runURLBarSearchTest({
    valueToOpen: "mozilla is awesome",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});
