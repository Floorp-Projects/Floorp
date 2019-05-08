/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that textbox context menu elements are still displayed correctly after reopening
// the toolbox. Fixes https://bugzilla.mozilla.org/show_bug.cgi?id=1510182.

add_task(async function() {
  await addTab(`data:text/html;charset=utf-8,<div>test</div>`);

  info("Testing the textbox context menu a first time");
  const {toolbox, inspector} = await openInspector();
  await checkContextMenuOnSearchbox(inspector, toolbox);

  // Destroy the toolbox and try the context menu again with a new toolbox.
  await toolbox.destroy();

  info("Testing the textbox context menu after reopening the toolbox");
  const {toolbox: newToolbox, inspector: newInspector} = await openInspector();
  await checkContextMenuOnSearchbox(newInspector, newToolbox);
});

async function checkContextMenuOnSearchbox(inspector, toolbox) {
  // The same context menu is used for any text input.
  // Here we use the inspector searchbox for this test because it is always available.
  const searchbox = inspector.panelDoc.getElementById("inspector-searchbox");

  info("Simulating context click on the textbox and expecting the menu to open");
  const onContextMenu = toolbox.once("menu-open");
  synthesizeContextMenuEvent(searchbox);
  await onContextMenu;

  const textboxContextMenu = toolbox.getTextBoxContextMenu();
  info("Wait until menu items are rendered");
  const pasteElement = textboxContextMenu.querySelector("#editmenu-paste");
  await waitUntil(() => !!pasteElement.getAttribute("label"));

  is(pasteElement.getAttribute("label"), "Paste", "Paste is visible and localized");

  info("Closing the menu");
  const onContextMenuHidden = toolbox.once("menu-close");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onContextMenuHidden;
}
