/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that clicking the pick button switches the toolbox to the inspector
// panel.

const TEST_URI = "data:text/html;charset=UTF-8," +
  "<p>Switch to inspector on pick</p>";

let test = asyncTest(function* () {
  let tab = yield addTab(TEST_URI);
  let toolbox = yield openToolbox(tab);

  yield startPickerAndAssertSwitchToInspector(toolbox);

  info("Stoppping element picker.");
  yield toolbox.highlighterUtils.stopPicker();
});

function openToolbox(tab) {
  info("Opening webconsole.");
  let target = TargetFactory.forTab(tab);
  return gDevTools.showToolbox(target, "webconsole");
}

function* startPickerAndAssertSwitchToInspector(toolbox) {
  info("Clicking element picker button.");
  let pickButton = toolbox.doc.querySelector("#command-button-pick");
  pickButton.click();

  info("Waiting for inspector to be selected.");
  yield toolbox.once("inspector-selected");
  is(toolbox.currentToolId, "inspector", "Switched to the inspector");

  info("Waiting for inspector to update.");
  yield toolbox.getCurrentPanel().once("inspector-updated");
}
