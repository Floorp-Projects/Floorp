/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Basic tests for the page data service.
 */

const TEST_URL = "https://example.com/";
const TEST_URL2 = "https://example.com/browser";

add_task(async function test_pagedata_no_data() {
  let promise = PageDataService.once("no-page-data");

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    let pageData = await promise;
    Assert.equal(pageData.url, TEST_URL, "Should have returned the loaded URL");
    Assert.deepEqual(pageData.data, [], "Should have returned no data");
    Assert.deepEqual(
      PageDataService.getCached(TEST_URL),
      pageData,
      "Should return the same data from the cache"
    );
    Assert.equal(
      pageData.weakBrowser.get(),
      browser,
      "Should return the collection browser"
    );

    promise = PageDataService.once("no-page-data");
    BrowserTestUtils.loadURI(browser, TEST_URL2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_URL2);
    pageData = await promise;
    Assert.equal(
      pageData.url,
      TEST_URL2,
      "Should have returned the loaded URL"
    );
    Assert.deepEqual(pageData.data, [], "Should have returned no data");
    Assert.deepEqual(
      PageDataService.getCached(TEST_URL2),
      pageData,
      "Should return the same data from the cache"
    );
    Assert.equal(
      pageData.weakBrowser.get(),
      browser,
      "Should return the collection browser"
    );

    info("Test going back still triggers collection");

    promise = PageDataService.once("no-page-data");
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_URL
    );
    browser.goBack();
    await locationChangePromise;
    pageData = await promise;

    Assert.equal(
      pageData.url,
      TEST_URL,
      "Should have returned the URL of the previous page"
    );
    Assert.deepEqual(pageData.data, [], "Should have returned no data");
    Assert.deepEqual(
      PageDataService.getCached(TEST_URL),
      pageData,
      "Should return the same data from the cache"
    );
    Assert.equal(
      pageData.weakBrowser.get(),
      browser,
      "Should return the collection browser"
    );
  });
});
