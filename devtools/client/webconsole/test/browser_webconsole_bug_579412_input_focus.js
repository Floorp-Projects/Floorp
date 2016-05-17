/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the input field is focused when the console is opened.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  let inputNode = hud.jsterm.inputNode;
  ok(inputNode.getAttribute("focused"), "input node is focused");
});
