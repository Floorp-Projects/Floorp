/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test inspector's markup view search filter context menu works properly.

const TEST_INPUT = "h1";
const TEST_URI = "<h1>test filter context menu</h1>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {toolbox, inspector} = await openInspector();
  const {searchBox} = inspector;
  await selectNode("h1", inspector);

  emptyClipboard();

  info("Opening context menu");
  const onFocus = once(searchBox, "focus");
  searchBox.focus();
  await onFocus;

  let onContextMenuOpen = toolbox.once("menu-open");
  synthesizeContextMenuEvent(searchBox);
  await onContextMenuOpen;

  let searchContextMenu = toolbox.getTextBoxContextMenu();
  ok(searchContextMenu,
    "The search filter context menu is loaded in the computed view");

  let cmdUndo = searchContextMenu.querySelector("#editmenu-undo");
  let cmdDelete = searchContextMenu.querySelector("#editmenu-delete");
  let cmdSelectAll = searchContextMenu.querySelector("#editmenu-selectAll");
  let cmdCut = searchContextMenu.querySelector("#editmenu-cut");
  let cmdCopy = searchContextMenu.querySelector("#editmenu-copy");
  let cmdPaste = searchContextMenu.querySelector("#editmenu-paste");

  is(cmdUndo.getAttribute("disabled"), "true", "cmdUndo is disabled");
  is(cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is(cmdSelectAll.getAttribute("disabled"), "true", "cmdSelectAll is disabled");

  // Cut/Copy items are enabled in context menu even if there
  // is no selection. See also Bug 1303033, and 1317322
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");

  info("Closing context menu");
  let onContextMenuClose = toolbox.once("menu-close");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onContextMenuClose;

  info("Copy text in search field using the context menu");
  searchBox.setUserInput(TEST_INPUT);
  searchBox.select();
  searchBox.focus();

  onContextMenuOpen = toolbox.once("menu-open");
  synthesizeContextMenuEvent(searchBox);
  await onContextMenuOpen;

  searchContextMenu = toolbox.getTextBoxContextMenu();
  cmdCopy = searchContextMenu.querySelector("#editmenu-copy");
  await waitForClipboardPromise(() => cmdCopy.click(), TEST_INPUT);

  onContextMenuClose = toolbox.once("menu-close");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onContextMenuClose;

  info("Reopen context menu and check command properties");

  onContextMenuOpen = toolbox.once("menu-open");
  synthesizeContextMenuEvent(searchBox);
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

  const onContextMenuHidden = toolbox.once("menu-close");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onContextMenuHidden;

  // We have to wait for search query to avoid test failure.
  info("Waiting for search query to complete and getting the suggestions");
  await inspector.searchSuggestions._lastQuery;
});
