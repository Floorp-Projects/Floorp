/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the correct editable fields are focused when tabbing and entering
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
  yield testEditableFieldFocus(inspector, view, "VK_RETURN");
  yield testEditableFieldFocus(inspector, view, "VK_TAB");
});

function* testEditableFieldFocus(inspector, view, commitKey) {
  info("Click on the selector of the inline style ('element')");
  let ruleEditor = getRuleViewRuleEditor(view, 0);
  let onFocus = once(ruleEditor.element, "focus", true);
  ruleEditor.selectorText.click();
  yield onFocus;
  assertEditor(view, ruleEditor.newPropSpan,
    "Focus should be in the element property span");

  info("Focus the next field with " + commitKey);
  ruleEditor = getRuleViewRuleEditor(view, 1);
  yield focusNextEditableField(view, ruleEditor, commitKey);
  assertEditor(view, ruleEditor.selectorText,
    "Focus should have moved to the next rule selector");

  for (let i = 0; i < ruleEditor.rule.textProps.length; i++) {
    let textProp = ruleEditor.rule.textProps[i];
    let propEditor = textProp.editor;

    info("Focus the next field with " + commitKey);
    // Expect a ruleview-changed event if we are moving from a property value
    // to the next property name (which occurs after the first iteration, as for
    // i=0, the previous field is the selector).
    let onRuleViewChanged = i > 0 ? view.once("ruleview-changed") : null;
    yield focusNextEditableField(view, ruleEditor, commitKey);
    yield onRuleViewChanged;
    assertEditor(view, propEditor.nameSpan,
      "Focus should have moved to the property name");

    info("Focus the next field with " + commitKey);
    yield focusNextEditableField(view, ruleEditor, commitKey);
    assertEditor(view, propEditor.valueSpan,
      "Focus should have moved to the property value");
  }

  // Expect a ruleview-changed event again as we're bluring a property value.
  let onRuleViewChanged = view.once("ruleview-changed");
  yield focusNextEditableField(view, ruleEditor, commitKey);
  yield onRuleViewChanged;
  assertEditor(view, ruleEditor.newPropSpan,
    "Focus should have moved to the new property span");

  ruleEditor = getRuleViewRuleEditor(view, 2);

  yield focusNextEditableField(view, ruleEditor, commitKey);
  assertEditor(view, ruleEditor.selectorText,
    "Focus should have moved to the next rule selector");

  info("Blur the selector field");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
}

function* focusNextEditableField(view, ruleEditor, commitKey) {
  let onFocus = once(ruleEditor.element, "focus", true);
  EventUtils.synthesizeKey(commitKey, {}, view.styleWindow);
  yield onFocus;
}

function assertEditor(view, element, message) {
  let editor = inplaceEditor(view.styleDocument.activeElement);
  is(inplaceEditor(element), editor, message);
}
