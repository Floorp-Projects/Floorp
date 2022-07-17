/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

let gDNSResolved = false;
let gRealDNSService = gDNSService;
add_setup(async function() {
  gDNSService = {
    asyncResolve() {
      gDNSResolved = true;
      return gRealDNSService.asyncResolve(...arguments);
    },
  };
  registerCleanupFunction(function() {
    gDNSService = gRealDNSService;
    Services.prefs.clearUserPref("browser.fixup.domainwhitelist.localhost");
  });
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
  enterSearchMode,
  expectSearch,
  expectNotification,
  expectDNSResolve,
  aWindow = window,
}) {
  gDNSResolved = false;
  // Test both directly setting a value and pressing enter, or setting the
  // value through input events, like the user would do.
  const setValueFns = [
    value => {
      aWindow.gURLBar.value = value;
      if (enterSearchMode) {
        // Ensure to open the panel.
        UrlbarTestUtils.fireInputEvent(aWindow);
      }
    },
    value => {
      return UrlbarTestUtils.promiseAutocompleteResultPopup({
        window: aWindow,
        value,
      });
    },
  ];

  for (let i = 0; i < setValueFns.length; ++i) {
    await setValueFns[i](valueToOpen);
    if (enterSearchMode) {
      if (!expectSearch) {
        throw new Error("Must execute a search in search mode");
      }
      await UrlbarTestUtils.enterSearchMode(aWindow);
    }

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

    if (!enterSearchMode) {
      await promiseNotification(
        aWindow.gBrowser,
        "keyword-uri-fixup",
        expectNotification,
        valueToOpen
      );
    }
    await docLoadPromise;

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
        notification.buttonContainer.querySelector("button").click();
        await docLoadPromise;
      } else {
        notificationBox.currentNotification.close();
      }
    }
    Assert.equal(
      gDNSResolved,
      expectDNSResolve,
      `Should${expectDNSResolve ? "" : " not"} DNS resolve "${valueToOpen}"`
    );
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
    expectDNSResolve: false,
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
    expectDNSResolve: false, // Possible IP in numeric format.
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
    expectDNSResolve: false,
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
    expectDNSResolve: false, // Possible IP in numeric format.
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
    expectDNSResolve: false, // Possible IP in numeric format.
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
    expectDNSResolve: false, // Possible IP in numeric format.
  });
  gBrowser.removeTab(tab);
});

function get_test_function_for_localhost_with_hostname(
  hostName,
  isPrivate = false
) {
  return async function test_navigate_single_host() {
    info(`Test ${hostName}${isPrivate ? " in Private Browsing mode" : ""}`);
    const pref = "browser.fixup.domainwhitelist.localhost";
    let win;
    if (isPrivate) {
      let promiseWin = BrowserTestUtils.waitForNewWindow();
      win = OpenBrowserWindow({ private: true });
      await promiseWin;
      await SimpleTest.promiseFocus(win);
      // We can do this since the window will be gone shortly.
      delete win.gDNSService;
      win.gDNSService = {
        asyncResolve() {
          gDNSResolved = true;
          return gRealDNSService.asyncResolve(...arguments);
        },
      };
    } else {
      win = window;
    }

    // Remove the domain from the whitelist
    Services.prefs.setBoolPref(pref, false);

    // The notification should not appear because the default value of
    // browser.urlbar.dnsResolveSingleWordsAfterSearch is 0
    await BrowserTestUtils.withNewTab(
      {
        gBrowser: win.gBrowser,
        url: "about:blank",
      },
      browser =>
        runURLBarSearchTest({
          valueToOpen: hostName,
          expectSearch: true,
          expectNotification: false,
          expectDNSResolve: false,
          aWindow: win,
        })
    );

    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.dnsResolveSingleWordsAfterSearch", 1]],
    });

    // The notification should appear, unless we are in private browsing mode.
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
          expectDNSResolve: true,
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
          expectDNSResolve: isPrivate,
          aWindow: win,
        })
    );

    if (isPrivate) {
      info("Waiting for private window to close");
      await BrowserTestUtils.closeWindow(win);
      await SimpleTest.promiseFocus(window);
    }

    await SpecialPowers.popPrefEnv();
  };
}

add_task(get_test_function_for_localhost_with_hostname("localhost"));
add_task(get_test_function_for_localhost_with_hostname("localhost."));
add_task(get_test_function_for_localhost_with_hostname("localhost", true));

add_task(async function test_dnsResolveSingleWordsAfterSearch() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.dnsResolveSingleWordsAfterSearch", 0],
      ["browser.fixup.domainwhitelist.localhost", false],
    ],
  });
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    browser =>
      runURLBarSearchTest({
        valueToOpen: "localhost",
        expectSearch: true,
        expectNotification: false,
        expectDNSResolve: false,
      })
  );
  await SpecialPowers.popPrefEnv();
});

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
    expectDNSResolve: false,
  });
  gBrowser.removeTab(tab);
});

add_task(async function test_search_mode() {
  info("When in search mode we should never query the DNS");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.suggest.enabled", false]],
  });
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "about:blank"
  ));
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await runURLBarSearchTest({
    enterSearchMode: true,
    valueToOpen: "mozilla",
    expectSearch: true,
    expectNotification: false,
    expectDNSResolve: false,
  });
  gBrowser.removeTab(tab);
});
