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

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testDisableProperty(inspector, view);
});

function* testDisableProperty(inspector, view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;

  info("Disabling a property");
  propEditor.enable.click();
  yield ruleEditor.rule._applyingModifications;

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should have been unset.");

  yield testEditDisableProperty(view, ruleEditor, propEditor,
    propEditor.nameSpan, "VK_ESCAPE");
  yield testEditDisableProperty(view, ruleEditor, propEditor,
    propEditor.valueSpan, "VK_ESCAPE");
  yield testEditDisableProperty(view, ruleEditor, propEditor,
    propEditor.valueSpan, "VK_TAB");
  yield testEditDisableProperty(view, ruleEditor, propEditor,
    propEditor.valueSpan, "VK_RETURN");
}

function* testEditDisableProperty(view, ruleEditor, propEditor,
    editableField, commitKey) {
  let editor = yield focusEditableField(view, editableField);

  ok(!propEditor.element.classList.contains("ruleview-overridden"),
    "property is not overridden.");
  is(propEditor.enable.style.visibility, "hidden",
    "property enable checkbox is hidden.");

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should remain unset.");

  let onBlur = once(editor.input, "blur");
  EventUtils.synthesizeKey(commitKey, {}, view.styleWindow);
  yield onBlur;
  yield ruleEditor.rule._applyingModifications;

  ok(!propEditor.prop.enabled, "property is disabled.");
  ok(propEditor.element.classList.contains("ruleview-overridden"),
    "property is overridden.");
  is(propEditor.enable.style.visibility, "visible",
    "property enable checkbox is visible.");
  ok(!propEditor.enable.getAttribute("checked"),
    "property enable checkbox is not checked.");

  newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should remain unset.");
}
