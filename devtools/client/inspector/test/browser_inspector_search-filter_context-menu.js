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

  const win = inspector.panelWin;
  const searchContextMenu = toolbox.textBoxContextMenuPopup;
  ok(searchContextMenu,
    "The search filter context menu is loaded in the inspector");

  const cmdUndo = searchContextMenu.querySelector("[command=cmd_undo]");
  const cmdDelete = searchContextMenu.querySelector("[command=cmd_delete]");
  const cmdSelectAll = searchContextMenu.querySelector("[command=cmd_selectAll]");
  const cmdCut = searchContextMenu.querySelector("[command=cmd_cut]");
  const cmdCopy = searchContextMenu.querySelector("[command=cmd_copy]");
  const cmdPaste = searchContextMenu.querySelector("[command=cmd_paste]");

  emptyClipboard();

  info("Opening context menu");
  const onFocus = once(searchBox, "focus");
  searchBox.focus();
  await onFocus;

  const onContextMenuPopup = once(searchContextMenu, "popupshowing");
  EventUtils.synthesizeMouse(searchBox, 2, 2,
    {type: "contextmenu", button: 2}, win);
  await onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "true", "cmdUndo is disabled");
  is(cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is(cmdSelectAll.getAttribute("disabled"), "true", "cmdSelectAll is disabled");

  // Cut/Copy items are enabled in context menu even if there
  // is no selection. See also Bug 1303033, and 1317322
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");

  info("Closing context menu");
  const onContextMenuHidden = once(searchContextMenu, "popuphidden");
  searchContextMenu.hidePopup();
  await onContextMenuHidden;

  info("Copy text in search field using the context menu");
  searchBox.setUserInput(TEST_INPUT);
  searchBox.select();
  searchBox.focus();
  EventUtils.synthesizeMouse(searchBox, 2, 2,
    {type: "contextmenu", button: 2}, win);
  await onContextMenuPopup;
  await waitForClipboardPromise(() => cmdCopy.click(), TEST_INPUT);
  searchContextMenu.hidePopup();
  await onContextMenuHidden;

  info("Reopen context menu and check command properties");
  EventUtils.synthesizeMouse(searchBox, 2, 2,
    {type: "contextmenu", button: 2}, win);
  await onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "", "cmdUndo is enabled");
  is(cmdDelete.getAttribute("disabled"), "", "cmdDelete is enabled");
  is(cmdSelectAll.getAttribute("disabled"), "", "cmdSelectAll is enabled");
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");

  // We have to wait for search query to avoid test failure.
  info("Waiting for search query to complete and getting the suggestions");
  await inspector.searchSuggestions._lastQuery;
});
