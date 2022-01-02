/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the style sheet list can be navigated with keyboard.

const TESTCASE_URI = TEST_BASE_HTTP + "four.html";

add_task(async function() {
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);

  info("Waiting for source editor to load.");
  await ui.editors[0].getSourceEditor();

  const selected = ui.once("editor-selected");

  info("Testing keyboard navigation on the sheet list.");
  testKeyboardNavigation(ui.editors[0], panel);

  info("Waiting for editor #2 to be selected due to keyboard navigation.");
  await selected;

  ok(ui.editors[2].sourceEditor.hasFocus(), "Editor #2 has focus.");
});

function getStylesheetNameLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}

function testKeyboardNavigation(editor, panel) {
  const panelWindow = panel.panelWindow;
  const ui = panel.UI;
  waitForFocus(function() {
    const summary = editor.summary;
    EventUtils.synthesizeMouseAtCenter(summary, {}, panelWindow);

    let item = getStylesheetNameLinkFor(ui.editors[0]);
    is(
      panelWindow.document.activeElement,
      item,
      "editor 0 item is the active element"
    );

    EventUtils.synthesizeKey("VK_DOWN", {}, panelWindow);
    item = getStylesheetNameLinkFor(ui.editors[1]);
    is(
      panelWindow.document.activeElement,
      item,
      "editor 1 item is the active element"
    );

    EventUtils.synthesizeKey("VK_HOME", {}, panelWindow);
    item = getStylesheetNameLinkFor(ui.editors[0]);
    is(
      panelWindow.document.activeElement,
      item,
      "fist editor item is the active element"
    );

    EventUtils.synthesizeKey("VK_END", {}, panelWindow);
    item = getStylesheetNameLinkFor(ui.editors[3]);
    is(
      panelWindow.document.activeElement,
      item,
      "last editor item is the active element"
    );

    EventUtils.synthesizeKey("VK_UP", {}, panelWindow);
    item = getStylesheetNameLinkFor(ui.editors[2]);
    is(
      panelWindow.document.activeElement,
      item,
      "editor 2 item is the active element"
    );

    EventUtils.synthesizeKey("VK_RETURN", {}, panelWindow);
    // this will attach and give focus editor 2
  }, panelWindow);
}
