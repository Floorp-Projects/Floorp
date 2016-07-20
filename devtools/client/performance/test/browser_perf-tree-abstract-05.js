/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the abstract tree base class for the profiler's tree view
 * supports PageUp/PageDown/Home/End keys.
 */

const { appendAndWaitForPaint } = require("devtools/client/performance/test/helpers/dom-utils");
const { synthesizeCustomTreeClass } = require("devtools/client/performance/test/helpers/synth-utils");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { MyCustomTreeItem } = synthesizeCustomTreeClass();

  let container = document.createElement("vbox");
  container.style.height = '100%';
  container.style.overflow = 'scroll';
  yield appendAndWaitForPaint(gBrowser.selectedBrowser.parentNode, container);

  let myDataSrc = {
    label: "root",
    children: []
  };

  for (let i = 0; i < 1000; i++) {
    myDataSrc.children.push({
      label: "child-" + i,
      children: []
    });
  }

  let treeRoot = new MyCustomTreeItem(myDataSrc, { parent: null });
  treeRoot.attachTo(container);
  treeRoot.focus();
  treeRoot.expand();

  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The root node is focused.");

  // Test HOME and END

  key("VK_END");
  is(document.commandDispatcher.focusedElement,
    treeRoot.getChild(myDataSrc.children.length - 1).target,
    "The last node is focused.");

  key("VK_HOME");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The first (root) node is focused.");

  // Test PageUp and PageDown

  let nodesPerPageSize = treeRoot._getNodesPerPageSize();

  key("VK_PAGE_DOWN");
  is(document.commandDispatcher.focusedElement,
    treeRoot.getChild(nodesPerPageSize - 1).target,
    "The first node in the second page is focused.");

  key("VK_PAGE_DOWN");
  is(document.commandDispatcher.focusedElement,
    treeRoot.getChild(nodesPerPageSize * 2 - 1).target,
    "The first node in the third page is focused.");

  key("VK_PAGE_UP");
  is(document.commandDispatcher.focusedElement,
    treeRoot.getChild(nodesPerPageSize - 1).target,
    "The first node in the second page is focused.");

  key("VK_PAGE_UP");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The first (root) node is focused.");

  // Test PageUp in the middle of the first page

  let middleIndex = Math.floor(nodesPerPageSize / 2);

  treeRoot.getChild(middleIndex).target.focus();
  is(document.commandDispatcher.focusedElement,
    treeRoot.getChild(middleIndex).target,
    "The middle node in the first page is focused.");

  key("VK_PAGE_UP");
  is(document.commandDispatcher.focusedElement, treeRoot.target,
    "The first (root) node is focused.");

  // Test PageDown in the middle of the last page

  middleIndex = Math.ceil(myDataSrc.children.length - middleIndex);

  treeRoot.getChild(middleIndex).target.focus();
  is(document.commandDispatcher.focusedElement,
    treeRoot.getChild(middleIndex).target,
    "The middle node in the last page is focused.");

  key("VK_PAGE_DOWN");
  is(document.commandDispatcher.focusedElement,
    treeRoot.getChild(myDataSrc.children.length - 1).target,
    "The last node is focused.");

  container.remove();
});
