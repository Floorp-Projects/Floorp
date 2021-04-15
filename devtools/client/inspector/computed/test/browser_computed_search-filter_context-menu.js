/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests computed view search filter context menu works properly.

const TEST_INPUT = "h1";

const TEST_URI = "<h1>test filter context menu</h1>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { toolbox, inspector, view } = await openComputedView();
  await selectNode("h1", inspector);

  const searchField = view.searchField;

  info("Opening context menu");

  emptyClipboard();

  const onFocus = once(searchField, "focus");
  searchField.focus();
  await onFocus;

  let onContextMenuOpen = toolbox.once("menu-open");
  synthesizeContextMenuEvent(searchField);
  await onContextMenuOpen;

  let searchContextMenu = toolbox.getTextBoxContextMenu();
  ok(
    searchContextMenu,
    "The search filter context menu is loaded in the computed view"
  );

  let cmdUndo = searchContextMenu.querySelector("#editmenu-undo");
  let cmdDelete = searchContextMenu.querySelector("#editmenu-delete");
  let cmdSelectAll = searchContextMenu.querySelector("#editmenu-selectAll");
  let cmdCut = searchContextMenu.querySelector("#editmenu-cut");
  let cmdCopy = searchContextMenu.querySelector("#editmenu-copy");
  let cmdPaste = searchContextMenu.querySelector("#editmenu-paste");

  is(cmdUndo.getAttribute("disabled"), "true", "cmdUndo is disabled");
  is(cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is(cmdSelectAll.getAttribute("disabled"), "true", "cmdSelectAll is disabled");

  is(cmdCut.getAttribute("disabled"), "true", "cmdCut is disabled");
  is(cmdCopy.getAttribute("disabled"), "true", "cmdCopy is disabled");
  if (isWindows()) {
    // emptyClipboard only works on Windows (666254), assert paste only for this OS.
    is(cmdPaste.getAttribute("disabled"), "true", "cmdPaste is disabled");
  }

  info("Closing context menu");
  let onContextMenuClose = toolbox.once("menu-close");
  searchContextMenu.hidePopup();
  await onContextMenuClose;

  info("Copy text in search field using the context menu");
  searchField.setUserInput(TEST_INPUT);
  searchField.select();

  onContextMenuOpen = toolbox.once("menu-open");
  synthesizeContextMenuEvent(searchField);
  await onContextMenuOpen;

  searchContextMenu = toolbox.getTextBoxContextMenu();
  cmdCopy = searchContextMenu.querySelector("#editmenu-copy");
  onContextMenuClose = toolbox.once("menu-close");
  await waitForClipboardPromise(
    () => searchContextMenu.activateItem(cmdCopy),
    TEST_INPUT
  );
  await onContextMenuClose;

  info("Reopen context menu and check command properties");

  onContextMenuOpen = toolbox.once("menu-open");
  synthesizeContextMenuEvent(searchField);
  await onContextMenuOpen;

  searchContextMenu = toolbox.getTextBoxContextMenu();
  cmdUndo = searchContextMenu.querySelector("#editmenu-undo");
  cmdDelete = searchContextMenu.querySelector("#editmenu-delete");
  cmdSelectAll = searchContextMenu.querySelector("#editmenu-selectAll");
  cmdCut = searchContextMenu.querySelector("#editmenu-cut");
  cmdCopy = searchContextMenu.querySelector("#editmenu-copy");
  cmdPaste = searchContextMenu.querySelector("#editmenu-paste");

  is(cmdUndo.getAttribute("disabled"), "", "cmdUndo is enabled");
  is(cmdDelete.getAttribute("disabled"), "", "cmdDelete is enabled");
  is(cmdSelectAll.getAttribute("disabled"), "", "cmdSelectAll is enabled");
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");

  onContextMenuClose = toolbox.once("menu-close");
  searchContextMenu.hidePopup();
  await onContextMenuClose;
});
