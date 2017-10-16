let testURL = "http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html";

add_task(async function() {
  let normalWindow = await BrowserTestUtils.openNewBrowserWindow();
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  await runTest(normalWindow, privateWindow, false);
  await BrowserTestUtils.closeWindow(normalWindow);
  await BrowserTestUtils.closeWindow(privateWindow);

  normalWindow = await BrowserTestUtils.openNewBrowserWindow();
  privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  await runTest(privateWindow, normalWindow, false);
  await BrowserTestUtils.closeWindow(normalWindow);
  await BrowserTestUtils.closeWindow(privateWindow);

  privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  await runTest(privateWindow, privateWindow, false);
  await BrowserTestUtils.closeWindow(privateWindow);

  normalWindow = await BrowserTestUtils.openNewBrowserWindow();
  await runTest(normalWindow, normalWindow, true);
  await BrowserTestUtils.closeWindow(normalWindow);
});

async function runTest(aSourceWindow, aDestWindow, aExpectSwitch, aCallback) {
  await BrowserTestUtils.openNewForegroundTab(aSourceWindow.gBrowser, testURL);
  let testTab = await BrowserTestUtils.openNewForegroundTab(aDestWindow.gBrowser);

  info("waiting for focus on the window");
  await SimpleTest.promiseFocus(aDestWindow);
  info("got focus on the window");

  // Select the testTab
  aDestWindow.gBrowser.selectedTab = testTab;

  // Ensure that this tab has no history entries
  let sessionHistoryCount = await new Promise(resolve => {
    SessionStore.getSessionHistory(gBrowser.selectedTab, function(sessionHistory) {
      resolve(sessionHistory.entries.length);
    });
  });

  ok(sessionHistoryCount < 2,
     `The test tab has 1 or fewer history entries. sessionHistoryCount=${sessionHistoryCount}`);
  // Ensure that this tab is on about:blank
  is(testTab.linkedBrowser.currentURI.spec, "about:blank",
     "The test tab is on about:blank");
  // Ensure that this tab's document has no child nodes
  await ContentTask.spawn(testTab.linkedBrowser, null, async function() {
    ok(!content.document.body.hasChildNodes(),
       "The test tab has no child nodes");
  });
  ok(!testTab.hasAttribute("busy"),
     "The test tab doesn't have the busy attribute");

  // Wait for the Awesomebar popup to appear.
  await promiseAutocompleteResultPopup(testURL, aDestWindow);

  info(`awesomebar popup appeared. aExpectSwitch: ${aExpectSwitch}`);
  // Make sure the last match is selected.
  let {controller, popup} = aDestWindow.gURLBar;
  while (popup.selectedIndex < controller.matchCount - 1) {
    info("handling key navigation for DOM_VK_DOWN key");
    controller.handleKeyNavigation(KeyEvent.DOM_VK_DOWN);
  }

  let awaitTabSwitch;
  if (aExpectSwitch) {
    awaitTabSwitch = BrowserTestUtils.tabRemoved(testTab);
  }

  // Execute the selected action.
  controller.handleEnter(true);
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
