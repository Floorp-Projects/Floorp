/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that any output created from calls to the console API comes after the
// echoed JavaScript.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  let jsterm = hud.jsterm;

  jsterm.clearOutput();
  jsterm.execute("console.log('foo', 'bar');");

  let [functionCall, result, consoleMessage] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "console.log('foo', 'bar');",
      category: CATEGORY_INPUT,
    },
    {
      text: "undefined",
      category: CATEGORY_OUTPUT,
    },
    {
      text: "foo bar",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    }],
  });

  let fncallNode = [...functionCall.matched][0];
  let resultNode = [...result.matched][0];
  let consoleMessageNode = [...consoleMessage.matched][0];
  is(fncallNode.nextElementSibling, resultNode,
     "console.log() is followed by undefined");
  is(resultNode.nextElementSibling, consoleMessageNode,
     "undefined is followed by 'foo' 'bar'");
});
