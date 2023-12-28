/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This test ensures we compare URLs correctly. For more info on the scores,
 * please read the function definition.
 */

ChromeUtils.defineESModuleGetters(this, {
  SearchSERPTelemetry: "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

const TESTS = [
  {
    title: "No difference",
    url1: "https://www.example.org/search?a=b&c=d#hash",
    url2: "https://www.example.org/search?a=b&c=d#hash",
    expected: Infinity,
  },
  {
    // Since the ordering is different, a strict equality match is not going
    // match. The score will be high, but not Infinity.
    title: "Different ordering of query parameters",
    url1: "https://www.example.org/search?c=d&a=b#hash",
    url2: "https://www.example.org/search?a=b&c=d#hash",
    expected: 7,
  },
  {
    title: "Different protocol",
    url1: "http://www.example.org/search",
    url2: "https://www.example.org/search",
    expected: 0,
  },
  {
    title: "Different origin",
    url1: "https://example.org/search",
    url2: "https://www.example.org/search",
    expected: 0,
  },
  {
    title: "Different path",
    url1: "https://www.example.org/serp",
    url2: "https://www.example.org/search",
    expected: 1,
  },
  {
    title: "Different path, path on",
    url1: "https://www.example.org/serp",
    url2: "https://www.example.org/search",
    options: {
      path: true,
    },
    expected: 0,
  },
  {
    title: "Different query parameter keys",
    url1: "https://www.example.org/search?a=c",
    url2: "https://www.example.org/search?b=c",
    expected: 3,
  },
  {
    title: "Different query parameter keys, paramValues on",
    url1: "https://www.example.org/search?a=c",
    url2: "https://www.example.org/search?b=c",
    options: {
      paramValues: true,
    },
    // Shouldn't change the score because the option should only nullify
    // the result if one of the keys match but has different values.
    expected: 3,
  },
  {
    title: "Some different query parameter keys",
    url1: "https://www.example.org/search?a=b&c=d",
    url2: "https://www.example.org/search?a=b",
    expected: 5,
  },
  {
    title: "Some different query parameter keys, paramValues on",
    url1: "https://www.example.org/search?a=b&c=d",
    url2: "https://www.example.org/search?a=b",
    options: {
      paramValues: true,
    },
    // Shouldn't change the score because the option should only trigger
    // if the keys match but values differ.
    expected: 5,
  },
  {
    title: "Different query parameter values",
    url1: "https://www.example.org/search?a=b",
    url2: "https://www.example.org/search?a=c",
    expected: 4,
  },
  {
    title: "Different query parameter values, paramValues on",
    url1: "https://www.example.org/search?a=b&c=d",
    url2: "https://www.example.org/search?a=b&c=e",
    options: {
      paramValues: true,
    },
    expected: 0,
  },
  {
    title: "Some different query parameter values",
    url1: "https://www.example.org/search?a=b&c=d",
    url2: "https://www.example.org/search?a=b&c=e",
    expected: 6,
  },
  {
    title: "Different query parameter values, paramValues on",
    url1: "https://www.example.org/search?a=b&c=d",
    url2: "https://www.example.org/search?a=b&c=e",
    options: {
      paramValues: true,
    },
    expected: 0,
  },
  {
    title: "Empty query parameter",
    url1: "https://www.example.org/search?a=b&c",
    url2: "https://www.example.org/search?c&a=b",
    expected: 7,
  },
  {
    title: "Empty query parameter, paramValues on",
    url1: "https://www.example.org/search?a=b&c",
    url2: "https://www.example.org/search?c&a=b",
    options: {
      paramValues: true,
    },
    expected: 7,
  },
  {
    title: "Missing empty query parameter",
    url1: "https://www.example.org/search?c&a=b",
    url2: "https://www.example.org/search?a=b",
    expected: 5,
  },
  {
    title: "Missing empty query parameter, paramValues on",
    url1: "https://www.example.org/search?c&a=b",
    url2: "https://www.example.org/search?a=b",
    options: {
      paramValues: true,
    },
    expected: 5,
  },
  {
    title: "Different empty query parameter",
    url1: "https://www.example.org/search?c&a=b",
    url2: "https://www.example.org/search?a=b&c=foo",
    expected: 6,
  },
  {
    title: "Different empty query parameter, paramValues on",
    url1: "https://www.example.org/search?c&a=b",
    url2: "https://www.example.org/search?a=b&c=foo",
    options: {
      paramValues: true,
    },
    expected: 0,
  },
];

add_setup(async function () {
  await SearchSERPTelemetry.init();
});

add_task(async function test_parsing_extracted_urls() {
  for (let test of TESTS) {
    info(test.title);
    let result = SearchSERPTelemetry.compareUrls(
      new URL(test.url1),
      new URL(test.url2),
      test.options
    );
    Assert.equal(result, test.expected, "Equality: url1, url2");

    // Flip the URLs to ensure order doesn't matter.
    result = SearchSERPTelemetry.compareUrls(
      new URL(test.url2),
      new URL(test.url1),
      test.options
    );
    Assert.equal(result, test.expected, "Equality: url2, url1");
  }
});
