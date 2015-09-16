/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTestRoot = getRootDirectory(gTestPath);
var gBugWindow = null;
var gIterations = 5;

function onLoad() {
  gBugWindow.close();
}

function onUnload() {
  if (!gIterations) {
    gBugWindow = null;
    Services.obs.removeObserver(onLoad, "bug839193-loaded");
    Services.obs.removeObserver(onUnload, "bug839193-unloaded");

    window.focus();
    finish();
  } else {
    gBugWindow = window.openDialog(gTestRoot + "bug839193.xul");
    gIterations--;
  }
}

// This test is about leaks, which are handled by the test harness, so
// there are no actual checks here. Whether or not this test passes or fails
// will be apparent by the checks the harness performs.
function test() {
  waitForExplicitFinish();

  // This test relies on the test timing out in order to indicate failure so
  // let's add a dummy pass.
  ok(true, "Each test requires at least one pass, fail or todo so here is a pass.");

  Services.obs.addObserver(onLoad, "bug839193-loaded", false);
  Services.obs.addObserver(onUnload, "bug839193-unloaded", false);

  gBugWindow = window.openDialog(gTestRoot + "bug839193.xul");
}
