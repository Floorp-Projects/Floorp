/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a disabled property is re-enabled if the property name or value is
// modified

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: blue;
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  let rule = getRuleViewRuleEditor(view, 1).rule;
  let prop = rule.textProps[0];

  info("Disabling background-color property");
  yield togglePropStatus(view, prop);

  let newValue = yield getRulePropertyValue("background-color");
  is(newValue, "", "background-color should have been unset.");

  info("Entering a new property name, including : to commit and " +
       "focus the value");

  yield focusEditableField(view, prop.editor.nameSpan);
  let onNameDone = view.once("ruleview-changed");
  EventUtils.sendString("border-color:", view.styleWindow);
  yield onNameDone;

  info("Escape editing the property value");
  let onValueDone = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  yield onValueDone;

  newValue = yield getRulePropertyValue("border-color");
  is(newValue, "blue", "border-color should have been set.");

  ok(prop.enabled, "border-color property is enabled.");
  ok(!prop.editor.element.classList.contains("ruleview-overridden"),
    "border-color is not overridden");

  info("Disabling border-color property");
  yield togglePropStatus(view, prop);

  newValue = yield getRulePropertyValue("border-color");
  is(newValue, "", "border-color should have been unset.");

  info("Enter a new property value for the border-color property");
  yield setProperty(view, prop, "red");

  newValue = yield getRulePropertyValue("border-color");
  is(newValue, "red", "new border-color should have been set.");

  ok(prop.enabled, "border-color property is enabled.");
  ok(!prop.editor.element.classList.contains("ruleview-overridden"),
    "border-color is not overridden");
});

function* getRulePropertyValue(name) {
  let propValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: name
  });
  return propValue;
}
