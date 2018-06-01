/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that pseudoelements are displayed correctly in the rule view.

const TEST_URI = URL_ROOT + "doc_pseudoelement.html";

add_task(async function() {
  await addTab(TEST_URI);
  const {inspector, view} = await openComputedView();
  await testTopLeft(inspector, view);
});

async function testTopLeft(inspector, view) {
  const node = await getNodeFront("#topleft", inspector.markup);
  await selectNode(node, inspector);
  const float = getComputedViewPropertyValue(view, "float");
  is(float, "left", "The computed view shows the correct float");

  const children = await inspector.markup.walker.children(node);
  is(children.nodes.length, 3, "Element has correct number of children");

  const beforeElement = children.nodes[0];
  await selectNode(beforeElement, inspector);
  let top = getComputedViewPropertyValue(view, "top");
  is(top, "0px", "The computed view shows the correct top");
  let left = getComputedViewPropertyValue(view, "left");
  is(left, "0px", "The computed view shows the correct left");

  const afterElement = children.nodes[children.nodes.length - 1];
  await selectNode(afterElement, inspector);
  top = getComputedViewPropertyValue(view, "top");
  is(top, "96px", "The computed view shows the correct top");
  left = getComputedViewPropertyValue(view, "left");
  is(left, "96px", "The computed view shows the correct left");
}
