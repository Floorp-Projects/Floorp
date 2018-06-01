/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that whitespace text nodes do show up in the markup-view when needed.

const TEST_URL = URL_ROOT + "doc_markup_whitespace.html";

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);
  const {markup} = inspector;

  await markup.expandAll();

  info("Verify the number of child nodes and child elements in body");

  // Body has 5 element children, but there are 6 text nodes in there too, they come from
  // the HTML file formatting (spaces and carriage returns).
  let {numNodes, numChildren} = await testActor.getNodeInfo("body");
  is(numNodes, 11, "The body node has 11 child nodes (includes text nodes)");
  is(numChildren, 5, "The body node has 5 child elements (only element nodes)");

  // In body, there are only block-level elements, so whitespace text nodes do not have
  // layout, so they should be skipped in the markup-view.
  info("Check that the body's whitespace text node children aren't shown");
  const bodyContainer = markup.getContainer(inspector.selection.nodeFront);
  let childContainers = bodyContainer.getChildContainers();
  is(childContainers.length, 5,
     "Only the element nodes are shown in the markup view");

  // div#inline has 3 element children, but there are 4 text nodes in there too, like in
  // body, they come from spaces and carriage returns in the HTML file.
  info("Verify the number of child nodes and child elements in div#inline");
  ({numNodes, numChildren} = await testActor.getNodeInfo("#inline"));
  is(numNodes, 7, "The div#inline node has 7 child nodes (includes text nodes)");
  is(numChildren, 3, "The div#inline node has 3 child elements (only element nodes)");

  // Within the inline formatting context in div#inline, the whitespace text nodes between
  // the images have layout, so they should appear in the markup-view.
  info("Check that the div#inline's whitespace text node children are shown");
  await selectNode("#inline", inspector);
  let divContainer = markup.getContainer(inspector.selection.nodeFront);
  childContainers = divContainer.getChildContainers();
  is(childContainers.length, 5,
     "Both the element nodes and some text nodes are shown in the markup view");

  // div#pre has 2 element children, but there are 3 text nodes in there too, like in
  // div#inline, they come from spaces and carriage returns in the HTML file.
  info("Verify the number of child nodes and child elements in div#pre");
  ({numNodes, numChildren} = await testActor.getNodeInfo("#pre"));
  is(numNodes, 5, "The div#pre node has 5 child nodes (includes text nodes)");
  is(numChildren, 2, "The div#pre node has 2 child elements (only element nodes)");

  // Within the inline formatting context in div#pre, the whitespace text nodes between
  // the images have layout, so they should appear in the markup-view, but since
  // white-space is set to pre, then the whitespace text nodes before and after the first
  // and last image should also appear.
  info("Check that the div#pre's whitespace text node children are shown");
  await selectNode("#pre", inspector);
  divContainer = markup.getContainer(inspector.selection.nodeFront);
  childContainers = divContainer.getChildContainers();
  is(childContainers.length, 5,
     "Both the element nodes and all text nodes are shown in the markup view");
});
