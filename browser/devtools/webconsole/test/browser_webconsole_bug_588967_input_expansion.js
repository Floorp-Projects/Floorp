/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-console.html";

var test = asyncTest(function* () {
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
