/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item sizing UI contains the right sections, depending on which
// element is selected. Some items may be clamped, others not, so not all sections are
// visible at all times.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

const TEST_DATA = [
  {
    selector: ".shrinking .item",
    expectedSections: ["Base Size", "Flexibility", "Final Size"],
  },
  {
    selector: ".shrinking.is-clamped .item",
    expectedSections: [
      "Base Size",
      "Flexibility",
      "Minimum Size",
      "Final Size",
    ],
  },
  {
    selector: ".growing .item",
    expectedSections: ["Base Size", "Flexibility", "Final Size"],
  },
  {
    selector: ".growing.is-clamped .item",
    expectedSections: [
      "Base Size",
      "Flexibility",
      "Maximum Size",
      "Final Size",
    ],
  },
];

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc, store } = flexboxInspector;

  for (const { selector, expectedSections } of TEST_DATA) {
    info(`Checking the list of sections for the flex item ${selector}`);
    const sections = await selectNodeAndGetFlexSizingSections(
      selector,
      store,
      inspector,
      doc
    );

    is(
      sections.length,
      expectedSections.length,
      "Correct number of sections found"
    );
    expectedSections.forEach((expectedSection, i) => {
      ok(
        sections[i].includes(expectedSection),
        `The ${expectedSection} section was found`
      );
    });
  }
});

async function selectNodeAndGetFlexSizingSections(
  selector,
  store,
  inspector,
  doc
) {
  const onUpdate = waitUntilAction(store, "UPDATE_FLEXBOX");
  await selectNode(selector, inspector);
  await onUpdate;

  info(`Getting the list of displayed sections for ${selector}`);
  const allSections = [
    ...doc.querySelectorAll("ul.flex-item-sizing .section .name"),
  ];
  const allSectionTitles = allSections.map(el => el.textContent);

  return allSectionTitles;
}
