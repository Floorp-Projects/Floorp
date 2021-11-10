/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Basic tests for the page data service.
 */

const BASE_URL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test_single_product_data() {
  let promise = PageDataService.once("page-data");

  const TEST_URL = BASE_URL + "product1.html";

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    let pageData = await promise;
    Assert.equal(pageData.url, TEST_URL, "Should have returned the loaded URL");
    Assert.equal(
      pageData.image,
      BASE_URL + "bon-echo-microwave-17in.jpg",
      "Should have returned the product image"
    );
    Assert.deepEqual(
      pageData.data,
      {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bon Echo Microwave",
          price: {
            value: 3,
            currency: "GBP",
          },
        },
      },
      "Should have returned the expected data"
    );
  });
});

add_task(async function test_single_multiple_data() {
  let promise = PageDataService.once("page-data");

  const TEST_URL = BASE_URL + "product2.html";

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    let pageData = await promise;
    Assert.equal(pageData.url, TEST_URL, "Should have returned the loaded URL");
    Assert.equal(
      pageData.image,
      BASE_URL + "bon-echo-microwave-17in.jpg",
      "Should have the right image."
    );
    Assert.deepEqual(
      pageData.data,
      {
        [PageDataSchema.DATA_TYPE.PRODUCT]: {
          name: "Bon Echo Microwave",
          price: {
            value: 3,
            currency: "GBP",
          },
        },
      },
      "Should have returned the expected data"
    );
  });
});
