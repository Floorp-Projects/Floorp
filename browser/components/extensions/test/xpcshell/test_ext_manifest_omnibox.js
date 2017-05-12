/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testKeyword(params) {
  let normalized = await ExtensionTestUtils.normalizeManifest({
    "omnibox": {
      "keyword": params.keyword,
    },
  });

  if (params.expectError) {
    let expectedError = (
      String.raw`omnibox.keyword: String "${params.keyword}" ` +
      String.raw`must match /^[^?\s:]([^\s:]*[^/\s:])?$/`
    );
    ok(normalized.error.includes(expectedError),
       `The manifest error ${JSON.stringify(normalized.error)} ` +
       `must contain ${JSON.stringify(expectedError)}`);
  } else {
    equal(normalized.error, undefined, "Should not have an error");
    equal(normalized.errors.length, 0, "Should not have warnings");
  }
}

add_task(async function test_manifest_commands() {
  // accepted single character keywords
  await testKeyword({keyword: "a", expectError: false});
  await testKeyword({keyword: "-", expectError: false});
  await testKeyword({keyword: "嗨", expectError: false});
  await testKeyword({keyword: "*", expectError: false});
  await testKeyword({keyword: "/", expectError: false});

  // rejected single character keywords
  await testKeyword({keyword: "?", expectError: true});
  await testKeyword({keyword: " ", expectError: true});
  await testKeyword({keyword: ":", expectError: true});

  // accepted multi-character keywords
  await testKeyword({keyword: "aa", expectError: false});
  await testKeyword({keyword: "http", expectError: false});
  await testKeyword({keyword: "f?a", expectError: false});
  await testKeyword({keyword: "fa?", expectError: false});
  await testKeyword({keyword: "f/x", expectError: false});
  await testKeyword({keyword: "/fx", expectError: false});

  // rejected multi-character keywords
  await testKeyword({keyword: " a", expectError: true});
  await testKeyword({keyword: "a ", expectError: true});
  await testKeyword({keyword: "  ", expectError: true});
  await testKeyword({keyword: " a ", expectError: true});
  await testKeyword({keyword: "?fx", expectError: true});
  await testKeyword({keyword: "fx/", expectError: true});
  await testKeyword({keyword: "f:x", expectError: true});
  await testKeyword({keyword: "fx:", expectError: true});
  await testKeyword({keyword: "f x", expectError: true});

  // miscellaneous tests
  await testKeyword({keyword: "こんにちは", expectError: false});
  await testKeyword({keyword: "http://", expectError: true});
});
