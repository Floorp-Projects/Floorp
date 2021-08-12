/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test for unEscapeURIForUI function in UrlbarUtils.
 */

"use strict";

const TEST_DATA = [
  {
    description: "Test for characters including percent encoded chars",
    input: "A%E3%81%82%F0%A0%AE%B7%21",
    expected: "Aあ𠮷!",
    testMessage: "Unescape given characters correctly",
  },
  {
    description: "Test for characters over the limit",
    input: "A%E3%81%82%F0%A0%AE%B7%21".repeat(
      Math.ceil(UrlbarUtils.MAX_TEXT_LENGTH / 25)
    ),
    expected: "A%E3%81%82%F0%A0%AE%B7%21".repeat(
      Math.ceil(UrlbarUtils.MAX_TEXT_LENGTH / 25)
    ),
    testMessage: "Return given characters as it is because of over the limit",
  },
];

add_task(function() {
  for (const { description, input, expected, testMessage } of TEST_DATA) {
    info(description);

    const result = UrlbarUtils.unEscapeURIForUI(input);
    Assert.equal(result, expected, testMessage);
  }
});
