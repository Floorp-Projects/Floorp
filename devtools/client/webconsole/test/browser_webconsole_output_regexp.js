/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the webconsole output for various types of objects.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-output-regexp.html";

var inputTests = [
  // 0
  {
    input: "/foo/igym",
    output: "/foo/gimy",
    printOutput: "Error: source called",
    inspectable: true,
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
  inputTests = null;
  finishTest();
}
