/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly after
// editing the selector.

const TEST_URI = `
  <style type='text/css'>
  div {
    background-color: blue;
    background-color: chartreuse;
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
  let rule = elementStyle.rules[1];
  checkProperties(rule);

  let ruleEditor = getRuleViewRuleEditor(view, 1);
  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(view, ruleEditor.selectorText);

  info("Entering a new selector name and committing");
  editor.input.value = "div[class]";

  let onRuleViewChanged = once(view, "ruleview-changed");
  info("Entering the commit key");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewChanged;

  view.searchField.focus();
  checkProperties(rule);
}

// A helper to perform a repeated set of checks.
function checkProperties(rule) {
  let prop = rule.textProps[0];
  is(prop.name, "background-color",
     "First property should be background-color");
  is(prop.value, "blue", "First property value should be blue");
  ok(prop.overridden, "prop should be overridden.");
  prop = rule.textProps[1];
  is(prop.name, "background-color",
     "Second property should be background-color");
  is(prop.value, "chartreuse", "First property value should be chartreuse");
  ok(!prop.overridden, "prop should not be overridden.");
}
