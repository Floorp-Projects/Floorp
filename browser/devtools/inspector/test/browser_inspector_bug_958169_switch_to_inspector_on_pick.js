/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let doc;
let toolbox;

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload(evt) {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(startTests, content);
  }, true);

  content.location = "data:text/html,<p>Switch to inspector on pick</p>";
}

function startTests() {
  Task.spawn(function() {
    yield openToolbox();
    yield startPickerAndAssertSwitchToInspector();
    yield toolbox.highlighterUtils.stopPicker();

    finishTests();
  }).then(null, Cu.reportError);
}

function openToolbox() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.showToolbox(target, "webconsole").then(aToolbox => {
    toolbox = aToolbox;
  });
}

function startPickerAndAssertSwitchToInspector() {
  let deferred = promise.defer();

  let pickButton = toolbox.doc.querySelector("#command-button-pick");
  pickButton.click();
  toolbox.once("inspector-selected", () => {
    is(toolbox.currentToolId, "inspector", "Switched to the inspector");
    toolbox.getCurrentPanel().once("inspector-updated", deferred.resolve);
  });

  return deferred.promise;
}

function finishTests() {
  doc = toolbox = null;
  gBrowser.removeCurrentTab();
  finish();
}
