"use strict";

ChromeUtils.defineModuleGetter(this, "SessionStore",
                               "resource:///modules/sessionstore/SessionStore.jsm");
ChromeUtils.defineModuleGetter(this, "TabStateFlusher",
                               "resource:///modules/sessionstore/TabStateFlusher.jsm");
const {Utils} = ChromeUtils.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

async function doorhangerTest(testFn) {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.tabhide.enabled", true]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "tabHide"],
      icons: {
        48: "addon-icon.png",
      },
    },
    background() {
      browser.test.onMessage.addListener(async (msg, data) => {
        let tabs = await browser.tabs.query(data);
        await browser.tabs[msg](tabs.map(t => t.id));
        browser.test.sendMessage("done");
      });
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  // Open some tabs so we can hide them.
  let firstTab = gBrowser.selectedTab;
  let tabs = [
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/?one", true, true),
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/?two", true, true),
  ];
  gBrowser.selectedTab = firstTab;

  await testFn(extension);

  BrowserTestUtils.removeTab(tabs[0]);
  BrowserTestUtils.removeTab(tabs[1]);

  await extension.unload();
}

add_task(function test_doorhanger_keep() {
  return doorhangerTest(async function(extension) {
    is(gBrowser.visibleTabs.length, 3, "There are 3 visible tabs");

    // Hide the first tab, expect the doorhanger.
    let panel = document.getElementById("extension-notification-panel");
    let popupShown = promisePopupShown(panel);
    extension.sendMessage("hide", {url: "*://*/?one"});
    await extension.awaitMessage("done");
    await popupShown;

    is(gBrowser.visibleTabs.length, 2, "There are 2 visible tabs now");
    is(panel.anchorNode.closest("toolbarbutton").id,
       "alltabs-button", "The doorhanger is anchored to the all tabs button");

    // Click the Keep Tabs Hidden button.
    let popupnotification = document.getElementById("extension-tab-hide-notification");
    let popupHidden = promisePopupHidden(panel);
    document.getAnonymousElementByAttribute(
      popupnotification, "anonid", "button").click();
    await popupHidden;

    // Hide another tab and ensure the popup didn't open.
    extension.sendMessage("hide", {url: "*://*/?two"});
    await extension.awaitMessage("done");
    is(panel.state, "closed", "The popup is still closed");
    is(gBrowser.visibleTabs.length, 1, "There's one visible tab now");

    extension.sendMessage("show", {});
    await extension.awaitMessage("done");
  });
});

add_task(function test_doorhanger_disable() {
  return doorhangerTest(async function(extension) {
    is(gBrowser.visibleTabs.length, 3, "There are 3 visible tabs");

    // Hide the first tab, expect the doorhanger.
    let panel = document.getElementById("extension-notification-panel");
    let popupShown = promisePopupShown(panel);
    extension.sendMessage("hide", {url: "*://*/?one"});
    await extension.awaitMessage("done");
    await popupShown;

    is(gBrowser.visibleTabs.length, 2, "There are 2 visible tabs now");
    is(panel.anchorNode.closest("toolbarbutton").id,
       "alltabs-button", "The doorhanger is anchored to the all tabs button");

    // verify the contents of the description.
    let popupnotification = document.getElementById("extension-tab-hide-notification");
    let description = popupnotification.querySelector("description");
    let addon = await AddonManager.getAddonByID(extension.id);
    ok(description.textContent.includes(addon.name),
       "The extension name is in the description");
    let images = Array.from(description.querySelectorAll("image"));
    is(images.length, 2, "There are two images");
    ok(images.some(img => img.src.includes("addon-icon.png")),
       "There's an icon for the extension");
    ok(images.some(img => getComputedStyle(img).backgroundImage.includes("arrow-dropdown-16.svg")),
       "There's an icon for the all tabs menu");

    // Click the Disable Extension button.
    let popupHidden = promisePopupHidden(panel);
    document.getAnonymousElementByAttribute(
      popupnotification, "anonid", "secondarybutton").click();
    await popupHidden;

    is(gBrowser.visibleTabs.length, 3, "There are 3 visible tabs again");
    is(addon.userDisabled, true, "The extension is now disabled");
  });
});

add_task(async function test_tabs_showhide() {
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
    useAddonManager: "temporary", // So the doorhanger can find the addon.
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

  for (let win of BrowserWindowIterator()) {
    let allTabsButton = win.document.getElementById("alltabs-button");
    is(getComputedStyle(allTabsButton).visibility, "collapse", "The all tabs button is hidden");
  }

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
      let id = SessionStore.getCustomTabValue(tabs[i], "hiddenBy");
      is(id, extension.id, "tab hiddenBy value is correct");
      await TabStateFlusher.flush(tabs[i].linkedBrowser);
    }

    let allTabsButton = win.document.getElementById("alltabs-button");
    is(getComputedStyle(allTabsButton).visibility, "visible", "The all tabs button is visible");
  }

  // Close the other window then restore it to test that the tabs are
  // restored with proper hidden state, and the correct extension id.
  await BrowserTestUtils.closeWindow(otherwin);

  otherwin = SessionStore.undoCloseWindow(0);
  await BrowserTestUtils.waitForEvent(otherwin, "load");
  let tabs = Array.from(otherwin.gBrowser.tabs.values());
  ok(!tabs[0].hidden, "first tab not hidden");
  for (let i = 1; i < tabs.length; i++) {
    ok(tabs[i].hidden, "tab hidden value is correct");
    let id = SessionStore.getCustomTabValue(tabs[i], "hiddenBy");
    is(id, extension.id, "tab hiddenBy value is correct");
  }

  // Test closing the last visible tab, the next tab which is hidden should become
  // the selectedTab and will be visible.
  ok(!otherwin.gBrowser.selectedTab.hidden, "selected tab is not hidden");
  BrowserTestUtils.removeTab(otherwin.gBrowser.selectedTab);
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
  BrowserTestUtils.removeTab(tabs[0]);
  BrowserTestUtils.removeTab(tabs[1]);
});

// Ensure the pref prevents API use when the extension has the tabHide permission.
add_task(async function test_pref_disabled() {
  // This should run last since SpecialPowers.pushPrefEnv won't cleanup until
  // this file finishes executing.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.tabhide.enabled", false]],
  });

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
