/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
