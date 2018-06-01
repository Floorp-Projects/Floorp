/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that pseudo-elements, anonymous nodes and slotted nodes are not draggable.

const TEST_URL = URL_ROOT + "doc_markup_dragdrop.html";

add_task(async function() {
  await pushPref("devtools.inspector.showAllAnonymousContent", true);
  await pushPref("dom.webcomponents.shadowdom.enabled", true);
  await pushPref("dom.webcomponents.customelements.enabled", true);

  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Expanding nodes below #test");
  const parentFront = await getNodeFront("#test", inspector);
  await inspector.markup.expandNode(parentFront);
  await waitForMultipleChildrenUpdates(inspector);

  info("Getting the ::before pseudo element and selecting it");
  const parentContainer = await getContainerForNodeFront(parentFront, inspector);
  const beforePseudo = parentContainer.elt.children[1].firstChild.container;
  parentContainer.elt.scrollIntoView(true);
  await selectNode(beforePseudo.node, inspector);

  info("Simulate dragging the ::before pseudo element");
  await simulateNodeDrag(inspector, beforePseudo);

  ok(!beforePseudo.isDragging, "::before pseudo element isn't dragging");

  info("Expanding nodes below #anonymousParent");
  const inputFront = await getNodeFront("#anonymousParent", inspector);
  await inspector.markup.expandNode(inputFront);
  await waitForMultipleChildrenUpdates(inspector);

  info("Getting the anonymous node and selecting it");
  const inputContainer = await getContainerForNodeFront(inputFront, inspector);
  const anonymousDiv = inputContainer.elt.children[1].firstChild.container;
  inputContainer.elt.scrollIntoView(true);
  await selectNode(anonymousDiv.node, inspector);

  info("Simulate dragging the anonymous node");
  await simulateNodeDrag(inspector, anonymousDiv);

  ok(!anonymousDiv.isDragging, "anonymous node isn't dragging");

  info("Expanding all nodes below test-component");
  const testComponentFront = await getNodeFront("test-component", inspector);
  await inspector.markup.expandAll(testComponentFront);
  await waitForMultipleChildrenUpdates(inspector);

  info("Getting a slotted node and selecting it");
  // Directly use the markup getContainer API in order to retrieve the slotted container
  // for a given node front.
  const slotted1Front = await getNodeFront(".slotted1", inspector);
  const slottedContainer = inspector.markup.getContainer(slotted1Front, true);
  slottedContainer.elt.scrollIntoView(true);
  await selectNode(slotted1Front, inspector, "no-reason", true);

  info("Simulate dragging the slotted node");
  await simulateNodeDrag(inspector, slottedContainer);

  ok(!slottedContainer.isDragging, "slotted node isn't dragging");
});
