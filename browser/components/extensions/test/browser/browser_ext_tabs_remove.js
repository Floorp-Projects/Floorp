/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function undoCloseAfterExtRemovesOneTab() {
  let initialTab = gBrowser.selectedTab;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: async function () {
      let tabs = await browser.tabs.query({});

      browser.test.assertEq(3, tabs.length, "Should have 3 tabs");

      let tabIdsToRemove = (
        await browser.tabs.query({
          url: "https://example.com/closeme/*",
        })
      ).map(tab => tab.id);

      await browser.tabs.remove(tabIdsToRemove);
      browser.test.sendMessage("removedtabs");
    },
  });

  await Promise.all([
    BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/1"),
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "https://example.com/closeme/2"
    ),
  ]);

  await extension.startup();
  await extension.awaitMessage("removedtabs");

  is(
    gBrowser.tabs.length,
    2,
    "Once extension has closed a tab, there should be 2 tabs open"
  );

  // The tabs.remove API makes no promises about SessionStore's updates
  // having been completed by the time it returns. So we need to wait separately
  // for the closed tab count to be updated the correct value. This is OK because
  // we can observe above that the tabs length has changed to reflect that
  // some were closed.
  await TestUtils.waitForCondition(
    () => SessionStore.getLastClosedTabCount(window) == 1,
    "SessionStore should know that one tab was closed"
  );

  undoCloseTab();

  is(
    gBrowser.tabs.length,
    3,
    "All tabs should be restored for a total of 3 tabs"
  );

  await BrowserTestUtils.waitForEvent(gBrowser.tabs[2], "SSTabRestored");

  is(
    gBrowser.tabs[2].linkedBrowser.currentURI.spec,
    "https://example.com/closeme/2",
    "Restored tab at index 2 should have expected URL"
  );

  await extension.unload();
  gBrowser.removeAllTabsBut(initialTab);
});

add_task(async function undoCloseAfterExtRemovesMultipleTabs() {
  let initialTab = gBrowser.selectedTab;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: async function () {
      let tabIds = (await browser.tabs.query({})).map(tab => tab.id);

      browser.test.assertEq(
        8,
        tabIds.length,
        "Should have 8 total tabs (4 in each window: the initial blank tab and the 3 opened by this test)"
      );

      let tabIdsToRemove = (
        await browser.tabs.query({
          url: "https://example.com/closeme/*",
        })
      ).map(tab => tab.id);

      await browser.tabs.remove(tabIdsToRemove);

      browser.test.sendMessage("removedtabs");
    },
  });

  await Promise.all([
    BrowserTestUtils.openNewForegroundTab(gBrowser, "https://example.com/1"),
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "https://example.com/closeme/2"
    ),
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "https://example.com/closeme/3"
    ),
  ]);

  let window2 = await BrowserTestUtils.openNewBrowserWindow();

  await Promise.all([
    BrowserTestUtils.openNewForegroundTab(
      window2.gBrowser,
      "https://example.com/4"
    ),
    BrowserTestUtils.openNewForegroundTab(
      window2.gBrowser,
      "https://example.com/closeme/5"
    ),
    BrowserTestUtils.openNewForegroundTab(
      window2.gBrowser,
      "https://example.com/closeme/6"
    ),
  ]);

  await extension.startup();
  await extension.awaitMessage("removedtabs");

  is(
    gBrowser.tabs.length,
    2,
    "Original window should have 2 tabs still open, after closing tabs"
  );

  is(
    window2.gBrowser.tabs.length,
    2,
    "Second window should have 2 tabs still open, after closing tabs"
  );

  // The tabs.remove API makes no promises about SessionStore's updates
  // having been completed by the time it returns. So we need to wait separately
  // for the closed tab count to be updated the correct value. This is OK because
  // we can observe above that the tabs length has changed to reflect that
  // some were closed.
  await TestUtils.waitForCondition(
    () => SessionStore.getLastClosedTabCount(window) == 2,
    "Last closed tab count is 2"
  );

  await TestUtils.waitForCondition(
    () => SessionStore.getLastClosedTabCount(window2) == 2,
    "Last closed tab count is 2"
  );

  undoCloseTab();
  window2.undoCloseTab();

  is(
    gBrowser.tabs.length,
    4,
    "All tabs in original window should be restored for a total of 4 tabs"
  );

  is(
    window2.gBrowser.tabs.length,
    4,
    "All tabs in second window should be restored for a total of 4 tabs"
  );

  await Promise.all([
    BrowserTestUtils.waitForEvent(gBrowser.tabs[2], "SSTabRestored"),
    BrowserTestUtils.waitForEvent(gBrowser.tabs[3], "SSTabRestored"),
    BrowserTestUtils.waitForEvent(window2.gBrowser.tabs[2], "SSTabRestored"),
    BrowserTestUtils.waitForEvent(window2.gBrowser.tabs[3], "SSTabRestored"),
  ]);

  is(
    gBrowser.tabs[2].linkedBrowser.currentURI.spec,
    "https://example.com/closeme/2",
    "Original window restored tab at index 2 should have expected URL"
  );

  is(
    gBrowser.tabs[3].linkedBrowser.currentURI.spec,
    "https://example.com/closeme/3",
    "Original window restored tab at index 3 should have expected URL"
  );

  is(
    window2.gBrowser.tabs[2].linkedBrowser.currentURI.spec,
    "https://example.com/closeme/5",
    "Second window restored tab at index 2 should have expected URL"
  );

  is(
    window2.gBrowser.tabs[3].linkedBrowser.currentURI.spec,
    "https://example.com/closeme/6",
    "Second window restored tab at index 3 should have expected URL"
  );

  await extension.unload();
  await BrowserTestUtils.closeWindow(window2);
  gBrowser.removeAllTabsBut(initialTab);
});

add_task(async function closeWindowIfExtClosesAllTabs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.closeWindowWithLastTab", true],
      ["browser.tabs.warnOnClose", true],
    ],
  });

  let extension = ExtensionTestUtils.loadExtension({
    background: async function () {
      let tabsToRemove = await browser.tabs.query({ currentWindow: true });

      let currentWindowId = tabsToRemove[0].windowId;

      browser.test.assertEq(
        2,
        tabsToRemove.length,
        "Current window should have 2 tabs to remove"
      );

      await browser.tabs.remove(tabsToRemove.map(tab => tab.id));

      await browser.test.assertRejects(
        browser.windows.get(currentWindowId),
        RegExp(`Invalid window ID: ${currentWindowId}`),
        "After closing tabs, 2nd window should be closed and querying for it should be rejected"
      );

      browser.test.notifyPass("done");
    },
  });

  let window2 = await BrowserTestUtils.openNewBrowserWindow();

  await BrowserTestUtils.openNewForegroundTab(
    window2.gBrowser,
    "https://example.com/"
  );

  await extension.startup();
  await extension.awaitFinish("done");
  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
