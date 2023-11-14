/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `
<style type="text/css">
  #rule-from-stylesheet {
    color: red;
  }
</style>
<div id=inline style="cursor:pointer">
  A
  <div id=inherited>B</div>
</div>
<div id=rule-from-stylesheet>C</a>
`;

// This test will assert that specific elements of a ruleview rule have been
// rendered in the expected order. This is specifically done to check the fix
// for Bug 1664511, where some elements were rendered out of order due to
// unexpected async processing.
add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await selectNode("#inline", inspector);
  checkRuleViewRuleMarkupOrder(view, "element");
  await selectNode("#inherited", inspector);
  checkRuleViewRuleMarkupOrder(view, "element", 1);
  await selectNode("#rule-from-stylesheet", inspector);
  checkRuleViewRuleMarkupOrder(view, "#rule-from-stylesheet");
});

function checkRuleViewRuleMarkupOrder(view, selectorText, index = 0) {
  const rule = getRuleViewRule(view, selectorText, index);

  // Retrieve the individual elements to assert.
  const selectorContainer = rule.querySelector(".ruleview-selectors-container");
  const highlighterIcon = rule.querySelector(".js-toggle-selector-highlighter");
  const ruleOpenBrace = rule.querySelector(".ruleview-ruleopen");

  const parentNode = selectorContainer.parentNode;
  const childNodes = [...parentNode.childNodes];

  ok(
    childNodes.indexOf(selectorContainer) < childNodes.indexOf(highlighterIcon),
    "Selector text is rendered before the highlighter icon"
  );
  ok(
    childNodes.indexOf(highlighterIcon) < childNodes.indexOf(ruleOpenBrace),
    "Highlighter icon is rendered before the opening brace"
  );
}
