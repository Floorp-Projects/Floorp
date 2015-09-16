/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 1148759 - Test the webconsole can display promises inside objects.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,test for console and promises";

var {DebuggerServer} = require("devtools/server/main");

var LONG_STRING_LENGTH = DebuggerServer.LONG_STRING_LENGTH;
var LONG_STRING_INITIAL_LENGTH = DebuggerServer.LONG_STRING_INITIAL_LENGTH;
DebuggerServer.LONG_STRING_LENGTH = 100;
DebuggerServer.LONG_STRING_INITIAL_LENGTH = 50;

var longString = (new Array(DebuggerServer.LONG_STRING_LENGTH + 4)).join("a");
var initialString = longString.substring(0,
  DebuggerServer.LONG_STRING_INITIAL_LENGTH);

var inputTests = [
  // 0
  {
    input: "({ x: Promise.resolve() })",
    output: "Object { x: Promise }",
    printOutput: "[object Object]",
    inspectable: true,
    variablesViewLabel: "Object"
  },
];

longString = initialString = null;

function test() {
  requestLongerTimeout(2);

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
  longString = initialString = inputTests = null;
  finishTest();
}
