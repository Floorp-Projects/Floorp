"use strict";

const {Utils} = Cu.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

// Ensure the pref prevents API use when the extension has the tabHide permission.
add_task(async function test_pref_disabled() {
  async function background() {
    let tabs = await browser.tabs.query({hidden: false});
    let ids = tabs.map(tab => tab.id);

    await browser.test.assertRejects(
      browser.tabs.hide(ids),
      /tabs.hide is currently experimental/,
      "Got the expected error when pref not enabled"
    ).catch(err => {
      browser.test.notifyFail("pref-test");
      throw err;
    });

    browser.test.notifyPass("pref-test");
  }

  let extdata = {
    manifest: {permissions: ["tabs", "tabHide"]},
    background,
  };
  let extension = ExtensionTestUtils.loadExtension(extdata);
  await extension.startup();
  await extension.awaitFinish("pref-test");
  await extension.unload();
});

add_task(async function test_tabs_showhide() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.tabhide.enabled", true]],
  });

  async function background() {
    browser.test.onMessage.addListener(async (msg, data) => {
      switch (msg) {
        case "hideall": {
          let tabs = await browser.tabs.query({hidden: false});
          browser.test.assertEq(tabs.length, 5, "got 5 tabs");
          let ids = tabs.map(tab => tab.id);
          browser.test.log(`working with ids ${JSON.stringify(ids)}`);

          let hidden = await browser.tabs.hide(ids);
          browser.test.assertEq(hidden.length, 3, "hid 3 tabs");
          tabs = await browser.tabs.query({hidden: true});
          ids = tabs.map(tab => tab.id);
          browser.test.assertEq(JSON.stringify(hidden.sort()),
                                JSON.stringify(ids.sort()), "hidden tabIds match");

          browser.test.sendMessage("hidden", {hidden});
          break;
        }
        case "showall": {
          let tabs = await browser.tabs.query({hidden: true});
          for (let tab of tabs) {
            browser.test.assertTrue(tab.hidden, "tab is hidden");
          }
          let ids = tabs.map(tab => tab.id);
          browser.tabs.show(ids);
          browser.test.sendMessage("shown");
          break;
        }
      }
    });
  }

  let extdata = {
    manifest: {permissions: ["tabs", "tabHide"]},
    background,
  };
  let extension = ExtensionTestUtils.loadExtension(extdata);
  await extension.startup();

  let sessData = {
    windows: [{
      tabs: [
        {entries: [{url: "about:blank", triggeringPrincipal_base64}]},
        {entries: [{url: "https://example.com/", triggeringPrincipal_base64}]},
        {entries: [{url: "https://mochi.test:8888/", triggeringPrincipal_base64}]},
      ],
    }, {
      tabs: [
        {entries: [{url: "about:blank", triggeringPrincipal_base64}]},
        {entries: [{url: "http://test1.example.com/", triggeringPrincipal_base64}]},
      ],
    }],
  };

  // Set up a test session with 2 windows and 5 tabs.
  let oldState = SessionStore.getBrowserState();
  let restored = TestUtils.topicObserved("sessionstore-browser-state-restored");
  SessionStore.setBrowserState(JSON.stringify(sessData));
  await restored;

  // Attempt to hide all the tabs, however the active tab in each window cannot
  // be hidden, so the result will be 3 hidden tabs.
  extension.sendMessage("hideall");
  await extension.awaitMessage("hidden");

  // We have 2 windows in this session.  Otherwin is the non-current window.
  // In each window, the first tab will be the selected tab and should not be
  // hidden.  The rest of the tabs should be hidden at this point.  Hidden
  // status was already validated inside the extension, this double checks
  // from chrome code.
  let otherwin;
  for (let win of BrowserWindowIterator()) {
    if (win != window) {
      otherwin = win;
    }
    let tabs = Array.from(win.gBrowser.tabs.values());
    ok(!tabs[0].hidden, "first tab not hidden");
    for (let i = 1; i < tabs.length; i++) {
      ok(tabs[i].hidden, "tab hidden value is correct");
    }
  }

  // Test closing the last visible tab, the next tab which is hidden should become
  // the selectedTab and will be visible.
  ok(!otherwin.gBrowser.selectedTab.hidden, "selected tab is not hidden");
  await BrowserTestUtils.removeTab(otherwin.gBrowser.selectedTab);
  ok(!otherwin.gBrowser.selectedTab.hidden, "tab was unhidden");

  // Showall will unhide any remaining hidden tabs.
  extension.sendMessage("showall");
  await extension.awaitMessage("shown");

  // Check from chrome code that all tabs are visible again.
  for (let win of BrowserWindowIterator()) {
    let tabs = Array.from(win.gBrowser.tabs.values());
    for (let i = 0; i < tabs.length; i++) {
      ok(!tabs[i].hidden, "tab hidden value is correct");
    }
  }

  // Close second window.
  await BrowserTestUtils.closeWindow(otherwin);

  await extension.unload();

  // Restore pre-test state.
  restored = TestUtils.topicObserved("sessionstore-browser-state-restored");
  SessionStore.setBrowserState(oldState);
  await restored;
});

// Test our shutdown handling.  Currently this means any hidden tabs will be
// shown when a tabHide extension is shutdown.  We additionally test the
// tabs.onUpdated listener gets called with hidden state changes.
add_task(async function test_tabs_shutdown() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.tabhide.enabled", true]],
  });

  let tabs = [
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/", true, true),
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/", true, true),
  ];

  async function background() {
    let tabs = await browser.tabs.query({url: "http://example.com/"});
    let testTab = tabs[0];

    browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if ("hidden" in changeInfo) {
        browser.test.assertEq(tabId, testTab.id, "correct tab was hidden");
        browser.test.assertTrue(changeInfo.hidden, "tab is hidden");
        browser.test.sendMessage("changeInfo");
      }
    });

    let hidden = await browser.tabs.hide(testTab.id);
    browser.test.assertEq(hidden[0], testTab.id, "tab was hidden");
    tabs = await browser.tabs.query({hidden: true});
    browser.test.assertEq(tabs[0].id, testTab.id, "tab was hidden");
    browser.test.sendMessage("ready");
  }

  let extdata = {
    manifest: {permissions: ["tabs", "tabHide"]},
    useAddonManager: "temporary", // For testing onShutdown.
    background,
  };
  let extension = ExtensionTestUtils.loadExtension(extdata);
  await extension.startup();

  // test onUpdated
  await Promise.all([
    extension.awaitMessage("ready"),
    extension.awaitMessage("changeInfo"),
  ]);
  Assert.ok(tabs[0].hidden, "Tab is hidden by extension");

  await extension.unload();

  Assert.ok(!tabs[0].hidden, "Tab is not hidden after unloading extension");
  await BrowserTestUtils.removeTab(tabs[0]);
  await BrowserTestUtils.removeTab(tabs[1]);
});
