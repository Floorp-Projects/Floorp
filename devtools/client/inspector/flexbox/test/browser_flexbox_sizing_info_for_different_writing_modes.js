/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item sizing info shows the correct dimension values for different
// writing modes. For vertical writing modes, row items should display height values and
// column items should display width values. The opposite is true for horizontal mode
// where rows display width values and columns display height.

const TEST_URI = URL_ROOT + "doc_flexbox_writing_modes.html";

async function checkFlexItemDimension(inspector, doc, selector, expectedDimension) {
  info("Select the container's flex item.");
  const onFlexItemSizingRendered = waitForDOM(doc, "ul.flex-item-sizing");
  await selectNode(selector, inspector);
  const [flexItemSizingContainer] = await onFlexItemSizingRendered;

  info("Check that the minimum size section shows the correct dimension.");
  const [sectionMinRowItem] = [...flexItemSizingContainer.querySelectorAll(
    ".section.min")];
  const minDimension = sectionMinRowItem.querySelector(".theme-fg-color5");

  is(minDimension.textContent, expectedDimension,
    "The flex item sizing has the correct dimension value.");
}

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  await checkFlexItemDimension(inspector, doc, ".row.vertical.item", "min-height");
  await checkFlexItemDimension(inspector, doc, ".column.vertical.item", "min-width");
  await checkFlexItemDimension(inspector, doc, ".row.horizontal.item", "min-width");
  await checkFlexItemDimension(inspector, doc, ".column.horizontal.item", "min-height");
});
