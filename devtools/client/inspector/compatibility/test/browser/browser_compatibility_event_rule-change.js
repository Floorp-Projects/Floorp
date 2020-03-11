/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the content of the issue list will be changed when the rules are changed
// on the rule view.

const TEST_URI = `<div style="border-block-color: lime;"></div>`;

const {
  COMPATIBILITY_UPDATE_NODES_COMPLETE,
} = require("devtools/client/inspector/compatibility/actions/index");

add_task(async function() {
  info("Enable 3 pane mode");
  await pushPref("devtools.inspector.three-pane-enabled", true);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, selectedElementPane } = await openCompatibilityView();
  await selectNode("div", inspector);

  info("Check the initial issue");
  await assertIssueList(selectedElementPane, [
    { property: "border-block-color" },
  ]);

  info("Check the issue after toggling the property");
  await _togglePropRule(inspector, 0, 0);
  await assertIssueList(selectedElementPane, []);

  info("Check the issue after toggling the property again");
  await _togglePropRule(inspector, 0, 0);
  await assertIssueList(selectedElementPane, [
    { property: "border-block-color" },
  ]);
});

async function _togglePropRule(inspector, ruleIndex, propIndex) {
  const ruleView = inspector.getPanel("ruleview").view;
  const onNodesUpdated = waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_NODES_COMPLETE
  );
  const rule = getRuleViewRuleEditor(ruleView, ruleIndex).rule;
  const textProp = rule.textProps[propIndex];
  textProp.editor.enable.click();
  await onNodesUpdated;
}
