/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a flex item's min width/height value is not displayed if it's unspecified in
// the CSS.

const TEST_URI = URL_ROOT + "doc_flexbox_unauthored_min_dimension.html";

async function checkFlexItemCSSProperty(inspector, store, doc, selector) {
  info("Select the container's flex item sizing info.");
  const onUpdate = waitUntilAction(store, "UPDATE_FLEXBOX");
  await selectNode(selector, inspector);
  await onUpdate;

  info(
    "Check that the minimum size section does not display minimum dimension text."
  );
  const [sectionMinRowItem] = [
    ...doc.querySelectorAll(".flex-item-sizing .section.min"),
  ];
  const minDimension = sectionMinRowItem.querySelector(".css-property-link");

  ok(!minDimension, "Minimum dimension property should not be displayed.");
}

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc, store } = flexboxInspector;

  await checkFlexItemCSSProperty(
    inspector,
    store,
    doc,
    "#flex-item-with-unauthored-min-width"
  );
  await checkFlexItemCSSProperty(
    inspector,
    store,
    doc,
    "#flex-item-with-unauthored-min-height"
  );
});
