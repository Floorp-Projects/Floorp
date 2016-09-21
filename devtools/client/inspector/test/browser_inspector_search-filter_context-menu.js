/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test inspector's markup view search filter context menu works properly.

const TEST_INPUT = "h1";
const TEST_URI = "<h1>test filter context menu</h1>";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector} = yield openInspector();
  let {searchBox} = inspector;
  yield selectNode("h1", inspector);

  let win = inspector.panelWin;
  let searchContextMenu = toolbox.textboxContextMenuPopup;
  ok(searchContextMenu,
    "The search filter context menu is loaded in the inspector");

  emptyClipboard();

  let cmdUndo = searchContextMenu.querySelector("[command=cmd_undo]");
  let cmdDelete = searchContextMenu.querySelector("[command=cmd_delete]");
  let cmdSelectAll = searchContextMenu.querySelector("[command=cmd_selectAll]");
  let cmdCut = searchContextMenu.querySelector("[command=cmd_cut]");
  let cmdCopy = searchContextMenu.querySelector("[command=cmd_copy]");
  let cmdPaste = searchContextMenu.querySelector("[command=cmd_paste]");

  info("Opening context menu");
  searchBox.focus();
  let onContextMenuPopup = once(searchContextMenu, "popupshowing");
  EventUtils.synthesizeMouse(searchBox, 2, 2,
    {type: "contextmenu", button: 2}, win);
  yield onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "true", "cmdUndo is disabled");
  is(cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is(cmdSelectAll.getAttribute("disabled"), "true", "cmdSelectAll is disabled");
  is(cmdCut.getAttribute("disabled"), "true", "cmdCut is disabled");
  is(cmdCopy.getAttribute("disabled"), "true", "cmdCopy is disabled");
  is(cmdPaste.getAttribute("disabled"), "true", "cmdPaste is disabled");

  info("Closing context menu");
  let onContextMenuHidden = once(searchContextMenu, "popuphidden");
  searchContextMenu.hidePopup();
  yield onContextMenuHidden;

  info("Copy text in search field using the context menu");
  searchBox.value = TEST_INPUT;
  searchBox.select();
  searchBox.focus();
  EventUtils.synthesizeMouse(searchBox, 2, 2,
    {type: "contextmenu", button: 2}, win);
  yield onContextMenuPopup;
  yield waitForClipboardPromise(() => cmdCopy.click(), TEST_INPUT);
  searchContextMenu.hidePopup();
  yield onContextMenuHidden;

  info("Reopen context menu and check command properties");
  EventUtils.synthesizeMouse(searchBox, 2, 2,
    {type: "contextmenu", button: 2}, win);
  yield onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "", "cmdUndo is enabled");
  is(cmdDelete.getAttribute("disabled"), "", "cmdDelete is enabled");
  is(cmdSelectAll.getAttribute("disabled"), "", "cmdSelectAll is enabled");
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");
});
