/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if the profiler's tree view implementation works properly and
 * can toggle categories hidden or visible.
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
  treeRoot.attachTo(container);

  let categories = container.querySelectorAll(".call-tree-category");
  is(categories.length, 6,
    "The call tree displays a correct number of categories.");
  ok(!container.hasAttribute("categories-hidden"),
    "All categories should be visible in the tree.");

  treeRoot.toggleCategories(false);
  is(categories.length, 6,
    "The call tree displays the same number of categories.");
  ok(container.hasAttribute("categories-hidden"),
    "All categories should now be hidden in the tree.");
});
