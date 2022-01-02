/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test different kinds of drag and drop node re-ordering.

const TEST_URL = URL_ROOT + "doc_markup_dragdrop.html";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  let ids;

  info("Expand #test node");
  const parentFront = await getNodeFront("#test", inspector);
  await inspector.markup.expandNode(parentFront);
  await waitForMultipleChildrenUpdates(inspector);

  info("Scroll #test into view");
  const parentContainer = await getContainerForNodeFront(
    parentFront,
    inspector
  );
  parentContainer.elt.scrollIntoView(true);

  info("Test putting an element back at its original place");
  await dragElementToOriginalLocation("#firstChild", inspector);
  ids = await getChildrenIDsOf(parentFront, inspector);
  is(ids[0], "firstChild", "#firstChild is still the first child of #test");
  is(ids[1], "middleChild", "#middleChild is still the second child of #test");

  info("Testing switching elements inside their parent");
  await moveElementDown("#firstChild", "#middleChild", inspector);
  ids = await getChildrenIDsOf(parentFront, inspector);
  is(ids[0], "middleChild", "#firstChild is now the second child of #test");
  is(ids[1], "firstChild", "#middleChild is now the first child of #test");

  info("Testing switching elements with a last child");
  await moveElementDown("#firstChild", "#lastChild", inspector);
  ids = await getChildrenIDsOf(parentFront, inspector);
  is(ids[1], "lastChild", "#lastChild is now the second child of #test");
  is(ids[2], "firstChild", "#firstChild is now the last child of #test");

  info("Testing appending element to a parent");
  await moveElementDown("#before", "#test", inspector);
  ids = await getChildrenIDsOf(parentFront, inspector);
  is(ids.length, 4, "New element appended to #test");
  is(
    ids[0],
    "before",
    "New element is appended at the right place (currently first child)"
  );

  info("Testing moving element to after it's parent");
  await moveElementDown("#firstChild", "#test", inspector);
  ids = await getChildrenIDsOf(parentFront, inspector);
  is(ids.length, 3, "#firstChild is no longer #test's child");
  const siblingFront = await inspector.walker.nextSibling(parentFront);
  is(
    siblingFront.id,
    "firstChild",
    "#firstChild is now #test's nextElementSibling"
  );
});

async function dragElementToOriginalLocation(selector, inspector) {
  info("Picking up and putting back down " + selector);

  function onMutation() {
    ok(false, "Mutation received from dragging a node back to its location");
  }
  inspector.on("markupmutation", onMutation);
  await simulateNodeDragAndDrop(inspector, selector, 0, 0);

  // Wait a bit to make sure the event never fires.
  // This doesn't need to catch *all* cases, since the mutation
  // will cause failure later in the test when it checks element ordering.
  await wait(500);
  inspector.off("markupmutation", onMutation);
}

async function moveElementDown(selector, next, inspector) {
  info("Switching " + selector + " with " + next);

  const container = await getContainerForSelector(next, inspector);
  const height = container.tagLine.getBoundingClientRect().height;

  const onMutated = inspector.once("markupmutation");
  const uiUpdate = inspector.once("inspector-updated");

  await simulateNodeDragAndDrop(inspector, selector, 0, Math.round(height) + 2);

  const mutations = await onMutated;
  await uiUpdate;

  is(mutations.length, 2, "2 mutations were received");
}

async function getChildrenIDsOf(parentFront, { walker }) {
  const { nodes } = await walker.children(parentFront);
  // Filter out non-element nodes since children also returns pseudo-elements.
  return nodes
    .filter(node => {
      return !node.isPseudoElement;
    })
    .map(node => {
      return node.id;
    });
}
