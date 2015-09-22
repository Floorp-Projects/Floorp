/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the input box expands as the user types long lines.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  let input = hud.jsterm.inputNode;
  input.focus();

  is(input.getAttribute("multiline"), "true", "multiline is enabled");
  // Tests if the inputNode expands.
  input.value = "hello\nworld\n";
  let length = input.value.length;
  input.selectionEnd = length;
  input.selectionStart = length;
  function getHeight() {
    return input.clientHeight;
  }
  let initialHeight = getHeight();
  // Performs an "d". This will trigger/test for the input event that should
  // change the "row" attribute of the inputNode.
  EventUtils.synthesizeKey("d", {});
  let newHeight = getHeight();
  ok(initialHeight < newHeight, "Height changed: " + newHeight);

  // Add some more rows. Tests for the 8 row limit.
  input.value = "row1\nrow2\nrow3\nrow4\nrow5\nrow6\nrow7\nrow8\nrow9\nrow10\n";
  length = input.value.length;
  input.selectionEnd = length;
  input.selectionStart = length;
  EventUtils.synthesizeKey("d", {});
  let newerHeight = getHeight();

  ok(newerHeight > newHeight, "height changed: " + newerHeight);

  // Test if the inputNode shrinks again.
  input.value = "";
  EventUtils.synthesizeKey("d", {});
  let height = getHeight();
  info("height: " + height);
  info("initialHeight: " + initialHeight);
  let finalHeightDifference = Math.abs(initialHeight - height);
  ok(finalHeightDifference <= 1, "height shrank to original size within 1px");
});
