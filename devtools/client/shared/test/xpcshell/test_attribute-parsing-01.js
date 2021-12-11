/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test splitBy from node-attribute-parser.js

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const { splitBy } = require("devtools/client/shared/node-attribute-parser");

const TEST_DATA = [
  {
    value: "this is a test",
    splitChar: " ",
    expected: [
      { value: "this" },
      { value: " ", type: "string" },
      { value: "is" },
      { value: " ", type: "string" },
      { value: "a" },
      { value: " ", type: "string" },
      { value: "test" },
    ],
  },
  {
    value: "/path/to/handler",
    splitChar: " ",
    expected: [{ value: "/path/to/handler" }],
  },
  {
    value: "test",
    splitChar: " ",
    expected: [{ value: "test" }],
  },
  {
    value: " test ",
    splitChar: " ",
    expected: [
      { value: " ", type: "string" },
      { value: "test" },
      { value: " ", type: "string" },
    ],
  },
  {
    value: "",
    splitChar: " ",
    expected: [],
  },
  {
    value: "   ",
    splitChar: " ",
    expected: [
      { value: " ", type: "string" },
      { value: " ", type: "string" },
      { value: " ", type: "string" },
    ],
  },
];

function run_test() {
  for (const { value, splitChar, expected } of TEST_DATA) {
    info("Splitting string: " + value);
    const tokens = splitBy(value, splitChar);

    info("Checking that the number of parsed tokens is correct");
    Assert.equal(tokens.length, expected.length);

    for (let i = 0; i < tokens.length; i++) {
      info("Checking the data in token " + i);
      Assert.equal(tokens[i].value, expected[i].value);
      if (expected[i].type) {
        Assert.equal(tokens[i].type, expected[i].type);
      }
    }
  }
}
