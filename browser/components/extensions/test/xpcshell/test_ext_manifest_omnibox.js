/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* testKeyword(params) {
  let normalized = yield ExtensionTestUtils.normalizeManifest({
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

add_task(function* test_manifest_commands() {
  // accepted single character keywords
  yield testKeyword({keyword: "a", expectError: false});
  yield testKeyword({keyword: "-", expectError: false});
  yield testKeyword({keyword: "嗨", expectError: false});
  yield testKeyword({keyword: "*", expectError: false});
  yield testKeyword({keyword: "/", expectError: false});

  // rejected single character keywords
  yield testKeyword({keyword: "?", expectError: true});
  yield testKeyword({keyword: " ", expectError: true});
  yield testKeyword({keyword: ":", expectError: true});

  // accepted multi-character keywords
  yield testKeyword({keyword: "aa", expectError: false});
  yield testKeyword({keyword: "http", expectError: false});
  yield testKeyword({keyword: "f?a", expectError: false});
  yield testKeyword({keyword: "fa?", expectError: false});
  yield testKeyword({keyword: "f/x", expectError: false});
  yield testKeyword({keyword: "/fx", expectError: false});

  // rejected multi-character keywords
  yield testKeyword({keyword: " a", expectError: true});
  yield testKeyword({keyword: "a ", expectError: true});
  yield testKeyword({keyword: "  ", expectError: true});
  yield testKeyword({keyword: " a ", expectError: true});
  yield testKeyword({keyword: "?fx", expectError: true});
  yield testKeyword({keyword: "fx/", expectError: true});
  yield testKeyword({keyword: "f:x", expectError: true});
  yield testKeyword({keyword: "fx:", expectError: true});
  yield testKeyword({keyword: "f x", expectError: true});

  // miscellaneous tests
  yield testKeyword({keyword: "こんにちは", expectError: false});
  yield testKeyword({keyword: "http://", expectError: true});
});
