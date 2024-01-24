/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test ensures that we don't switch between tabs from normal window to
 * private browsing window or opposite.
 */

const TEST_URL = `${TEST_BASE_URL}dummy_page.html`;

add_task(async function () {
  let normalWindow = await BrowserTestUtils.openNewBrowserWindow();
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await runTest(normalWindow, privateWindow, false);
  await BrowserTestUtils.closeWindow(normalWindow);
  await BrowserTestUtils.closeWindow(privateWindow);

  normalWindow = await BrowserTestUtils.openNewBrowserWindow();
  privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await runTest(privateWindow, normalWindow, false);
  await BrowserTestUtils.closeWindow(normalWindow);
  await BrowserTestUtils.closeWindow(privateWindow);

  privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await runTest(privateWindow, privateWindow, true);
  await BrowserTestUtils.closeWindow(privateWindow);

  normalWindow = await BrowserTestUtils.openNewBrowserWindow();
  await runTest(normalWindow, normalWindow, true);
  await BrowserTestUtils.closeWindow(normalWindow);
});

async function runTest(aSourceWindow, aDestWindow, aExpectSwitch, aCallback) {
  await BrowserTestUtils.openNewForegroundTab(aSourceWindow.gBrowser, TEST_URL);
  let testTab = await BrowserTestUtils.openNewForegroundTab(
    aDestWindow.gBrowser
  );

  info("waiting for focus on the window");
  await SimpleTest.promiseFocus(aDestWindow);
  info("got focus on the window");

  // Select the testTab
  aDestWindow.gBrowser.selectedTab = testTab;

  // Ensure that this tab has no history entries
  let sessionHistoryCount = await new Promise(resolve => {
    SessionStore.getSessionHistory(
      gBrowser.selectedTab,
      function (sessionHistory) {
        resolve(sessionHistory.entries.length);
      }
    );
  });

  Assert.less(
    sessionHistoryCount,
    2,
    `The test tab has 1 or fewer history entries. sessionHistoryCount=${sessionHistoryCount}`
  );
  // Ensure that this tab is on about:blank
  is(
    testTab.linkedBrowser.currentURI.spec,
    "about:blank",
    "The test tab is on about:blank"
  );
  // Ensure that this tab's document has no child nodes
  await SpecialPowers.spawn(testTab.linkedBrowser, [], async function () {
    ok(
      !content.document.body.hasChildNodes(),
      "The test tab has no child nodes"
    );
  });
  ok(
    !testTab.hasAttribute("busy"),
    "The test tab doesn't have the busy attribute"
  );

  // Wait for the Awesomebar popup to appear.
  let searchString = TEST_URL;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: aDestWindow,
    value: searchString,
  });

  info(`awesomebar popup appeared. aExpectSwitch: ${aExpectSwitch}`);
  // Make sure the last match is selected.
  while (
    UrlbarTestUtils.getSelectedRowIndex(aDestWindow) <
    UrlbarTestUtils.getResultCount(aDestWindow) - 1
  ) {
    info("handling key navigation for DOM_VK_DOWN key");
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, aDestWindow);
  }

  let awaitTabSwitch;
  if (aExpectSwitch) {
    awaitTabSwitch = BrowserTestUtils.waitForTabClosing(testTab);
  }

  // Execute the selected action.
  EventUtils.synthesizeKey("KEY_Enter", {}, aDestWindow);
  info("sent Enter command to the controller");

  if (aExpectSwitch) {
    // If we expect a tab switch then the current tab
    // will be closed and we switch to the other tab.
    await awaitTabSwitch;
  } else {
    // If we don't expect a tab switch then wait for the tab to load.
    await BrowserTestUtils.browserLoaded(testTab.linkedBrowser);
  }
}
