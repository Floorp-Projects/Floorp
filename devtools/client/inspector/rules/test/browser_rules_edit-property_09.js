/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a newProperty editor is only created if no other editor was
// previously displayed.

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
  yield testClickOnEmptyAreaToCloseEditor(inspector, view);
});

function synthesizeMouseOnEmptyArea(ruleEditor, view) {
  // any text property editor will do
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let valueContainer = propEditor.valueContainer;
  let valueRect = valueContainer.getBoundingClientRect();
  // click right next to the ";" at the end of valueContainer
  EventUtils.synthesizeMouse(valueContainer, valueRect.width + 1, 1, {},
   view.styleWindow);
}

function* testClickOnEmptyAreaToCloseEditor(inspector, view) {
  // Start at the beginning: start to add a rule to the element's style
  // declaration, add some text, then press escape.
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;

  info("Create a property value editor");
  let editor = yield focusEditableField(view, propEditor.valueSpan);
  ok(editor.input, "The inplace-editor field is ready");

  info("Close the property value editor by clicking on an empty area " +
    "in the rule editor");
  let onRuleViewChanged = view.once("ruleview-changed");
  let onBlur = once(editor.input, "blur");
  synthesizeMouseOnEmptyArea(ruleEditor, view);
  yield onBlur;
  yield onRuleViewChanged;
  ok(!view.isEditing, "No inplace editor should be displayed in the ruleview");

  info("Create new newProperty editor by clicking again on the empty area");
  let onFocus = once(ruleEditor.element, "focus", true);
  synthesizeMouseOnEmptyArea(ruleEditor, view);
  yield onFocus;
  editor = inplaceEditor(ruleEditor.element.ownerDocument.activeElement);
  is(inplaceEditor(ruleEditor.newPropSpan), editor,
   "New property editor was created");

  info("Close the newProperty editor by clicking again on the empty area");
  onBlur = once(editor.input, "blur");
  synthesizeMouseOnEmptyArea(ruleEditor, view);
  yield onBlur;

  ok(!view.isEditing, "No inplace editor should be displayed in the ruleview");
}
