/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that context menu for CodeMirror is properly localized.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html><p>test page</p>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;

  const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);

  // Open context menu and wait until it's visible
  const element = jsterm.node.querySelector(".CodeMirror-wrap");
  const menuPopup = await openTextBoxContextMenu(toolbox, element);

  // Check label of the 'undo' menu item.
  const undoMenuItem = menuPopup.querySelector("#editmenu-undo");
  await waitUntil(() => !!undoMenuItem.getAttribute("label"));

  is(
    undoMenuItem.getAttribute("label"),
    "Undo",
    "Undo is visible and localized"
  );
});

async function openTextBoxContextMenu(toolbox, element) {
  const onConsoleMenuOpened = toolbox.once("menu-open");
  synthesizeContextMenuEvent(element);
  await onConsoleMenuOpened;
  return toolbox.getTextBoxContextMenu();
}
