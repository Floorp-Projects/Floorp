/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that selected sheet and cursor position is reset during navigation.

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";
const NEW_URI = TEST_BASE_HTTPS + "media.html";

const LINE_NO = 5;
const COL_NO = 3;

add_task(function* () {
  let { ui } = yield openStyleEditorForURL(TESTCASE_URI);
  loadCommonFrameScript();

  is(ui.editors.length, 2, "Two sheets present after load.");

  info("Selecting the second editor");
  yield ui.selectStyleSheet(ui.editors[1].styleSheet, LINE_NO, COL_NO);

  info("Navigating to another page.");
  executeInContent("devtools:test:navigate", { location: NEW_URI }, {}, false);

  info("Waiting for sheets to be loaded after navigation.");
  yield ui.once("stylesheets-reset");

  info("Waiting for source editor to be ready.");
  yield ui.editors[0].getSourceEditor();

  is(ui.selectedEditor, ui.editors[0], "first editor is selected");

  let {line, ch} = ui.selectedEditor.sourceEditor.getCursor();
  is(line, 0, "first line is selected");
  is(ch, 0, "first column is selected");
});
