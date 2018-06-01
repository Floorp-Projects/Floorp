/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that when the viewport is resized, the rule-view refreshes.

const TEST_URI = "data:text/html;charset=utf-8,<html><style>" +
                 "div {" +
                 "  width: 500px;" +
                 "  height: 10px;" +
                 "  background: purple;" +
                 "} " +
                 "@media screen and (max-width: 200px) {" +
                 "  div { " +
                 "    width: 100px;" +
                 "  }" +
                 "};" +
                 "</style><div></div></html>";

addRDMTask(TEST_URI, async function({ ui, manager }) {
  info("Open the responsive design mode and set its size to 500x500 to start");
  await setViewportSize(ui, manager, 500, 500);

  info("Open the inspector, rule-view and select the test node");
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  info("Try shrinking the viewport and checking the applied styles");
  await testShrink(view, ui, manager);

  info("Try growing the viewport and checking the applied styles");
  await testGrow(view, ui, manager);

  info("Check that ESC still opens the split console");
  await testEscapeOpensSplitConsole(inspector);

  await closeToolbox();
});

async function testShrink(ruleView, ui, manager) {
  is(numberOfRules(ruleView), 2, "Should have two rules initially.");

  info("Resize to 100x100 and wait for the rule-view to update");
  const onRefresh = ruleView.once("ruleview-refreshed");
  await setViewportSize(ui, manager, 100, 100);
  await onRefresh;

  is(numberOfRules(ruleView), 3, "Should have three rules after shrinking.");
}

async function testGrow(ruleView, ui, manager) {
  info("Resize to 500x500 and wait for the rule-view to update");
  const onRefresh = ruleView.once("ruleview-refreshed");
  await setViewportSize(ui, manager, 500, 500);
  await onRefresh;

  is(numberOfRules(ruleView), 2, "Should have two rules after growing.");
}

async function testEscapeOpensSplitConsole(inspector) {
  ok(!inspector._toolbox._splitConsole, "Console is not split.");

  info("Press escape");
  const onSplit = inspector._toolbox.once("split-console");
  EventUtils.synthesizeKey("KEY_Escape");
  await onSplit;

  ok(inspector._toolbox._splitConsole, "Console is split after pressing ESC.");
}

function numberOfRules(ruleView) {
  return ruleView.element.querySelectorAll(".ruleview-code").length;
}
