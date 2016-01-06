/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly based on the
// priority for the rule

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: blue;
  }
  .testclass {
    background-color: green !important;
  }
  </style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

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
  ok(idProp.overridden, "Not-important rule should be overridden.");

  let classRule = elementStyle.rules[2];
  let classProp = classRule.textProps[0];
  ok(!classProp.overridden, "Important rule should not be overridden.");

  let elementRule = elementStyle.rules[0];
  let elementProp = elementRule.createProperty("background-color", "purple",
    "important");
  yield elementRule._applyingModifications;

  ok(!elementProp.overridden, "New important prop should not be overriden.");
  ok(idProp.overridden, "ID property should be overridden.");
  ok(classProp.overridden, "Class property should be overridden.");
}
