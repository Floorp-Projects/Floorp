/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//

"use strict";

thisTestLeaksUncaughtRejectionsAndShouldBeFixed("null");

// Test the webconsole output for various types of objects.

const TEST_URI = "data:text/html;charset=utf8,test for console output - 01";

var {DebuggerServer} = require("devtools/server/main");

var longString = (new Array(DebuggerServer.LONG_STRING_LENGTH + 4)).join("a");
var initialString = longString.substring(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH);

var inputTests = [
  // 0
  {
    input: "'hello \\nfrom \\rthe \\\"string world!'",
    output: "\"hello \nfrom \rthe \"string world!\"",
    consoleOutput: "hello \nfrom \rthe \"string world!",
  },

  // 1
  {
    // unicode test
    input: "'\xFA\u1E47\u0129\xE7\xF6d\xEA \u021B\u0115\u0219\u0165'",
    output: "\"\xFA\u1E47\u0129\xE7\xF6d\xEA \u021B\u0115\u0219\u0165\"",
    consoleOutput: "\xFA\u1E47\u0129\xE7\xF6d\xEA \u021B\u0115\u0219\u0165",
  },

  // 2
  {
    input: "'" + longString + "'",
    output: '"' + initialString + "\"[\u2026]",
    consoleOutput: initialString + "[\u2026]",
    printOutput: initialString,
  },

  // 3
  {
    input: "''",
    output: '""',
    consoleOutput: "",
    printOutput: '""',
  },

  // 4
  {
    input: "0",
    output: "0",
  },

  // 5
  {
    input: "'0'",
    output: '"0"',
    consoleOutput: "0",
  },

  // 6
  {
    input: "42",
    output: "42",
  },

  // 7
  {
    input: "'42'",
    output: '"42"',
    consoleOutput: "42",
  },

  // 8
  {
    input: "/foobar/",
    output: "/foobar/",
    inspectable: true,
  },

  // 9
  {
    input: "Symbol()",
    output: "Symbol()"
  },

  // 10
  {
    input: "Symbol('foo')",
    output: "Symbol(foo)"
  },

  // 11
  {
    input: "Symbol.iterator",
    output: "Symbol(Symbol.iterator)"
  },
];

longString = initialString = null;

function test() {
  requestLongerTimeout(2);

  Task.spawn(function* () {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    return checkOutputForInputs(hud, inputTests);
  }).then(finishUp);
}

function finishUp() {
  longString = initialString = inputTests = null;
  finishTest();
}
