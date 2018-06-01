/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that changing the current element's attributes refreshes the rule-view

const TEST_URI = `
  <style type="text/css">
    #testid {
      background-color: blue;
    }
    .testclass {
      background-color: green;
    }
  </style>
  <div id="testid" class="testclass" style="margin-top: 1px; padding-top: 5px;">
    Styled Node
  </div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view, testActor} = await openRuleView();
  await selectNode("#testid", inspector);

  info("Checking that the rule-view has the element, #testid and " +
    ".testclass selectors");
  checkRuleViewContent(view, ["element", "#testid", ".testclass"]);

  info("Changing the node's ID attribute and waiting for the " +
    "rule-view refresh");
  let ruleViewRefreshed = inspector.once("rule-view-refreshed");
  await testActor.setAttribute("#testid", "id", "differentid");
  await ruleViewRefreshed;

  info("Checking that the rule-view doesn't have the #testid selector anymore");
  checkRuleViewContent(view, ["element", ".testclass"]);

  info("Reverting the ID attribute change");
  ruleViewRefreshed = inspector.once("rule-view-refreshed");
  await testActor.setAttribute("#differentid", "id", "testid");
  await ruleViewRefreshed;

  info("Checking that the rule-view has all the selectors again");
  checkRuleViewContent(view, ["element", "#testid", ".testclass"]);
});

function checkRuleViewContent(view, expectedSelectors) {
  const selectors = view.styleDocument
    .querySelectorAll(".ruleview-selectorcontainer");

  is(selectors.length, expectedSelectors.length,
    expectedSelectors.length + " selectors are displayed");

  for (let i = 0; i < expectedSelectors.length; i++) {
    is(selectors[i].textContent.indexOf(expectedSelectors[i]), 0,
      "Selector " + (i + 1) + " is " + expectedSelectors[i]);
  }
}
