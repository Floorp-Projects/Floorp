/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that toggling a CSS declaration in the Rule view is tracked.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: red;
    }
  </style>
  <div></div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);

  await selectNode("div", inspector);
  const prop = getTextProperty(ruleView, 1, { color: "red" });

  let onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  info("Disable the first declaration");
  await togglePropStatus(ruleView, prop);
  info("Wait for change to be tracked");
  await onTrackChange;

  let removedDeclarations = getRemovedDeclarations(doc);
  is(
    removedDeclarations.length,
    1,
    "Only one declaration was tracked as removed"
  );

  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  info("Re-enable the first declaration");
  await togglePropStatus(ruleView, prop);
  info("Wait for change to be tracked");
  await onTrackChange;

  const addedDeclarations = getAddedDeclarations(doc);
  removedDeclarations = getRemovedDeclarations(doc);
  is(addedDeclarations.length, 0, "No declarations were tracked as added");
  is(removedDeclarations.length, 0, "No declarations were tracked as removed");
});
