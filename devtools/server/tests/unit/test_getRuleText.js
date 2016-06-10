/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {getRuleText} = require("devtools/server/actors/styles");

const TEST_DATA = [
  {
    desc: "Empty input",
    input: "",
    line: 1,
    column: 1,
    throws: true
  },
  {
    desc: "Simplest test case",
    input: "#id{color:red;background:yellow;}",
    line: 1,
    column: 1,
    expected: {offset: 4, text: "color:red;background:yellow;"}
  },
  {
    desc: "Multiple rules test case",
    input: "#id{color:red;background:yellow;}.class-one .class-two { position:absolute; line-height: 45px}",
    line: 1,
    column: 34,
    expected: {offset: 56, text: " position:absolute; line-height: 45px"}
  },
  {
    desc: "Unclosed rule",
    input: "#id{color:red;background:yellow;",
    line: 1,
    column: 1,
    expected: {offset: 4, text: "color:red;background:yellow;"}
  },
  {
    desc: "Null input",
    input: null,
    line: 1,
    column: 1,
    throws: true
  },
  {
    desc: "Missing loc",
    input: "#id{color:red;background:yellow;}",
    throws: true
  },
  {
    desc: "Multi-lines CSS",
    input: [
      "/* this is a multi line css */",
      "body {",
      "  color: green;",
      "  background-repeat: no-repeat",
      "}",
      " /*something else here */",
      "* {",
      "  color: purple;",
      "}"
    ].join("\n"),
    line: 7,
    column: 1,
    expected: {offset: 116, text: "\n  color: purple;\n"}
  },
  {
    desc: "Multi-lines CSS and multi-line rule",
    input: [
      "/* ",
      "* some comments",
      "*/",
      "",
      "body {",
      "    margin: 0;",
      "    padding: 15px 15px 2px 15px;",
      "    color: red;",
      "}",
      "",
      "#header .btn, #header .txt {",
      "    font-size: 100%;",
      "}",
      "",
      "#header #information {",
      "    color: #dddddd;",
      "    font-size: small;",
      "}",
    ].join("\n"),
    line: 5,
    column: 1,
    expected: {
      offset: 30,
      text: "\n    margin: 0;\n    padding: 15px 15px 2px 15px;\n    color: red;\n"}
  },
  {
    desc: "Content string containing a } character",
    input: "   #id{border:1px solid red;content: '}';color:red;}",
    line: 1,
    column: 4,
    expected: {offset: 7, text: "border:1px solid red;content: '}';color:red;"}
  },
  {
    desc: "Rule contains no tokens",
    input: "div{}",
    line: 1,
    column: 1,
    expected: {offset: 4, text: ""}
  },
];

function run_test() {
  for (let test of TEST_DATA) {
    do_print("Starting test: " + test.desc);
    do_print("Input string " + test.input);
    let output;
    try {
      output = getRuleText(test.input, test.line, test.column);
      if (test.throws) {
        do_print("Test should have thrown");
        do_check_true(false);
      }
    } catch (e) {
      do_print("getRuleText threw an exception with the given input string");
      if (test.throws) {
        do_print("Exception expected");
        do_check_true(true);
      } else {
        do_print("Exception unexpected\n" + e);
        do_check_true(false);
      }
    }
    if (output) {
      deepEqual(output, test.expected);
    }
  }
}
