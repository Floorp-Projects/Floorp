/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling (expand/collapse) elements by alt-clicking on twisties, which
// should expand/collapse all the descendants

const TEST_URL = URL_ROOT + "doc_markup_toggle.html";

add_task(async function() {
  let {inspector} = await openInspectorForURL(TEST_URL);

  info("Getting the container for the UL parent element");
  let container = await getContainerForSelector("ul", inspector);

  info("Alt-clicking on collapsed expander should expand all children");
  let onUpdated = inspector.once("inspector-updated");
  EventUtils.synthesizeMouseAtCenter(container.expander, {altKey: true},
    inspector.markup.doc.defaultView);
  await onUpdated;
  await waitForMultipleChildrenUpdates(inspector);

  info("Checking that all nodes exist and are expanded");
  let nodeFronts = await getNodeFronts(inspector);
  for (let nodeFront of nodeFronts) {
    let nodeContainer = getContainerForNodeFront(nodeFront, inspector);
    ok(nodeContainer, "Container for node " + nodeFront.tagName + " exists");
    ok(nodeContainer.expanded,
      "Container for node " + nodeFront.tagName + " is expanded");
  }

  info("Alt-clicking on expanded expander should collapse all children");
  EventUtils.synthesizeMouseAtCenter(container.expander, {altKey: true},
    inspector.markup.doc.defaultView);
  await waitForMultipleChildrenUpdates(inspector);
  // No need to wait for inspector-updated here since we are not retrieving new nodes.

  info("Checking that all nodes are collapsed");
  nodeFronts = await getNodeFronts(inspector);
  for (let nodeFront of nodeFronts) {
    let nodeContainer = getContainerForNodeFront(nodeFront, inspector);
    ok(!nodeContainer.expanded,
      "Container for node " + nodeFront.tagName + " is collapsed");
  }
});

async function getNodeFronts(inspector) {
  let nodeList = await inspector.walker.querySelectorAll(
    inspector.walker.rootNode, "ul, li, span, em");
  return nodeList.items();
}
