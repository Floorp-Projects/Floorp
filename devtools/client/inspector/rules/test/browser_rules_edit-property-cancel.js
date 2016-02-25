/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests editing a property name or value and escaping will revert the
// changes and restore the original value.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    background-color: #00F;
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testEditPropertyAndCancel(inspector, view);
});

function* testEditPropertyAndCancel(inspector, view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;

  yield focusEditableField(view, propEditor.nameSpan);
  yield sendCharsAndWaitForFocus(view, ruleEditor.element,
    ["VK_DELETE", "VK_ESCAPE"]);
  yield ruleEditor.rule._applyingModifications;

  is(propEditor.nameSpan.textContent, "background-color",
    "'background-color' property name is correctly set.");
  is((yield getComputedStyleProperty("#testid", null, "background-color")),
    "rgb(0, 0, 255)", "#00F background color is set.");

  yield focusEditableField(view, propEditor.valueSpan);
  yield sendCharsAndWaitForFocus(view, ruleEditor.element,
    ["VK_DELETE", "VK_ESCAPE"]);
  yield ruleEditor.rule._applyingModifications;

  is(propEditor.valueSpan.textContent, "#00F",
    "'#00F' property value is correctly set.");
  is((yield getComputedStyleProperty("#testid", null, "background-color")),
    "rgb(0, 0, 255)", "#00F background color is set.");
}

function* sendCharsAndWaitForFocus(view, element, chars) {
  let onFocus = once(element, "focus", true);
  for (let ch of chars) {
    EventUtils.sendChar(ch, view.styleWindow);
  }
  yield onFocus;
}
