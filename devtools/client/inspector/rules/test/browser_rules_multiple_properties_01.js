/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering mutliple and/or
// unfinished properties/values in inplace-editors.

const TEST_URI = "<div>Test Element</div>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("div", inspector);

  const ruleEditor = getRuleViewRuleEditor(view, 0);
  // Note that we wait for a markup mutation here because this new rule will end
  // up creating a style attribute on the node shown in the markup-view.
  // (we also wait for the rule-view to refresh).
  const onMutation = inspector.once("markupmutation");
  const onRuleViewChanged = view.once("ruleview-changed");
  await createNewRuleViewProperty(ruleEditor,
    "color:blue;background : orange   ; text-align:center; " +
    "border-color: green;");
  await onMutation;
  await onRuleViewChanged;

  is(ruleEditor.rule.textProps.length, 4,
    "Should have created a new text property.");
  is(ruleEditor.propertyList.children.length, 5,
    "Should have created a new property editor.");

  is(ruleEditor.rule.textProps[0].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "blue",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "background",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "orange",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[2].name, "text-align",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[2].value, "center",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[3].name, "border-color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[3].value, "green",
    "Should have correct property value");
});
