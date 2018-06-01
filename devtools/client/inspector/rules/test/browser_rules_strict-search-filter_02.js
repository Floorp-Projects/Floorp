/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view strict search filter works properly for stylesheet
// source.

const SEARCH = "`doc_urls_clickable.css:1`";
const TEST_URI = URL_ROOT + "doc_urls_clickable.html";

add_task(async function() {
  await addTab(TEST_URI);
  const {inspector, view} = await openRuleView();
  await selectNode(".relative1", inspector);
  await testAddTextInFilter(inspector, view);
});

async function testAddTextInFilter(inspector, view) {
  await setSearchFilter(view, SEARCH);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const source = rule.textProps[0].editor.ruleEditor.source;

  is(rule.selectorText, ".relative1", "Second rule is .relative1.");
  ok(source.classList.contains("ruleview-highlight"),
    "stylesheet source is correctly highlighted.");
}
