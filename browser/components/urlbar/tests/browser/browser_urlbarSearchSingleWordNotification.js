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
      resolve(
        BrowserTestUtils.waitForNotificationInNotificationBox(
          notificationBox,
          value
        )
      );
    } else {
      setTimeout(() => {
        is(
          notificationBox.getNotificationWithValue(value),
          null,
          `We are expecting to not get a notification for ${input}`
        );
        resolve();
      }, 1000);
    }
  });
}

async function runURLBarSearchTest({
  valueToOpen,
  expectSearch,
  expectNotification,
  aWindow = window,
}) {
  // Test both directly setting a value and pressing enter, or setting the
  // value through input events, like the user would do.
  const setValueFns = [
    value => {
      aWindow.gURLBar.value = value;
    },
    value => {
      return UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        waitForFocus,
        value,
      });
    },
  ];

  for (let i = 0; i < setValueFns.length; ++i) {
    await setValueFns[i](valueToOpen);
    let expectedURI;
    if (!expectSearch) {
      expectedURI = "http://" + valueToOpen + "/";
    } else {
      expectedURI = (await Services.search.getDefault()).getSubmission(
        valueToOpen,
        null,
        "keyword"
      ).uri.spec;
    }
    aWindow.gURLBar.focus();
    let docLoadPromise = BrowserTestUtils.waitForDocLoadAndStopIt(
      expectedURI,
      aWindow.gBrowser.selectedBrowser
    );
    EventUtils.synthesizeKey("VK_RETURN", {}, aWindow);

    await Promise.all([
      docLoadPromise,
      promiseNotification(
        aWindow.gBrowser,
        "keyword-uri-fixup",
        expectNotification,
        valueToOpen
      ),
    ]);

    if (expectNotification) {
      let notificationBox = aWindow.gBrowser.getNotificationBox(
        aWindow.gBrowser.selectedBrowser
      );
      let notification = notificationBox.getNotificationWithValue(
        "keyword-uri-fixup"
      );
      // Confirm the notification only on the last loop.
      if (i == setValueFns.length - 1) {
        docLoadPromise = BrowserTestUtils.waitForDocLoadAndStopIt(
          "http://" + valueToOpen + "/",
          aWindow.gBrowser.selectedBrowser
        );
        notification.querySelector("button").click();
        await docLoadPromise;
      } else {
        notificationBox.currentNotification.close();
      }
    }
  }
}

add_task(async function test_navigate_full_domain() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:blank"
  ));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "www.singlewordtest.org",
    expectSearch: false,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_decimal_ip() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:blank"
  ));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "1234",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_decimal_ip_with_path() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:blank"
  ));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "1234/12",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_large_number() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:blank"
  ));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "123456789012345",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_small_hex_number() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:blank"
  ));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "0x1f00ffff",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_navigate_large_hex_number() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:blank"
  ));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "0x7f0000017f000001",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});

function get_test_function_for_localhost_with_hostname(hostName, isPrivate) {
  return async function test_navigate_single_host() {
    const pref = "browser.fixup.domainwhitelist.localhost";
    let win;
    if (isPrivate) {
      let promiseWin = BrowserTestUtils.waitForNewWindow();
      win = OpenBrowserWindow({ private: true });
      await promiseWin;
      await new Promise(resolve => {
        waitForFocus(resolve, win);
      });
    } else {
      win = window;
    }

    // Remove the domain from the whitelist.
    Services.prefs.setBoolPref(pref, false);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser: win.gBrowser,
        url: "about:blank",
      },
      browser =>
        runURLBarSearchTest({
          valueToOpen: hostName,
          expectSearch: true,
          expectNotification: true,
          aWindow: win,
        })
    );

    // check pref value
    let prefValue = Services.prefs.getBoolPref(pref);
    is(prefValue, !isPrivate, "Pref should have the correct state.");

    // Now try again with the pref set.
    // In a private window, the notification should appear again.
    await BrowserTestUtils.withNewTab(
      {
        gBrowser: win.gBrowser,
        url: "about:blank",
      },
      browser =>
        runURLBarSearchTest({
          valueToOpen: hostName,
          expectSearch: isPrivate,
          expectNotification: isPrivate,
          aWindow: win,
        })
    );

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
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:blank"
  ));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    valueToOpen: "mozilla is awesome",
    expectSearch: true,
    expectNotification: false,
  });
  gBrowser.removeTab(tab);
});
