/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the webconsole output for various types of objects.

const TEST_URI = "data:text/html;charset=utf8,test for console output - 01";

let {DebuggerServer} = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});

let LONG_STRING_LENGTH = DebuggerServer.LONG_STRING_LENGTH;
let LONG_STRING_INITIAL_LENGTH = DebuggerServer.LONG_STRING_INITIAL_LENGTH;
DebuggerServer.LONG_STRING_LENGTH = 100;
DebuggerServer.LONG_STRING_INITIAL_LENGTH = 50;

let longString = (new Array(DebuggerServer.LONG_STRING_LENGTH + 4)).join("a");
let initialString = longString.substring(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH);

let inputTests = [
  // 0
  {
    input: "'hello \\nfrom \\rthe \\\"string world!'",
    output: "\"hello \nfrom \rthe \"string world!\"",
  },

  // 1
  {
    // unicode test
    input: "'\xFA\u1E47\u0129\xE7\xF6d\xEA \u021B\u0115\u0219\u0165'",
    output: "\"\xFA\u1E47\u0129\xE7\xF6d\xEA \u021B\u0115\u0219\u0165\"",
  },

  // 2
  {
    input: "'" + longString + "'",
    output: '"' + initialString + "\"[\u2026]",
    printOutput: initialString,
  },

  // 3
  {
    input: "''",
    output: '""',
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
  },

  // 8
  {
    input: "/foobar/",
    output: "/foobar/",
    inspectable: true,
  },
];

if (typeof Symbol !== "undefined") {
  inputTests.push(
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
    });
}

longString = initialString = null;

function test() {
  registerCleanupFunction(() => {
    DebuggerServer.LONG_STRING_LENGTH = LONG_STRING_LENGTH;
    DebuggerServer.LONG_STRING_INITIAL_LENGTH = LONG_STRING_INITIAL_LENGTH;
  });

  Task.spawn(function*() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    return checkOutputForInputs(hud, inputTests);
  }).then(finishUp);
}

function finishUp() {
  inputTests = null;
  finishTest();
}
