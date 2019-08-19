/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that CSS property names are case insensitive when validating.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
  </style>
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();

  await selectNode("div", inspector);
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const prop = rule.textProps[0];
  let onRuleViewChanged;

  info(`Rename the CSS property name to "Color"`);
  onRuleViewChanged = ruleView.once("ruleview-changed");
  await renameProperty(ruleView, prop, "Color");
  info("Wait for Rule view to update");
  await onRuleViewChanged;

  is(prop.overridden, false, "Titlecase property is not overriden");
  is(prop.enabled, true, "Titlecase property is enabled");
  is(prop.isNameValid(), true, "Titlecase property is valid");

  info(`Rename the CSS property name to "COLOR"`);
  onRuleViewChanged = ruleView.once("ruleview-changed");
  await renameProperty(ruleView, prop, "COLOR");
  info("Wait for Rule view to update");
  await onRuleViewChanged;

  is(prop.overridden, false, "Uppercase property is not overriden");
  is(prop.enabled, true, "Uppercase property is enabled");
  is(prop.isNameValid(), true, "Uppercase property is valid");
});
