/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the abstract tree base class for the profiler's tree view
 * is keyboard accessible.
 */

const { appendAndWaitForPaint } = require("devtools/client/performance/test/helpers/dom-utils");
const { synthesizeCustomTreeClass } = require("devtools/client/performance/test/helpers/synth-utils");

add_task(async function() {
  const { MyCustomTreeItem, myDataSrc } = synthesizeCustomTreeClass();

  const container = document.createElement("vbox");
  await appendAndWaitForPaint(gBrowser.selectedBrowser.parentNode, container);

  // Populate the tree by pressing RIGHT...

  const treeRoot = new MyCustomTreeItem(myDataSrc, { parent: null });
  treeRoot.attachTo(container);
  treeRoot.focus();

  key("VK_RIGHT");
  ok(treeRoot.expanded,
    "The root node is now expanded.");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is still focused.");

  const fooItem = treeRoot.getChild(0);
  const barItem = treeRoot.getChild(1);

  key("VK_RIGHT");
  ok(!fooItem.expanded,
    "The 'foo' node is not expanded yet.");
  is(document.commandDispatcher.focusedElement, fooItem.target,
    "The 'foo' node is now focused.");

  key("VK_RIGHT");
  ok(fooItem.expanded,
    "The 'foo' node is now expanded.");
  is(document.commandDispatcher.focusedElement, fooItem.target,
    "The 'foo' node is still focused.");

  key("VK_RIGHT");
  ok(!barItem.expanded,
    "The 'bar' node is not expanded yet.");
  is(document.commandDispatcher.focusedElement, barItem.target,
    "The 'bar' node is now focused.");

  key("VK_RIGHT");
  ok(barItem.expanded,
    "The 'bar' node is now expanded.");
  is(document.commandDispatcher.focusedElement, barItem.target,
    "The 'bar' node is still focused.");

  const bazItem = barItem.getChild(0);

  key("VK_RIGHT");
  ok(!bazItem.expanded,
    "The 'baz' node is not expanded yet.");
  is(document.commandDispatcher.focusedElement, bazItem.target,
    "The 'baz' node is now focused.");

  key("VK_RIGHT");
  ok(bazItem.expanded,
    "The 'baz' node is now expanded.");
  is(document.commandDispatcher.focusedElement, bazItem.target,
    "The 'baz' node is still focused.");

  // Test RIGHT on a leaf node.

  key("VK_RIGHT");
  is(document.commandDispatcher.focusedElement, bazItem.target,
    "The 'baz' node is still focused.");

  // Test DOWN on a leaf node.

  key("VK_DOWN");
  is(document.commandDispatcher.focusedElement, bazItem.target,
    "The 'baz' node is now refocused.");

  // Test UP.

  key("VK_UP");
  is(document.commandDispatcher.focusedElement, barItem.target,
    "The 'bar' node is now refocused.");

  key("VK_UP");
  is(document.commandDispatcher.focusedElement, fooItem.target,
    "The 'foo' node is now refocused.");

  key("VK_UP");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is now refocused.");

  // Test DOWN.

  key("VK_DOWN");
  is(document.commandDispatcher.focusedElement, fooItem.target,
    "The 'foo' node is now refocused.");

  key("VK_DOWN");
  is(document.commandDispatcher.focusedElement, barItem.target,
    "The 'bar' node is now refocused.");

  key("VK_DOWN");
  is(document.commandDispatcher.focusedElement, bazItem.target,
    "The 'baz' node is now refocused.");

  // Test LEFT.

  key("VK_LEFT");
  ok(barItem.expanded,
    "The 'bar' node is still expanded.");
  is(document.commandDispatcher.focusedElement, barItem.target,
    "The 'bar' node is now refocused.");

  key("VK_LEFT");
  ok(!barItem.expanded,
    "The 'bar' node is not expanded anymore.");
  is(document.commandDispatcher.focusedElement, barItem.target,
    "The 'bar' node is still focused.");

  key("VK_LEFT");
  ok(treeRoot.expanded,
    "The root node is still expanded.");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is now refocused.");

  key("VK_LEFT");
  ok(!treeRoot.expanded,
    "The root node is not expanded anymore.");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is still focused.");

  // Test LEFT on the root node.

  key("VK_LEFT");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is still focused.");

  // Test UP on the root node.

  key("VK_UP");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is still focused.");

  container.remove();
});
