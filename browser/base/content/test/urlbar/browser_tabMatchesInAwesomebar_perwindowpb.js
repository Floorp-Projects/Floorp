let testURL = "http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html";

add_task(function*() {
  let normalWindow = yield BrowserTestUtils.openNewBrowserWindow();
  let privateWindow = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  yield runTest(normalWindow, privateWindow, false);
  yield BrowserTestUtils.closeWindow(normalWindow);
  yield BrowserTestUtils.closeWindow(privateWindow);

  normalWindow = yield BrowserTestUtils.openNewBrowserWindow();
  privateWindow = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  yield runTest(privateWindow, normalWindow, false);
  yield BrowserTestUtils.closeWindow(normalWindow);
  yield BrowserTestUtils.closeWindow(privateWindow);

  privateWindow = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  yield runTest(privateWindow, privateWindow, false);
  yield BrowserTestUtils.closeWindow(privateWindow);

  normalWindow = yield BrowserTestUtils.openNewBrowserWindow();
  yield runTest(normalWindow, normalWindow, true);
  yield BrowserTestUtils.closeWindow(normalWindow);
});

function* runTest(aSourceWindow, aDestWindow, aExpectSwitch, aCallback) {
  let baseTab = yield BrowserTestUtils.openNewForegroundTab(aSourceWindow.gBrowser, testURL);
  let testTab = yield BrowserTestUtils.openNewForegroundTab(aDestWindow.gBrowser);

  info("waiting for focus on the window");
  yield SimpleTest.promiseFocus(aDestWindow);
  info("got focus on the window");

  // Select the testTab
  aDestWindow.gBrowser.selectedTab = testTab;

  // Ensure that this tab has no history entries
  let sessionHistoryCount = yield new Promise(resolve => {
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
  yield ContentTask.spawn(testTab.linkedBrowser, null, function*() {
    ok(!content.document.body.hasChildNodes(),
       "The test tab has no child nodes");
  });
  ok(!testTab.hasAttribute("busy"),
     "The test tab doesn't have the busy attribute");

  // Wait for the Awesomebar popup to appear.
  yield promiseAutocompleteResultPopup(testURL, aDestWindow);

  info(`awesomebar popup appeared. aExpectSwitch: ${aExpectSwitch}`);
  // Make sure the last match is selected.
  let {controller, popup} = aDestWindow.gURLBar;
  while (popup.selectedIndex < controller.matchCount - 1) {
    info("handling key navigation for DOM_VK_DOWN key");
    controller.handleKeyNavigation(KeyEvent.DOM_VK_DOWN);
  }

  let awaitTabSwitch;
  if (aExpectSwitch) {
    awaitTabSwitch = BrowserTestUtils.removeTab(testTab, {dontRemove: true})
  }

  // Execute the selected action.
  controller.handleEnter(true);
  info("sent Enter command to the controller");

  if (aExpectSwitch) {
    // If we expect a tab switch then the current tab
    // will be closed and we switch to the other tab.
    yield awaitTabSwitch;
  } else {
    // If we don't expect a tab switch then wait for the tab to load.
    yield BrowserTestUtils.browserLoaded(testTab.linkedBrowser);
  }
}
