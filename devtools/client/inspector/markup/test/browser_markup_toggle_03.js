/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test toggling (expand/collapse) elements by alt-clicking on twisties, which
// should expand/collapse all the descendants

const TEST_URL = URL_ROOT + "doc_markup_toggle.html";

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);

  info("Getting the container for the UL parent element");
  const container = await getContainerForSelector("ul", inspector);

  info("Alt-clicking on collapsed expander should expand all children");
  await toggleContainerByClick(inspector, container, { altKey: true });

  info("Checking that all nodes exist and are expanded");
  let nodeFronts = await getNodeFronts(inspector);
  for (const nodeFront of nodeFronts) {
    const nodeContainer = getContainerForNodeFront(nodeFront, inspector);
    ok(nodeContainer, "Container for node " + nodeFront.tagName + " exists");
    ok(
      nodeContainer.expanded,
      "Container for node " + nodeFront.tagName + " is expanded"
    );
  }

  info("Alt-clicking on expanded expander should collapse all children");
  await toggleContainerByClick(inspector, container, { altKey: true });

  info("Checking that all nodes are collapsed");
  nodeFronts = await getNodeFronts(inspector);
  for (const nodeFront of nodeFronts) {
    const nodeContainer = getContainerForNodeFront(nodeFront, inspector);
    ok(
      !nodeContainer.expanded,
      "Container for node " + nodeFront.tagName + " is collapsed"
    );
  }
});

async function getNodeFronts(inspector) {
  const nodeList = await inspector.walker.querySelectorAll(
    inspector.walker.rootNode,
    "ul, li, span, em"
  );
  return nodeList.items();
}
