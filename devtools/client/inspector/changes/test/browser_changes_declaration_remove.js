/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that removing a CSS declaration from a rule in the Rule view is tracked.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
  </style>
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);
  const panel = doc.querySelector("#sidebar-panel-changes");

  await selectNode("div", inspector);
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const prop = rule.textProps[0];

  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Remove the first declaration");
  await removeProperty(ruleView, prop);
  info("Wait for change to be tracked");
  await onTrackChange;

  const removeName = panel.querySelector(".diff-remove .declaration-name");
  const removeValue = panel.querySelector(".diff-remove .declaration-value");
  is(removeName.textContent, "color", "Correct declaration name was tracked as removed");
  is(removeValue.textContent, "red", "Correct declaration value was tracked as removed");
});
