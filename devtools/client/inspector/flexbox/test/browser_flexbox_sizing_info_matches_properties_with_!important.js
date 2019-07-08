/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a flex item's sizing section shows the highest value of specificity for its
// CSS property.

const TEST_URI = URL_ROOT + "doc_flexbox_CSS_property_with_!important.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info("Select the container's flex item sizing info.");
  const onFlexItemSizingRendered = waitForDOM(doc, "ul.flex-item-sizing");
  await selectNode(".item", inspector);
  const [flexItemSizingContainer] = await onFlexItemSizingRendered;

  info(
    "Check that the max and flexibility sections show correct CSS property value."
  );
  const flexSection = flexItemSizingContainer.querySelector(
    ".section.flexibility"
  );
  const maxSection = flexItemSizingContainer.querySelector(".section.max");
  const flexGrow = flexSection.querySelector(".css-property-link");
  const maxSize = maxSection.querySelector(".css-property-link");

  is(
    maxSize.textContent,
    "(max-width: 400px !important)",
    "Maximum size section shows CSS property value with highest specificity."
  );
  is(
    flexGrow.textContent,
    "(flex-grow: 5 !important)",
    "Flexibility size section shows CSS property value with highest specificity."
  );
});
