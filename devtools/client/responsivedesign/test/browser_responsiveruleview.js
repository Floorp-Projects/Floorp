/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that when the viewport is resized, the rule-view refreshes.
// Also test that ESC does open the split-console, and that the RDM menu item
// gets updated correctly when needed.
// TODO: split this test.

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

add_task(function* () {
  yield addTab(TEST_URI);

  info("Open the responsive design mode and set its size to 500x500 to start");
  let { rdm, manager } = yield openRDM();
  yield setSize(rdm, manager, 500, 500);

  info("Open the inspector, rule-view and select the test node");
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);

  info("Try shrinking the viewport and checking the applied styles");
  yield testShrink(view, rdm, manager);

  info("Try growing the viewport and checking the applied styles");
  yield testGrow(view, rdm, manager);

  info("Check that ESC still opens the split console");
  yield testEscapeOpensSplitConsole(inspector);

  yield closeToolbox();

  info("Test the state of the RDM menu item");
  yield testMenuItem(rdm);

  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
});

function* testShrink(ruleView, rdm, manager) {
  is(numberOfRules(ruleView), 2, "Should have two rules initially.");

  info("Resize to 100x100 and wait for the rule-view to update");
  let onRefresh = ruleView.once("ruleview-refreshed");
  yield setSize(rdm, manager, 100, 100);
  yield onRefresh;

  is(numberOfRules(ruleView), 3, "Should have three rules after shrinking.");
}

function* testGrow(ruleView, rdm, manager) {
  info("Resize to 500x500 and wait for the rule-view to update");
  let onRefresh = ruleView.once("ruleview-refreshed");
  yield setSize(rdm, manager, 500, 500);
  yield onRefresh;

  is(numberOfRules(ruleView), 2, "Should have two rules after growing.");
}

function* testEscapeOpensSplitConsole(inspector) {
  ok(!inspector._toolbox._splitConsole, "Console is not split.");

  info("Press escape");
  let onSplit = inspector._toolbox.once("split-console");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield onSplit;

  ok(inspector._toolbox._splitConsole, "Console is split after pressing ESC.");
}

function* testMenuItem(rdm) {
  is(document.getElementById("menu_responsiveUI").getAttribute("checked"),
     "true", "The menu item is checked");

  yield closeRDM(rdm);

  is(document.getElementById("menu_responsiveUI").getAttribute("checked"),
     "false", "The menu item is unchecked");
}

function numberOfRules(ruleView) {
  return ruleView.element.querySelectorAll(".ruleview-code").length;
}
