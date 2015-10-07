/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cu = Components.utils;
const {require} = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm");
const Services = require("Services");
const {
  EXPAND_TAB,
  TAB_SIZE,
  DETECT_INDENT,
  getIndentationFromPrefs,
  getIndentationFromIteration,
  getIndentationFromString,
} = require("devtools/shared/shared/indentation");

function test_indent_from_prefs() {
  Services.prefs.setBoolPref(DETECT_INDENT, true);
  equal(getIndentationFromPrefs(), false,
        "getIndentationFromPrefs returning false");

  Services.prefs.setIntPref(TAB_SIZE, 73);
  Services.prefs.setBoolPref(EXPAND_TAB, false);
  Services.prefs.setBoolPref(DETECT_INDENT, false);
  deepEqual(getIndentationFromPrefs(), {indentUnit: 73, indentWithTabs: true},
            "getIndentationFromPrefs basic test");
}

const TESTS = [
  {
    desc: "two spaces",
    input: [
      "/*",
      " * tricky comment block",
      " */",
      "div {",
      "  color: red;",
      "  background: blue;",
      "}",
      "     ",
      "span {",
      "  padding-left: 10px;",
      "}"
    ],
    expected: {indentUnit: 2, indentWithTabs: false}
  },
  {
    desc: "four spaces",
    input: [
      "var obj = {",
      "    addNumbers: function() {",
      "        var x = 5;",
      "        var y = 18;",
      "        return x + y;",
      "    },",
      "   ",
      "    /*",
      "     * Do some stuff to two numbers",
      "     * ",
      "     * @param x",
      "     * @param y",
      "     * ",
      "     * @return the result of doing stuff",
      "     */",
      "    subtractNumbers: function(x, y) {",
      "        var x += 7;",
      "        var y += 18;",
      "        var result = x - y;",
      "        result %= 2;",
      "    }",
      "}"
    ],
    expected: {indentUnit: 4, indentWithTabs: false}
  },
  {
    desc: "tabs",
    input: [
      "/*",
      " * tricky comment block",
      " */",
      "div {",
      "\tcolor: red;",
      "\tbackground: blue;",
      "}",
      "",
      "span {",
      "\tpadding-left: 10px;",
      "}"
    ],
    expected: {indentUnit: 2, indentWithTabs: true}
  },
  {
    desc: "no indent",
    input: [
      "var x = 0;",
      "           // stray thing",
      "var y = 9;",
      "    ",
      ""
    ],
    expected: {indentUnit: 2, indentWithTabs: false}
  },
];

function test_indent_detection() {
  Services.prefs.setIntPref(TAB_SIZE, 2);
  Services.prefs.setBoolPref(EXPAND_TAB, true);
  Services.prefs.setBoolPref(DETECT_INDENT, true);

  for (let test of TESTS) {
    let iterFn = function(start, end, callback) {
      test.input.slice(start, end).forEach(callback);
    };

    deepEqual(getIndentationFromIteration(iterFn), test.expected,
              "test getIndentationFromIteration " + test.desc);
  }

  for (let test of TESTS) {
    deepEqual(getIndentationFromString(test.input.join("\n")), test.expected,
              "test getIndentationFromString " + test.desc);
  }
}

function run_test() {
  try {
    test_indent_from_prefs();
    test_indent_detection();
  } finally {
    Services.prefs.clearUserPref(TAB_SIZE);
    Services.prefs.clearUserPref(EXPAND_TAB);
    Services.prefs.clearUserPref(DETECT_INDENT);
  }
}
