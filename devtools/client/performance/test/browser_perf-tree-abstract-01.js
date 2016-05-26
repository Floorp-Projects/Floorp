/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the abstract tree base class for the profiler's tree view
 * works as advertised.
 */

const { appendAndWaitForPaint } = require("devtools/client/performance/test/helpers/dom-utils");
const { synthesizeCustomTreeClass } = require("devtools/client/performance/test/helpers/synth-utils");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { MyCustomTreeItem, myDataSrc } = synthesizeCustomTreeClass();

  let container = document.createElement("vbox");
  yield appendAndWaitForPaint(gBrowser.selectedBrowser.parentNode, container);

  // Populate the tree and test the root item...

  let treeRoot = new MyCustomTreeItem(myDataSrc, { parent: null });
  treeRoot.attachTo(container);

  ok(!treeRoot.expanded,
    "The root node should not be expanded yet.");
  ok(!treeRoot.populated,
    "The root node should not be populated yet.");

  is(container.childNodes.length, 1,
    "The container node should have one child available.");
  is(container.childNodes[0], treeRoot.target,
    "The root node's target is a child of the container node.");

  is(treeRoot.root, treeRoot,
    "The root node has the correct root.");
  is(treeRoot.parent, null,
    "The root node has the correct parent.");
  is(treeRoot.level, 0,
    "The root node has the correct level.");
  is(treeRoot.target.style.marginInlineStart, "0px",
    "The root node's indentation is correct.");
  is(treeRoot.target.textContent, "root",
    "The root node's text contents are correct.");
  is(treeRoot.container, container,
    "The root node's container is correct.");

  // Expand the root and test the child items...

  let receivedExpandEvent = once(treeRoot, "expand", { spreadArgs: true });
  let receivedFocusEvent = once(treeRoot, "focus");
  mousedown(treeRoot.target.querySelector(".arrow"));

  let [_, eventItem] = yield receivedExpandEvent;
  is(eventItem, treeRoot,
    "The 'expand' event target is correct (1).");

  yield receivedFocusEvent;
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is now focused.");

  let fooItem = treeRoot.getChild(0);
  let barItem = treeRoot.getChild(1);

  is(container.childNodes.length, 3,
    "The container node should now have three children available.");
  is(container.childNodes[0], treeRoot.target,
    "The root node's target is a child of the container node.");
  is(container.childNodes[1], fooItem.target,
    "The 'foo' node's target is a child of the container node.");
  is(container.childNodes[2], barItem.target,
    "The 'bar' node's target is a child of the container node.");

  is(fooItem.root, treeRoot,
    "The 'foo' node has the correct root.");
  is(fooItem.parent, treeRoot,
    "The 'foo' node has the correct parent.");
  is(fooItem.level, 1,
    "The 'foo' node has the correct level.");
  is(fooItem.target.style.marginInlineStart, "10px",
    "The 'foo' node's indentation is correct.");
  is(fooItem.target.textContent, "foo",
    "The 'foo' node's text contents are correct.");
  is(fooItem.container, container,
    "The 'foo' node's container is correct.");

  is(barItem.root, treeRoot,
    "The 'bar' node has the correct root.");
  is(barItem.parent, treeRoot,
    "The 'bar' node has the correct parent.");
  is(barItem.level, 1,
    "The 'bar' node has the correct level.");
  is(barItem.target.style.marginInlineStart, "10px",
    "The 'bar' node's indentation is correct.");
  is(barItem.target.textContent, "bar",
    "The 'bar' node's text contents are correct.");
  is(barItem.container, container,
    "The 'bar' node's container is correct.");

  // Test clicking on the `foo` node...

  receivedFocusEvent = once(treeRoot, "focus", { spreadArgs: true });
  mousedown(fooItem.target);

  [_, eventItem] = yield receivedFocusEvent;
  is(eventItem, fooItem,
    "The 'focus' event target is correct (2).");
  is(document.commandDispatcher.focusedElement, fooItem.target,
    "The 'foo' node is now focused.");

  // Test double clicking on the `bar` node...

  receivedExpandEvent = once(treeRoot, "expand", { spreadArgs: true });
  receivedFocusEvent = once(treeRoot, "focus");
  dblclick(barItem.target);

  [_, eventItem] = yield receivedExpandEvent;
  is(eventItem, barItem,
    "The 'expand' event target is correct (3).");

  yield receivedFocusEvent;
  is(document.commandDispatcher.focusedElement, barItem.target,
    "The 'foo' node is now focused.");

  // A child item got expanded, test the descendants...

  let bazItem = barItem.getChild(0);

  is(container.childNodes.length, 4,
    "The container node should now have four children available.");
  is(container.childNodes[0], treeRoot.target,
    "The root node's target is a child of the container node.");
  is(container.childNodes[1], fooItem.target,
    "The 'foo' node's target is a child of the container node.");
  is(container.childNodes[2], barItem.target,
    "The 'bar' node's target is a child of the container node.");
  is(container.childNodes[3], bazItem.target,
    "The 'baz' node's target is a child of the container node.");

  is(bazItem.root, treeRoot,
    "The 'baz' node has the correct root.");
  is(bazItem.parent, barItem,
    "The 'baz' node has the correct parent.");
  is(bazItem.level, 2,
    "The 'baz' node has the correct level.");
  is(bazItem.target.style.marginInlineStart, "20px",
    "The 'baz' node's indentation is correct.");
  is(bazItem.target.textContent, "baz",
    "The 'baz' node's text contents are correct.");
  is(bazItem.container, container,
    "The 'baz' node's container is correct.");

  container.remove();
});
