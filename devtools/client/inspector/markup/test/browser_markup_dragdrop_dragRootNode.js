/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the root node isn't draggable (as well as head and body).

const TEST_URL = URL_ROOT + "doc_markup_dragdrop.html";
const TEST_DATA = ["html", "head", "body"];

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  for (const selector of TEST_DATA) {
    info("Try to drag/drop node " + selector);
    await simulateNodeDrag(inspector, selector);

    const container = await getContainerForSelector(selector, inspector);
    ok(!container.isDragging, "The container hasn't been marked as dragging");
  }
});
