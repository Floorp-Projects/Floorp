/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test drag and dropping a node before a ::marker pseudo.

const TEST_URL = URL_ROOT + "doc_markup_dragdrop.html";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Expand #list node");
  const parentFront = await getNodeFront("#list", inspector);
  await inspector.markup.expandNode(parentFront.parentNode());
  await inspector.markup.expandNode(parentFront);
  await waitForMultipleChildrenUpdates(inspector);

  info("Scroll #list into view");
  const parentContainer = await getContainerForNodeFront(parentFront, inspector);
  parentContainer.elt.scrollIntoView(true);

  info("Test placing an element before a ::marker psuedo");
  await moveElementBeforeMarker("#last-list-child", parentFront, inspector);
  const childNodes = await getChildrenOf(parentFront, inspector);
  is(childNodes[0], "_moz_generated_content_marker",
     "::marker is still the first child of #list");
  is(childNodes[1], "last-list-child",
     "#last-list-child is now the second child of #list");
  is(childNodes[2], "first-list-child",
     "#first-list-child is now the last child of #list");
});

async function moveElementBeforeMarker(selector, parentFront, inspector) {
  info(`Placing ${selector} before its parent's ::marker`);

  const container = await getContainerForSelector(selector, inspector);
  const parentContainer = await getContainerForNodeFront(parentFront, inspector);
  const offsetY = (parentContainer.tagLine.offsetTop +
    parentContainer.tagLine.offsetHeight) - container.tagLine.offsetTop;

  const onMutated = inspector.once("markupmutation");
  const uiUpdate = inspector.once("inspector-updated");

  await simulateNodeDragAndDrop(inspector, selector, 0, offsetY);

  const mutations = await onMutated;
  await uiUpdate;

  is(mutations.length, 2, "2 mutations were received");
}

async function getChildrenOf(parentFront, {walker}) {
  const {nodes} = await walker.children(parentFront);
  return nodes.map(node => {
    if (node.isMarkerPseudoElement) {
      return node.displayName;
    }
    return node.id;
  });
}
