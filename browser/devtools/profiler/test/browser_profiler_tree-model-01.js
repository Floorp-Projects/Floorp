/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a call tree model can be correctly computed from a samples array.
 */

function test() {
  let { ThreadNode } = devtools.require("devtools/profiler/tree-model");

  // Create a root node from a given samples array.

  let root = new ThreadNode(gSamples);

  // Test the root node.

  is(root.duration, 18,
    "The correct duration was calculated for the root node.");
  is(root.getInfo().nodeType, "Thread",
    "The correct node type was retrieved for the root node.");
  is(root.getInfo().functionName, "(root)",
    "The correct function name was retrieved for the root node.");
  is(root.getInfo().categoryData.toSource(), "({})",
    "The correct empty category data was retrieved for the root node.");

  is(Object.keys(root.calls).length, 1,
    "The correct number of child calls were calculated for the root node.");
  is(Object.keys(root.calls)[0], "A",
    "The root node's only child call is correct.");

  // Test all the descendant nodes.

  is(Object.keys(root.calls.A.calls).length, 2,
    "The correct number of child calls were calculated for the '.A' node.");
  is(Object.keys(root.calls.A.calls)[0], "B",
    "The '.A' node's first child call is correct.");
  is(Object.keys(root.calls.A.calls)[1], "E",
    "The '.A' node's second child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls).length, 2,
    "The correct number of child calls were calculated for the '.A.B' node.");
  is(Object.keys(root.calls.A.calls.B.calls)[0], "C",
    "The '.A.B' node's first child call is correct.");
  is(Object.keys(root.calls.A.calls.B.calls)[1], "D",
    "The '.A.B' node's second child call is correct.");

  is(Object.keys(root.calls.A.calls.E.calls).length, 1,
    "The correct number of child calls were calculated for the '.A.E' node.");
  is(Object.keys(root.calls.A.calls.E.calls)[0], "F",
    "The '.A.E' node's only child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls.C.calls).length, 0,
    "The correct number of child calls were calculated for the '.A.B.C' node.");
  is(Object.keys(root.calls.A.calls.B.calls.D.calls).length, 0,
    "The correct number of child calls were calculated for the '.A.B.D' node.");
  is(Object.keys(root.calls.A.calls.E.calls.F.calls).length, 0,
    "The correct number of child calls were calculated for the '.A.E.F' node.");

  // Insert new nodes in the tree.

  root.insert({
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
  });

  // Retest the root node.

  is(root.duration, 20,
    "The correct duration was recalculated for the root node.");

  is(Object.keys(root.calls).length, 1,
    "The correct number of child calls were calculated for the root node.");
  is(Object.keys(root.calls)[0], "A",
    "The root node's only child call is correct.");

  // Retest all the descendant nodes.

  is(Object.keys(root.calls.A.calls).length, 2,
    "The correct number of child calls were calculated for the '.A' node.");
  is(Object.keys(root.calls.A.calls)[0], "B",
    "The '.A' node's first child call is correct.");
  is(Object.keys(root.calls.A.calls)[1], "E",
    "The '.A' node's second child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls).length, 2,
    "The correct number of child calls were calculated for the '.A.B' node.");
  is(Object.keys(root.calls.A.calls.B.calls)[0], "C",
    "The '.A.B' node's first child call is correct.");
  is(Object.keys(root.calls.A.calls.B.calls)[1], "D",
    "The '.A.B' node's second child call is correct.");

  is(Object.keys(root.calls.A.calls.E.calls).length, 1,
    "The correct number of child calls were calculated for the '.A.E' node.");
  is(Object.keys(root.calls.A.calls.E.calls)[0], "F",
    "The '.A.E' node's only child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls.C.calls).length, 1,
    "The correct number of child calls were calculated for the '.A.B.C' node.");
  is(Object.keys(root.calls.A.calls.B.calls.C.calls)[0], "D",
    "The '.A.B.C' node's only child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls.C.calls.D.calls).length, 1,
    "The correct number of child calls were calculated for the '.A.B.C.D' node.");
  is(Object.keys(root.calls.A.calls.B.calls.C.calls.D.calls)[0], "E",
    "The '.A.B.C.D' node's only child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls.C.calls.D.calls.E.calls).length, 1,
    "The correct number of child calls were calculated for the '.A.B.C.D.E' node.");
  is(Object.keys(root.calls.A.calls.B.calls.C.calls.D.calls.E.calls)[0], "F",
    "The '.A.B.C.D.E' node's only child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls.C.calls.D.calls.E.calls.F.calls).length, 1,
    "The correct number of child calls were calculated for the '.A.B.C.D.E.F' node.");
  is(Object.keys(root.calls.A.calls.B.calls.C.calls.D.calls.E.calls.F.calls)[0], "G",
    "The '.A.B.C.D.E.F' node's only child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls.C.calls.D.calls.E.calls.F.calls.G.calls).length, 0,
    "The correct number of child calls were calculated for the '.A.B.D.E.F.G' node.");
  is(Object.keys(root.calls.A.calls.B.calls.D.calls).length, 0,
    "The correct number of child calls were calculated for the '.A.B.D' node.");
  is(Object.keys(root.calls.A.calls.E.calls.F.calls).length, 0,
    "The correct number of child calls were calculated for the '.A.E.F' node.");

  // Check the location, sample times, duration and invocations of the root.

  is(root.calls.A.location, "A",
    "The '.A' node has the correct location.");
  is(root.calls.A.sampleTimes.toSource(),
    "[{start:5, end:10}, {start:11, end:17}, {start:18, end:25}, {start:20, end:22}]",
    "The '.A' node has the correct sample times.");
  is(root.calls.A.duration, 20,
    "The '.A' node has the correct duration in milliseconds.");
  is(root.calls.A.invocations, 4,
    "The '.A' node has the correct number of invocations.");

  // ...and the rightmost leaf.

  is(root.calls.A.calls.E.calls.F.location, "F",
    "The '.A.E.F' node has the correct location.");
  is(root.calls.A.calls.E.calls.F.sampleTimes.toSource(),
    "[{start:18, end:25}]",
    "The '.A.E.F' node has the correct sample times.");
  is(root.calls.A.calls.E.calls.F.duration, 7,
    "The '.A.E.F' node has the correct duration in milliseconds.");
  is(root.calls.A.calls.E.calls.F.invocations, 1,
    "The '.A.E.F' node has the correct number of invocations.");

  // ...and the leftmost leaf.

  is(root.calls.A.calls.B.calls.C.calls.D.calls.E.calls.F.calls.G.location, "G",
    "The '.A.B.C.D.E.F.G' node has the correct location.");
  is(root.calls.A.calls.B.calls.C.calls.D.calls.E.calls.F.calls.G.sampleTimes.toSource(),
    "[{start:20, end:22}]",
    "The '.A.B.C.D.E.F.G' node has the correct sample times.");
  is(root.calls.A.calls.B.calls.C.calls.D.calls.E.calls.F.calls.G.duration, 2,
    "The '.A.B.C.D.E.F.G' node has the correct duration in milliseconds.");
  is(root.calls.A.calls.B.calls.C.calls.D.calls.E.calls.F.calls.G.invocations, 1,
    "The '.A.B.C.D.E.F.G' node has the correct number of invocations.");

  finish();
}

let gSamples = [{
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
}];
