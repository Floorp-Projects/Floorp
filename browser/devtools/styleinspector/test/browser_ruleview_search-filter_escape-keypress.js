/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter escape keypress will clear the search
// field.

let TEST_URI = [
  '<style type="text/css">',
  '  #testid {',
  '    background-color: #00F;',
  '  }',
  '  .testclass {',
  '    width: 100%;',
  '  }',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testAddTextInFilter(inspector, view);
  yield testEscapeKeypress(inspector, view);
});

function* testAddTextInFilter(inspector, ruleView) {
  info("Setting filter text to \"00F\"");

  let win = ruleView.styleWindow;
  let searchField = ruleView.searchField;
  let onRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  synthesizeKeys("00F", win);
  yield onRuleViewFiltered;

  info("Check that the correct rules are visible");
  is(ruleView.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(ruleView, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let rule = getRuleViewRuleEditor(ruleView, 1).rule;

  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(rule.textProps[0].editor.container.classList.contains("ruleview-highlight"),
    "background-color text property is correctly highlighted.");
}

function* testEscapeKeypress(inspector, ruleView) {
  info("Pressing the escape key on search filter");

  let doc = ruleView.styleDocument;
  let win = ruleView.styleWindow;
  let searchField = ruleView.searchField;
  let onRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  yield onRuleViewFiltered;

  info("Check the search filter is cleared and no rules are highlighted");
  is(ruleView.element.children.length, 3, "Should have 3 rules.");
  ok(!searchField.value, "Search filter is cleared");
  ok(!doc.querySelectorAll(".ruleview-highlight").length,
    "No rules are higlighted");
}
