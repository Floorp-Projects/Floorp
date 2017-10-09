/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that any output created from calls to the console API comes before the
// echoed JavaScript.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  let jsterm = hud.jsterm;

  jsterm.clearOutput();
  jsterm.execute("console.log('foo', 'bar');");

  let [functionCall, consoleMessage, result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "console.log('foo', 'bar');",
      category: CATEGORY_INPUT,
    },
      {
        text: "foo bar",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      },
      {
        text: "undefined",
        category: CATEGORY_OUTPUT,
      }]
  });

  let fncallNode = [...functionCall.matched][0];
  let consoleMessageNode = [...consoleMessage.matched][0];
  let resultNode = [...result.matched][0];
  is(fncallNode.nextElementSibling, consoleMessageNode,
     "console.log() is followed by 'foo' 'bar'");
  is(consoleMessageNode.nextElementSibling, resultNode,
     "'foo' 'bar' is followed by undefined");
});
