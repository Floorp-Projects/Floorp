/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that renaming the property of a CSS declaration in the Rule view is tracked.

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
  const prop = getTextProperty(ruleView, 1, { color: "red" });

  let onTrackChange;

  const oldPropertyName = "color";
  const newPropertyName = "background-color";

  info(`Rename the CSS declaration name to ${newPropertyName}`);
  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  await renameProperty(ruleView, prop, newPropertyName);
  info("Wait for the change to be tracked");
  await onTrackChange;

  const removeDecl = getRemovedDeclarations(doc);
  const addDecl = getAddedDeclarations(doc);

  is(removeDecl.length, 1, "One declaration tracked as removed");
  is(
    removeDecl[0].property,
    oldPropertyName,
    `Removed declaration has old property name: ${oldPropertyName}`
  );
  is(addDecl.length, 1, "One declaration tracked as added");
  is(
    addDecl[0].property,
    newPropertyName,
    `Added declaration has new property name: ${newPropertyName}`
  );

  info(
    `Reverting the CSS declaration name to ${oldPropertyName} should clear changes.`
  );
  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  await renameProperty(ruleView, prop, oldPropertyName);
  info("Wait for the change to be tracked");
  await onTrackChange;

  await waitFor(
    () => !getRemovedDeclarations(doc).length,
    "No declaration tracked as removed"
  );
  await waitFor(
    () => !getAddedDeclarations(doc).length,
    "No declaration tracked as added"
  );
});
