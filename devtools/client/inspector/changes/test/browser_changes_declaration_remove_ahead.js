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

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();
  const { document: doc, store } = selectChangesView(inspector);

  await selectNode("div", inspector);
  const prop1 = getTextProperty(ruleView, 1, { color: "red" });
  const prop2 = getTextProperty(ruleView, 1, { display: "block" });

  let onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  info("Change the second declaration");
  await setProperty(ruleView, prop2, "grid");
  await onTrackChange;

  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  info("Remove the first declaration");
  await removeProperty(ruleView, prop1);
  await onTrackChange;

  onTrackChange = waitForDispatch(store, "TRACK_CHANGE");
  info("Change the second declaration again");
  await setProperty(ruleView, prop2, "flex");
  info("Wait for change to be tracked");
  await onTrackChange;

  // Ensure changes to the second declaration were tracked after removing the first one.
  await waitFor(
    () => getRemovedDeclarations(doc).length == 2,
    "Two declarations should have been tracked as removed"
  );
  await waitFor(() => {
    const addDecl = getAddedDeclarations(doc);
    return addDecl.length == 1 && addDecl[0].value == "flex";
  }, "One declaration should have been tracked as added, and the added declaration to have updated property value");
});
