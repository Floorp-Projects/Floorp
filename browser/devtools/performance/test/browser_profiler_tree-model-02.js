/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a call tree model ignores samples with no timing information.
 */

function test() {
  let { ThreadNode } = devtools.require("devtools/performance/tree-model");

  // Create a root node from a given samples array.

  let root = getFrameNodePath(new ThreadNode(gThread), "(root)");

  // Test the root node.

  is(root.duration, 5,
    "The correct duration was calculated for the root node.");

  is(root.calls.length, 1,
    "The correct number of child calls were calculated for the root node.");
  ok(getFrameNodePath(root, "A"),
    "The root node's only child call is correct.");

  // Test all the descendant nodes.

  is(getFrameNodePath(root, "A").calls.length, 1,
    "The correct number of child calls were calculated for the 'A' node.");
  ok(getFrameNodePath(root, "A > B"),
    "The 'A' node's only child call is correct.");

  is(getFrameNodePath(root, "A > B").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B' node.");
  ok(getFrameNodePath(root, "A > B > C"),
    "The 'A > B' node's only child call is correct.");

  is(getFrameNodePath(root, "A > B > C").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > B > C' node.");

  finish();
}

let gThread = synthesizeProfileForTest([{
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
