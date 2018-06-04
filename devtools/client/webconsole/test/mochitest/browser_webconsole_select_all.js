/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the global Firefox "Select All" functionality (e.g. Edit >
// Select All) works properly in the Web Console.

/* import-globals-from head.js */

const TEST_URI = "http://example.com/";

add_task(async function testSelectAll() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await testSelectionWhenMovingBetweenBoxes(hud);
  testBrowserMenuSelectAll(hud);
  await testContextMenuSelectAll(hud);
});

async function testSelectionWhenMovingBetweenBoxes(hud) {
  const jsterm = hud.jsterm;

  // Fill the console with some output.
  hud.ui.clearOutput();
  await jsterm.execute("1 + 2");
  await waitFor(() => findMessage(hud, "3"));
  await jsterm.execute("3 + 4");
  await waitFor(() => findMessage(hud, "7"));
  await jsterm.execute("5 + 6");
  await waitFor(() => findMessage(hud, "11"));
}

function testBrowserMenuSelectAll(hud) {
  const { ui } = hud;
  const outputContainer = ui.outputNode.querySelector(".webconsole-output");

  is(outputContainer.childNodes.length, 6,
    "the output node contains the expected number of children");

  // The focus is on the JsTerm, so we need to blur it for the copy comand to
  // work.
  outputContainer.ownerDocument.activeElement.blur();

  // Test that the global Firefox "Select All" functionality (e.g. Edit >
  // Select All) works properly in the Web Console.
  goDoCommand("cmd_selectAll");

  checkMessagesSelected(outputContainer);
  hud.iframeWindow.getSelection().removeAllRanges();
}

// Test the context menu "Select All" (which has a different code path) works
// properly as well.
async function testContextMenuSelectAll(hud) {
  const { ui } = hud;
  const outputContainer = ui.outputNode.querySelector(".webconsole-output");
  const contextMenu = await openContextMenu(hud, outputContainer);

  const selectAllItem = contextMenu.querySelector("#console-menu-select");
  ok(selectAllItem,
     `the context menu on the output node has a "Select All" item`);

  outputContainer.focus();
  selectAllItem.click();

  checkMessagesSelected(outputContainer);
  hud.iframeWindow.getSelection().removeAllRanges();
}

function checkMessagesSelected(outputContainer) {
  const selection = outputContainer.ownerDocument.getSelection();
  const messages = outputContainer.querySelectorAll(".message");

  for (const message of messages) {
    const selected = selection.containsNode(message);
    ok(selected, `Node containing text "${message.textContent}" was selected`);
  }
}
