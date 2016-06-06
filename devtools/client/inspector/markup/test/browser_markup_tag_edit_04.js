/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a node can be deleted from the markup-view with the delete key.
// Also checks that after deletion the correct element is highlighted.
// The next sibling is preferred, but the parent is a fallback.

const HTML = `<style type="text/css">
                #pseudo::before { content: 'before'; }
                #pseudo::after { content: 'after'; }
              </style>
              <div id="parent">
                <div id="first"></div>
                <div id="second"></div>
                <div id="third"></div>
              </div>
              <div id="only-child">
                <div id="fourth"></div>
              </div>
              <div id="pseudo">
                <div id="fifth"></div>
              </div>`;
const TEST_URL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

// List of all the test cases. Each item is an object with the following props:
// - selector: the css selector of the node that should be selected
// - key: the key to press to delete the node (delete or back_space)
// - focusedSelector: the css selector of the node we expect to be selected as
//   a result of the deletion
// - pseudo: (optional) if the focused node is actually supposed to be a pseudo element
//   of the specified selector.
// Note that after each test case, undo is called.
const TEST_DATA = [{
  selector: "#first",
  key: "delete",
  focusedSelector: "#second"
}, {
  selector: "#second",
  key: "delete",
  focusedSelector: "#third"
}, {
  selector: "#third",
  key: "delete",
  focusedSelector: "#second"
}, {
  selector: "#fourth",
  key: "delete",
  focusedSelector: "#only-child"
}, {
  selector: "#fifth",
  key: "delete",
  focusedSelector: "#pseudo",
  pseudo: "after"
}, {
  selector: "#first",
  key: "back_space",
  focusedSelector: "#second"
}, {
  selector: "#second",
  key: "back_space",
  focusedSelector: "#first"
}, {
  selector: "#third",
  key: "back_space",
  focusedSelector: "#second"
}, {
  selector: "#fourth",
  key: "back_space",
  focusedSelector: "#only-child"
}, {
  selector: "#fifth",
  key: "back_space",
  focusedSelector: "#pseudo",
  pseudo: "before"
}];

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  for (let data of TEST_DATA) {
    yield checkDeleteAndSelection(inspector, data);
  }
});

function* checkDeleteAndSelection(inspector, {key, selector, focusedSelector, pseudo}) {
  info("Test deleting node " + selector + " with " + key + ", " +
       "expecting " + focusedSelector + " to be focused");

  info("Select node " + selector + " and make sure it is focused");
  yield selectNode(selector, inspector);
  yield clickContainer(selector, inspector);

  info("Delete the node with: " + key);
  let mutated = inspector.once("markupmutation");
  EventUtils.sendKey(key, inspector.panelWin);
  yield Promise.all([mutated, inspector.once("inspector-updated")]);

  let nodeFront = yield getNodeFront(focusedSelector, inspector);
  if (pseudo) {
    // Update the selector for logging in case of failure.
    focusedSelector = focusedSelector + "::" + pseudo;
    // Retrieve the :before or :after pseudo element of the nodeFront.
    let {nodes} = yield inspector.walker.children(nodeFront);
    nodeFront = pseudo === "before" ? nodes[0] : nodes[nodes.length - 1];
  }

  is(inspector.selection.nodeFront, nodeFront,
     focusedSelector + " is selected after deletion");

  info("Check that the node was really removed");
  let node = yield getNodeFront(selector, inspector);
  ok(!node, "The node can't be found in the page anymore");

  info("Undo the deletion to restore the original markup");
  yield undoChange(inspector);
  node = yield getNodeFront(selector, inspector);
  ok(node, "The node is back");
}
