/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the ruleview is still correctly displayed after reloading the page.
 * See Bug 1487284.
 */

// To trigger the initial issue, the stylesheet needs to be fetched from the network
// monitor, so we can not use a data:uri with inline styles here.
const TEST_URI = `${URL_ROOT}doc_toolbox_rule_view.html`;

add_task(async function() {
  const tab = await addTab(TEST_URI);

  info("Open the rule-view and select the test node before opening RDM");
  const { inspector, testActor, view } = await openRuleView();
  await selectNode("div", inspector);

  is(numberOfRules(view), 2, "Rule view has two rules.");

  info("Open RDM");
  await openRDM(tab);

  info("Reload the current page");
  const onNewRoot = inspector.once("new-root");
  const onRuleViewRefreshed = inspector.once("rule-view-refreshed");
  await testActor.reload();
  await onNewRoot;
  await inspector.markup._waitForChildren();
  await onRuleViewRefreshed;

  is(numberOfRules(view), 2, "Rule view still has two rules and is not empty.");

  await closeRDM(tab);
  await removeTab(tab);
});

function numberOfRules(ruleView) {
  return ruleView.element.querySelectorAll(".ruleview-code").length;
}
