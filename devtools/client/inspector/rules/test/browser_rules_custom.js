/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = URL_ROOT + "doc_custom.html";

// Tests the display of custom declarations in the rule-view.

add_task(function*() {
  yield addTab(TEST_URI);
  let {inspector, view} = yield openRuleView();

  yield simpleCustomOverride(inspector, view);
  yield importantCustomOverride(inspector, view);
  yield disableCustomOverride(inspector, view);
});

function* simpleCustomOverride(inspector, view) {
  yield selectNode("#testidSimple", inspector);

  let elementStyle = view._elementStyle;

  let idRule = elementStyle.rules[1];
  let idProp = idRule.textProps[0];
  is(idProp.name, "--background-color",
     "First ID prop should be --background-color");
  ok(!idProp.overridden, "ID prop should not be overridden.");

  let classRule = elementStyle.rules[2];
  let classProp = classRule.textProps[0];
  is(classProp.name, "--background-color",
     "First class prop should be --background-color");
  ok(classProp.overridden, "Class property should be overridden.");

  // Override --background-color by changing the element style.
  let elementRule = elementStyle.rules[0];
  elementRule.createProperty("--background-color", "purple", "");
  yield elementRule._applyingModifications;

  let elementProp = elementRule.textProps[0];
  is(classProp.name, "--background-color",
     "First element prop should now be --background-color");
  ok(!elementProp.overridden,
     "Element style property should not be overridden");
  ok(idProp.overridden, "ID property should be overridden");
  ok(classProp.overridden, "Class property should be overridden");
}

function* importantCustomOverride(inspector, view) {
  yield selectNode("#testidImportant", inspector);

  let elementStyle = view._elementStyle;

  let idRule = elementStyle.rules[1];
  let idProp = idRule.textProps[0];
  ok(idProp.overridden, "Not-important rule should be overridden.");

  let classRule = elementStyle.rules[2];
  let classProp = classRule.textProps[0];
  ok(!classProp.overridden, "Important rule should not be overridden.");
}

function* disableCustomOverride(inspector, view) {
  yield selectNode("#testidDisable", inspector);

  let elementStyle = view._elementStyle;

  let idRule = elementStyle.rules[1];
  let idProp = idRule.textProps[0];

  idProp.setEnabled(false);
  yield idRule._applyingModifications;

  let classRule = elementStyle.rules[2];
  let classProp = classRule.textProps[0];
  ok(!classProp.overridden,
     "Class prop should not be overridden after id prop was disabled.");
}
