/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item outline is rotated for flex items in a column flexbox layout.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  // Select a flex item in the row flexbox layout.
  let onFlexItemOutlineRendered = waitForDOM(
    doc,
    ".flex-outline-container .flex-outline"
  );
  await selectNode(".container .item", inspector);
  let [flexOutline] = await onFlexItemOutlineRendered;

  ok(
    flexOutline.classList.contains("horizontal-lr"),
    "The flex outline has the horizontal-lr class"
  );

  // Check that the outline is wider than it is tall in the configuration.
  let bounds = flexOutline.getBoxQuads()[0].getBounds();
  ok(bounds.width > bounds.height, "The outline looks like a row");

  // Select a flex item in the column flexbox layout.
  onFlexItemOutlineRendered = waitForDOM(
    doc,
    ".flex-outline-container .flex-outline"
  );
  await selectNode(".container.column .item", inspector);
  await waitUntil(() => {
    flexOutline = doc.querySelector(
      ".flex-outline-container .flex-outline.vertical-tb"
    );
    return flexOutline;
  });
  ok(true, "The flex outline has the vertical-tb class");

  // Check that the outline is taller than it is wide in the configuration.
  bounds = flexOutline.getBoxQuads()[0].getBounds();
  ok(bounds.height > bounds.width, "The outline looks like a column");
});
