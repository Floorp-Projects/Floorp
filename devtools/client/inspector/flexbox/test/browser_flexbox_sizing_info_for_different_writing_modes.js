/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item sizing info shows the correct dimension values for different
// writing modes. For vertical writing modes, row items should display height values and
// column items should display width values. The opposite is true for horizontal mode
// where rows display width values and columns display height.

const TEST_URI = URL_ROOT + "doc_flexbox_writing_modes.html";

async function checkFlexItemDimension(
  inspector,
  store,
  doc,
  selector,
  expected
) {
  info("Select the container's flex item.");
  const onUpdate = waitForDispatch(store, "UPDATE_FLEXBOX");
  await selectNode(selector, inspector);
  await onUpdate;

  info("Check that the minimum size section shows the correct dimension.");
  const sectionMinRowItem = doc.querySelector(".flex-item-sizing .section.min");
  const minDimension = sectionMinRowItem.querySelector(".css-property-link");

  ok(
    minDimension.textContent.includes(expected),
    "The flex item sizing has the correct dimension value."
  );
}

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc, store } = flexboxInspector;

  await checkFlexItemDimension(
    inspector,
    store,
    doc,
    ".row.vertical-rl.item",
    "min-height"
  );
  await checkFlexItemDimension(
    inspector,
    store,
    doc,
    ".column.vertical-tb.item",
    "min-height"
  );
  await checkFlexItemDimension(
    inspector,
    store,
    doc,
    ".row.vertical-bt.item",
    "min-height"
  );
  await checkFlexItemDimension(
    inspector,
    store,
    doc,
    ".column.horizontal-rl.item",
    "min-width"
  );
  await checkFlexItemDimension(
    inspector,
    store,
    doc,
    ".row.horizontal-lr.item",
    "min-width"
  );
});
