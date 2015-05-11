/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a call tree model can be correctly computed from a samples array.
 */

function test() {
  const { ThreadNode } = devtools.require("devtools/shared/profiler/tree-model");

  // Create a root node from a given samples array.

  let threadNode = new ThreadNode(gThread);
  let root = getFrameNodePath(threadNode, "(root)");

  // Test the root node.

  is(threadNode.getInfo().nodeType, "Thread",
    "The correct node type was retrieved for the root node.");

  is(root.duration, 20,
    "The correct duration was calculated for the root node.");
  is(root.getInfo().functionName, "(root)",
    "The correct function name was retrieved for the root node.");
  is(root.getInfo().categoryData.toSource(), "({})",
    "The correct empty category data was retrieved for the root node.");

  is(root.calls.length, 1,
    "The correct number of child calls were calculated for the root node.");
  ok(getFrameNodePath(root, "A"),
    "The root node's only child call is correct.");

  // Test all the descendant nodes.

  is(getFrameNodePath(root, "A").calls.length, 2,
    "The correct number of child calls were calculated for the 'A' node.");
  ok(getFrameNodePath(root, "A > B"),
    "The 'A' node has a 'B' child call.");
  ok(getFrameNodePath(root, "A > E"),
    "The 'A' node has a 'E' child call.");

  is(getFrameNodePath(root, "A > B").calls.length, 2,
    "The correct number of child calls were calculated for the 'A > B' node.");
  ok(getFrameNodePath(root, "A > B > C"),
    "The 'A > B' node has a 'C' child call.");
  ok(getFrameNodePath(root, "A > B > D"),
    "The 'A > B' node has a 'D' child call.");

  is(getFrameNodePath(root, "A > E").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > E' node.");
  ok(getFrameNodePath(root, "A > E > F"),
    "The 'A > E' node has a 'F' child call.");

  is(getFrameNodePath(root, "A > B > C").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B > C' node.");
  ok(getFrameNodePath(root, "A > B > C > D"),
    "The 'A > B > C' node has a 'D' child call.");

  is(getFrameNodePath(root, "A > B > C > D").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B > C > D' node.");
  ok(getFrameNodePath(root, "A > B > C > D > E"),
    "The 'A > B > C > D' node has a 'E' child call.");

  is(getFrameNodePath(root, "A > B > C > D > E").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B > C > D > E' node.");
  ok(getFrameNodePath(root, "A > B > C > D > E > F"),
    "The 'A > B > C > D > E' node has a 'F' child call.");

  is(getFrameNodePath(root, "A > B > C > D > E > F").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B > C > D > E > F' node.");
  ok(getFrameNodePath(root, "A > B > C > D > E > F > G"),
    "The 'A > B > C > D > E > F' node has a 'G' child call.");

  is(getFrameNodePath(root, "A > B > C > D > E > F > G").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > B > C > D > E > F > G' node.");
  is(getFrameNodePath(root, "A > B > D").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > B > D' node.");
  is(getFrameNodePath(root, "A > E > F").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > E > F' node.");

  // Check the location, sample times, duration and samples of the root.

  is(getFrameNodePath(root, "A").location, "A",
    "The 'A' node has the correct location.");
  is(getFrameNodePath(root, "A").duration, 20,
    "The 'A' node has the correct duration in milliseconds.");
  is(getFrameNodePath(root, "A").samples, 4,
    "The 'A' node has the correct number of samples.");

  // ...and the rightmost leaf.

  is(getFrameNodePath(root, "A > E > F").location, "F",
    "The 'A > E > F' node has the correct location.");
  is(getFrameNodePath(root, "A > E > F").duration, 7,
    "The 'A > E > F' node has the correct duration in milliseconds.");
  is(getFrameNodePath(root, "A > E > F").samples, 1,
    "The 'A > E > F' node has the correct number of samples.");

  // ...and the leftmost leaf.

  is(getFrameNodePath(root, "A > B > C > D > E > F > G").location, "G",
    "The 'A > B > C > D > E > F > G' node has the correct location.");
  is(getFrameNodePath(root, "A > B > C > D > E > F > G").duration, 2,
    "The 'A > B > C > D > E > F > G' node has the correct duration in milliseconds.");
  is(getFrameNodePath(root, "A > B > C > D > E > F > G").samples, 1,
    "The 'A > B > C > D > E > F > G' node has the correct number of samples.");

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
