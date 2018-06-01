/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if a call tree model can be correctly computed from a samples array,
 * while at the same time filtering by duration.
 */

add_task(function test() {
  const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");

  // Create a root node from a given samples array, filtering by time.
  //
  // Filtering from 5 to 18 includes the 2nd and 3rd samples. The 2nd sample
  // starts exactly on 5 and ends at 11. The 3rd sample starts at 11 and ends
  // exactly at 18.
  const startTime = 5;
  const endTime = 18;
  const thread = new ThreadNode(gThread, { startTime, endTime });
  const root = getFrameNodePath(thread, "(root)");

  // Test the root node.

  equal(thread.duration, endTime - startTime,
    "The correct duration was calculated for the ThreadNode.");

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

  equal(getFrameNodePath(root, "A > B").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > B' node.");
  ok(getFrameNodePath(root, "A > B > D"),
    "The 'A > B' node's only child call is correct.");

  equal(getFrameNodePath(root, "A > E").calls.length, 1,
    "The correct number of child calls were calculated for the 'A > E' node.");
  ok(getFrameNodePath(root, "A > E > F"),
    "The 'A > E' node's only child call is correct.");

  equal(getFrameNodePath(root, "A > B > D").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > B > D' node.");
  equal(getFrameNodePath(root, "A > E > F").calls.length, 0,
    "The correct number of child calls were calculated for the 'A > E > F' node.");
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
  time: 5 + 6 + 7 + 8,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "C" },
    { location: "D" }
  ]
}]);
