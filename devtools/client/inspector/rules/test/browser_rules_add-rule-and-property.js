/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests adding a new rule and a new property in this rule.

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,<div id='testid'>Styled Node</div>");
  const {inspector, view} = await openRuleView();

  info("Selecting the test node");
  await selectNode("#testid", inspector);

  info("Adding a new rule for this node and blurring the new selector field");
  await addNewRuleAndDismissEditor(inspector, view, "#testid", 1);

  info("Adding a new property for this rule");
  const ruleEditor = getRuleViewRuleEditor(view, 1);

  const onRuleViewChanged = view.once("ruleview-changed");
  ruleEditor.addProperty("font-weight", "bold", "", true);
  await onRuleViewChanged;

  const textProps = ruleEditor.rule.textProps;
  const prop = textProps[textProps.length - 1];
  is(prop.name, "font-weight", "The last property name is font-weight");
  is(prop.value, "bold", "The last property value is bold");
});
