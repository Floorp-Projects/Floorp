/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether ESCAPE keypress cancels dragging of an element.

const TEST_URL = URL_ROOT + "doc_markup_dragdrop.html";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);
  const {markup} = inspector;

  info("Get a test container");
  await selectNode("#test", inspector);
  const container = await getContainerForSelector("#test", inspector);

  info("Simulate a drag/drop on this container");
  await simulateNodeDrag(inspector, "#test");

  ok(container.isDragging && markup.isDragging,
     "The container is being dragged");
  ok(markup.doc.body.classList.contains("dragging"),
     "The dragging css class was added");

  info("Simulate ESCAPE keypress");
  EventUtils.sendKey("escape", inspector.panelWin);

  ok(!container.isDragging && !markup.isDragging,
     "The dragging has stopped");
  ok(!markup.doc.body.classList.contains("dragging"),
     "The dragging css class was removed");
});
