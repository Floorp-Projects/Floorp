/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadHelperScript("helper_edits.js");

// Test ProjectEditor reaction to external changes (made outside of the)
// editor.
let test = asyncTest(function*() {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  let TEMP_PATH = projecteditor.project.allPaths()[0];

  is (getTempFile("").path, TEMP_PATH, "Temp path is set correctly.");

  ok (projecteditor.currentEditor, "There is an editor for projecteditor");
  let resources = projecteditor.project.allResources();

  for (let data of helperEditData) {
    info ("Processing " + data.path);
    let resource = resources.filter(r=>r.basename === data.basename)[0];
    yield selectFile(projecteditor, resource);
    yield testChangeFileExternally(projecteditor, getTempFile(data.path).path, data.newContent);
  }
});

function testChangeFileExternally(projecteditor, filePath, newData) {
  info ("Testing file external changes for: " + filePath);

  let editor = projecteditor.currentEditor;
  let resource = projecteditor.resourceFor(editor);
  let initialData = yield getFileData(filePath);

  is (resource.path, filePath, "Resource path is set correctly");
  is (editor.editor.getText(), initialData, "Editor is loaded with correct file contents");

  info ("Editor has been selected, writing to file externally");
  yield writeToFile(resource.path, newData);

  info ("Selecting another resource, then reselecting this one");
  projecteditor.projectTree.selectResource(resource.store.root);
  yield onceEditorActivated(projecteditor);
  projecteditor.projectTree.selectResource(resource);
  yield onceEditorActivated(projecteditor);

  let editor = projecteditor.currentEditor;
  info ("Checking to make sure the editor is now populated correctly");
  is (editor.editor.getText(), newData, "Editor has been updated with correct file contents");

  info ("Finished checking saving for " + filePath);
}
