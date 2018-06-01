/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that StyleEditorUI.selectStyleSheet selects the correct sheet, line and
// column.

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

const LINE_NO = 5;
const COL_NO = 0;

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);
  const editor = ui.editors[1];

  info("Selecting style sheet #1.");
  await ui.selectStyleSheet(editor.styleSheet.href, LINE_NO);

  is(ui.selectedEditor, ui.editors[1], "Second editor is selected.");
  const {line, ch} = ui.selectedEditor.sourceEditor.getCursor();

  is(line, LINE_NO, "correct line selected");
  is(ch, COL_NO, "correct column selected");
});
