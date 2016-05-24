/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that adding nodes does work as expected: the parent gets expanded, the
// new node gets selected.

const TEST_URL = URL_ROOT + "doc_inspector_add_node.html";
const PARENT_TREE_LEVEL = 3;

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  info("Adding in element that has no children and is collapsed");
  let parentNode = yield getNodeFront("#foo", inspector);
  yield selectNode(parentNode, inspector);
  yield testAddNode(parentNode, inspector);

  info("Adding in element with children but that has not been expanded yet");
  parentNode = yield getNodeFront("#bar", inspector);
  yield selectNode(parentNode, inspector);
  yield testAddNode(parentNode, inspector);

  info("Adding in element with children that has been expanded then collapsed");
  // Select again #bar and collapse it.
  parentNode = yield getNodeFront("#bar", inspector);
  yield selectNode(parentNode, inspector);
  collapseNode(parentNode, inspector);
  yield testAddNode(parentNode, inspector);

  info("Adding in element with children that is expanded");
  parentNode = yield getNodeFront("#bar", inspector);
  yield selectNode(parentNode, inspector);
  yield testAddNode(parentNode, inspector);
});

function* testAddNode(parentNode, inspector) {
  let btn = inspector.panelDoc.querySelector("#inspector-element-add-button");
  let markupWindow = inspector.markup.win;
  let parentContainer = inspector.markup.getContainer(parentNode);

  is(parentContainer.tagLine.getAttribute("aria-level"), PARENT_TREE_LEVEL,
    "Parent level should be up to date.");

  info("Clicking 'add node' and expecting a markup mutation and focus event");
  let onMutation = inspector.once("markupmutation");
  btn.click();
  let mutations = yield onMutation;

  info("Expecting an inspector-updated event right after the mutation event " +
       "to wait for the new node selection");
  yield inspector.once("inspector-updated");

  is(mutations.length, 1, "There is one mutation only");
  is(mutations[0].added.length, 1, "There is one new node only");

  let newNode = mutations[0].added[0];

  is(newNode, inspector.selection.nodeFront,
     "The new node is selected");

  ok(parentContainer.expanded, "The parent node is now expanded");

  is(inspector.selection.nodeFront.parentNode(), parentNode,
     "The new node is inside the right parent");

  let focusedElement = markupWindow.document.activeElement;
  let focusedContainer = focusedElement.container;
  let selectedContainer = inspector.markup._selectedContainer;
  is(selectedContainer.tagLine.getAttribute("aria-level"),
    PARENT_TREE_LEVEL + 1, "Added container level should be up to date.");
  is(selectedContainer.node, inspector.selection.nodeFront,
     "The right container is selected in the markup-view");
  ok(selectedContainer.selected, "Selected container is set to selected");
  is(focusedContainer.toString(), "[root container]",
    "Root container is focused");
}

function collapseNode(node, inspector) {
  let container = inspector.markup.getContainer(node);
  container.setExpanded(false);
}
