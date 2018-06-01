/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the property name and value editors can be triggered when
// clicking on the property-name, the property-value, the colon or semicolon.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    margin: 0;
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);
  await testEditPropertyAndCancel(inspector, view);
});

async function testEditPropertyAndCancel(inspector, view) {
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const propEditor = ruleEditor.rule.textProps[0].editor;

  info("Test editor is created when clicking on property name");
  await focusEditableField(view, propEditor.nameSpan);
  ok(propEditor.nameSpan.inplaceEditor, "Editor created for property name");
  await sendKeysAndWaitForFocus(view, ruleEditor.element, ["ESCAPE"]);

  info("Test editor is created when clicking on ':' next to property name");
  const nameRect = propEditor.nameSpan.getBoundingClientRect();
  await focusEditableField(view, propEditor.nameSpan, nameRect.width + 1);
  ok(propEditor.nameSpan.inplaceEditor, "Editor created for property name");
  await sendKeysAndWaitForFocus(view, ruleEditor.element, ["ESCAPE"]);

  info("Test editor is created when clicking on property value");
  await focusEditableField(view, propEditor.valueSpan);
  ok(propEditor.valueSpan.inplaceEditor, "Editor created for property value");
  // When cancelling a value edition, the text-property-editor will trigger
  // a modification to make sure the property is back to its original value
  // => need to wait on "ruleview-changed" to avoid unhandled promises
  let onRuleviewChanged = view.once("ruleview-changed");
  await sendKeysAndWaitForFocus(view, ruleEditor.element, ["ESCAPE"]);
  await onRuleviewChanged;

  info("Test editor is created when clicking on ';' next to property value");
  const valueRect = propEditor.valueSpan.getBoundingClientRect();
  await focusEditableField(view, propEditor.valueSpan, valueRect.width + 1);
  ok(propEditor.valueSpan.inplaceEditor, "Editor created for property value");
  // When cancelling a value edition, the text-property-editor will trigger
  // a modification to make sure the property is back to its original value
  // => need to wait on "ruleview-changed" to avoid unhandled promises
  onRuleviewChanged = view.once("ruleview-changed");
  await sendKeysAndWaitForFocus(view, ruleEditor.element, ["ESCAPE"]);
  await onRuleviewChanged;
}
