/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that the page data service can parse Open Graph metadata.
 */

const BASE_URL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test_type_website() {
  let promise = PageDataService.once("page-data");

  const TEST_URL = BASE_URL + "opengraph1.html";

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    let pageData = await promise;
    Assert.equal(pageData.url, TEST_URL, "Should have returned the loaded URL");
    Assert.equal(pageData.data.length, 1, "Should have only one data item");
    Assert.deepEqual(
      pageData.data,
      [
        {
          type: PageDataCollector.DATA_TYPE.GENERAL,
          data: [
            {
              type: "website",
              site_name: "Mozilla",
              url: "https://www.mozilla.org/",
              image: "https://example.com/preview-image",
              title: "Internet for people, not profit",
            },
          ],
        },
      ],
      "Should have returned the expected data"
    );
    Assert.equal(
      pageData.weakBrowser.get(),
      browser,
      "Should return the collection browser"
    );
  });
});

add_task(async function test_type_movie() {
  let promise = PageDataService.once("page-data");

  const TEST_URL = BASE_URL + "opengraph2.html";

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    let pageData = await promise;
    Assert.equal(pageData.url, TEST_URL, "Should have returned the loaded URL");
    Assert.equal(pageData.data.length, 1, "Should have only one data item");
    Assert.deepEqual(
      pageData.data,
      [
        {
          type: PageDataCollector.DATA_TYPE.GENERAL,
          data: [
            {
              type: "video.movie",
              site_name: undefined,
              url: "https://www.imdb.com/title/tt0499004/",
              image: "https://example.com/preview-code-rush",
              title: "Code Rush (TV Movie 2000) - IMDb",
            },
          ],
        },
      ],
      "Should have returned the expected data"
    );
    Assert.equal(
      pageData.weakBrowser.get(),
      browser,
      "Should return the collection browser"
    );
  });
});
