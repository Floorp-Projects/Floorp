/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for property line input.

let TEST_URI = [
  '<style type="text/css">',
  '  #testid {',
  '    background-color: #FFF;',
  '  }',
  '  .testclass {',
  '    background-color: #00F;',
  '  }',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testAddTextInFilter(inspector, view);
});

function* testAddTextInFilter(inspector, ruleView) {
  info("Setting filter text to \"background-color:#00F\"");

  let win = ruleView.doc.defaultView;
  let searchField = ruleView.searchField;
  let onRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  synthesizeKeys("background-color:#00F", win);
  yield onRuleViewFiltered;

  info("Check that the correct rules are visible");
  is(ruleView.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(ruleView, 0).rule.selectorText, "element", "First rule is inline element.");
  is(getRuleViewRuleEditor(ruleView, 1).rule.selectorText, ".testclass", "Second rule is .testclass.");
  ok(getRuleViewRuleEditor(ruleView, 1).rule.textProps[0].editor.element.classList.contains("ruleview-highlight"),
    "background-color text property is correctly highlighted.");
}
