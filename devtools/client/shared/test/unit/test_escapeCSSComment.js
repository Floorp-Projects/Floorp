/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
Cu.import("resource://gre/modules/devtools/Loader.jsm");
const {escapeCSSComment, _unescapeCSSComment} =
      devtools.require("devtools/client/shared/css-parsing-utils");

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
  for (let test of TEST_DATA) {
    ++i;
    do_print("Test #" + i);

    let escaped = escapeCSSComment(test.input);
    equal(escaped, test.expected);
    let unescaped = _unescapeCSSComment(escaped);
    equal(unescaped, test.input);
  }
}
