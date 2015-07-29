/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly based on the
// specificity of the rule

let TEST_URI = [
  "<style type='text/css'>",
  "#testid {",
  "  background-color: blue;",
  "}",
  ".testclass {",
  "  background-color: green;",
  "}",
  "</style>",
  "<div id='testid' class='testclass'>Styled Node</div>"
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testMarkOverridden(inspector, view);
});

function* testMarkOverridden(inspector, view) {
  let elementStyle = view._elementStyle;

  let idRule = elementStyle.rules[1];
  let idProp = idRule.textProps[0];
  is(idProp.name, "background-color",
    "First ID property should be background-color");
  is(idProp.value, "blue", "First ID property value should be blue");
  ok(!idProp.overridden, "ID prop should not be overridden.");
  ok(!idProp.editor.element.classList.contains("ruleview-overridden"),
    "ID property editor should not have ruleview-overridden class");

  let classRule = elementStyle.rules[2];
  let classProp = classRule.textProps[0];
  is(classProp.name, "background-color",
    "First class prop should be background-color");
  is(classProp.value, "green", "First class property value should be green");
  ok(classProp.overridden, "Class property should be overridden.");
  ok(classProp.editor.element.classList.contains("ruleview-overridden"),
    "Class property editor should have ruleview-overridden class");

  // Override background-color by changing the element style.
  let elementRule = elementStyle.rules[0];
  elementRule.createProperty("background-color", "purple", "");
  yield elementRule._applyingModifications;

  let elementProp = elementRule.textProps[0];
  ok(!elementProp.overridden,
    "Element style property should not be overridden");
  ok(idProp.overridden, "ID property should be overridden");
  ok(idProp.editor.element.classList.contains("ruleview-overridden"),
    "ID property editor should have ruleview-overridden class");
  ok(classProp.overridden, "Class property should be overridden");
  ok(classProp.editor.element.classList.contains("ruleview-overridden"),
    "Class property editor should have ruleview-overridden class");
}
