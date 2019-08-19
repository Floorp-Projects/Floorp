/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test tracking changes to CSS declarations in different stylesheets but in rules
// with identical selectors.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
  </style>
  <style type='text/css'>
    div {
      font-size: 1em;
    }
  </style>
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);

  await selectNode("div", inspector);
  const rule1 = getRuleViewRuleEditor(ruleView, 1).rule;
  const rule2 = getRuleViewRuleEditor(ruleView, 2).rule;
  const prop1 = rule1.textProps[0];
  const prop2 = rule2.textProps[0];
  let onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Disable the declaration in the first rule");
  await togglePropStatus(ruleView, prop1);
  info("Wait for change to be tracked");
  await onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Disable the declaration in the second rule");
  await togglePropStatus(ruleView, prop2);
  info("Wait for change to be tracked");
  await onTrackChange;

  const removeDecl = getRemovedDeclarations(doc);
  is(removeDecl.length, 2, "Two declarations tracked as removed");
  // The last of the two matching rules shows up first in Rule view given that the
  // specificity is the same. This is correct. If the properties were the same, the latest
  // declaration would overwrite the first and thus show up on top.
  is(
    removeDecl[0].property,
    "font-size",
    "Correct property name for second declaration"
  );
  is(
    removeDecl[0].value,
    "1em",
    "Correct property value for second declaration"
  );
  is(
    removeDecl[1].property,
    "color",
    "Correct property name for first declaration"
  );
  is(
    removeDecl[1].value,
    "red",
    "Correct property value for first declaration"
  );
});
