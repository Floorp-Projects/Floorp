/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the treeview expander arrow doesn't react to dblclick events.
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

  let originalTreeRootExpandedState = treeRoot.expanded;
  info("Double clicking on the root item arrow and waiting for focus event.");

  let receivedFocusEvent = once(treeRoot, "focus");
  dblclick(treeRoot.target.querySelector(".arrow"));
  yield receivedFocusEvent;

  is(treeRoot.expanded, originalTreeRootExpandedState,
    "A double click on the arrow was ignored.");

  container.remove();
});
