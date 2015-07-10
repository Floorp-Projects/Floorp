/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test cancelling the addition of a new property in the rule-view

const TEST_URI = [
  "<style>",
  "#testid {background-color: blue;}",
  ".testclass, .unmatched {background-color: green;}",
  "</style>",
  "<div id='testid' class='testclass'>Styled Node</div>",
  "<div id='testid2'>Styled Node</div>"
].join("");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openRuleView();
  yield testCancelNew(inspector, view);
  yield testCancelNewOnEscape(inspector, view);
});

function* testCancelNew(inspector, ruleView) {
  // Start at the beginning: start to add a rule to the element's style
  // declaration, but leave it empty.

  let elementRuleEditor = getRuleViewRuleEditor(ruleView, 0);
  let editor = yield focusEditableField(ruleView, elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor,
    "Property editor is focused");

  let onBlur = once(editor.input, "blur");
  editor.input.blur();
  yield onBlur;

  ok(!elementRuleEditor.rule._applyingModifications, "Shouldn't have an outstanding modification request after a cancel.");
  is(elementRuleEditor.rule.textProps.length, 0, "Should have canceled creating a new text property.");
  ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
}

function* testCancelNewOnEscape(inspector, ruleView) {
  // Start at the beginning: start to add a rule to the element's style
  // declaration, add some text, then press escape.

  let elementRuleEditor = getRuleViewRuleEditor(ruleView, 0);
  let editor = yield focusEditableField(ruleView, elementRuleEditor.closeBrace);

  is(inplaceEditor(elementRuleEditor.newPropSpan), editor, "Next focused editor should be the new property editor.");

  EventUtils.sendString("background", ruleView.doc.defaultView)

  let onBlur = once(editor.input, "blur");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield onBlur;

  ok(!elementRuleEditor.rule._applyingModifications, "Shouldn't have an outstanding modification request after a cancel.");
  is(elementRuleEditor.rule.textProps.length, 0, "Should have canceled creating a new text property.");
  ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
}
