/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test prettifyCSS.

"use strict";

const {CssLogic} = require("devtools/shared/styleinspector/css-logic");


const TESTS = [
  { name: "simple test",
    input: "div { font-family:'Arial Black', Arial, sans-serif; }",
    expected: [
      "div {",
      "\tfont-family:'Arial Black', Arial, sans-serif;",
      "}"
    ]
  },

  { name: "whitespace before open brace",
    input: "div{}",
    expected: [
      "div {",
      "}"
    ]
  },

  { name: "minified with trailing newline",
    input: "\nbody{background:white;}div{font-size:4em;color:red}span{color:green;}\n",
    expected: [
      "",
      "body {",
      "\tbackground:white;",
      "}",
      "div {",
      "\tfont-size:4em;",
      "\tcolor:red",
      "}",
      "span {",
      "\tcolor:green;",
      "}"
    ]
  },

];

function run_test() {
  // Note that CssLogic.LINE_SEPARATOR is computed lazily, so we
  // ensure it is set.
  CssLogic.prettifyCSS("");

  for (let test of TESTS) {
    do_print(test.name);

    let input = test.input.split("\n").join(CssLogic.LINE_SEPARATOR);
    let output = CssLogic.prettifyCSS(input);
    let expected = test.expected.join(CssLogic.LINE_SEPARATOR) +
        CssLogic.LINE_SEPARATOR;
    equal(output, expected, test.name);
  }
}
