/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that adding nodes does work as expected: the parent node remains selected and the
// new node is created inside the parent.

const TEST_URL = URL_ROOT + "doc_inspector_add_node.html";
const PARENT_TREE_LEVEL = 3;

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Adding a node in an element that has no children and is collapsed");
  let parentNode = await getNodeFront("#foo", inspector);
  await selectNode(parentNode, inspector);
  await testAddNode(parentNode, inspector);

  info("Adding a node in an element with children but that has not been expanded yet");
  parentNode = await getNodeFront("#bar", inspector);
  await selectNode(parentNode, inspector);
  await testAddNode(parentNode, inspector);

  info("Adding a node in an element with children that has been expanded then collapsed");
  // Select again #bar and collapse it.
  parentNode = await getNodeFront("#bar", inspector);
  await selectNode(parentNode, inspector);
  collapseNode(parentNode, inspector);
  await testAddNode(parentNode, inspector);

  info("Adding a node in an element with children that is expanded");
  parentNode = await getNodeFront("#bar", inspector);
  await selectNode(parentNode, inspector);
  await testAddNode(parentNode, inspector);
});

async function testAddNode(parentNode, inspector) {
  const btn = inspector.panelDoc.querySelector("#inspector-element-add-button");
  const parentContainer = inspector.markup.getContainer(parentNode);

  is(parentContainer.tagLine.getAttribute("aria-level"), PARENT_TREE_LEVEL,
     "The parent aria-level is up to date.");

  info("Clicking 'add node' and expecting a markup mutation and a new container");
  const onMutation = inspector.once("markupmutation");
  const onNewContainer = inspector.once("container-created");
  btn.click();
  let mutations = await onMutation;
  await onNewContainer;

  // We are only interested in childList mutations here. Filter everything else out as
  // there may be unrelated mutations (e.g. "events") grouped in.
  mutations = mutations.filter(({ type }) => type === "childList");

  is(mutations.length, 1, "There is one mutation only");
  is(mutations[0].added.length, 1, "There is one new node only");

  const newNode = mutations[0].added[0];

  is(parentNode, inspector.selection.nodeFront, "The parent node is still selected");
  is(newNode.parentNode(), parentNode, "The new node is inside the right parent");

  const newNodeContainer = inspector.markup.getContainer(newNode);

  is(newNodeContainer.tagLine.getAttribute("aria-level"), PARENT_TREE_LEVEL + 1,
     "The child aria-level is up to date.");
}

function collapseNode(node, inspector) {
  const container = inspector.markup.getContainer(node);
  container.setExpanded(false);
}
