/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering mutliple and/or
// unfinished properties/values in inplace-editors.

const TEST_URI = "<div>Test Element</div>";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);

  let ruleEditor = getRuleViewRuleEditor(view, 0);
  // Note that we wait for a markup mutation here because this new rule will end
  // up creating a style attribute on the node shown in the markup-view.
  // (we also wait for the rule-view to refresh).
  let onMutation = inspector.once("markupmutation");
  let onRuleViewChanged = view.once("ruleview-changed");
  yield createNewRuleViewProperty(ruleEditor,
    "color:red;width:100px;height: 100px;");
  yield onMutation;
  yield onRuleViewChanged;

  is(ruleEditor.rule.textProps.length, 3,
    "Should have created new text properties.");
  is(ruleEditor.propertyList.children.length, 4,
    "Should have created new property editors.");

  is(ruleEditor.rule.textProps[0].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "red",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "width",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "100px",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "height",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "100px",
    "Should have correct property value");
});
