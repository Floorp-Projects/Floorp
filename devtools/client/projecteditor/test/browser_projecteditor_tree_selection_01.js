/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test tree selection functionality

add_task(function* () {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  let TEMP_PATH = projecteditor.project.allPaths()[0];

  is(getTempFile("").path, TEMP_PATH, "Temp path is set correctly.");

  ok(projecteditor.currentEditor, "There is an editor for projecteditor");
  let resources = projecteditor.project.allResources();

  is(
    resources.map(r=>r.basename).join("|"),
    TEMP_FOLDER_NAME + "|css|styles.css|data|img|icons|128x128.png|16x16.png|32x32.png|vector.svg|fake.png|js|script.js|index.html|LICENSE|README.md",
    "Resources came through in proper order"
  );

  for (let i = 0; i < resources.length; i++) {
    yield selectFileFirstLoad(projecteditor, resources[i]);
  }
  for (let i = 0; i < resources.length; i++) {
    yield selectFileSubsequentLoad(projecteditor, resources[i]);
  }
  for (let i = 0; i < resources.length; i++) {
    yield selectFileSubsequentLoad(projecteditor, resources[i]);
  }
});

function* selectFileFirstLoad(projecteditor, resource) {
  ok(resource && resource.path, "A valid resource has been passed in for selection " + (resource && resource.path));
  projecteditor.projectTree.selectResource(resource);
  let container = projecteditor.projectTree.getViewContainer(resource);

  if (resource.isRoot) {
    ok(container.expanded, "The root directory is expanded by default.");
    container.line.click();
    ok(container.expanded, "Clicking on the line does not toggles expansion.");
    return;
  }
  if (resource.isDir) {
    ok(!container.expanded, "A directory is not expanded by default.");
    container.line.click();
    ok(container.expanded, "Clicking on the line toggles expansion.");
    container.line.click();
    ok(!container.expanded, "Clicking on the line toggles expansion.");
    return;
  }

  let [editorCreated, editorLoaded, editorActivated] = yield promise.all([
    onceEditorCreated(projecteditor),
    onceEditorLoad(projecteditor),
    onceEditorActivated(projecteditor)
  ]);

  is(editorCreated, projecteditor.currentEditor, "Editor has been created for " + resource.path);
  is(editorActivated, projecteditor.currentEditor, "Editor has been activated for " + resource.path);
  is(editorLoaded, projecteditor.currentEditor, "Editor has been loaded for " + resource.path);
}

function* selectFileSubsequentLoad(projecteditor, resource) {
  ok(resource && resource.path, "A valid resource has been passed in for selection " + (resource && resource.path));
  projecteditor.projectTree.selectResource(resource);

  if (resource.isDir) {
    return;
  }

  // Make sure text editors are focused immediately when selected.
  let focusPromise = promise.resolve();
  if (projecteditor.currentEditor.editor) {
    focusPromise = onEditorFocus(projecteditor.currentEditor);
  }

  // Only activated should fire the next time
  // (may add load() if we begin checking for changes from disk)
  let [editorActivated] = yield promise.all([
    onceEditorActivated(projecteditor)
  ]);

  is(editorActivated, projecteditor.currentEditor, "Editor has been activated for " + resource.path);

  yield focusPromise;
}

function onEditorFocus(editor) {
  let def = promise.defer();
  editor.on("focus", function focus() {
    editor.off("focus", focus);
    def.resolve();
  });
  return def.promise;
}
