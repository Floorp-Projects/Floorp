/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view search filter works properly for stylesheet source.

const SEARCH = "doc_urls_clickable.css";
const TEST_URI = TEST_URL_ROOT + "doc_urls_clickable.html";

add_task(function*() {
  yield addTab(TEST_URI);
  let {toolbox, inspector, view} = yield openRuleView();
  yield selectNode(".relative1", inspector);
  yield testAddTextInFilter(inspector, view);
});

function* testAddTextInFilter(inspector, ruleView) {
  info("Setting filter text to \"" + SEARCH + "\"");

  let win = ruleView.styleWindow;
  let searchField = ruleView.searchField;
  let onRuleViewFiltered = inspector.once("ruleview-filtered");

  searchField.focus();
  synthesizeKeys(SEARCH, win);
  yield onRuleViewFiltered;

  info("Check that the correct rules are visible");
  is(ruleView.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(ruleView, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let rule = getRuleViewRuleEditor(ruleView, 1).rule;
  let source = rule.textProps[0].editor.ruleEditor.source;

  is(rule.selectorText, ".relative1", "Second rule is .relative1.");
  ok(source.classList.contains("ruleview-highlight"),
    "stylesheet source is correctly highlighted.");
}
