/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the webconsole output for various types of objects.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
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
