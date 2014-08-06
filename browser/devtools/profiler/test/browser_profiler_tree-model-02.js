/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a call tree model ignores samples with no timing information.
 */

function test() {
  let { ThreadNode } = devtools.require("devtools/profiler/tree-model");

  // Create a root node from a given samples array.

  let root = new ThreadNode(gSamples);

  // Test the root node.

  is(root.duration, 5,
    "The correct duration was calculated for the root node.");

  is(Object.keys(root.calls).length, 1,
    "The correct number of child calls were calculated for the root node.");
  is(Object.keys(root.calls)[0], "A",
    "The root node's only child call is correct.");

  // Test all the descendant nodes.

  is(Object.keys(root.calls.A.calls).length, 1,
    "The correct number of child calls were calculated for the '.A' node.");
  is(Object.keys(root.calls.A.calls)[0], "B",
    "The '.A.B' node's only child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls).length, 1,
    "The correct number of child calls were calculated for the '.A.B' node.");
  is(Object.keys(root.calls.A.calls.B.calls)[0], "C",
    "The '.A.B' node's only child call is correct.");

  is(Object.keys(root.calls.A.calls.B.calls.C.calls).length, 0,
    "The correct number of child calls were calculated for the '.A.B.C' node.");

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
  time: null,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "D" }
  ]
}];
