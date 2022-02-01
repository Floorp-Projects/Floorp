/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that the page data service can parse Open Graph metadata.
 */

add_task(async function test_type_website() {
  await verifyPageData(
    `
      <!DOCTYPE html>
      <html>
      <head>
        <title>Internet for people, not profit — Mozilla</title>
        <meta property="og:type" content="website">
        <meta property="og:site_name" content="Mozilla">
        <meta property="og:url" content="https://www.mozilla.org/">
        <meta property="og:image" content="https://example.com/preview-image">
        <meta property="og:title" content="Internet for people, not profit">
        <!-- We expect the test will ignore tags the parser does not recognize. -->
        <meta property="og:locale" content="en_CA">
        <meta property="og:description" content="Mozilla is the not-for-profit behind the lightning fast Firefox browser. We put people over profit to give everyone more power online.">
      </head>
      <body>
        <p>Test page</p>
      </body>
      </html>
    `,
    {
      siteName: "Mozilla",
      description:
        "Mozilla is the not-for-profit behind the lightning fast Firefox browser. We put people over profit to give everyone more power online.",
      image: "https://example.com/preview-image",
      data: {},
    }
  );
});

add_task(async function test_type_movie() {
  await verifyPageData(
    `
      <!DOCTYPE html>
      <html>
      <head>
        <title>Code Rush (TV Movie 2000)</title>
        <meta property="og:url" content="https://www.imdb.com/title/tt0499004/"/>
        <!-- Omitting og:site_name to test that the parser doesn't break on missing tags. -->
        <meta property="og:title" content="Code Rush (TV Movie 2000) - IMDb"/>
        <meta property="og:description" content="This is the description of the movie."/>
        <meta property="og:type" content="video.movie"/>
        <meta property="og:image" content="https://example.com/preview-code-rush"/>
        <meta property="og:image:height" content="750"/>
        <meta property="og:image:width" content="1000"/>
      </head>
      <body>
        <p>Test page</p>
      </body>
      </html>
    `,
    {
      image: "https://example.com/preview-code-rush",
      description: "This is the description of the movie.",
      data: {},
    }
  );
});
