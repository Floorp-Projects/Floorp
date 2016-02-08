/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  testInputExpansion(hud);
});

function testInputExpansion(hud) {
  let input = hud.jsterm.inputNode;

  input.focus();

  is(input.getAttribute("multiline"), "true", "multiline is enabled");

  let ordinaryHeight = input.clientHeight;

  // Tests if the inputNode expands.
  input.value = "hello\nworld\n";
  let length = input.value.length;
  input.selectionEnd = length;
  input.selectionStart = length;
  // Performs an "d". This will trigger/test for the input event that should
  // change the height of the inputNode.
  EventUtils.synthesizeKey("d", {});
  ok(input.clientHeight > ordinaryHeight, "the input expanded");

  // Test if the inputNode shrinks again.
  input.value = "";
  EventUtils.synthesizeKey("d", {});
  is(input.clientHeight, ordinaryHeight, "the input's height is normal again");

  input = length = null;
}
