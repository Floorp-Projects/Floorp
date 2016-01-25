/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "http://example.com/";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();
  yield testSelectionWhenMovingBetweenBoxes(hud);
  performTestsAfterOutput(hud);
});

var testSelectionWhenMovingBetweenBoxes = Task.async(function*(hud) {
  let jsterm = hud.jsterm;

  // Fill the console with some output.
  jsterm.clearOutput();
  yield jsterm.execute("1 + 2");
  yield jsterm.execute("3 + 4");
  yield jsterm.execute("5 + 6");

  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "3",
      category: CATEGORY_OUTPUT,
    },
    {
      text: "7",
      category: CATEGORY_OUTPUT,
    },
    {
      text: "11",
      category: CATEGORY_OUTPUT,
    }],
  });
});

function performTestsAfterOutput(hud) {
  let outputNode = hud.outputNode;

  ok(outputNode.childNodes.length >= 3, "the output node has children after " +
     "executing some JavaScript");

  // Test that the global Firefox "Select All" functionality (e.g. Edit >
  // Select All) works properly in the Web Console.
  let commandController = hud.ui._commandController;
  ok(commandController != null, "the window has a command controller object");

  commandController.selectAll();

  let selectedCount = hud.ui.output.getSelectedMessages().length;
  is(selectedCount, outputNode.childNodes.length,
     "all console messages are selected after performing a regular browser " +
     "select-all operation");

  hud.iframeWindow.getSelection().removeAllRanges();

  // Test the context menu "Select All" (which has a different code path) works
  // properly as well.
  let contextMenuId = hud.ui.outputWrapper.getAttribute("context");
  let contextMenu = hud.ui.document.getElementById(contextMenuId);
  ok(contextMenu != null, "the output node has a context menu");

  let selectAllItem = contextMenu.querySelector("*[command='cmd_selectAll']");
  ok(selectAllItem != null,
     "the context menu on the output node has a \"Select All\" item");

  outputNode.focus();

  selectAllItem.doCommand();

  selectedCount = hud.ui.output.getSelectedMessages().length;
  is(selectedCount, outputNode.childNodes.length,
     "all console messages are selected after performing a select-all " +
     "operation from the context menu");

  hud.iframeWindow.getSelection().removeAllRanges();
}
