/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

// This test makes sure that the title of existing history entries does not
// change inside a private window.

add_task(async function test() {
  const TEST_URL =
    "http://mochi.test:8888/browser/browser/components/" +
    "privatebrowsing/test/browser/title.sjs";
  let cm = Services.cookies;

  function cleanup() {
    // delete all cookies
    cm.removeAll();
    // delete all history items
    return PlacesUtils.history.clear();
  }

  await cleanup();
  registerCleanupFunction(cleanup);

  let win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
  });

  let promiseTitleChanged = PlacesTestUtils.waitForNotification(
    "page-title-changed",
    events => events[0].url == TEST_URL
  );
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, TEST_URL);
  await promiseTitleChanged;
  await BrowserTestUtils.waitForCondition(async function () {
    let entry = await PlacesUtils.history.fetch(TEST_URL);
    return entry && entry.title == "No Cookie";
  }, "The page should be loaded without any cookie for the first time");

  promiseTitleChanged = PlacesTestUtils.waitForNotification(
    "page-title-changed",
    events => events[0].url == TEST_URL
  );
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, TEST_URL);
  await promiseTitleChanged;
  await BrowserTestUtils.waitForCondition(async function () {
    let entry = await PlacesUtils.history.fetch(TEST_URL);
    return entry && entry.title == "Cookie";
  }, "The page should be loaded with a cookie for the second time");

  await cleanup();

  promiseTitleChanged = PlacesTestUtils.waitForNotification(
    "page-title-changed",
    events => events[0].url == TEST_URL
  );
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, TEST_URL);
  await promiseTitleChanged;
  await BrowserTestUtils.waitForCondition(async function () {
    let entry = await PlacesUtils.history.fetch(TEST_URL);
    return entry && entry.title == "No Cookie";
  }, "The page should be loaded without any cookie again");

  // Reopen the page in a private browser window, it should not notify a title
  // change.
  let win2 = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  registerCleanupFunction(async () => {
    let promisePBExit = TestUtils.topicObserved("last-pb-context-exited");
    await BrowserTestUtils.closeWindow(win2);
    await promisePBExit;
  });

  await BrowserTestUtils.openNewForegroundTab(win2.gBrowser, TEST_URL);
  // Wait long enough to be sure history didn't set a title.
  await new Promise(resolve => setTimeout(resolve, 1000));
  is(
    (await PlacesUtils.history.fetch(TEST_URL)).title,
    "No Cookie",
    "The title remains the same after visiting in private window"
  );
});
