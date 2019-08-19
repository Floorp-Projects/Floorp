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

  await selectNode("div", inspector);
  const rule = getRuleViewRuleEditor(ruleView, 1).rule;
  const prop = rule.textProps[0];

  const onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Remove the first declaration");
  await removeProperty(ruleView, prop);
  info("Wait for change to be tracked");
  await onTrackChange;

  const removeDecl = getRemovedDeclarations(doc);
  is(removeDecl.length, 1, "One declaration was tracked as removed");
  is(
    removeDecl[0].property,
    "color",
    "Correct declaration name was tracked as removed"
  );
  is(
    removeDecl[0].value,
    "red",
    "Correct declaration value was tracked as removed"
  );
});
