/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that projecteditor can be initialized.

function test() {
  info("Initializing projecteditor");
  addProjectEditorTab().then((projecteditor) => {
    ok(projecteditor, "Load callback has been called");
    ok(projecteditor.shells, "ProjectEditor has shells");
    ok(projecteditor.project, "ProjectEditor has a project");
    finish();
  });
}

