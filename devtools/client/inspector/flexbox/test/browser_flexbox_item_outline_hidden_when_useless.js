/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item outline is not rendered when it isn't useful.
// For now, that means when the item's base, delta and final sizes are all 0.

const TEST_URI = `
  <div style="display:flex">
    <div></div>
  </div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info(
    "Select the item in the document and wait for the sizing section to appear"
  );
  const onFlexItemSizingRendered = waitForDOM(doc, "ul.flex-item-sizing");
  await selectNode("div > div", inspector);
  const [flexSizingContainer] = await onFlexItemSizingRendered;

  const outlineEls = doc.querySelectorAll(".flex-outline-container");
  is(outlineEls.length, 0, "The outline has not been rendered for this item");

  info("Also check that the sizing section shows the correct information");
  const allSections = [
    ...flexSizingContainer.querySelectorAll(".section .name"),
  ];

  is(allSections.length, 2, "There are 2 parts in the sizing section");
  is(
    allSections[0].textContent,
    "Content Size",
    "The first part is the content size"
  );
  is(
    allSections[1].textContent,
    "Final Size",
    "The second part is the final size"
  );
});
