/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if a call tree model can be correctly computed from a samples array,
 * while at the same time filtering by duration and content-only frames.
 */

function run_test() {
  run_next_test();
}

add_task(function test() {
  let { ThreadNode } = require("devtools/performance/tree-model");

  // Create a root node from a given samples array, filtering by time.

  let startTime = 5;
  let endTime = 18;
  let thread = new ThreadNode(gThread, { startTime, endTime, contentOnly: true });
  let root = getFrameNodePath(thread, "(root)");

  // Test the ThreadNode, only node which should have duration
  equal(thread.duration, endTime - startTime,
    "The correct duration was calculated for the root ThreadNode.");

  equal(root.calls.length, 2,
    "The correct number of child calls were calculated for the root node.");
  ok(getFrameNodePath(root, "http://D"),
    "The root has a 'http://D' child call.");
  ok(getFrameNodePath(root, "http://A"),
    "The root has a 'http://A' child call.");

  // Test all the descendant nodes.

  equal(getFrameNodePath(root, "http://A").calls.length, 1,
    "The correct number of child calls were calculated for the 'http://A' node.");
  ok(getFrameNodePath(root, "http://A > https://E"),
    "The 'http://A' node's only child call is correct.");

  equal(getFrameNodePath(root, "http://A > https://E").calls.length, 1,
    "The correct number of child calls were calculated for the 'http://A > http://E' node.");
  ok(getFrameNodePath(root, "http://A > https://E > file://F"),
    "The 'http://A > https://E' node's only child call is correct.");

  equal(getFrameNodePath(root, "http://A > https://E > file://F").calls.length, 1,
    "The correct number of child calls were calculated for the 'http://A > https://E >> file://F' node.");
  ok(getFrameNodePath(root, "http://A > https://E > file://F > app://H"),
    "The 'http://A > https://E >> file://F' node's only child call is correct.");

  equal(getFrameNodePath(root, "http://D").calls.length, 0,
    "The correct number of child calls were calculated for the 'http://D' node.");
});

var gThread = synthesizeProfileForTest([{
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
    { location: "jar:file://G" },
    { location: "http://D" }
  ]
}, {
  time: 5 + 6 + 7,
  frames: [
    { location: "(root)" },
    { location: "http://A" },
    { location: "https://E" },
    { location: "file://F" },
    { location: "app://H" },
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
}]);
