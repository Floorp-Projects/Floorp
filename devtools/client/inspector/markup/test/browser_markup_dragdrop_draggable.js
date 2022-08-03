/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test which nodes are consider draggable by the markup-view.

const TEST_URL = URL_ROOT + "doc_markup_dragdrop.html";

// Test cases should be objects with the following properties:
// - node {String|Function} A CSS selector that uniquely identifies the node to
//   be tested. Or a generator function called in a Task that should return the
//   corresponding MarkupContainer object to be tested.
// - draggable {Boolean} Whether or not the node should be draggable.
const TEST_DATA = [
  { node: "head", draggable: false },
  { node: "body", draggable: false },
  { node: "html", draggable: false },
  { node: "style", draggable: true },
  { node: "a", draggable: true },
  { node: "p", draggable: true },
  { node: "input", draggable: true },
  { node: "div", draggable: true },
  {
    async node(inspector) {
      const parentFront = await getNodeFront("#before", inspector);
      const { nodes } = await inspector.walker.children(parentFront);
      // Getting the comment node.
      return getContainerForNodeFront(nodes[1], inspector);
    },
    draggable: true,
  },
  {
    async node(inspector) {
      const parentFront = await getNodeFront("#test", inspector);
      const { nodes } = await inspector.walker.children(parentFront);
      // Getting the ::before pseudo element.
      return getContainerForNodeFront(nodes[0], inspector);
    },
    draggable: false,
  },
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  await inspector.markup.expandAll();

  for (const { node, draggable } of TEST_DATA) {
    let container;
    let name;
    if (typeof node === "string") {
      container = await getContainerForSelector(node, inspector);
      name = node;
    } else {
      container = await node(inspector);
      name = container.toString();
    }

    const status = draggable ? "draggable" : "not draggable";
    info(`Testing ${name}, expecting it to be ${status}`);
    is(container.isDraggable(), draggable, `The node is ${status}`);
  }
});
