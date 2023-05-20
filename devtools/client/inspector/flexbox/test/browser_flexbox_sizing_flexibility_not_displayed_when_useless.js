/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flexibility section in the flex item sizing properties is not displayed
// when the item did not grow or shrink.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc, store } = flexboxInspector;

  info(
    "Select an item with flex:0 and wait for the sizing info to be rendered"
  );
  let onUpdate = waitForDispatch(store, "UPDATE_FLEXBOX");
  await selectNode("#did-not-grow-or-shrink div", inspector);
  await onUpdate;

  let flexSections = doc.querySelectorAll(
    ".flex-item-sizing .section.flexibility"
  );
  is(
    flexSections.length,
    0,
    "The flexibility section was not found in the DOM"
  );

  info(
    "Select a more complex item which also doesn't flex and wait for the sizing info"
  );
  onUpdate = waitForDispatch(store, "UPDATE_FLEXBOX");
  await selectNode(
    "#just-enough-space-for-clamped-items div:last-child",
    inspector
  );
  await onUpdate;

  flexSections = doc.querySelectorAll(".flex-item-sizing .section.flexibility");
  is(
    flexSections.length,
    0,
    "The flexibility section was not found in the DOM"
  );
});
