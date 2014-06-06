/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test ProjectEditor basic functionality
let test = asyncTest(function*() {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  let TEMP_PATH = projecteditor.project.allPaths()[0];

  is (getTempFile("").path, TEMP_PATH, "Temp path is set correctly.");

  ok (projecteditor.currentEditor, "There is an editor for projecteditor");
  let resources = projecteditor.project.allResources();

  resources.forEach((r, i) => {
    console.log("Resource detected", r.path, i);
  });

  let stylesCss = resources.filter(r=>r.basename === "styles.css")[0];
  yield selectFile(projecteditor, stylesCss);
  yield testEditFile(projecteditor, getTempFile("css/styles.css").path, "body,html { color: orange; }");

  let indexHtml = resources.filter(r=>r.basename === "index.html")[0];
  yield selectFile(projecteditor, indexHtml);
  yield testEditFile(projecteditor, getTempFile("index.html").path, "<h1>Changed Content Again</h1>");

  let license = resources.filter(r=>r.basename === "LICENSE")[0];
  yield selectFile(projecteditor, license);
  yield testEditFile(projecteditor, getTempFile("LICENSE").path, "My new license");

  let readmeMd = resources.filter(r=>r.basename === "README.md")[0];
  yield selectFile(projecteditor, readmeMd);
  yield testEditFile(projecteditor, getTempFile("README.md").path, "My new license");

  let scriptJs = resources.filter(r=>r.basename === "script.js")[0];
  yield selectFile(projecteditor, scriptJs);
  yield testEditFile(projecteditor, getTempFile("js/script.js").path, "alert('hi')");

  let vectorSvg = resources.filter(r=>r.basename === "vector.svg")[0];
  yield selectFile(projecteditor, vectorSvg);
  yield testEditFile(projecteditor, getTempFile("img/icons/vector.svg").path, "<svg></svg>");
});

function selectFile (projecteditor, resource) {
  ok (resource && resource.path, "A valid resource has been passed in for selection " + (resource && resource.path));
  projecteditor.projectTree.selectResource(resource);

  if (resource.isDir) {
    return;
  }

  let [editorActivated] = yield promise.all([
    onceEditorActivated(projecteditor)
  ]);

  is (editorActivated, projecteditor.currentEditor,  "Editor has been activated for " + resource.path);
}

function testEditFile(projecteditor, filePath, newData) {
  info ("Testing file editing for: " + filePath);

  let initialData = yield getFileData(filePath);
  let editor = projecteditor.currentEditor;
  let resource = projecteditor.resourceFor(editor);
  let viewContainer= projecteditor.projectTree.getViewContainer(resource);
  let originalTreeLabel = viewContainer.label.textContent;

  is (resource.path, filePath, "Resource path is set correctly");
  is (editor.editor.getText(), initialData, "Editor is loaded with correct file contents");

  info ("Setting text in the editor and doing checks before saving");

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
