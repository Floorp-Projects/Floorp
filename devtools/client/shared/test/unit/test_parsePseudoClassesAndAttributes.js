/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {
  parsePseudoClassesAndAttributes,
  SELECTOR_ATTRIBUTE,
  SELECTOR_ELEMENT,
  SELECTOR_PSEUDO_CLASS
} = require("devtools/shared/css-parsing-utils");

const TEST_DATA = [
  // Test that a null input throws an exception
  {
    input: null,
    throws: true
  },
  // Test that a undefined input throws an exception
  {
    input: undefined,
    throws: true
  },
  {
    input: ":root",
    expected: [
      { value: ":root", type: SELECTOR_PSEUDO_CLASS }
    ]
  },
  {
    input: ".testclass",
    expected: [
      { value: ".testclass", type: SELECTOR_ELEMENT }
    ]
  },
  {
    input: "div p",
    expected: [
      { value: "div p", type: SELECTOR_ELEMENT }
    ]
  },
  {
    input: "div > p",
    expected: [
      { value: "div > p", type: SELECTOR_ELEMENT }
    ]
  },
  {
    input: "a[hidden]",
    expected: [
      { value: "a", type: SELECTOR_ELEMENT },
      { value: "[hidden]", type: SELECTOR_ATTRIBUTE }
    ]
  },
  {
    input: "a[hidden=true]",
    expected: [
      { value: "a", type: SELECTOR_ELEMENT },
      { value: "[hidden=true]", type: SELECTOR_ATTRIBUTE }
    ]
  },
  {
    input: "a[hidden=true] p:hover",
    expected: [
      { value: "a", type: SELECTOR_ELEMENT },
      { value: "[hidden=true]", type: SELECTOR_ATTRIBUTE },
      { value: " p", type: SELECTOR_ELEMENT },
      { value: ":hover", type: SELECTOR_PSEUDO_CLASS }
    ]
  },
  {
    input: "a[checked=\"true\"]",
    expected: [
      { value: "a", type: SELECTOR_ELEMENT },
      { value: "[checked=\"true\"]", type: SELECTOR_ATTRIBUTE }
    ]
  },
  {
    input: "a[title~=test]",
    expected: [
      { value: "a", type: SELECTOR_ELEMENT },
      { value: "[title~=test]", type: SELECTOR_ATTRIBUTE }
    ]
  },
  {
    input: "h1[hidden=\"true\"][title^=\"Important\"]",
    expected: [
      { value: "h1", type: SELECTOR_ELEMENT },
      { value: "[hidden=\"true\"]", type: SELECTOR_ATTRIBUTE },
      { value: "[title^=\"Important\"]", type: SELECTOR_ATTRIBUTE}
    ]
  },
  {
    input: "p:hover",
    expected: [
      { value: "p", type: SELECTOR_ELEMENT },
      { value: ":hover", type: SELECTOR_PSEUDO_CLASS }
    ]
  },
  {
    input: "p + .testclass:hover",
    expected: [
      { value: "p + .testclass", type: SELECTOR_ELEMENT },
      { value: ":hover", type: SELECTOR_PSEUDO_CLASS }
    ]
  },
  {
    input: "p::before",
    expected: [
      { value: "p", type: SELECTOR_ELEMENT },
      { value: "::before", type: SELECTOR_PSEUDO_CLASS }
    ]
  },
  {
    input: "p:nth-child(2)",
    expected: [
      { value: "p", type: SELECTOR_ELEMENT },
      { value: ":nth-child(2)", type: SELECTOR_PSEUDO_CLASS }
    ]
  },
  {
    input: "p:not([title=\"test\"]) .testclass",
    expected: [
      { value: "p", type: SELECTOR_ELEMENT },
      { value: ":not([title=\"test\"])", type: SELECTOR_PSEUDO_CLASS },
      { value: " .testclass", type: SELECTOR_ELEMENT }
    ]
  },
  {
    input: "a\\:hover",
    expected: [
      { value: "a\\:hover", type: SELECTOR_ELEMENT }
    ]
  },
  {
    input: ":not(:lang(it))",
    expected: [
      { value: ":not(:lang(it))", type: SELECTOR_PSEUDO_CLASS }
    ]
  },
  {
    input: "p:not(:lang(it))",
    expected: [
      { value: "p", type: SELECTOR_ELEMENT },
      { value: ":not(:lang(it))", type: SELECTOR_PSEUDO_CLASS }
    ]
  },
  {
    input: "p:not(p:lang(it))",
    expected: [
      { value: "p", type: SELECTOR_ELEMENT },
      { value: ":not(p:lang(it))", type: SELECTOR_PSEUDO_CLASS }
    ]
  },
  {
    input: ":not(:lang(it)",
    expected: [
      { value: ":not(:lang(it)", type: SELECTOR_ELEMENT }
    ]
  },
  {
    input: ":not(:lang(it)))",
    expected: [
      { value: ":not(:lang(it))", type: SELECTOR_PSEUDO_CLASS },
      { value: ")", type: SELECTOR_ELEMENT }
    ]
  }
];

function run_test() {
  for (let test of TEST_DATA) {
    dump("Test input string " + test.input + "\n");
    let output;

    try {
      output = parsePseudoClassesAndAttributes(test.input);
    } catch (e) {
      dump("parsePseudoClassesAndAttributes threw an exception with the " +
        "given input string\n");
      if (test.throws) {
        ok(true, "Exception expected");
      } else {
        dump();
        ok(false, "Exception unexpected\n" + e);
      }
    }

    if (output) {
      assertOutput(output, test.expected);
    }
  }
}

function assertOutput(actual, expected) {
  if (actual.length === expected.length) {
    for (let i = 0; i < expected.length; i++) {
      dump("Check that the output item has the expected value and type\n");
      ok(!!actual[i]);
      equal(expected[i].value, actual[i].value);
      equal(expected[i].type, actual[i].type);
    }
  } else {
    for (let prop of actual) {
      dump("Actual output contained: {value: " + prop.value + ", type: " +
        prop.type + "}\n");
    }
    equal(actual.length, expected.length);
  }
}
