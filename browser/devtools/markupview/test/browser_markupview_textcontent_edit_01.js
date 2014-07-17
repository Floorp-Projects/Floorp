/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing a node's text content

const TEST_URL = TEST_URL_ROOT + "doc_markup_edit.html";

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Expanding all nodes");
  yield inspector.markup.expandAll();
  yield waitForMultipleChildrenUpdates(inspector);

  let node = getNode(".node6").firstChild;
  is(node.nodeValue, "line6", "The test node's text content is correct");

  info("Changing the text content");

  info("Listening to the markupmutation event");
  let onMutated = inspector.once("markupmutation");
  let container = yield getContainerForSelector(".node6", inspector);
  let field = container.elt.querySelector("pre");
  setEditableFieldValue(field, "New text", inspector);
  yield onMutated;

  is(node.nodeValue, "New text", "Test test node's text content has changed");

  yield inspector.once("inspector-updated");
});

// The expand all operation of the markup-view calls itself recursively and
// there's not one event we can wait for to know when it's done
function* waitForMultipleChildrenUpdates(inspector) {
  // As long as child updates are queued up while we wait for an update already
  // wait again
  if (inspector.markup._queuedChildUpdates &&
      inspector.markup._queuedChildUpdates.size) {
    yield waitForChildrenUpdated(inspector);
    return yield waitForMultipleChildrenUpdates(inspector);
  }
}
