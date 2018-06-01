/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a disabled property remains disabled when the escaping out of
// the property editor.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: blue;
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const prop = rule.textProps[0];

  info("Disabling a property");
  await togglePropStatus(view, prop);

  const newValue = await executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should have been unset.");

  await testEditDisableProperty(view, rule, prop, "name", "VK_ESCAPE");
  await testEditDisableProperty(view, rule, prop, "value", "VK_ESCAPE");
  await testEditDisableProperty(view, rule, prop, "value", "VK_TAB");
  await testEditDisableProperty(view, rule, prop, "value", "VK_RETURN");
});

async function testEditDisableProperty(view, rule, prop, fieldType, commitKey) {
  const field = fieldType === "name" ? prop.editor.nameSpan
                                   : prop.editor.valueSpan;

  const editor = await focusEditableField(view, field);

  ok(!prop.editor.element.classList.contains("ruleview-overridden"),
    "property is not overridden.");
  is(prop.editor.enable.style.visibility, "hidden",
    "property enable checkbox is hidden.");

  let newValue = await executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should remain unset.");

  let onChangeDone;
  if (fieldType === "value") {
    onChangeDone = view.once("ruleview-changed");
  }

  const onBlur = once(editor.input, "blur");
  EventUtils.synthesizeKey(commitKey, {}, view.styleWindow);
  await onBlur;
  await onChangeDone;

  ok(!prop.enabled, "property is disabled.");
  ok(prop.editor.element.classList.contains("ruleview-overridden"),
    "property is overridden.");
  is(prop.editor.enable.style.visibility, "visible",
    "property enable checkbox is visible.");
  ok(!prop.editor.enable.getAttribute("checked"),
    "property enable checkbox is not checked.");

  newValue = await executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should remain unset.");
}
