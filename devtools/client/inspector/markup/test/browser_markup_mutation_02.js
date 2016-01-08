/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that markup-containers in the markup-view do flash when their
// corresponding DOM nodes mutate

const TEST_URL = TEST_URL_ROOT + "doc_markup_flashing.html";

// The test data contains a list of mutations to test.
// Each item is an object:
// - desc: a description of the test step, for better logging
// - mutate: a function that should make changes to the content DOM
// - attribute: if set, the test will expect the corresponding attribute to flash
//   instead of the whole node
// - flashedNode: [optional] the css selector of the node that is expected to
//   flash in the markup-view as a result of the mutation.
//   If missing, the rootNode (".list") will be expected to flash
const TEST_DATA = [{
  desc: "Adding a new node should flash the new node",
  mutate: (doc, rootNode) => {
    let newLi = doc.createElement("LI");
    newLi.textContent = "new list item";
    rootNode.appendChild(newLi);
  },
  flashedNode: ".list li:nth-child(3)"
}, {
  desc: "Removing a node should flash its parent",
  mutate: (doc, rootNode) => {
    rootNode.removeChild(rootNode.lastElementChild);
  }
}, {
  desc: "Re-appending an existing node should only flash this node",
  mutate: (doc, rootNode) => {
    rootNode.appendChild(rootNode.firstElementChild);
  },
  flashedNode: ".list .item:last-child"
}, {
  desc: "Adding an attribute should flash the attribute",
  attribute: "test-name",
  mutate: (doc, rootNode) => {
    rootNode.setAttribute("test-name", "value-" + Date.now());
  }
}, {
  desc: "Editing an attribute should flash the attribute",
  attribute: "class",
  mutate: (doc, rootNode) => {
    rootNode.setAttribute("class", "list value-" + Date.now());
  }
}, {
  desc: "Multiple changes to an attribute should flash the attribute",
  attribute: "class",
  mutate: (doc, rootNode) => {
    rootNode.removeAttribute("class");
    rootNode.setAttribute("class", "list value-" + Date.now());
    rootNode.setAttribute("class", "list value-" + Date.now());
    rootNode.removeAttribute("class");
    rootNode.setAttribute("class", "list value-" + Date.now());
    rootNode.setAttribute("class", "list value-" + Date.now());
  }
}, {
  desc: "Removing an attribute should flash the node",
  mutate: (doc, rootNode) => {
    rootNode.removeAttribute("class");
  }
}];

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  // Make sure mutated nodes flash for a very long time so we can more easily
  // assert they do
  inspector.markup.CONTAINER_FLASHING_DURATION = 1000 * 60 * 60;

  info("Getting the <ul.list> root node to test mutations on");
  let rootNode = getNode(".list");
  let rootNodeFront = yield getNodeFront(".list", inspector);

  info("Selecting the last element of the root node before starting");
  yield selectNode(".list .item:nth-child(2)", inspector);

  for (let {mutate, flashedNode, desc, attribute} of TEST_DATA) {
    info("Starting test: " + desc);

    info("Mutating the DOM and listening for markupmutation event");
    let mutated = inspector.once("markupmutation");
    mutate(content.document, rootNode);
    yield mutated;

    info("Asserting that the correct markup-container is flashing");
    let flashingNodeFront = rootNodeFront;
    if (flashedNode) {
      flashingNodeFront = yield getNodeFront(flashedNode, inspector);
    }

    if (attribute) {
      yield assertAttributeFlashing(flashingNodeFront, attribute, inspector);
    } else {
      yield assertNodeFlashing(flashingNodeFront, inspector);
    }
  }
});

function* assertNodeFlashing(nodeFront, inspector) {
  let container = getContainerForNodeFront(nodeFront, inspector);
  ok(container, "Markup container for node found");
  ok(container.tagState.classList.contains("theme-bg-contrast"),
    "Markup container for node is flashing");

  // Clear the mutation flashing timeout now that we checked the node was flashing
  let markup = inspector.markup;
  clearTimeout(container._flashMutationTimer);
  container._flashMutationTimer = null;
  container.tagState.classList.remove("theme-bg-contrast");
}

function* assertAttributeFlashing(nodeFront, attribute, inspector) {
  let container = getContainerForNodeFront(nodeFront, inspector);
  ok(container, "Markup container for node found");
  ok(container.editor.attrElements.get(attribute), "Attribute exists on editor");

  let attributeElement = container.editor.getAttributeElement(attribute);

  ok(attributeElement.classList.contains("theme-bg-contrast"),
    "Element for " + attribute + " attribute is flashing");

  attributeElement.classList.remove("theme-bg-contrast");
}
