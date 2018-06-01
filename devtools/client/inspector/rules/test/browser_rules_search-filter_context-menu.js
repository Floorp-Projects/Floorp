/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test rule view search filter context menu works properly.

const TEST_INPUT = "h1";
const TEST_URI = "<h1>test filter context menu</h1>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {toolbox, inspector, view} = await openRuleView();
  await selectNode("h1", inspector);

  const win = view.styleWindow;
  const searchField = view.searchField;
  const searchContextMenu = toolbox.textBoxContextMenuPopup;
  ok(searchContextMenu,
    "The search filter context menu is loaded in the rule view");

  const cmdUndo = searchContextMenu.querySelector("[command=cmd_undo]");
  const cmdDelete = searchContextMenu.querySelector("[command=cmd_delete]");
  const cmdSelectAll = searchContextMenu.querySelector("[command=cmd_selectAll]");
  const cmdCut = searchContextMenu.querySelector("[command=cmd_cut]");
  const cmdCopy = searchContextMenu.querySelector("[command=cmd_copy]");
  const cmdPaste = searchContextMenu.querySelector("[command=cmd_paste]");

  info("Opening context menu");

  emptyClipboard();

  const onFocus = once(searchField, "focus");
  searchField.focus();
  await onFocus;

  const onContextMenuPopup = once(searchContextMenu, "popupshowing");
  EventUtils.synthesizeMouse(searchField, 2, 2,
    {type: "contextmenu", button: 2}, win);
  await onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "true", "cmdUndo is disabled");
  is(cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is(cmdSelectAll.getAttribute("disabled"), "true", "cmdSelectAll is disabled");

  // Cut/Copy/Paste items are enabled in context menu even if there is no
  // selection. See also Bug 1303033, and 1317322
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");

  info("Closing context menu");
  const onContextMenuHidden = once(searchContextMenu, "popuphidden");
  searchContextMenu.hidePopup();
  await onContextMenuHidden;

  info("Copy text in search field using the context menu");
  searchField.setUserInput(TEST_INPUT);
  searchField.select();
  EventUtils.synthesizeMouse(searchField, 2, 2,
    {type: "contextmenu", button: 2}, win);
  await onContextMenuPopup;
  await waitForClipboardPromise(() => cmdCopy.click(), TEST_INPUT);
  searchContextMenu.hidePopup();
  await onContextMenuHidden;

  info("Reopen context menu and check command properties");
  EventUtils.synthesizeMouse(searchField, 2, 2,
    {type: "contextmenu", button: 2}, win);
  await onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "", "cmdUndo is enabled");
  is(cmdDelete.getAttribute("disabled"), "", "cmdDelete is enabled");
  is(cmdSelectAll.getAttribute("disabled"), "", "cmdSelectAll is enabled");
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");
});
