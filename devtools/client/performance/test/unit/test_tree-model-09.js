/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that when displaying only content nodes, platform nodes are generalized.
 */

function run_test() {
  run_next_test();
}

add_task(function test() {
  let { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  let url = (n) => `http://content/${n}`;

  // Create a root node from a given samples array.

  let root = getFrameNodePath(new ThreadNode(gThread, { startTime: 5, endTime: 25,
                                                        contentOnly: true }), "(root)");

  /*
   * should have a tree like:
   * root
   *   - (Tools)
   *   - A
   *     - B
   *       - C
   *       - D
   *     - E
   *       - F
   *         - (Tools)
   */

  // Test the root node.

  equal(root.calls.length, 2, "root has 2 children");
  ok(getFrameNodePath(root, url("A")), "root has content child");
  ok(getFrameNodePath(root, "9000"),
    "root has platform generalized child from Chrome JS");
  equal(getFrameNodePath(root, "9000").calls.length, 0,
    "platform generalized child is a leaf.");

  ok(getFrameNodePath(root, `${url("A")} > ${url("E")} > ${url("F")} > 9000`),
     "a second leaf of the generalized Chrome JS exists.");

  equal(getFrameNodePath(root, "9000").category,
     getFrameNodePath(root, `${url("A")} > ${url("E")} > ${url("F")} > 9000`).category,
     "generalized frames of same type are duplicated in top-down view");
});

var gThread = synthesizeProfileForTest([{
  time: 5,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "http://content/B" },
    { location: "http://content/C" }
  ]
}, {
  time: 5 + 6,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "http://content/B" },
    { location: "fn (resource://loader.js -> resource://devtools/timeline.js)" },
    { location: "http://content/D" }
  ]
}, {
  time: 5 + 6 + 7,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "http://content/E" },
    { location: "http://content/F" },
    { location: "fn (resource://loader.js -> resource://devtools/promise.js)" }
  ]
}, {
  time: 5 + 20,
  frames: [
    { location: "(root)" },
    { location: "somefn (resource://loader.js -> resource://devtools/framerate.js)" }
  ]
}]);
