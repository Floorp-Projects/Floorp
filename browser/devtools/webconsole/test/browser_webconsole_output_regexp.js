/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the webconsole output for various types of objects.

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-console-output-regexp.html";

var dateNow = Date.now();

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
  Task.spawn(function*() {
    let {tab} = yield loadTab(TEST_URI);
    let hud = yield openConsole(tab);
    return checkOutputForInputs(hud, inputTests);
  }).then(finishUp);
}

function finishUp() {
  inputTests = dateNow = null;
  finishTest();
}
