/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1148759 - Test the webconsole can display promises inside objects.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,test for console and promises";

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

function test() {
  requestLongerTimeout(2);

  Task.spawn(function* () {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    return checkOutputForInputs(hud, inputTests);
  }).then(finishUp);
}

function finishUp() {
  finishTest();
}
