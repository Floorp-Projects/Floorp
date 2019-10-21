/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the content of the issue list will be changed when the rules are changed
// on the rule view.

const TEST_URI = `<div style="border-block-color: lime;"></div>`;

add_task(async function() {
  info("Enable 3 pane mode");
  await pushPref("devtools.inspector.three-pane-enabled", true);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, panel } = await openCompatibilityView();
  await selectNode("div", inspector);

  info("Check the initial issue");
  await assertIssueList(panel, [{ property: "border-block-color" }]);

  info("Check the issue after toggling the property");
  const view = inspector.getPanel("ruleview").view;
  const rule = getRuleViewRuleEditor(view, 0).rule;
  await _togglePropStatus(view, rule.textProps[0]);
  await assertIssueList(panel, []);

  info("Check the issue after toggling the property again");
  await _togglePropStatus(view, rule.textProps[0]);
  await assertIssueList(panel, [{ property: "border-block-color" }]);
});

async function _togglePropStatus(view, textProp) {
  const onRuleViewRefreshed = view.once("ruleview-changed");
  textProp.editor.enable.click();
  await onRuleViewRefreshed;
}
