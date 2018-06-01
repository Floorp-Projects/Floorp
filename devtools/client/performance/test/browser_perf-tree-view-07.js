/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler's tree view implementation works properly and
 * has the correct 'root', 'parent', 'level' etc. accessors on child nodes.
 */

const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
const { CallView } = require("devtools/client/performance/modules/widgets/tree-view");
const { synthesizeProfile } = require("devtools/client/performance/test/helpers/synth-utils");

add_task(function() {
  const profile = synthesizeProfile();
  const threadNode = new ThreadNode(profile.threads[0], { startTime: 0, endTime: 20 });

  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;

  const treeRoot = new CallView({ frame: threadNode });
  const container = document.createElement("vbox");
  container.id = "call-tree-container";
  treeRoot.attachTo(container);

  const A = treeRoot.getChild();
  const B = A.getChild();
  const D = B.getChild();

  is(D.root, treeRoot,
    "The .A.B.D node has the correct root.");
  is(D.parent, B,
    "The .A.B.D node has the correct parent.");
  is(D.level, 3,
    "The .A.B.D node has the correct level.");
  is(D.target.className, "call-tree-item",
    "The .A.B.D node has the correct target node.");
  is(D.container.id, "call-tree-container",
    "The .A.B.D node has the correct container node.");
});
