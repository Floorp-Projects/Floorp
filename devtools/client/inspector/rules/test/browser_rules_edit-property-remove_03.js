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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  info("Getting the second property in the rule");
  let rule = getRuleViewRuleEditor(view, 1).rule;
  let prop = rule.textProps[1];

  info("Clearing the property value and pressing shift-tab");
  let editor = yield focusEditableField(view, prop.editor.valueSpan);
  let onValueDone = view.once("ruleview-changed");
  editor.input.value = "";
  EventUtils.synthesizeKey("VK_TAB", {shiftKey: true}, view.styleWindow);
  yield onValueDone;

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "color"
  });
  is(newValue, "", "color should have been unset.");
  is(prop.editor.valueSpan.textContent, "",
    "'' property value is correctly set.");

  info("Pressing shift-tab again to focus the previous property value");
  let onValueFocused = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_TAB", {shiftKey: true}, view.styleWindow);
  yield onValueFocused;

  info("Getting the first property in the rule");
  prop = rule.textProps[0];

  editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(prop.editor.valueSpan), editor,
    "Focus should have moved to the previous property value");

  info("Pressing shift-tab again to focus the property name");
  let onNameFocused = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_TAB", {shiftKey: true}, view.styleWindow);
  yield onNameFocused;

  info("Removing the name and pressing shift-tab to focus the selector");
  let onNameDeleted = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_DELETE", {}, view.styleWindow);
  EventUtils.synthesizeKey("VK_TAB", {shiftKey: true}, view.styleWindow);
  yield onNameDeleted;

  newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should have been unset.");

  editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(rule.editor.selectorText), editor,
    "Focus should have moved to the selector text.");
  is(rule.textProps.length, 0,
    "All properties should have been removed.");
  ok(!rule.editor.propertyList.hasChildNodes(),
    "Should not have any properties.");
});
