/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test splitBy from node-attribute-parser.js

var Cu = Components.utils;
var {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {splitBy} = require("devtools/client/shared/node-attribute-parser");

const TEST_DATA = [{
  value: "this is a test",
  splitChar: " ",
  expected: [
    {value: "this"},
    {value: " ", type: "string"},
    {value: "is"},
    {value: " ", type: "string"},
    {value: "a"},
    {value: " ", type: "string"},
    {value: "test"}
  ]
}, {
  value: "/path/to/handler",
  splitChar: " ",
  expected: [
    {value: "/path/to/handler"}
  ]
}, {
  value: "test",
  splitChar: " ",
  expected: [
    {value: "test"}
  ]
}, {
  value: " test ",
  splitChar: " ",
  expected: [
    {value: " ", type: "string"},
    {value: "test"},
    {value: " ", type: "string"}
  ]
}, {
  value: "",
  splitChar: " ",
  expected: []
}, {
  value: "   ",
  splitChar: " ",
  expected: [
    {value: " ", type: "string"},
    {value: " ", type: "string"},
    {value: " ", type: "string"}
  ]
}];

function run_test() {
  for (let {value, splitChar, expected} of TEST_DATA) {
    do_print("Splitting string: " + value);
    let tokens = splitBy(value, splitChar);

    do_print("Checking that the number of parsed tokens is correct");
    do_check_eq(tokens.length, expected.length);

    for (let i = 0; i < tokens.length; i++) {
      do_print("Checking the data in token " + i);
      do_check_eq(tokens[i].value, expected[i].value);
      if (expected[i].type) {
        do_check_eq(tokens[i].type, expected[i].type);
      }
    }
  }
}
