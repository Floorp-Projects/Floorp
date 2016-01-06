/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly in the computed list
// for newly modified property values.

const SEARCH = "0px";

const TEST_URI = `
  <style type='text/css'>
    #testid {
      margin: 4px;
      top: 0px;
    }
  </style>
  <h1 id='testid'>Styled Node</h1>
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testModifyPropertyValueFilter(inspector, view);
});

function* testModifyPropertyValueFilter(inspector, view) {
  yield setSearchFilter(view, SEARCH);

  let rule = getRuleViewRuleEditor(view, 1).rule;
  let propEditor = rule.textProps[0].editor;
  let computed = propEditor.computed;
  let editor = yield focusEditableField(view, propEditor.valueSpan);

  info("Check that the correct rules are visible");
  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(!propEditor.container.classList.contains("ruleview-highlight"),
    "margin text property is not highlighted.");
  ok(rule.textProps[1].editor.container.classList
    .contains("ruleview-highlight"),
    "top text property is correctly highlighted.");

  let onBlur = once(editor.input, "blur");
  let onModification = view.once("ruleview-changed");
  EventUtils.sendString("4px 0px", view.styleWindow);
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onBlur;
  yield onModification;

  ok(propEditor.container.classList.contains("ruleview-highlight"),
    "margin text property is correctly highlighted.");
  ok(!computed.hasAttribute("filter-open"), "margin computed list is closed.");
  ok(!computed.children[0].classList.contains("ruleview-highlight"),
    "margin-top computed property is not highlighted.");
  ok(computed.children[1].classList.contains("ruleview-highlight"),
    "margin-right computed property is correctly highlighted.");
  ok(!computed.children[2].classList.contains("ruleview-highlight"),
    "margin-bottom computed property is not highlighted.");
  ok(computed.children[3].classList.contains("ruleview-highlight"),
    "margin-left computed property is correctly highlighted.");
}
