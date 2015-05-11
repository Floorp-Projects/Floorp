/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function *() {
  let testWindow = yield BrowserTestUtils.openNewBrowserWindow();

  testWindow.gBrowser.loadURI("data:text/html,<h1>A Page</h1>");
  yield BrowserTestUtils.browserLoaded(testWindow.gBrowser.selectedBrowser);

  yield SimpleTest.promiseFocus(testWindow.gBrowser.selectedBrowser);

  ok(!testWindow.gFindBarInitialized, "find bar is not initialized");

  let findBarOpenPromise = promiseWaitForEvent(testWindow.gBrowser, "findbaropen");
  EventUtils.synthesizeKey("/", {}, testWindow);
  yield findBarOpenPromise;

  ok(testWindow.gFindBarInitialized, "find bar is now initialized");

  yield BrowserTestUtils.closeWindow(testWindow);
});
