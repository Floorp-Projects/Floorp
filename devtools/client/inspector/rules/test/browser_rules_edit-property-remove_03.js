/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests removing a property by clearing the property name and pressing shift
// and tab keys, and checks if the focus is moved to the appropriate editable
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
  let propEditor = ruleEditor.rule.textProps[1].editor;

  yield focusEditableField(view, propEditor.valueSpan);
  yield sendKeysAndWaitForFocus(view, ruleEditor.element, [
    { key: "VK_DELETE", modifiers: {} },
    { key: "VK_TAB", modifiers: { shiftKey: true } }
  ]);
  yield ruleEditor.rule._applyingModifications;

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "color"
  });
  is(newValue, "", "color should have been unset.");
  is(propEditor.valueSpan.textContent, "",
    "'' property value is correctly set.");

  yield sendKeysAndWaitForFocus(view, ruleEditor.element, [
    { key: "VK_TAB", modifiers: { shiftKey: true } }
  ]);
  yield ruleEditor.rule._applyingModifications;

  propEditor = ruleEditor.rule.textProps[0].editor;

  let editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(propEditor.valueSpan), editor,
    "Focus should have moved to the previous property value");

  info("Focus the property name and remove the property");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element, [
    { key: "VK_TAB", modifiers: { shiftKey: true } },
    { key: "VK_DELETE", modifiers: {} },
    { key: "VK_TAB", modifiers: { shiftKey: true } }
  ]);

  yield ruleEditor.rule._applyingModifications;

  newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should have been unset.");

  editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(ruleEditor.selectorText), editor,
    "Focus should have moved to the selector text.");
  is(ruleEditor.rule.textProps.length, 0,
    "All properties should have been removed.");
  ok(!ruleEditor.propertyList.hasChildNodes(),
    "Should not have any properties.");
}

function* sendKeysAndWaitForFocus(view, element, keys) {
  let onFocus = once(element, "focus", true);
  for (let {key, modifiers} of keys) {
    EventUtils.synthesizeKey(key, modifiers, view.styleWindow);
  }
  yield onFocus;
}
