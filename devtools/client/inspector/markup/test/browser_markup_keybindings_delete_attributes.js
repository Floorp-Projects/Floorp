/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that attributes can be deleted from the markup-view with the delete key
// when they are focused.

const HTML = '<div id="id" class="class" data-id="id"></div>';
const TEST_URL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

// List of all the test cases. Each item is an object with the following props:
// - selector: the css selector of the node that should be selected
// - attribute: the name of the attribute that should be focused. Do not
//   specify an attribute that would make it impossible to find the node using
//   selector.
// Note that after each test case, undo is called.
const TEST_DATA = [{
  selector: "#id",
  attribute: "class"
}, {
  selector: "#id",
  attribute: "data-id"
}];

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);
  let {walker} = inspector;

  for (let {selector, attribute} of TEST_DATA) {
    info("Get the container for node " + selector);
    let {editor} = yield getContainerForSelector(selector, inspector);

    info("Focus attribute " + attribute);
    let attr = editor.attrElements.get(attribute).querySelector(".editable");
    attr.focus();

    info("Delete the attribute by pressing delete");
    let mutated = inspector.once("markupmutation");
    EventUtils.sendKey("delete", inspector.panelWin);
    yield mutated;

    info("Check that the node is still here");
    let node = yield walker.querySelector(walker.rootNode, selector);
    ok(node, "The node hasn't been deleted");

    info("Check that the attribute has been deleted");
    node = yield walker.querySelector(walker.rootNode,
                                      selector + "[" + attribute + "]");
    ok(!node, "The attribute does not exist anymore in the DOM");
    ok(!editor.attrElements.get(attribute),
       "The attribute has been removed from the container");

    info("Undo the change");
    yield undoChange(inspector);
    node = yield walker.querySelector(walker.rootNode,
                                      selector + "[" + attribute + "]");
    ok(node, "The attribute is back in the DOM");
    ok(editor.attrElements.get(attribute),
       "The attribute is back on the container");
  }
});
