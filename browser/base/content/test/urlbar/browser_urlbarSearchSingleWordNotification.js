/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

var notificationObserver;
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.fixup.domainwhitelist.localhost");
  if (notificationObserver) {
    notificationObserver.disconnect();
  }
});

function promiseNotification(aBrowser, value, expected, input) {
  return new Promise(resolve => {
    let notificationBox = aBrowser.getNotificationBox(aBrowser.selectedBrowser);
    if (expected) {
      info("Waiting for " + value + " notification");
      let checkForNotification = function() {
        if (notificationBox.getNotificationWithValue(value)) {
          info("Saw the notification");
          notificationObserver.disconnect();
          notificationObserver = null;
          resolve();
        }
      };
      if (notificationObserver) {
        notificationObserver.disconnect();
      }
      notificationObserver = new MutationObserver(checkForNotification);
      notificationObserver.observe(notificationBox, {childList: true});
    } else {
      setTimeout(() => {
        is(notificationBox.getNotificationWithValue(value), null,
           `We are expecting to not get a notification for ${input}`);
        resolve();
      }, 1000);
    }
  });
}

async function runURLBarSearchTest({valueToOpen, expectSearch, expectNotification, aWindow = window}) {
  aWindow.gURLBar.value = valueToOpen;
  let expectedURI;
  if (!expectSearch) {
    expectedURI = "http://" + valueToOpen + "/";
  } else {
    await new Promise(resolve => {
      Services.search.init(resolve);
    });
    expectedURI = Services.search.defaultEngine.getSubmission(valueToOpen, null, "keyword").uri.spec;
  }
  aWindow.gURLBar.focus();
  let docLoadPromise = waitForDocLoadAndStopIt(expectedURI, aWindow.gBrowser.selectedBrowser);
  EventUtils.synthesizeKey("VK_RETURN", {}, aWindow);

  await Promise.all([
    docLoadPromise,
    promiseNotification(aWindow.gBrowser, "keyword-uri-fixup", expectNotification, valueToOpen)
  ]);
}

add_task(async function test_navigate_full_domain() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "www.mozilla.org",
    expectSearch: false,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_decimal_ip() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "1234",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_decimal_ip_with_path() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "1234/12",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_large_number() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "123456789012345",
    expectSearch: true,
    expectNotification: false
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_small_hex_number() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "0x1f00ffff",
    expectSearch: true,
    expectNotification: false
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_large_hex_number() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "0x7f0000017f000001",
    expectSearch: true,
    expectNotification: false
  });
  gBrowser.removeTab(tab);
});

function get_test_function_for_localhost_with_hostname(hostName, isPrivate) {
  return async function test_navigate_single_host() {
    const pref = "browser.fixup.domainwhitelist.localhost";
    let win;
    if (isPrivate) {
      let promiseWin = BrowserTestUtils.waitForNewWindow();
      win = OpenBrowserWindow({private: true});
      await promiseWin;
      await new Promise(resolve => {
        waitForFocus(resolve, win);
      });
    } else {
      win = window;
    }
    let browser = win.gBrowser;
    let tab = await BrowserTestUtils.openNewForegroundTab(browser);

    Services.prefs.setBoolPref(pref, false);
    await runURLBarSearchTest({
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

    await docLoadPromise;
    browser.removeTab(tab);

    // Now try again with the pref set.
    tab = browser.selectedTab = browser.addTab("about:blank");
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    // In a private window, the notification should appear again.
    await runURLBarSearchTest({
      valueToOpen: hostName,
      expectSearch: isPrivate,
      expectNotification: isPrivate,
      aWindow: win,
    });
    browser.removeTab(tab);
    if (isPrivate) {
      info("Waiting for private window to close");
      await BrowserTestUtils.closeWindow(win);
      await new Promise(resolve => {
        info("Waiting for focus");
        waitForFocus(resolve, window);
      });
    }
  };
}

add_task(get_test_function_for_localhost_with_hostname("localhost"));
add_task(get_test_function_for_localhost_with_hostname("localhost."));
add_task(get_test_function_for_localhost_with_hostname("localhost", true));

add_task(async function test_navigate_invalid_url() {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "mozilla is awesome",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});
