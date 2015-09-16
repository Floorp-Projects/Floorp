/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a call tree model ignores samples with no timing information.
 */

function run_test() {
  run_next_test();
}

add_task(function test() {
  let { ThreadNode } = require("devtools/performance/tree-model");

  // Create a root node from a given samples array.

  let thread = new ThreadNode(gThread, { startTime: 0, endTime: 10 });
  let root = getFrameNodePath(thread, "(root)");

  // Test the ThreadNode, only node with a duration.
  equal(thread.duration, 10,
    "The correct duration was calculated for the ThreadNode.");

  equal(root.calls.length, 1,
    "The correct number of child calls were calculated for the root node.");
  ok(getFrameNodePath(root, "A"),
    "The root node's only child call is correct.");

  // Test all the descendant nodes.

  equal(getFrameNodePath(root, "A").calls.length, 1,
    "The correct number of child calls were calculated for the 'A' node.");
  ok(getFrameNodePath(root, "A > B"),
    "The 'A' node's only child call is correct.");

  equal(getFrameNodePath(root, "A > B").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B' node.");
  ok(getFrameNodePath(root, "A > B > C"),
    "The 'A > B' node's only child call is correct.");

  equal(getFrameNodePath(root, "A > B > C").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > B > C' node.");
});

var gThread = synthesizeProfileForTest([{
  time: 5,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "C" }
  ]
}, {
  time: null,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "D" }
  ]
}]);
