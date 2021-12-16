/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Basic tests for twitter cards.
 */

add_task(async function test_twitter_card() {
  await verifyPageData(
    `
      <!DOCTYPE html>
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
    `,
    {
      siteName: "@nytimes",
      description: "NEWARK - The guest list and parade of limousines",
      image:
        "http://graphics8.nytimes.com/images/2012/02/19/us/19whitney-span/19whitney-span-articleLarge.jpg",
      data: {},
    }
  );
});
