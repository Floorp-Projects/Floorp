/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item sizing information exists when a flex item is selected.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  // Select a flex item in the test document and wait for the sizing info to be rendered.
  // Note that we select an item that has base, delta and final sizes, so we can check
  // those sections exists.
  const onFlexItemSizingRendered = waitForDOM(doc, "ul.flex-item-sizing");
  await selectNode(".container.growing .item", inspector);
  const [flexSizingContainer] = await onFlexItemSizingRendered;

  ok(flexSizingContainer, "The flex sizing exists in the DOM");

  info("Check that the base, flexibility and final sizes are displayed");
  const allSections = [
    ...flexSizingContainer.querySelectorAll(".section .name"),
  ];
  const allSectionTitles = allSections.map(el => el.textContent);

  ["Base Size", "Flexibility", "Final Size"].forEach((expectedTitle, i) => {
    ok(
      allSectionTitles[i].includes(expectedTitle),
      `Sizing section #${i + 1} (${expectedTitle}) was found`
    );
  });
});
