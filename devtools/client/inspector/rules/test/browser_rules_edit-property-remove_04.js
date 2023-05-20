/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that removing the only declaration from a rule and unselecting then re-selecting
// the element will not restore the removed declaration. Bug 1512956

const TEST_URI = `
  <style type='text/css'>
  #testid {
    color: #00F;
  }
  </style>
  <div id='testid'>Styled Node</div>
  <div id='empty'></div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  info("Select original node");
  await selectNode("#testid", inspector);

  info("Get the first property in the #testid rule");
  const rule = getRuleViewRuleEditor(view, 1).rule;
  const prop = rule.textProps[0];

  info("Delete the property name to remove the declaration");
  const onRuleViewChanged = view.once("ruleview-changed");
  await removeProperty(view, prop, false);
  info("Wait for Rule view to update");
  await onRuleViewChanged;

  is(rule.textProps.length, 0, "No CSS properties left on the rule");

  info("Select another node");
  await selectNode("#empty", inspector);

  info("Select original node again");
  await selectNode("#testid", inspector);

  is(rule.textProps.length, 0, "Still no CSS properties on the rule");
});
