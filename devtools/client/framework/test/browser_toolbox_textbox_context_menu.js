/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// HTML inputs don't automatically get the 'edit' context menu, so we have
// a helper on the toolbox to do so. Make sure that shows menu items in the
// right state, and that it works for an input inside of a panel.

const URL = "data:text/html;charset=utf8,test for textbox context menu";
const textboxToolId = "test-tool-1";

registerCleanupFunction(() => {
  gDevTools.unregisterTool(textboxToolId);
});

add_task(async function checkMenuEntryStates() {
  info("Checking the state of edit menuitems with an empty clipboard");
  const toolbox = await openNewTabAndToolbox(URL, "inspector");
  const textboxContextMenu = toolbox.textBoxContextMenuPopup;

  emptyClipboard();

  // Make sure the focus is predictable.
  const inspector = toolbox.getPanel("inspector");
  const onFocus = once(inspector.searchBox, "focus");
  inspector.searchBox.focus();
  await onFocus;

  ok(textboxContextMenu, "The textbox context menu is loaded in the toolbox");

  const cmdUndo = textboxContextMenu.querySelector("[command=cmd_undo]");
  const cmdDelete = textboxContextMenu.querySelector("[command=cmd_delete]");
  const cmdSelectAll = textboxContextMenu.querySelector("[command=cmd_selectAll]");
  const cmdCut = textboxContextMenu.querySelector("[command=cmd_cut]");
  const cmdCopy = textboxContextMenu.querySelector("[command=cmd_copy]");
  const cmdPaste = textboxContextMenu.querySelector("[command=cmd_paste]");

  info("Opening context menu");

  const onContextMenuPopup = once(textboxContextMenu, "popupshowing");
  textboxContextMenu.openPopupAtScreen(0, 0, true);
  await onContextMenuPopup;

  is(cmdUndo.getAttribute("disabled"), "true", "cmdUndo is disabled");
  is(cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is(cmdSelectAll.getAttribute("disabled"), "true", "cmdSelectAll is disabled");

  // Cut/Copy/Paste items are enabled in context menu even if there
  // is no selection. See also Bug 1303033, and 1317322
  is(cmdCut.getAttribute("disabled"), "", "cmdCut is enabled");
  is(cmdCopy.getAttribute("disabled"), "", "cmdCopy is enabled");
  is(cmdPaste.getAttribute("disabled"), "", "cmdPaste is enabled");
});

add_task(async function automaticallyBindTexbox() {
  info("Registering a tool with an input field and making sure the context menu works");
  gDevTools.registerTool({
    id: textboxToolId,
    isTargetSupported: () => true,
    url: `data:text/html;charset=utf8,<input /><input type='text' />
            <input type='search' /><textarea></textarea><input type='radio' />`,
    label: "Context menu works without tool intervention",
    build: function(iframeWindow, toolbox) {
      this.panel = createTestPanel(iframeWindow, toolbox);
      return this.panel.open();
    },
  });

  const toolbox = await openNewTabAndToolbox(URL, textboxToolId);
  is(toolbox.currentToolId, textboxToolId, "The custom tool has been opened");

  const doc = toolbox.getCurrentPanel().document;
  await checkTextBox(doc.querySelector("input[type=text]"), toolbox);
  await checkTextBox(doc.querySelector("textarea"), toolbox);
  await checkTextBox(doc.querySelector("input[type=search]"), toolbox);
  await checkTextBox(doc.querySelector("input:not([type])"), toolbox);
  await checkNonTextInput(doc.querySelector("input[type=radio]"), toolbox);
});

async function checkNonTextInput(input, {textBoxContextMenuPopup}) {
  is(textBoxContextMenuPopup.state, "closed", "The menu is closed");

  info("Simulating context click on the non text input and expecting no menu to open");
  const eventBubbledUp = new Promise(resolve => {
    input.ownerDocument.addEventListener("contextmenu", resolve, { once: true });
  });
  EventUtils.synthesizeMouse(input, 2, 2, {type: "contextmenu", button: 2},
                             input.ownerDocument.defaultView);
  info("Waiting for event");
  await eventBubbledUp;
  is(textBoxContextMenuPopup.state, "closed", "The menu is still closed");
}

async function checkTextBox(textBox, {textBoxContextMenuPopup}) {
  is(textBoxContextMenuPopup.state, "closed", "The menu is closed");

  info("Simulating context click on the textbox and expecting the menu to open");
  const onContextMenu = once(textBoxContextMenuPopup, "popupshown");
  EventUtils.synthesizeMouse(textBox, 2, 2, {type: "contextmenu", button: 2},
                             textBox.ownerDocument.defaultView);
  await onContextMenu;

  is(textBoxContextMenuPopup.state, "open", "The menu is now visible");

  info("Closing the menu");
  const onContextMenuHidden = once(textBoxContextMenuPopup, "popuphidden");
  textBoxContextMenuPopup.hidePopup();
  await onContextMenuHidden;

  is(textBoxContextMenuPopup.state, "closed", "The menu is closed again");
}
