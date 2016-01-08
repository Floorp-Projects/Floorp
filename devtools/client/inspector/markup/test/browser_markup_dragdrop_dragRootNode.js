/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the root node isn't draggable (as well as head and body).

const TEST_URL = TEST_URL_ROOT + "doc_markup_dragdrop.html";
const TEST_DATA = ["html", "head", "body"];

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  for (let selector of TEST_DATA) {
    info("Try to drag/drop node " + selector);
    yield simulateNodeDrag(inspector, selector);

    let container = yield getContainerForSelector(selector, inspector);
    ok(!container.isDragging, "The container hasn't been marked as dragging");
  }
});
