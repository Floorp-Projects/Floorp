/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests removing a property by clearing the property value and pressing the
// return key, and checks if the focus is moved to the appropriate editable
// field.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: #00F;
    color: #00F;
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testEditPropertyAndRemove(inspector, view);
});

function* testEditPropertyAndRemove(inspector, view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;

  yield focusEditableField(view, propEditor.valueSpan);
  yield sendKeysAndWaitForFocus(view, ruleEditor.element,
    ["VK_DELETE", "VK_RETURN"]);
  yield ruleEditor.rule._applyingModifications;

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should have been unset.");

  propEditor = ruleEditor.rule.textProps[0].editor;

  let editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(propEditor.nameSpan), editor,
    "Focus should have moved to the next property name");

  info("Focus the property value and remove the property");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element,
    ["VK_TAB", "VK_DELETE", "VK_RETURN"]);
  yield ruleEditor.rule._applyingModifications;

  newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "color"
  });
  is(newValue, "", "color should have been unset.");

  editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(ruleEditor.newPropSpan), editor,
    "Focus should have moved to the new property span");
  is(ruleEditor.rule.textProps.length, 0,
    "All properties should have been removed.");
  is(ruleEditor.propertyList.children.length, 1,
    "Should have the new property span.");
}

function* sendKeysAndWaitForFocus(view, element, keys) {
  let onFocus = once(element, "focus", true);
  for (let key of keys) {
    EventUtils.synthesizeKey(key, {}, view.styleWindow);
  }
  yield onFocus;
}
