/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests removing a property by clearing the property name and pressing the
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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  info("Getting the first property in the #testid rule");
  let rule = getRuleViewRuleEditor(view, 1).rule;
  let prop = rule.textProps[0];

  info("Deleting the name of that property to remove the property");
  yield removeProperty(view, prop, false);

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should have been unset.");

  info("Getting the new first property in the rule");
  prop = rule.textProps[0];

  let editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(prop.editor.nameSpan), editor,
    "Focus should have moved to the next property name");

  info("Deleting the name of that property to remove the property");
  view.styleDocument.activeElement.blur();
  yield removeProperty(view, prop, false);

  newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "color"
  });
  is(newValue, "", "color should have been unset.");

  editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(rule.editor.newPropSpan), editor,
    "Focus should have moved to the new property span");
  is(rule.textProps.length, 0,
    "All properties should have been removed.");
  is(rule.editor.propertyList.children.length, 1,
    "Should have the new property span.");

  view.styleDocument.activeElement.blur();
});
