/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the correct declaration is identified and changed after removing a
// declaration positioned ahead of it in the same CSS rule.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
      display: block;
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
  const prop1 = rule.textProps[0];
  const prop2 = rule.textProps[1];

  let onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Change the second declaration");
  await setProperty(ruleView, prop2, "grid");
  await onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Remove the first declaration");
  await removeProperty(ruleView, prop1);
  await onTrackChange;

  onTrackChange = waitUntilAction(store, "TRACK_CHANGE");
  info("Change the second declaration again");
  await setProperty(ruleView, prop2, "flex");
  info("Wait for change to be tracked");
  await onTrackChange;

  const removeDecl = getRemovedDeclarations(doc);
  const addDecl = getAddedDeclarations(doc);

  is(removeDecl.length, 2, "Two declarations tracked as removed");
  is(addDecl.length, 1, "One declaration tracked as added");
  // Ensure changes to the second declaration were tracked after removing the first one.
  is(
    addDecl[0].property,
    "display",
    "Added declaration has updated property name"
  );
  is(addDecl[0].value, "flex", "Added declaration has updated property value");
});
