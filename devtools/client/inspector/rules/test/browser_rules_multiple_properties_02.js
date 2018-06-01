/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule-view behaves correctly when entering mutliple and/or
// unfinished properties/values in inplace-editors

const TEST_URI = "<div>Test Element</div>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("div", inspector);

  const ruleEditor = getRuleViewRuleEditor(view, 0);
  let onDone = view.once("ruleview-changed");
  await createNewRuleViewProperty(ruleEditor, "width:");
  await onDone;

  is(ruleEditor.rule.textProps.length, 1,
    "Should have created a new text property.");
  is(ruleEditor.propertyList.children.length, 1,
    "Should have created a property editor.");

  // Value is focused, lets add multiple rules here and make sure they get added
  onDone = view.once("ruleview-changed");
  const onMutation = inspector.once("markupmutation");
  const input = view.styleDocument.activeElement;
  input.value = "height: 10px;color:blue";
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onMutation;
  await onDone;

  is(ruleEditor.rule.textProps.length, 2,
    "Should have added the changed value.");
  is(ruleEditor.propertyList.children.length, 3,
    "Should have added the changed value editor.");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  is(ruleEditor.propertyList.children.length, 2,
    "Should have removed the value editor.");

  is(ruleEditor.rule.textProps[0].name, "width",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[0].value, "height: 10px",
    "Should have correct property value");

  is(ruleEditor.rule.textProps[1].name, "color",
    "Should have correct property name");
  is(ruleEditor.rule.textProps[1].value, "blue",
    "Should have correct property value");
});
