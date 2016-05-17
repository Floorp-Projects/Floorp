/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadHelperScript("helper_edits.js");

// Test that a prompt shows up when requested if a file is unsaved.
add_task(function* () {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  ok(true, "ProjectEditor has loaded");

  let resources = projecteditor.project.allResources();
  yield selectFile(projecteditor, resources[2]);
  let editor = projecteditor.currentEditor;
  let originalText = editor.editor.getText();

  ok(!projecteditor.hasUnsavedResources, "There are no unsaved resources");
  ok(projecteditor.confirmUnsaved(), "When there are no unsaved changes, confirmUnsaved() is true");
  editor.editor.setText("bar");
  editor.editor.setText(originalText);
  ok(!projecteditor.hasUnsavedResources, "There are no unsaved resources");
  ok(projecteditor.confirmUnsaved(), "When an editor has changed but is still the original text, confirmUnsaved() is true");

  editor.editor.setText("bar");

  checkConfirmYes(projecteditor);
  checkConfirmNo(projecteditor);
});

function checkConfirmYes(projecteditor, container) {
  function confirmYes(aSubject) {
    info("confirm dialog observed as expected, going to click OK");
    Services.obs.removeObserver(confirmYes, "common-dialog-loaded");
    Services.obs.removeObserver(confirmYes, "tabmodal-dialog-loaded");
    aSubject.Dialog.ui.button0.click();
  }

  Services.obs.addObserver(confirmYes, "common-dialog-loaded", false);
  Services.obs.addObserver(confirmYes, "tabmodal-dialog-loaded", false);

  ok(projecteditor.hasUnsavedResources, "There are unsaved resources");
  ok(projecteditor.confirmUnsaved(), "When there are unsaved changes, clicking OK makes confirmUnsaved() true");
}

function checkConfirmNo(projecteditor, container) {
  function confirmNo(aSubject) {
    info("confirm dialog observed as expected, going to click cancel");
    Services.obs.removeObserver(confirmNo, "common-dialog-loaded");
    Services.obs.removeObserver(confirmNo, "tabmodal-dialog-loaded");
    aSubject.Dialog.ui.button1.click();
  }

  Services.obs.addObserver(confirmNo, "common-dialog-loaded", false);
  Services.obs.addObserver(confirmNo, "tabmodal-dialog-loaded", false);

  ok(projecteditor.hasUnsavedResources, "There are unsaved resources");
  ok(!projecteditor.confirmUnsaved(), "When there are unsaved changes, clicking cancel makes confirmUnsaved() false");
}
