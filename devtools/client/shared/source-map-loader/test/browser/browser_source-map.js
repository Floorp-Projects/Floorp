/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Cover the high level API of these modules:
// getOriginalURLs getGeneratedRangesForOriginal functions

async function assertFixtureOriginalURLs(
  fixtureName,
  expectedUrls,
  testMessage
) {
  const originalSources = await fetchFixtureSourceMap(fixtureName);
  const urls = originalSources.map(s => s.url);
  Assert.deepEqual(urls, expectedUrls, testMessage);
}

add_task(async function testGetOriginalURLs() {
  await assertFixtureOriginalURLs(
    "absolute",
    ["https://example.com/cheese/heart.js"],
    "Test absolute URL"
  );

  await assertFixtureOriginalURLs(
    "bundle",
    [
      "webpack:///webpack/bootstrap%204ef8c7ec7c1df790781e",
      "webpack:///entry.js",
      "webpack:///times2.js",
      "webpack:///output.js",
      "webpack:///opts.js",
    ],
    "Test source with a url"
  );

  await assertFixtureOriginalURLs(
    "empty",
    [`${URL_ROOT_SSL}fixtures/heart.js`],
    "Test empty sourceRoot resolution"
  );

  await assertFixtureOriginalURLs(
    "noroot",
    [`${URL_ROOT_SSL}fixtures/heart.js`],
    "Test Non-existing sourceRoot resolution"
  );

  await assertFixtureOriginalURLs(
    "noroot2",
    [`${URL_ROOT_SSL}fixtures/heart.js`],
    "Test Non-existing sourceRoot resolution with relative URLs"
  );
});

add_task(async function testGetGeneratedRangesForOriginal() {
  const originals = await fetchFixtureSourceMap("intermingled-sources");

  const ranges = await gSourceMapLoader.getGeneratedRangesForOriginal(
    originals[0].id
  );

  Assert.deepEqual(
    ranges,
    [
      {
        start: {
          line: 4,
          column: 69,
        },
        end: {
          line: 9,
          column: Infinity,
        },
      },
      {
        start: {
          line: 11,
          column: 0,
        },
        end: {
          line: 17,
          column: 3,
        },
      },
      {
        start: {
          line: 19,
          column: 18,
        },
        end: {
          line: 19,
          column: 22,
        },
      },
      {
        start: {
          line: 26,
          column: 0,
        },
        end: {
          line: 26,
          column: Infinity,
        },
      },
      {
        start: {
          line: 28,
          column: 0,
        },
        end: {
          line: 28,
          column: Infinity,
        },
      },
    ],
    "Test the overall generated ranges on the source"
  );

  {
    // Note that we have to clear the source map in order to get the merged ranges,
    // otherwise we are still fetching the previous unmerged ones!
    const secondOriginals = await fetchFixtureSourceMap("intermingled-sources");
    const mergedRanges = await gSourceMapLoader.getGeneratedRangesForOriginal(
      secondOriginals[0].id,
      true
    );

    Assert.deepEqual(
      mergedRanges,
      [
        {
          start: {
            line: 4,
            column: 69,
          },
          end: {
            line: 28,
            column: Infinity,
          },
        },
      ],
      "Test the merged generated ranges on the source"
    );
  }
});

add_task(async function testBaseURLErrorHandling() {
  const source = {
    id: "missingmap.js",
    sourceMapURL: "missingmap.js.map",
    // Notice the duplicated ":" which cause the error here
    sourceMapBaseURL: "http:://example.com/",
  };

  try {
    await gSourceMapLoader.getOriginalURLs(source);
    ok(false, "Should throw");
  } catch (e) {
    is(e.message, "URL constructor: http:://example.com/ is not a valid URL.");
  }
});
