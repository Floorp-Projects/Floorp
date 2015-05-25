/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("destroy");

loadHelperScript("helper_edits.js");

// Test ProjectEditor basic functionality
add_task(function*() {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  let TEMP_PATH = projecteditor.project.allPaths()[0];

  is (getTempFile("").path, TEMP_PATH, "Temp path is set correctly.");

  ok (projecteditor.currentEditor, "There is an editor for projecteditor");
  let resources = projecteditor.project.allResources();

  for (let data of helperEditData) {
    info ("Processing " + data.path);
    let resource = resources.filter(r=>r.basename === data.basename)[0];
    yield selectFile(projecteditor, resource);
    yield testEditFile(projecteditor, getTempFile(data.path).path, data.newContent);
  }
});

function* testEditFile(projecteditor, filePath, newData) {
  info ("Testing file editing for: " + filePath);

  let initialData = yield getFileData(filePath);
  let editor = projecteditor.currentEditor;
  let resource = projecteditor.resourceFor(editor);
  let viewContainer= projecteditor.projectTree.getViewContainer(resource);
  let originalTreeLabel = viewContainer.label.textContent;

  is (resource.path, filePath, "Resource path is set correctly");
  is (editor.editor.getText(), initialData, "Editor is loaded with correct file contents");

  info ("Setting text in the editor and doing checks before saving");

  editor.editor.undo();
  editor.editor.undo();
  is (editor.editor.getText(), initialData, "Editor is still loaded with correct contents after undo");

  editor.editor.setText(newData);
  is (editor.editor.getText(), newData, "Editor has been filled with new data");
  is (viewContainer.label.textContent, "*" + originalTreeLabel, "Label is marked as changed");

  info ("Saving the editor and checking to make sure the file gets saved on disk");

  editor.save(resource);

  let savedResource = yield onceEditorSave(projecteditor);

  is (viewContainer.label.textContent, originalTreeLabel, "Label is unmarked as changed");
  is (savedResource.path, filePath, "The saved resouce path matches the original file path");
  is (savedResource, resource, "The saved resource is the same as the original resource");

  let savedData = yield getFileData(filePath);
  is (savedData, newData, "Data has been correctly saved to disk");

  info ("Finished checking saving for " + filePath);

}
