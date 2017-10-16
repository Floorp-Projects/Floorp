/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

// Test to make sure that the visited page titles do not get updated inside the
// private browsing mode.
"use strict";

add_task(async function test() {
  const TEST_URL = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/browser_privatebrowsing_placesTitleNoUpdate.html";
  const TITLE_1 = "Title 1";
  const TITLE_2 = "Title 2";

  await PlacesUtils.history.clear();

  let promiseTitleChanged = PlacesTestUtils.waitForNotification(
    "onTitleChanged", (uri, title) => uri.spec == TEST_URL, "history");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  registerCleanupFunction(async () => {
    await BrowserTestUtils.removeTab(tab);
  });
  info("Wait for a title change notification.");
  await promiseTitleChanged;
  await BrowserTestUtils.waitForCondition(async function() {
    let entry = await PlacesUtils.history.fetch(TEST_URL);
    return entry && entry.title == TITLE_1;
  }, "The title matches the original title after first visit");

  promiseTitleChanged = PlacesTestUtils.waitForNotification(
    "onTitleChanged", (uri, title) => uri.spec == TEST_URL, "history");
  await PlacesTestUtils.addVisits({ uri: TEST_URL, title: TITLE_2 });
  info("Wait for a title change notification.");
  await promiseTitleChanged;
  await BrowserTestUtils.waitForCondition(async function() {
    let entry = await PlacesUtils.history.fetch(TEST_URL);
    return entry && entry.title == TITLE_2;
  }, "The title matches the original title after updating visit");

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(privateWin);
  });
  await BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser, TEST_URL);
  // Wait long enough to be sure history didn't set a title.
  await new Promise(resolve => setTimeout(resolve, 1000));
  is((await PlacesUtils.history.fetch(TEST_URL)).title, TITLE_2,
     "The title remains the same after visiting in private window");
});
