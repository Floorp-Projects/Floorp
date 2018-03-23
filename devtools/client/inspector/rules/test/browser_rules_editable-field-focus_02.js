/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the correct editable fields are focused when shift tabbing
// through the rule view.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: blue;
    color: red;
    margin: 0;
    padding: 0;
  }
  div {
    border-color: red
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testEditableFieldFocus(inspector, view, "VK_TAB", { shiftKey: true });
});

function* testEditableFieldFocus(inspector, view, commitKey, options = {}) {
  let ruleEditor = getRuleViewRuleEditor(view, 2);
  let editor = yield focusEditableField(view, ruleEditor.selectorText);
  is(inplaceEditor(ruleEditor.selectorText), editor,
    "Focus should be in the 'div' rule selector");

  ruleEditor = getRuleViewRuleEditor(view, 1);

  yield focusNextField(view, ruleEditor, commitKey, options);
  assertEditor(view, ruleEditor.newPropSpan,
    "Focus should have moved to the new property span");

  for (let textProp of ruleEditor.rule.textProps.slice(0).reverse()) {
    let propEditor = textProp.editor;

    yield focusNextField(view, ruleEditor, commitKey, options);
    yield assertEditor(view, propEditor.valueSpan,
      "Focus should have moved to the property value");

    yield focusNextFieldAndExpectChange(view, ruleEditor, commitKey, options);
    yield assertEditor(view, propEditor.nameSpan,
      "Focus should have moved to the property name");
  }

  ruleEditor = getRuleViewRuleEditor(view, 1);

  yield focusNextField(view, ruleEditor, commitKey, options);
  yield assertEditor(view, ruleEditor.selectorText,
    "Focus should have moved to the '#testid' rule selector");

  ruleEditor = getRuleViewRuleEditor(view, 0);

  yield focusNextField(view, ruleEditor, commitKey, options);
  assertEditor(view, ruleEditor.newPropSpan,
    "Focus should have moved to the new property span");
}

function* focusNextFieldAndExpectChange(view, ruleEditor, commitKey, options) {
  let onRuleViewChanged = view.once("ruleview-changed");
  yield focusNextField(view, ruleEditor, commitKey, options);
  yield onRuleViewChanged;
}

function* focusNextField(view, ruleEditor, commitKey, options) {
  let onFocus = once(ruleEditor.element, "focus", true);
  EventUtils.synthesizeKey(commitKey, options, view.styleWindow);
  yield onFocus;
}

function* assertEditor(view, element, message) {
  let editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(element), editor, message);
}
