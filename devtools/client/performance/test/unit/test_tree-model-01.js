/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a call tree model can be correctly computed from a samples array.
 */

function run_test() {
  run_next_test();
}

add_task(function test() {
  const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");

  // Create a root node from a given samples array.

  let threadNode = new ThreadNode(gThread, { startTime: 0, endTime: 20 });
  let root = getFrameNodePath(threadNode, "(root)");

  // Test the root node.

  equal(threadNode.getInfo().nodeType, "Thread",
    "The correct node type was retrieved for the root node.");

  equal(threadNode.duration, 20,
    "The correct duration was calculated for the ThreadNode.");
  equal(root.getInfo().functionName, "(root)",
    "The correct function name was retrieved for the root node.");
  equal(root.getInfo().categoryData.abbrev, "other",
    "The correct empty category data was retrieved for the root node.");

  equal(root.calls.length, 1,
    "The correct number of child calls were calculated for the root node.");
  ok(getFrameNodePath(root, "A"),
    "The root node's only child call is correct.");

  // Test all the descendant nodes.

  equal(getFrameNodePath(root, "A").calls.length, 2,
    "The correct number of child calls were calculated for the 'A' node.");
  ok(getFrameNodePath(root, "A > B"),
    "The 'A' node has a 'B' child call.");
  ok(getFrameNodePath(root, "A > E"),
    "The 'A' node has a 'E' child call.");

  equal(getFrameNodePath(root, "A > B").calls.length, 2,
    "The correct number of child calls were calculated for the 'A > B' node.");
  ok(getFrameNodePath(root, "A > B > C"),
    "The 'A > B' node has a 'C' child call.");
  ok(getFrameNodePath(root, "A > B > D"),
    "The 'A > B' node has a 'D' child call.");

  equal(getFrameNodePath(root, "A > E").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > E' node.");
  ok(getFrameNodePath(root, "A > E > F"),
    "The 'A > E' node has a 'F' child call.");

  equal(getFrameNodePath(root, "A > B > C").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B > C' node.");
  ok(getFrameNodePath(root, "A > B > C > D"),
    "The 'A > B > C' node has a 'D' child call.");

  equal(getFrameNodePath(root, "A > B > C > D").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B > C > D' node.");
  ok(getFrameNodePath(root, "A > B > C > D > E"),
    "The 'A > B > C > D' node has a 'E' child call.");

  equal(getFrameNodePath(root, "A > B > C > D > E").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B > C > D > E' node.");
  ok(getFrameNodePath(root, "A > B > C > D > E > F"),
    "The 'A > B > C > D > E' node has a 'F' child call.");

  equal(getFrameNodePath(root, "A > B > C > D > E > F").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B > C > D > E > F' node.");
  ok(getFrameNodePath(root, "A > B > C > D > E > F > G"),
    "The 'A > B > C > D > E > F' node has a 'G' child call.");

  equal(getFrameNodePath(root, "A > B > C > D > E > F > G").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > B > C > D > E > F > G' node.");
  equal(getFrameNodePath(root, "A > B > D").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > B > D' node.");
  equal(getFrameNodePath(root, "A > E > F").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > E > F' node.");

  // Check the location, sample times, and samples of the root.

  equal(getFrameNodePath(root, "A").location, "A",
    "The 'A' node has the correct location.");
  equal(getFrameNodePath(root, "A").youngestFrameSamples, 0,
    "The 'A' has correct `youngestFrameSamples`");
  equal(getFrameNodePath(root, "A").samples, 4,
    "The 'A' has correct `samples`");

  // A frame that is both a leaf and caught in another stack
  equal(getFrameNodePath(root, "A > B > C").youngestFrameSamples, 1,
    "The 'A > B > C' has correct `youngestFrameSamples`");
  equal(getFrameNodePath(root, "A > B > C").samples, 2,
    "The 'A > B > C' has correct `samples`");

  // ...and the rightmost leaf.

  equal(getFrameNodePath(root, "A > E > F").location, "F",
    "The 'A > E > F' node has the correct location.");
  equal(getFrameNodePath(root, "A > E > F").samples, 1,
    "The 'A > E > F' node has the correct number of samples.");
  equal(getFrameNodePath(root, "A > E > F").youngestFrameSamples, 1,
    "The 'A > E > F' node has the correct number of youngestFrameSamples.");

  // ...and the leftmost leaf.

  equal(getFrameNodePath(root, "A > B > C > D > E > F > G").location, "G",
    "The 'A > B > C > D > E > F > G' node has the correct location.");
  equal(getFrameNodePath(root, "A > B > C > D > E > F > G").samples, 1,
    "The 'A > B > C > D > E > F > G' node has the correct number of samples.");
  equal(getFrameNodePath(root, "A > B > C > D > E > F > G").youngestFrameSamples, 1,
    "The 'A > B > C > D > E > F > G' node has the correct number of youngestFrameSamples.");
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
  time: 5 + 6,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "D" }
  ]
}, {
  time: 5 + 6 + 7,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "E" },
    { location: "F" }
  ]
}, {
  time: 20,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "C" },
    { location: "D" },
    { location: "E" },
    { location: "F" },
    { location: "G" }
  ]
}]);
