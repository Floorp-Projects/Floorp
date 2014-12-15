/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test ProjectEditor basic functionality
add_task(function*() {
  let projecteditor = yield addProjectEditorTabForTempDirectory();
  let TEMP_PATH = projecteditor.project.allPaths()[0];
  is (getTempFile("").path, TEMP_PATH, "Temp path is set correctly.");

  is (projecteditor.project.allPaths().length, 1, "1 path is set");
  projecteditor.project.removeAllStores();
  is (projecteditor.project.allPaths().length, 0, "No paths are remaining");
});
