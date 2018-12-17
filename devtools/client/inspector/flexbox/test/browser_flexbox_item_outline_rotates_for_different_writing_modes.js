/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item outline is rotated to match its main axis direction.

const TEST_URI = URL_ROOT + "doc_flexbox_writing_modes.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info("Check that a vertical row flex item rotates to vertical-tb.");
  let onFlexItemOutlineRendered = waitForDOM(doc,
    ".flex-outline-container .flex-outline");
  await selectNode(".row.vertical.item", inspector);
  let [flexOutline] = await onFlexItemOutlineRendered;

  ok(flexOutline.classList.contains("vertical-tb"),
    "Horizontal item outline orientation has been rotated to vertical-tb.");

  info("Check that a vertical-rl column flex item rotates to horizontal-rl.");
  onFlexItemOutlineRendered = waitForDOM(doc,
    ".flex-outline-container .flex-outline");
  await selectNode(".column.vertical.item", inspector);
  await waitUntil(() => {
    flexOutline =
      doc.querySelector(".flex-outline-container .flex-outline.horizontal-rl");
    return flexOutline;
  });

  ok(true, "Vertical-rl item outline orientation has been rotated to horizontal-rl.");
});
