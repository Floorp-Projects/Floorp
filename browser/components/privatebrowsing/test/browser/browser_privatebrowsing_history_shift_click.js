/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function () {
  await testShiftClickOpensNewWindow("back-button");
});

add_task(async function () {
  await testShiftClickOpensNewWindow("forward-button");
});

// Create new private browser, open new tab and set history state, then return the window
async function createPrivateWindow() {
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await BrowserTestUtils.openNewForegroundTab(
    privateWindow.gBrowser,
    "http://example.com"
  );
  await SpecialPowers.spawn(
    privateWindow.gBrowser.selectedBrowser,
    [],
    async function () {
      content.history.pushState({}, "first item", "first-item.html");
      content.history.pushState({}, "second item", "second-item.html");
      content.history.pushState({}, "third item", "third-item.html");
      content.history.back();
    }
  );
  await TestUtils.topicObserved("sessionstore-state-write-complete");

  // Wait for the session data to be flushed before continuing the test
  await new Promise(resolve =>
    SessionStore.getSessionHistory(privateWindow.gBrowser.selectedTab, resolve)
  );

  info("Private window created");

  return privateWindow;
}

async function testShiftClickOpensNewWindow(buttonId) {
  const privateWindow = await createPrivateWindow();

  const button = privateWindow.document.getElementById(buttonId);
  // Wait for the new private window to be created after click
  const newPrivateWindowPromise = BrowserTestUtils.waitForNewWindow();

  EventUtils.synthesizeMouseAtCenter(button, { shiftKey: true }, privateWindow);

  info("Waiting for new private browser to open");

  const newPrivateWindow = await newPrivateWindowPromise;

  ok(
    PrivateBrowsingUtils.isBrowserPrivate(newPrivateWindow.gBrowser),
    "New window is private"
  );

  // Cleanup
  await Promise.all([
    BrowserTestUtils.closeWindow(privateWindow),
    BrowserTestUtils.closeWindow(newPrivateWindow),
  ]);

  info("Closed all windows");
}
