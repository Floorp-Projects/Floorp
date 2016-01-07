/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a node can be deleted from the markup-view with the delete key.
// Also checks that after deletion the correct element is highlighted.
// The next sibling is preferred, but the parent is a fallback.

const HTML = `<div id="parent">
                <div id="first"></div>
                <div id="second"></div>
                <div id="third"></div>
              </div>`;
const TEST_URL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

// List of all the test cases. Each item is an object with the following props:
// - selector: the css selector of the node that should be selected
// - key: the key to press to delete the node (delete or back_space)
// - focusedSelector: the css selector of the node we expect to be selected as
//   a result of the deletion
// - setup: an optional function that will be run before selecting and deleting
//   the node
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
  setup: function*(inspector) {
    // Removing the siblings of #first in order to test with an only child.
    let mutated = inspector.once("markupmutation");
    for (let node of content.document.querySelectorAll("#second, #third")) {
      node.remove();
    }
    yield mutated;
  },
  selector: "#first",
  key: "delete",
  focusedSelector: "#parent"
}, {
  selector: "#first",
  key: "back_space",
  focusedSelector: "#parent"
}];

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  for (let {setup, selector, key, focusedSelector} of TEST_DATA) {
    if (setup) {
      yield setup(inspector);
    }

    yield checkDeleteAndSelection(inspector, key, selector, focusedSelector);
  }
});

function* checkDeleteAndSelection(inspector, key, selector, focusedSelector) {
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
