/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that renaming a property works.

const TEST_URI = `
  <style type="text/css">
  #testid {
    color: #FFF;
  }
  </style>
  <div style='color: red' id='testid'>Styled Node</div>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testRenameProperty(inspector, view);
});

function* testRenameProperty(inspector, view) {
  let ruleEditor = getRuleViewRuleEditor(view, 0);
  let propEditor = ruleEditor.rule.textProps[0].editor;

  is(ruleEditor.rule.textProps[0].name, "color");

  info("Focus on property name");
  let editor = yield focusEditableField(ruleEditor.ruleView,
    propEditor.nameSpan, 32, 1);

  info("Renaming the property");
  let onValueFocus = once(ruleEditor.element, "focus", true);
  let onModifications = ruleEditor.ruleView.once("ruleview-changed");
  EventUtils.sendString("background-color:", ruleEditor.doc.defaultView);
  yield onValueFocus;
  yield onModifications;

  is(ruleEditor.rule.textProps[0].name, "background-color");
  yield waitForComputedStyleProperty("#testid", null, "background-color",
    "rgb(255, 0, 0)");

  is((yield getComputedStyleProperty("#testid", null, "color")),
    "rgb(255, 255, 255)", "color is white");
}
