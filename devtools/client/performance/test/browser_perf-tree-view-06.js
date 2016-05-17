/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler's tree view implementation works properly and
 * correctly emits events when certain DOM nodes are clicked.
 */

const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
const { CallView } = require("devtools/client/performance/modules/widgets/tree-view");
const { synthesizeProfile } = require("devtools/client/performance/test/helpers/synth-utils");
const { idleWait, waitUntil } = require("devtools/client/performance/test/helpers/wait-utils");

add_task(function* () {
  let profile = synthesizeProfile();
  let threadNode = new ThreadNode(profile.threads[0], { startTime: 0, endTime: 20 });

  // Don't display the synthesized (root) and the real (root) node twice.
  threadNode.calls = threadNode.calls[0].calls;

  let treeRoot = new CallView({ frame: threadNode });
  let container = document.createElement("vbox");
  treeRoot.attachTo(container);

  let A = treeRoot.getChild();
  let B = A.getChild();
  let D = B.getChild();

  let linkEvent = null;
  let handler = (_, e) => linkEvent = e;

  treeRoot.on("link", handler);

  // Fire right click.
  rightMousedown(D.target.querySelector(".call-tree-url"));

  // Ensure link was not called for right click.
  yield idleWait(100);
  ok(!linkEvent, "The `link` event not fired for right click.");

  // Fire left click.
  mousedown(D.target.querySelector(".call-tree-url"));

  // Ensure link was called for left click.
  yield waitUntil(() => linkEvent);
  is(linkEvent, D, "The `link` event target is correct.");

  treeRoot.off("link", handler);
});
