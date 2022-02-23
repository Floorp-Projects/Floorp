/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item outline renders the basis and final points as a single point
// if their sizes are equal. If not, then render as separate points.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc, store } = flexboxInspector;

  info("Select a flex item whose basis size matches its final size.");
  let onUpdate = waitUntilAction(store, "UPDATE_FLEXBOX");
  await selectNode(".item", inspector);
  await onUpdate;

  const [basisFinalPoint] = [
    ...doc.querySelectorAll(".flex-outline-point.basisfinal"),
  ];

  ok(basisFinalPoint, "The basis/final point exists");

  info("Select a flex item whose basis size is different than its final size.");
  onUpdate = waitUntilAction(store, "UPDATE_FLEXBOX");
  await selectNode(".shrinking .item", inspector);
  await onUpdate;

  const [basis, final] = [
    ...doc.querySelectorAll(
      ".flex-outline-point.basis, .flex-outline-point.final"
    ),
  ];

  ok(basis, "The basis point exists");
  ok(final, "The final point exists");
});
