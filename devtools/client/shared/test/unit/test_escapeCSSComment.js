/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const {escapeCSSComment, _unescapeCSSComment} = require("devtools/shared/css/parsing-utils");

const TEST_DATA = [
  {
    input: "simple",
    expected: "simple"
  },
  {
    input: "/* comment */",
    expected: "/\\* comment *\\/"
  },
  {
    input: "/* two *//* comments */",
    expected: "/\\* two *\\//\\* comments *\\/"
  },
  {
    input: "/* nested /\\* comment *\\/ */",
    expected: "/\\* nested /\\\\* comment *\\\\/ *\\/",
  }
];

function run_test() {
  let i = 0;
  for (const test of TEST_DATA) {
    ++i;
    info("Test #" + i);

    const escaped = escapeCSSComment(test.input);
    equal(escaped, test.expected);
    const unescaped = _unescapeCSSComment(escaped);
    equal(unescaped, test.input);
  }
}
