/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = URL_ROOT + "doc_custom.html";

// Tests the display of custom declarations in the rule-view.

add_task(async function() {
  await addTab(TEST_URI);
  const {inspector, view} = await openRuleView();

  await simpleCustomOverride(inspector, view);
  await importantCustomOverride(inspector, view);
  await disableCustomOverride(inspector, view);
});

async function simpleCustomOverride(inspector, view) {
  await selectNode("#testidSimple", inspector);

  const idRule = getRuleViewRuleEditor(view, 1).rule;
  const idRuleProp = idRule.textProps[0];

  is(idRuleProp.name, "--background-color",
     "First ID prop should be --background-color");
  ok(!idRuleProp.overridden, "ID prop should not be overridden.");

  const classRule = getRuleViewRuleEditor(view, 2).rule;
  const classRuleProp = classRule.textProps[0];

  is(classRuleProp.name, "--background-color",
     "First class prop should be --background-color");
  ok(classRuleProp.overridden, "Class property should be overridden.");

  // Override --background-color by changing the element style.
  const elementProp = await addProperty(view, 0, "--background-color", "purple");

  is(classRuleProp.name, "--background-color",
     "First element prop should now be --background-color");
  ok(!elementProp.overridden,
     "Element style property should not be overridden");
  ok(idRuleProp.overridden, "ID property should be overridden");
  ok(classRuleProp.overridden, "Class property should be overridden");
}

async function importantCustomOverride(inspector, view) {
  await selectNode("#testidImportant", inspector);

  const idRule = getRuleViewRuleEditor(view, 1).rule;
  const idRuleProp = idRule.textProps[0];
  ok(idRuleProp.overridden, "Not-important rule should be overridden.");

  const classRule = getRuleViewRuleEditor(view, 2).rule;
  const classRuleProp = classRule.textProps[0];
  ok(!classRuleProp.overridden, "Important rule should not be overridden.");
}

async function disableCustomOverride(inspector, view) {
  await selectNode("#testidDisable", inspector);

  const idRule = getRuleViewRuleEditor(view, 1).rule;
  const idRuleProp = idRule.textProps[0];

  await togglePropStatus(view, idRuleProp);

  const classRule = getRuleViewRuleEditor(view, 2).rule;
  const classRuleProp = classRule.textProps[0];
  ok(!classRuleProp.overridden,
     "Class prop should not be overridden after id prop was disabled.");
}
