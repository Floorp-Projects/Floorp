/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = URL_ROOT + "doc_custom.html";

// Tests the display of custom declarations in the rule-view.

add_task(function* () {
  yield addTab(TEST_URI);
  let {inspector, view} = yield openRuleView();

  yield simpleCustomOverride(inspector, view);
  yield importantCustomOverride(inspector, view);
  yield disableCustomOverride(inspector, view);
});

function* simpleCustomOverride(inspector, view) {
  yield selectNode("#testidSimple", inspector);

  let idRule = getRuleViewRuleEditor(view, 1).rule;
  let idRuleProp = idRule.textProps[0];

  is(idRuleProp.name, "--background-color",
     "First ID prop should be --background-color");
  ok(!idRuleProp.overridden, "ID prop should not be overridden.");

  let classRule = getRuleViewRuleEditor(view, 2).rule;
  let classRuleProp = classRule.textProps[0];

  is(classRuleProp.name, "--background-color",
     "First class prop should be --background-color");
  ok(classRuleProp.overridden, "Class property should be overridden.");

  // Override --background-color by changing the element style.
  let elementProp = yield addProperty(view, 0, "--background-color", "purple");

  is(classRuleProp.name, "--background-color",
     "First element prop should now be --background-color");
  ok(!elementProp.overridden,
     "Element style property should not be overridden");
  ok(idRuleProp.overridden, "ID property should be overridden");
  ok(classRuleProp.overridden, "Class property should be overridden");
}

function* importantCustomOverride(inspector, view) {
  yield selectNode("#testidImportant", inspector);

  let idRule = getRuleViewRuleEditor(view, 1).rule;
  let idRuleProp = idRule.textProps[0];
  ok(idRuleProp.overridden, "Not-important rule should be overridden.");

  let classRule = getRuleViewRuleEditor(view, 2).rule;
  let classRuleProp = classRule.textProps[0];
  ok(!classRuleProp.overridden, "Important rule should not be overridden.");
}

function* disableCustomOverride(inspector, view) {
  yield selectNode("#testidDisable", inspector);

  let idRule = getRuleViewRuleEditor(view, 1).rule;
  let idRuleProp = idRule.textProps[0];

  yield togglePropStatus(view, idRuleProp);

  let classRule = getRuleViewRuleEditor(view, 2).rule;
  let classRuleProp = classRule.textProps[0];
  ok(!classRuleProp.overridden,
     "Class prop should not be overridden after id prop was disabled.");
}
