/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test file rename functionality

add_task(function* () {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  ok(true, "ProjectEditor has loaded");

  let root = [...projecteditor.project.allStores()][0].root;
  is(root.path, TEMP_PATH, "The root store is set to the correct temp path.");
  for (let child of root.children) {
    yield renameWithContextMenu(projecteditor,
      projecteditor.projectTree.getViewContainer(child), ".renamed");
  }
});
