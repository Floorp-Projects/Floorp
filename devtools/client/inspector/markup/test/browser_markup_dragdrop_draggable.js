/* vim: set ts=2 et sw=2 tw=80: */
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
    node: function* (inspector) {
      let parentFront = yield getNodeFront("#before", inspector);
      let {nodes} = yield inspector.walker.children(parentFront);
      // Getting the comment node.
      return getContainerForNodeFront(nodes[1], inspector);
    },
    draggable: true
  },
  {
    node: function* (inspector) {
      let parentFront = yield getNodeFront("#test", inspector);
      let {nodes} = yield inspector.walker.children(parentFront);
      // Getting the ::before pseudo element.
      return getContainerForNodeFront(nodes[0], inspector);
    },
    draggable: false
  }
];

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);
  yield inspector.markup.expandAll();

  for (let {node, draggable} of TEST_DATA) {
    let container;
    let name;
    if (typeof node === "string") {
      container = yield getContainerForSelector(node, inspector);
      name = node;
    } else {
      container = yield node(inspector);
      name = container.toString();
    }

    let status = draggable ? "draggable" : "not draggable";
    info(`Testing ${name}, expecting it to be ${status}`);
    is(container.isDraggable(), draggable, `The node is ${status}`);
  }
});
