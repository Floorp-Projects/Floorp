/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function () {
  let testWindow = await BrowserTestUtils.openNewBrowserWindow();
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu early in the test.
  testWindow.gBrowser.selectedTab.focus();

  BrowserTestUtils.startLoadingURIString(
    testWindow.gBrowser,
    "data:text/html,<h1>A Page</h1>"
  );
  await BrowserTestUtils.browserLoaded(testWindow.gBrowser.selectedBrowser);

  await SimpleTest.promiseFocus(testWindow.gBrowser.selectedBrowser);

  ok(!testWindow.gFindBarInitialized, "find bar is not initialized");

  let findBarOpenPromise = BrowserTestUtils.waitForEvent(
    testWindow.gBrowser,
    "findbaropen"
  );
  EventUtils.synthesizeKey("/", {}, testWindow);
  await findBarOpenPromise;

  ok(testWindow.gFindBarInitialized, "find bar is now initialized");

  await BrowserTestUtils.closeWindow(testWindow);
});
