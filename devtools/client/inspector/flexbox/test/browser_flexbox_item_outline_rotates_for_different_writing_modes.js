/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item outline is rotated to match its main axis direction.

const TEST_URI = URL_ROOT + "doc_flexbox_writing_modes.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  info("Check that the row flex item rotated to vertical-bt.");
  let onFlexItemOutlineRendered = waitForDOM(
    doc,
    ".flex-outline-container .flex-outline"
  );
  await selectNode(".row.vertical-bt.item", inspector);
  let [flexOutline] = await onFlexItemOutlineRendered;

  ok(
    flexOutline.classList.contains("vertical-bt"),
    "Row outline has been rotated to vertical-bt."
  );

  info("Check that the column flex item rotated to horizontal-rl.");
  onFlexItemOutlineRendered = waitForDOM(
    doc,
    ".flex-outline-container .flex-outline"
  );
  await selectNode(".column.horizontal-rl.item", inspector);
  await waitUntil(() => {
    flexOutline = doc.querySelector(
      ".flex-outline-container .flex-outline.horizontal-rl"
    );
    return flexOutline;
  });

  ok(true, "Column outline has been rotated to horizontal-rl.");
});
