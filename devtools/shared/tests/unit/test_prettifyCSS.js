/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test prettifyCSS.

"use strict";

const {prettifyCSS} = require("devtools/shared/inspector/css-logic");
const Services = require("Services");

const EXPAND_TAB = "devtools.editor.expandtab";

const TESTS_TAB_INDENT = [
  { name: "simple test. indent using tabs",
    input: "div { font-family:'Arial Black', Arial, sans-serif; }",
    expected: [
      "div {",
      "\tfont-family:'Arial Black', Arial, sans-serif;",
      "}"
    ]
  },

  { name: "whitespace before open brace. indent using tabs",
    input: "div{}",
    expected: [
      "div {",
      "}"
    ]
  },

  { name: "minified with trailing newline. indent using tabs",
    input: "\nbody{background:white;}div{font-size:4em;color:red}span{color:green;}\n",
    expected: [
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

  { name: "leading whitespace. indent using tabs",
    input: "\n    div{color: red;}",
    expected: [
      "div {",
      "\tcolor: red;",
      "}"
    ]
  },

  { name: "CSS with extra closing brace. indent using tabs",
    input: "body{margin:0}} div{color:red}",
    expected: [
      "body {",
      "\tmargin:0",
      "}",
      "}",
      "div {",
      "\tcolor:red",
      "}",
    ]
  },
];

const TESTS_SPACE_INDENT = [
  { name: "simple test. indent using spaces",
    input: "div { font-family:'Arial Black', Arial, sans-serif; }",
    expected: [
      "div {",
      " font-family:'Arial Black', Arial, sans-serif;",
      "}"
    ]
  },

  { name: "whitespace before open brace. indent using spaces",
    input: "div{}",
    expected: [
      "div {",
      "}"
    ]
  },

  { name: "minified with trailing newline. indent using spaces",
    input: "\nbody{background:white;}div{font-size:4em;color:red}span{color:green;}\n",
    expected: [
      "body {",
      " background:white;",
      "}",
      "div {",
      " font-size:4em;",
      " color:red",
      "}",
      "span {",
      " color:green;",
      "}"
    ]
  },

  { name: "leading whitespace. indent using spaces",
    input: "\n    div{color: red;}",
    expected: [
      "div {",
      " color: red;",
      "}"
    ]
  },

  { name: "CSS with extra closing brace. indent using spaces",
    input: "body{margin:0}} div{color:red}",
    expected: [
      "body {",
      " margin:0",
      "}",
      "}",
      "div {",
      " color:red",
      "}",
    ]
  },
];

function run_test() {
  // Note that prettifyCSS.LINE_SEPARATOR is computed lazily, so we
  // ensure it is set.
  prettifyCSS("");

  Services.prefs.setBoolPref(EXPAND_TAB, true);
  for (let test of TESTS_SPACE_INDENT) {
    do_print(test.name);

    let input = test.input.split("\n").join(prettifyCSS.LINE_SEPARATOR);
    let output = prettifyCSS(input);
    let expected = test.expected.join(prettifyCSS.LINE_SEPARATOR) +
        prettifyCSS.LINE_SEPARATOR;
    equal(output, expected, test.name);
  }

  Services.prefs.setBoolPref(EXPAND_TAB, false);
  for (let test of TESTS_TAB_INDENT) {
    do_print(test.name);

    let input = test.input.split("\n").join(prettifyCSS.LINE_SEPARATOR);
    let output = prettifyCSS(input);
    let expected = test.expected.join(prettifyCSS.LINE_SEPARATOR) +
        prettifyCSS.LINE_SEPARATOR;
    equal(output, expected, test.name);
  }
  Services.prefs.clearUserPref(EXPAND_TAB);
}
