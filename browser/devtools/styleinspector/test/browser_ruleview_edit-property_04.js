/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a disabled property is previewed when the property name or value
// editor is focused and the property remains disabled when the escaping out of
// the property editor.

let TEST_URI = [
  "<style type='text/css'>",
  "#testid {",
  "  background-color: blue;",
  "}",
  "</style>",
  "<div id='testid'>Styled Node</div>",
].join("\n");

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

  yield testPreviewDisableProperty(view, ruleEditor, propEditor.nameSpan,
    "VK_ESCAPE");
  yield testPreviewDisableProperty(view, ruleEditor, propEditor.valueSpan,
    "VK_ESCAPE");
  yield testPreviewDisableProperty(view, ruleEditor, propEditor.valueSpan,
    "VK_TAB");
  yield testPreviewDisableProperty(view, ruleEditor, propEditor.valueSpan,
    "VK_RETURN");
}

function* testPreviewDisableProperty(view, ruleEditor, propEditor, commitKey) {
  let editor = yield focusEditableField(view, propEditor);
  yield ruleEditor.rule._applyingModifications;

  let newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "blue", "background-color should have been previewed.");

  let onBlur = once(editor.input, "blur");
  EventUtils.synthesizeKey(commitKey, {}, view.styleWindow);
  yield onBlur;
  yield ruleEditor.rule._applyingModifications;

  newValue = yield executeInContent("Test:GetRulePropertyValue", {
    styleSheetIndex: 0,
    ruleIndex: 0,
    name: "background-color"
  });
  is(newValue, "", "background-color should have been unset.");
}
