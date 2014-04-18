/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling (expand/collapse) elements by alt-clicking on twisties, which
// should expand all the descendants

const TEST_URL = TEST_URL_ROOT + "doc_markup_toggle.html";

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Getting the container for the UL parent element");
  let container = getContainerForRawNode("ul", inspector);

  info("Alt-clicking on the UL parent expander, and waiting for children");
  let onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.expander, {altKey: true},
    inspector.markup.doc.defaultView);
  yield onUpdated;
  yield waitForMultipleChildrenUpdates(inspector);

  info("Checking that all nodes exist and are expanded");
  for (let node of content.document.querySelectorAll("ul, li, span, em")) {
    let nodeContainer = getContainerForRawNode(node, inspector);
    ok(nodeContainer, "Container for node " + node.tagName + " exists");
    ok(nodeContainer.expanded,
      "Container for node " + node.tagName + " is expanded");
  }
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