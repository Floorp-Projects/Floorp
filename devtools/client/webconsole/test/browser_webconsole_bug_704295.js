/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for bug 704295

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  testCompletion(hud);
});

function testCompletion(hud) {
  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;

  // Test typing 'var d = 5;' and press RETURN
  jsterm.setInputValue("var d = ");
  EventUtils.synthesizeKey("5", {});
  EventUtils.synthesizeKey(";", {});
  is(input.value, "var d = 5;", "var d = 5;");
  is(jsterm.completeNode.value, "", "no completion");
  EventUtils.synthesizeKey("VK_RETURN", {});
  is(jsterm.completeNode.value, "", "clear completion on execute()");

  // Test typing 'var a = d' and press RETURN
  jsterm.setInputValue("var a = ");
  EventUtils.synthesizeKey("d", {});
  is(input.value, "var a = d", "var a = d");
  is(jsterm.completeNode.value, "", "no completion");
  EventUtils.synthesizeKey("VK_RETURN", {});
  is(jsterm.completeNode.value, "", "clear completion on execute()");
}
