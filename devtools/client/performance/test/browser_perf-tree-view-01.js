/* Any copyright is dedicated to the Public Domain.
   http://foo/bar/creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler's tree view implementation works properly and
 * creates the correct column structure.
 */

const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
const { CallView } = require("devtools/client/performance/modules/widgets/tree-view");
const { synthesizeProfile } = require("devtools/client/performance/test/helpers/synth-utils");

add_task(function () {
  let profile = synthesizeProfile();
  let threadNode = new ThreadNode(profile.threads[0], { startTime: 0, endTime: 20 });

  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;

  let treeRoot = new CallView({ frame: threadNode });
  let container = document.createElement("vbox");
  treeRoot.autoExpandDepth = 0;
  treeRoot.attachTo(container);

  is(container.childNodes.length, 1,
    "The container node should have one child available.");
  is(container.childNodes[0].className, "call-tree-item",
    "The root node in the tree has the correct class name.");

  is(container.childNodes[0].childNodes.length, 6,
    "The root node in the tree has the correct number of children.");
  is(container.childNodes[0].querySelectorAll(".call-tree-cell").length, 6,
    "The root node in the tree has only 6 'call-tree-cell' children.");

  is(container.childNodes[0].childNodes[0].getAttribute("type"), "duration",
    "The root node in the tree has a duration cell.");
  is(container.childNodes[0].childNodes[0].textContent.trim(), "20 ms",
    "The root node in the tree has the correct duration cell value.");

  is(container.childNodes[0].childNodes[1].getAttribute("type"), "percentage",
    "The root node in the tree has a percentage cell.");
  is(container.childNodes[0].childNodes[1].textContent.trim(), "100%",
    "The root node in the tree has the correct percentage cell value.");

  is(container.childNodes[0].childNodes[2].getAttribute("type"), "self-duration",
    "The root node in the tree has a self-duration cell.");
  is(container.childNodes[0].childNodes[2].textContent.trim(), "0 ms",
    "The root node in the tree has the correct self-duration cell value.");

  is(container.childNodes[0].childNodes[3].getAttribute("type"), "self-percentage",
    "The root node in the tree has a self-percentage cell.");
  is(container.childNodes[0].childNodes[3].textContent.trim(), "0%",
    "The root node in the tree has the correct self-percentage cell value.");

  is(container.childNodes[0].childNodes[4].getAttribute("type"), "samples",
    "The root node in the tree has an samples cell.");
  is(container.childNodes[0].childNodes[4].textContent.trim(), "0",
    "The root node in the tree has the correct samples cell value.");

  is(container.childNodes[0].childNodes[5].getAttribute("type"), "function",
    "The root node in the tree has a function cell.");
  is(container.childNodes[0].childNodes[5].style.marginInlineStart, "0px",
    "The root node in the tree has the correct indentation.");
});
