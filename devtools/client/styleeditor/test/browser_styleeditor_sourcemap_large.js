/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Covers the case from Bug 1128747, where loading a sourcemapped
// file prevents the correct editor from being selected on load,
// and causes a second iframe to be appended when the user clicks
// editor in the list.

const TESTCASE_URI = TEST_BASE_HTTPS + "sourcemaps-large.html";

add_task(async function() {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  await openEditor(ui.editors[0]);
  const iframes = ui.selectedEditor.details.querySelectorAll("iframe");

  is(iframes.length, 1, "There is only one editor iframe");
  ok(
    ui.selectedEditor.summary.classList.contains("splitview-active"),
    "The editor is selected"
  );
});

function openEditor(editor) {
  getLinkFor(editor).click();

  return editor.getSourceEditor();
}

function getLinkFor(editor) {
  return editor.summary.querySelector(".stylesheet-name");
}
