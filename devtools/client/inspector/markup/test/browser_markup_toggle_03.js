/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling (expand/collapse) elements by alt-clicking on twisties, which
// should expand all the descendants

const TEST_URL = TEST_URL_ROOT + "doc_markup_toggle.html";

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Getting the container for the UL parent element");
  let container = yield getContainerForSelector("ul", inspector);

  info("Alt-clicking on the UL parent expander, and waiting for children");
  let onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.expander, {altKey: true},
    inspector.markup.doc.defaultView);
  yield onUpdated;
  yield waitForMultipleChildrenUpdates(inspector);

  info("Checking that all nodes exist and are expanded");
  let nodeList = yield inspector.walker.querySelectorAll(
    inspector.walker.rootNode, "ul, li, span, em");
  let nodeFronts = yield nodeList.items();
  for (let nodeFront of nodeFronts) {
    let nodeContainer = getContainerForNodeFront(nodeFront, inspector);
    ok(nodeContainer, "Container for node " + nodeFront.tagName + " exists");
    ok(nodeContainer.expanded,
      "Container for node " + nodeFront.tagName + " is expanded");
  }
});
