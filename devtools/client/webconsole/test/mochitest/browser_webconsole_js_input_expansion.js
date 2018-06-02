/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the input box expands as the user types long lines.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-console.html";

add_task(async function() {
  const { jsterm, ui } = await openNewTabAndConsole(TEST_URI);
  ui.clearOutput();

  const { inputNode } = jsterm;
  const getInputHeight = () => inputNode.clientHeight;

  info("Get the initial (empty) height of the input");
  const emptyHeight = getInputHeight();

  info("Set some multiline text in the input");
  jsterm.setInputValue("hello\nworld\n");

  info("Get the new height of the input");
  const multiLineHeight = getInputHeight();

  ok(emptyHeight < multiLineHeight,
     `Height changed from ${emptyHeight} to ${multiLineHeight}`);

  info("Add some new, longer, multiline text");
  jsterm.setInputValue("row1\nrow2\nrow3\nrow4\nrow5\nrow6\nrow7\nrow8\nrow9\nrow10\n");

  info("Get the new height of the input");
  const longerHeight = getInputHeight();

  ok(multiLineHeight < longerHeight,
     `Height changed from ${multiLineHeight} to ${longerHeight}`);

  info("Test that the inputNode shrinks again");
  jsterm.setInputValue("");

  const backToEmptyHeight = getInputHeight();
  const diff = Math.abs(backToEmptyHeight - emptyHeight);
  ok(diff <= 1, "The input shrank back to its original size (within 1px)");
});
