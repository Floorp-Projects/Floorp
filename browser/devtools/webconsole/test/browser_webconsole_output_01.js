/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the webconsole output for various types of objects.

const TEST_URI = "data:text/html;charset=utf8,test for console output - 01";

let dateNow = Date.now();
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

  // 9
  {
    input: "/foo?b*\\s\"ar/igym",
    output: "/foo?b*\\s\"ar/gimy",
    printOutput: "/foo?b*\\s\"ar/gimy",
    inspectable: true,
  },

  // 10
  {
    input: "null",
    output: "null",
  },

  // 11
  {
    input: "undefined",
    output: "undefined",
  },

  // 12
  {
    input: "true",
    output: "true",
  },

  // 13
  {
    input: "false",
    output: "false",
  },


  // 14
  {
    input: "new Date(" + dateNow + ")",
    output: "Date " + (new Date(dateNow)).toISOString(),
    printOutput: (new Date(dateNow)).toString(),
    inspectable: true,
  },

  // 15
  {
    input: "new Date('test')",
    output: "Invalid Date",
    printOutput: "Invalid Date",
    inspectable: true,
    variablesViewLabel: "Invalid Date",
  },
];

longString = initialString = null;

function test() {
  registerCleanupFunction(() => {
    DebuggerServer.LONG_STRING_LENGTH = LONG_STRING_LENGTH;
    DebuggerServer.LONG_STRING_INITIAL_LENGTH = LONG_STRING_INITIAL_LENGTH;
  });

  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole().then((hud) => {
      return checkOutputForInputs(hud, inputTests);
    }).then(finishTest);
  }, true);
}
