/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that selected sheet and cursor position persists during reload.

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

const LINE_NO = 5;
const COL_NO = 3;

add_task(async function () {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, 2, "Two sheets present after load.");

  info("Selecting the second editor");
  await ui.selectStyleSheet(ui.editors[1].styleSheet, LINE_NO, COL_NO);
  const selectedStyleSheetIndex = ui.editors[1].styleSheet.styleSheetIndex;

  await reloadPageAndWaitForStyleSheets(ui, 2);

  info("Waiting for source editor to be ready.");
  const newEditor = findEditor(ui, selectedStyleSheetIndex);
  await newEditor.getSourceEditor();

  is(
    ui.selectedEditor,
    newEditor,
    "Editor of stylesheet that has styleSheetIndex we selected is selected after reload"
  );

  const { line, ch } = ui.selectedEditor.sourceEditor.getCursor();
  is(line, LINE_NO, "correct line selected");
  is(ch, COL_NO, "correct column selected");
});

function findEditor(ui, styleSheetIndex) {
  return ui.editors.find(
    editor => editor.styleSheet.styleSheetIndex === styleSheetIndex
  );
}
