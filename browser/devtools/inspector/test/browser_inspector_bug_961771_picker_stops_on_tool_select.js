/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the highlighter's picker should be stopped when a different tool is
// selected

function test() {
  let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(setupTest, content);
  }, true);
  content.location = "data:text/html,testing the highlighter goes away on tool selection";

  function setupTest() {
    openInspector((aInspector, toolbox) => {
      let pickerStopped = toolbox.once("picker-stopped");

      Task.spawn(function() {
        info("Starting the inspector picker");
        yield toolbox.highlighterUtils.startPicker();
        info("Selecting another tool than the inspector in the toolbox");
        yield toolbox.selectNextTool();
        info("Waiting for the picker-stopped event to be fired")
        yield pickerStopped;
        ok(true, "picker-stopped event fired after switch tools, so picker is closed");
      }).then(null, ok.bind(null, false)).then(finishUp);
    });
  }

  function finishUp() {
    gBrowser.removeCurrentTab();
    finish();
  }
}
