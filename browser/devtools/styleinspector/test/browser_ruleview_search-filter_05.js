/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for multiple rule selectors.

const SEARCH = "body";

let TEST_URI = [
  '<style type="text/css">',
  '  html, body, div {',
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
});

function* testAddTextInFilter(inspector, ruleView) {
  info("Setting filter text to \"" + SEARCH + "\"");

  let win = ruleView.doc.defaultView;
  let searchField = ruleView.searchField;
  let onRuleViewFilter = inspector.once("ruleview-filtered");

  searchField.focus();
  synthesizeKeys(SEARCH, win);
  yield onRuleViewFilter;

  info("Check that the correct rules are visible");
  is(ruleView.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(ruleView, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let ruleEditor = getRuleViewRuleEditor(ruleView, 1);

  is(ruleEditor.rule.selectorText, "html, body, div",
    "Second rule is html, body, div.");
  ok(ruleEditor.selectorText.children[2].classList.contains("ruleview-highlight"),
    "body selector is highlighted.")
}
