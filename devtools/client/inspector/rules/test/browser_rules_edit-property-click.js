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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testEditPropertyAndCancel(inspector, view);
});

function* testEditPropertyAndCancel(inspector, view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;

  info("Test editor is created when clicking on property name");
  yield focusEditableField(view, propEditor.nameSpan);
  ok(propEditor.nameSpan.inplaceEditor, "Editor created for property name");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element, ["ESCAPE"]);

  info("Test editor is created when clicking on ':' next to property name");
  let nameRect = propEditor.nameSpan.getBoundingClientRect();
  yield focusEditableField(view, propEditor.nameSpan, nameRect.width + 1);
  ok(propEditor.nameSpan.inplaceEditor, "Editor created for property name");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element, ["ESCAPE"]);

  info("Test editor is created when clicking on property value");
  yield focusEditableField(view, propEditor.valueSpan);
  ok(propEditor.valueSpan.inplaceEditor, "Editor created for property value");
  // When cancelling a value edition, the text-property-editor will trigger
  // a modification to make sure the property is back to its original value
  // => need to wait on "ruleview-changed" to avoid unhandled promises
  let onRuleviewChanged = view.once("ruleview-changed");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element, ["ESCAPE"]);
  yield onRuleviewChanged;

  info("Test editor is created when clicking on ';' next to property value");
  let valueRect = propEditor.valueSpan.getBoundingClientRect();
  yield focusEditableField(view, propEditor.valueSpan, valueRect.width + 1);
  ok(propEditor.valueSpan.inplaceEditor, "Editor created for property value");
  // When cancelling a value edition, the text-property-editor will trigger
  // a modification to make sure the property is back to its original value
  // => need to wait on "ruleview-changed" to avoid unhandled promises
  onRuleviewChanged = view.once("ruleview-changed");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element, ["ESCAPE"]);
  yield onRuleviewChanged;
}
