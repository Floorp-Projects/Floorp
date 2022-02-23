/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// HTML inputs don't automatically get the 'edit' context menu, so we have
// a helper on the toolbox to do so. Make sure that shows menu items in the
// right state, and that it works for an input inside of a panel.

const URL = "data:text/html;charset=utf8,test for textbox context menu";
const textboxToolId = "testtool1";

registerCleanupFunction(() => {
  gDevTools.unregisterTool(textboxToolId);
});

add_task(async function checkMenuEntryStates() {
  // We have to disable CSP for this test otherwise the CSP of
  // about:devtools-toolbox will block the data: url.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.csp.enable", false],
      ["dom.security.skip_about_page_has_csp_assert", true],
    ],
  });

  info("Checking the state of edit menuitems with an empty clipboard");
  const toolbox = await openNewTabAndToolbox(URL, "inspector");

  emptyClipboard();

  // Make sure the focus is predictable.
  const inspector = toolbox.getPanel("inspector");
  const onFocus = once(inspector.searchBox, "focus");
  inspector.searchBox.focus();
  await onFocus;

  info("Opening context menu");
  const onContextMenuPopup = toolbox.once("menu-open");
  synthesizeContextMenuEvent(inspector.searchBox);
  await onContextMenuPopup;

  const textboxContextMenu = toolbox.getTextBoxContextMenu();
  ok(textboxContextMenu, "The textbox context menu is loaded in the toolbox");

  const cmdUndo = textboxContextMenu.querySelector("#editmenu-undo");
  const cmdDelete = textboxContextMenu.querySelector("#editmenu-delete");
  const cmdSelectAll = textboxContextMenu.querySelector("#editmenu-selectAll");
  const cmdCut = textboxContextMenu.querySelector("#editmenu-cut");
  const cmdCopy = textboxContextMenu.querySelector("#editmenu-copy");
  const cmdPaste = textboxContextMenu.querySelector("#editmenu-paste");

  is(cmdUndo.getAttribute("disabled"), "true", "cmdUndo is disabled");
  is(cmdDelete.getAttribute("disabled"), "true", "cmdDelete is disabled");
  is(cmdSelectAll.getAttribute("disabled"), "true", "cmdSelectAll is disabled");
  is(cmdCut.getAttribute("disabled"), "true", "cmdCut is disabled");
  is(cmdCopy.getAttribute("disabled"), "true", "cmdCopy is disabled");

  if (isWindows()) {
    // emptyClipboard only works on Windows (666254), assert paste only for this OS.
    is(cmdPaste.getAttribute("disabled"), "true", "cmdPaste is disabled");
  }

  const onContextMenuHidden = toolbox.once("menu-close");
  if (Services.prefs.getBoolPref("widget.macos.native-context-menus", false)) {
    info("Using hidePopup semantics because of macOS native context menus.");
    textboxContextMenu.hidePopup();
  } else {
    EventUtils.sendKey("ESCAPE", toolbox.win);
  }
  await onContextMenuHidden;
});

add_task(async function automaticallyBindTexbox() {
  // We have to disable CSP for this test otherwise the CSP of
  // about:devtools-toolbox will block the data: url.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.csp.enable", false],
      ["dom.security.skip_about_page_has_csp_assert", true],
    ],
  });

  info(
    "Registering a tool with an input field and making sure the context menu works"
  );
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

async function checkNonTextInput(input, toolbox) {
  let textboxContextMenu = toolbox.getTextBoxContextMenu();
  ok(!textboxContextMenu, "The menu is closed");

  info(
    "Simulating context click on the non text input and expecting no menu to open"
  );
  const eventBubbledUp = new Promise(resolve => {
    input.ownerDocument.addEventListener("contextmenu", resolve, {
      once: true,
    });
  });
  synthesizeContextMenuEvent(input);
  info("Waiting for event");
  await eventBubbledUp;

  textboxContextMenu = toolbox.getTextBoxContextMenu();
  ok(!textboxContextMenu, "The menu is still closed");
}

async function checkTextBox(textBox, toolbox) {
  let textboxContextMenu = toolbox.getTextBoxContextMenu();
  ok(!textboxContextMenu, "The menu is closed");

  info(
    "Simulating context click on the textbox and expecting the menu to open"
  );
  const onContextMenu = toolbox.once("menu-open");
  synthesizeContextMenuEvent(textBox);
  await onContextMenu;

  textboxContextMenu = toolbox.getTextBoxContextMenu();
  ok(textboxContextMenu, "The menu is now visible");

  info("Closing the menu");
  const onContextMenuHidden = toolbox.once("menu-close");
  if (Services.prefs.getBoolPref("widget.macos.native-context-menus", false)) {
    info("Using hidePopup semantics because of macOS native context menus.");
    textboxContextMenu.hidePopup();
  } else {
    EventUtils.sendKey("ESCAPE", toolbox.win);
  }
  await onContextMenuHidden;

  textboxContextMenu = toolbox.getTextBoxContextMenu();
  ok(!textboxContextMenu, "The menu is closed again");
}
