/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  getRuleText,
} = require("resource://devtools/server/actors/utils/style-utils.js");

const TEST_DATA = [
  {
    desc: "Empty input",
    input: "",
    line: 1,
    column: 1,
    throws: true,
  },
  {
    desc: "Null input",
    input: null,
    line: 1,
    column: 1,
    throws: true,
  },
  {
    desc: "Missing loc",
    input: "#id{color:red;background:yellow;}",
    throws: true,
  },
  {
    desc: "No opening bracket",
    input: "/* hey */",
    line: 1,
    column: 1,
    throws: true,
  },
  {
    desc: "Simplest test case",
    input: "#id{color:red;background:yellow;}",
    line: 1,
    column: 1,
    expected: "color:red;background:yellow;",
  },
  {
    desc: "Multiple rules test case",
    input:
      "#id{color:red;background:yellow;}.class-one .class-two " +
      "{ position:absolute; line-height: 45px}",
    line: 1,
    column: 34,
    expected: " position:absolute; line-height: 45px",
  },
  {
    desc: "Unclosed rule",
    input: "#id{color:red;background:yellow;",
    line: 1,
    column: 1,
    expected: "color:red;background:yellow;",
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
      "}       ",
    ].join("\n"),
    line: 7,
    column: 1,
    expected: "\n  color: purple;\n",
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
    expected:
      "\n    margin: 0;\n    padding: 15px 15px 2px 15px;\n    color: red;\n",
  },
  {
    desc: "Content string containing a } character",
    input: "   #id{border:1px solid red;content: '}';color:red;}",
    line: 1,
    column: 4,
    expected: "border:1px solid red;content: '}';color:red;",
  },
  {
    desc: "Attribute selector containing a { character",
    input: `div[data-x="{"]{color: gold}`,
    line: 1,
    column: 1,
    expected: "color: gold",
  },
  {
    desc: "Rule contains no tokens",
    input: "div{}",
    line: 1,
    column: 1,
    expected: "",
  },
  {
    desc: "Rule contains invalid declaration",
    input: `#id{color;}`,
    line: 1,
    column: 1,
    expected: "color;",
  },
  {
    desc: "Rule contains invalid declaration",
    input: `#id{-}`,
    line: 1,
    column: 1,
    expected: "-",
  },
  {
    desc: "Rule contains nested rule",
    input: `#id{background: gold; .nested{color:blue;} color: tomato;  }`,
    line: 1,
    column: 1,
    expected: "background: gold; .nested{color:blue;} color: tomato;  ",
  },
  {
    desc: "Rule contains nested rule with invalid declaration",
    input: `#id{.nested{color;}}`,
    line: 1,
    column: 1,
    expected: ".nested{color;}",
  },
  {
    desc: "Rule contains unicode chars",
    input: `#id /*🙃*/ {content: "☃️";}`,
    line: 1,
    column: 1,
    expected: `content: "☃️";`,
  },
];

function run_test() {
  for (const test of TEST_DATA) {
    info("Starting test: " + test.desc);
    info("Input string " + test.input);
    let output;
    try {
      output = getRuleText(test.input, test.line, test.column);
      if (test.throws) {
        info("Test should have thrown");
        Assert.ok(false);
      }
    } catch (e) {
      info("getRuleText threw an exception with the given input string");
      if (test.throws) {
        info("Exception expected");
        Assert.ok(true);
      } else {
        info("Exception unexpected\n" + e);
        Assert.ok(false);
      }
    }
    if (output) {
      Assert.equal(output, test.expected);
    }
  }
}
