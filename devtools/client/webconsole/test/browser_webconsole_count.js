/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that console.count() counts as expected. See bug 922208.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-count.html";

function test() {
  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab(TEST_URI);
    const hud = yield openConsole(tab);

    BrowserTestUtils.synthesizeMouseAtCenter("#local", {}, gBrowser.selectedBrowser);
    let messages = [];
    [
      "start",
      "<no label>: 2",
      "console.count() testcounter: 1",
      "console.count() testcounter: 2",
      "console.count() testcounter: 3",
      "console.count() testcounter: 4",
      "end"
    ].forEach(function(msg) {
      messages.push({
        text: msg,
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG
      });
    });
    messages.push({
      name: "Three local counts with no label and count=1",
      text: "<no label>: 1",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      count: 3
    });
    yield waitForMessages({
      webconsole: hud,
      messages: messages
    });

    hud.jsterm.clearOutput();

    BrowserTestUtils.synthesizeMouseAtCenter("#external", {}, gBrowser.selectedBrowser);
    messages = [];
    [
      "start",
      "console.count() testcounter: 5",
      "console.count() testcounter: 6",
      "end"
    ].forEach(function(msg) {
      messages.push({
        text: msg,
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG
      });
    });
    messages.push({
      name: "Two external counts with no label and count=1",
      text: "<no label>: 1",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      count: 2
    });
    yield waitForMessages({
      webconsole: hud,
      messages: messages
    });
  }
}
