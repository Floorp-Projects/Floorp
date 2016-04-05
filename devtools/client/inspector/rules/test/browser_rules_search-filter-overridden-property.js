/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view overriden search filter works properly for
// overridden properties.

const TEST_URI = `
  <style type='text/css'>
  #testid {
    width: 100%;
  }
  h1 {
    width: 50%;
  }
  </style>
  <h1 id='testid' class='testclass'>Styled Node</h1>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testFilterOverriddenProperty(inspector, view);
});

function* testFilterOverriddenProperty(inspector, ruleView) {
  info("Check that the correct rules are visible");
  is(ruleView.element.children.length, 3, "Should have 3 rules.");

  let rule = getRuleViewRuleEditor(ruleView, 1).rule;
  let textPropEditor = rule.textProps[0].editor;
  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(!textPropEditor.element.classList.contains("ruleview-overridden"),
    "width property is not overridden.");
  ok(textPropEditor.filterProperty.hidden,
    "Overridden search button is hidden.");

  rule = getRuleViewRuleEditor(ruleView, 2).rule;
  textPropEditor = rule.textProps[0].editor;
  is(rule.selectorText, "h1", "Third rule is h1.");
  ok(textPropEditor.element.classList.contains("ruleview-overridden"),
    "width property is overridden.");
  ok(!textPropEditor.filterProperty.hidden,
    "Overridden search button is not hidden.");

  let searchField = ruleView.searchField;
  let onRuleViewFiltered = inspector.once("ruleview-filtered");

  info("Click the overridden search");
  textPropEditor.filterProperty.click();
  yield onRuleViewFiltered;

  info("Check that the overridden search is applied");
  is(searchField.value, "`width`", "The search field value is width.");

  rule = getRuleViewRuleEditor(ruleView, 1).rule;
  textPropEditor = rule.textProps[0].editor;
  is(rule.selectorText, "#testid", "Second rule is #testid.");
  ok(textPropEditor.container.classList.contains("ruleview-highlight"),
    "width property is correctly highlighted.");

  rule = getRuleViewRuleEditor(ruleView, 2).rule;
  textPropEditor = rule.textProps[0].editor;
  is(rule.selectorText, "h1", "Third rule is h1.");
  ok(textPropEditor.container.classList.contains("ruleview-highlight"),
    "width property is correctly highlighted.");
  ok(textPropEditor.element.classList.contains("ruleview-overridden"),
    "width property is overridden.");
  ok(!textPropEditor.filterProperty.hidden,
    "Overridden search button is not hidden.");
}
