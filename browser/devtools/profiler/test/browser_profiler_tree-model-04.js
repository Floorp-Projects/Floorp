/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a call tree model can be correctly computed from a samples array,
 * while at the same time filtering by duration and content-only frames.
 */

function test() {
  let { ThreadNode } = devtools.require("devtools/profiler/tree-model");

  // Create a root node from a given samples array, filtering by time.

  let root = new ThreadNode(gSamples, true, 11, 18);

  // Test the root node.

  is(root.duration, 18,
    "The correct duration was calculated for the root node.");

  is(Object.keys(root.calls).length, 2,
    "The correct number of child calls were calculated for the root node.");
  is(Object.keys(root.calls)[0], "http://D",
    "The root node's first child call is correct.");
  is(Object.keys(root.calls)[1], "http://A",
    "The root node's second child call is correct.");

  // Test all the descendant nodes.

  is(Object.keys(root.calls["http://A"].calls).length, 1,
    "The correct number of child calls were calculated for the '.A' node.");
  is(Object.keys(root.calls["http://A"].calls)[0], "https://E",
    "The '.A' node's only child call is correct.");

  is(Object.keys(root.calls["http://A"].calls["https://E"].calls).length, 1,
    "The correct number of child calls were calculated for the '.A.E' node.");
  is(Object.keys(root.calls["http://A"].calls["https://E"].calls)[0], "file://F",
    "The '.A.E' node's only child call is correct.");

  is(Object.keys(root.calls["http://A"].calls["https://E"].calls["file://F"].calls).length, 0,
    "The correct number of child calls were calculated for the '.A.E.F' node.");
  is(Object.keys(root.calls["http://D"].calls).length, 0,
    "The correct number of child calls were calculated for the '.D' node.");

  finish();
}

let gSamples = [{
  time: 5,
  frames: [
    { location: "(root)" },
    { location: "http://A" },
    { location: "http://B" },
    { location: "http://C" }
  ]
}, {
  time: 5 + 6,
  frames: [
    { location: "(root)" },
    { location: "chrome://A" },
    { location: "resource://B" },
    { location: "http://D" }
  ]
}, {
  time: 5 + 6 + 7,
  frames: [
    { location: "(root)" },
    { location: "http://A" },
    { location: "https://E" },
    { location: "file://F" }
  ]
}, {
  time: 5 + 6 + 7 + 8,
  frames: [
    { location: "(root)" },
    { location: "http://A" },
    { location: "http://B" },
    { location: "http://C" },
    { location: "http://D" }
  ]
}];
