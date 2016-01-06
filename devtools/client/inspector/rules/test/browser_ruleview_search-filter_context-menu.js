/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test rule view search filter context menu works properly.

const TEST_INPUT = "h1";
const TEST_URI = "<h1>test filter context menu</h1>";

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("h1", inspector);

  let win = view.styleWindow;
  let searchField = view.searchField;
  let searchContextMenu = toolbox.textboxContextMenuPopup;
  ok(searchContextMenu,
    "The search filter context menu is loaded in the rule view");

  let cmdUndo = searchContextMenu.querySelector("[command=cmd_undo]");
  let cmdDelete = searchContextMenu.querySelector("[command=cmd_delete]");
  let cmdSelectAll = searchContextMenu.querySelector("[command=cmd_selectAll]");
  let cmdCut = searchContextMenu.querySelector("[command=cmd_cut]");
  let cmdCopy = searchContextMenu.querySelector("[command=cmd_copy]");
  let cmdPaste = searchContextMenu.querySelector("[command=cmd_paste]");

  info("Opening context menu");
  let onContextMenuPopup = once(searchContextMenu, "popupshowing");
  EventUtils.synthesizeMouse(searchField, 2, 2,
    {type: "contextmenu", button: 2}, win);
  yield onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "true", "cmdUndo is disabled");
  is(cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is(cmdSelectAll.getAttribute("disabled"), "", "cmdSelectAll is enabled");
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "true", "cmdPaste is disabled");

  info("Closing context menu");
  let onContextMenuHidden = once(searchContextMenu, "popuphidden");
  searchContextMenu.hidePopup();
  yield onContextMenuHidden;

  info("Copy text in search field using the context menu");
  searchField.value = TEST_INPUT;
  searchField.select();
  EventUtils.synthesizeMouse(searchField, 2, 2,
    {type: "contextmenu", button: 2}, win);
  yield onContextMenuPopup;
  yield waitForClipboard(() => cmdCopy.click(), TEST_INPUT);
  searchContextMenu.hidePopup();
  yield onContextMenuHidden;

  info("Reopen context menu and check command properties");
  EventUtils.synthesizeMouse(searchField, 2, 2,
    {type: "contextmenu", button: 2}, win);
  yield onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "", "cmdUndo is enabled");
  is(cmdDelete.getAttribute("disabled"), "", "cmdDelete is enabled");
  is(cmdSelectAll.getAttribute("disabled"), "", "cmdSelectAll is enabled");
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");
});
