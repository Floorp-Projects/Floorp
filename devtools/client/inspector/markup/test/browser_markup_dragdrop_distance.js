/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that nodes don't start dragging before the mouse has moved by at least
// the minimum vertical distance defined in markup-view.js by
// DRAG_DROP_MIN_INITIAL_DISTANCE.

const TEST_URL = URL_ROOT + "doc_markup_dragdrop.html";
const TEST_NODE = "#test";

// Keep this in sync with DRAG_DROP_MIN_INITIAL_DISTANCE in markup-view.js
const MIN_DISTANCE = 10;

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Drag the test node by half of the minimum distance");
  await simulateNodeDrag(inspector, TEST_NODE, 0, MIN_DISTANCE / 2);
  await checkIsDragging(inspector, TEST_NODE, false);

  info("Drag the test node by exactly the minimum distance");
  await simulateNodeDrag(inspector, TEST_NODE, 0, MIN_DISTANCE);
  await checkIsDragging(inspector, TEST_NODE, true);
  inspector.markup.cancelDragging();

  info("Drag the test node by more than the minimum distance");
  await simulateNodeDrag(inspector, TEST_NODE, 0, MIN_DISTANCE * 2);
  await checkIsDragging(inspector, TEST_NODE, true);
  inspector.markup.cancelDragging();

  info("Drag the test node by minus the minimum distance");
  await simulateNodeDrag(inspector, TEST_NODE, 0, MIN_DISTANCE * -1);
  await checkIsDragging(inspector, TEST_NODE, true);
  inspector.markup.cancelDragging();
});

async function checkIsDragging(inspector, selector, isDragging) {
  const container = await getContainerForSelector(selector, inspector);
  if (isDragging) {
    ok(container.isDragging, "The container is being dragged");
    ok(inspector.markup.isDragging, "And the markup-view knows it");
  } else {
    ok(!container.isDragging, "The container hasn't been marked as dragging");
    ok(!inspector.markup.isDragging, "And the markup-view either");
  }
}
