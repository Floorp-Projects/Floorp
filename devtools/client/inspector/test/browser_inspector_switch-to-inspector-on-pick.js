/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that clicking the pick button switches the toolbox to the inspector
// panel.

const TEST_URI = "data:text/html;charset=UTF-8," +
  "<p>Switch to inspector on pick</p>";

add_task(async function() {
  const tab = await addTab(TEST_URI);
  const toolbox = await openToolbox(tab);

  await startPickerAndAssertSwitchToInspector(toolbox);

  info("Stoppping element picker.");
  await toolbox.highlighterUtils.stopPicker();
});

function openToolbox(tab) {
  info("Opening webconsole.");
  const target = TargetFactory.forTab(tab);
  return gDevTools.showToolbox(target, "webconsole");
}

async function startPickerAndAssertSwitchToInspector(toolbox) {
  info("Clicking element picker button.");
  const pickButton = toolbox.doc.querySelector("#command-button-pick");
  pickButton.click();

  info("Waiting for inspector to be selected.");
  await toolbox.once("inspector-selected");
  is(toolbox.currentToolId, "inspector", "Switched to the inspector");
}
