/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that emptying out an existing value removes the property and
// doesn't cause any other issues.  See also Bug 1150780.

const TEST_URI = `
  <style type="text/css">
  #testid {
    color: red;
    background-color: blue;
    font-size: 12px;
  }
  .testclass, .unmatched {
    background-color: green;
  }
  </style>
  <div id="testid" class="testclass">Styled Node</div>
  <div id="testid2">Styled Node</div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[1].editor;

  yield focusEditableField(view, propEditor.valueSpan);

  info("Deleting all the text out of a value field");
  let onRuleViewChanged = view.once("ruleview-changed");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element,
    ["DELETE", "RETURN"]);
  yield onRuleViewChanged;

  info("Pressing enter a couple times to cycle through editors");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element, ["RETURN"]);
  onRuleViewChanged = view.once("ruleview-changed");
  yield sendKeysAndWaitForFocus(view, ruleEditor.element, ["RETURN"]);
  yield onRuleViewChanged;

  isnot(ruleEditor.rule.textProps[1].editor.nameSpan.style.display, "none",
    "The name span is visible");
  is(ruleEditor.rule.textProps.length, 2, "Correct number of props");
});
