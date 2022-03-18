/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Background load tests for the page data service.
 */

const TEST_URL =
  "data:text/html," +
  encodeURIComponent(`
    <html>
    <head>
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

add_task(async function test_pagedata_no_data() {
  let pageData = await PageDataService.fetchPageData(TEST_URL);

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

  Assert.equal(
    PageDataService.getCached(TEST_URL),
    null,
    "Should not have cached this data"
  );
});
