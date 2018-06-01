/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view strict search filter works properly for selector
// values.

const SEARCH = "`.testclass`";

const TEST_URI = `
  <style type="text/css">
    .testclass1 {
      background-color: #00F;
    }
    .testclass {
      color: red;
    }
  </style>
  <h1 id="testid" class="testclass testclass1">Styled Node</h1>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);
  await testAddTextInFilter(inspector, view);
});

async function testAddTextInFilter(inspector, view) {
  await setSearchFilter(view, SEARCH);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  const ruleEditor = getRuleViewRuleEditor(view, 1);

  is(ruleEditor.rule.selectorText, ".testclass", "Second rule is .testclass.");
  ok(ruleEditor.selectorText.children[0].classList
    .contains("ruleview-highlight"), ".testclass selector is highlighted.");
}
