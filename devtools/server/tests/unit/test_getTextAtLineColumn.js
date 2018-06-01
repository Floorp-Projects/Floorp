/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {getTextAtLineColumn} = require("devtools/server/actors/styles");

const TEST_DATA = [
  {
    desc: "simplest",
    input: "#id{color:red;background:yellow;}",
    line: 1,
    column: 5,
    expected: {offset: 4, text: "color:red;background:yellow;}"}
  },
  {
    desc: "multiple lines",
    input: "one\n two\n  three",
    line: 3,
    column: 3,
    expected: {offset: 11, text: "three"}
  },
];

function run_test() {
  for (const test of TEST_DATA) {
    info("Starting test: " + test.desc);
    info("Input string " + test.input);

    const output = getTextAtLineColumn(test.input, test.line, test.column);
    deepEqual(output, test.expected);
  }
}
