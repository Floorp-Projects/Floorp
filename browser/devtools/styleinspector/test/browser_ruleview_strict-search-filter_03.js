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

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testAddTextInFilter(inspector, view);
});

function* testAddTextInFilter(inspector, view) {
  yield setSearchFilter(view, SEARCH);

  info("Check that the correct rules are visible");
  is(view.element.children.length, 2, "Should have 2 rules.");
  is(getRuleViewRuleEditor(view, 0).rule.selectorText, "element",
    "First rule is inline element.");

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  is(ruleEditor.rule.selectorText, ".testclass", "Second rule is .testclass.");
  ok(ruleEditor.selectorText.children[0].classList
    .contains("ruleview-highlight"), ".testclass selector is highlighted.");
}
