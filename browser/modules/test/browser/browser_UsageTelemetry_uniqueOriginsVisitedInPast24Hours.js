/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(this, "URICountListener",
                               "resource:///modules/BrowserUsageTelemetry.jsm");

add_task(async function test_uniqueOriginsVisitedInPast24Hours() {
  registerCleanupFunction(async () => {
    info("Cleaning up");
    URICountListener.resetUniqueOriginsVisitedInPast24Hours();
  });

  URICountListener.resetUniqueOriginsVisitedInPast24Hours();
  let startingCount = URICountListener.uniqueOriginsVisitedInPast24Hours;
  is(startingCount, 0, "We should have no origins recorded in the history right after resetting");

  // Add a new window and then some tabs in it.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "http://example.com");

  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "http://test1.example.com");
  is(URICountListener.uniqueOriginsVisitedInPast24Hours, startingCount + 1,
     "test1.example.com should only count as a unique visit if example.com wasn't visited before");

  // http://www.exämple.test
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "http://xn--exmple-cua.test");
  is(URICountListener.uniqueOriginsVisitedInPast24Hours, startingCount + 2,
     "www.exämple.test should count as a unique visit");

  // Set the expiry time to 1 second
  await SpecialPowers.pushPrefEnv({set: [["browser.engagement.recent_visited_origins.expiry", 1]]});

  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "http://127.0.0.1");
  is(URICountListener.uniqueOriginsVisitedInPast24Hours, startingCount + 3,
     "127.0.0.1 should count as a unique visit");

  let countBefore = URICountListener.uniqueOriginsVisitedInPast24Hours;

  await new Promise(resolve => {
    setTimeout(_ => {
      let countAfter = URICountListener.uniqueOriginsVisitedInPast24Hours;
      is(countAfter, countBefore - 1,
         "The expiry should work correctly");
      resolve();
    }, 1100);
  });

  BrowserTestUtils.removeTab(win.gBrowser.selectedTab);
  BrowserTestUtils.removeTab(win.gBrowser.selectedTab);
  await BrowserTestUtils.closeWindow(win);
});

