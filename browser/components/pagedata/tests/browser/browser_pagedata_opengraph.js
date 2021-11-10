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
    Assert.equal(pageData.siteName, "Mozilla", "Should have the site name");
    Assert.equal(
      pageData.image,
      "https://example.com/preview-image",
      "Should have the image"
    );
    Assert.deepEqual(pageData.data, {}, "Should have no specific data");
  });
});

add_task(async function test_type_movie() {
  let promise = PageDataService.once("page-data");

  const TEST_URL = BASE_URL + "opengraph2.html";

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    let pageData = await promise;
    Assert.equal(pageData.url, TEST_URL, "Should have returned the loaded URL");
    Assert.equal(pageData.siteName, undefined, "Should not have the site name");
    Assert.equal(
      pageData.image,
      "https://example.com/preview-code-rush",
      "Should have the image"
    );
    Assert.deepEqual(pageData.data, {}, "Should have no specific data");
  });
});
