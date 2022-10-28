/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests for the page data cache.
 */

const TEST_URL =
  "data:text/html," +
  encodeURIComponent(`
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="utf-8">
      <meta name="twitter:card" content="summary_large_image">
      <meta name="twitter:site" content="@nytimes">
      <meta name="twitter:creator" content="@SarahMaslinNir">
      <meta name="twitter:title" content="Parade of Fans for Houston’s Funeral">
      <meta name="twitter:description" content="NEWARK - The guest list and parade of limousines">
      <meta name="twitter:image" content="http://graphics8.nytimes.com/images/2012/02/19/us/19whitney-span/19whitney-span-articleLarge.jpg">
    </head>
    <body>
    </body>
    </html>
`);

/**
 * Runs a task with a new page loaded into a tab in a new browser window.
 *
 * @param {string} url
 *   The url to load.
 * @param {Function} task
 *   The task to run. May return a promise.
 */
async function withBrowserInNewWindow(url, task) {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let tab = await BrowserTestUtils.openNewForegroundTab(newWin.gBrowser, url);
  await task(tab.linkedBrowser);
  await BrowserTestUtils.closeWindow(newWin);
}

add_task(async function test_pagedata_cache() {
  let promise = PageDataService.once("page-data");

  Assert.equal(
    PageDataService.getCached(TEST_URL),
    null,
    "Should be no data cached."
  );

  await BrowserTestUtils.withNewTab(TEST_URL, async () => {
    let pageData = await promise;

    Assert.deepEqual(
      PageDataService.getCached(TEST_URL),
      pageData,
      "Should return the same data from the cache"
    );

    delete pageData.date;

    Assert.deepEqual(
      pageData,
      {
        url: TEST_URL,
        siteName: "@nytimes",
        description: "NEWARK - The guest list and parade of limousines",
        image:
          "http://graphics8.nytimes.com/images/2012/02/19/us/19whitney-span/19whitney-span-articleLarge.jpg",
        data: {},
      },
      "Should have returned the right data"
    );
  });

  Assert.equal(
    PageDataService.getCached(TEST_URL),
    null,
    "Data should no longer be cached."
  );

  promise = PageDataService.once("page-data");

  // Checks that closing a window containing a tracked tab stops tracking the tab.
  await withBrowserInNewWindow(TEST_URL, async () => {
    let pageData = await promise;

    Assert.deepEqual(
      PageDataService.getCached(TEST_URL),
      pageData,
      "Should return the same data from the cache"
    );

    delete pageData.date;
    Assert.deepEqual(
      pageData,
      {
        url: TEST_URL,
        siteName: "@nytimes",
        description: "NEWARK - The guest list and parade of limousines",
        image:
          "http://graphics8.nytimes.com/images/2012/02/19/us/19whitney-span/19whitney-span-articleLarge.jpg",
        data: {},
      },
      "Should have returned the right data"
    );
  });

  Assert.equal(
    PageDataService.getCached(TEST_URL),
    null,
    "Data should no longer be cached."
  );

  let actor = {};
  PageDataService.lockEntry(actor, TEST_URL);

  promise = PageDataService.once("page-data");

  // Closing a tracked tab shouldn't expire the data here as we have another lock.
  await BrowserTestUtils.withNewTab(TEST_URL, async () => {
    await promise;
  });

  promise = PageDataService.once("page-data");

  // Closing a window with a tracked tab shouldn't expire the data here as we have another lock.
  await withBrowserInNewWindow(TEST_URL, async () => {
    await promise;
  });

  let cached = PageDataService.getCached(TEST_URL);
  delete cached.date;
  Assert.deepEqual(
    cached,
    {
      url: TEST_URL,
      siteName: "@nytimes",
      description: "NEWARK - The guest list and parade of limousines",
      image:
        "http://graphics8.nytimes.com/images/2012/02/19/us/19whitney-span/19whitney-span-articleLarge.jpg",
      data: {},
    },
    "Entry should still be cached"
  );

  PageDataService.unlockEntry(actor, TEST_URL);

  Assert.equal(
    PageDataService.getCached(TEST_URL),
    null,
    "Data should no longer be cached."
  );
});
