/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that when displaying only content nodes, platform nodes are generalized.
 */

var { CATEGORY_MASK } = require("devtools/performance/global");

function run_test() {
  run_next_test();
}

add_task(function test() {
  let { ThreadNode } = require("devtools/performance/tree-model");
  let url = (n) => `http://content/${n}`;

  // Create a root node from a given samples array.

  let root = getFrameNodePath(new ThreadNode(gThread, { startTime: 5, endTime: 30, contentOnly: true }), "(root)");

  /*
   * should have a tree like:
   * root
   *   - (JS)
   *   - A
   *     - (GC)
   *     - B
   *       - C
   *       - D
   *     - E
   *       - F
   *         - (JS)
   */

  // Test the root node.

  equal(root.calls.length, 2, "root has 2 children");
  ok(getFrameNodePath(root, url("A")), "root has content child");
  ok(getFrameNodePath(root, "64"), "root has platform generalized child");
  equal(getFrameNodePath(root, "64").calls.length, 0, "platform generalized child is a leaf.");

  ok(getFrameNodePath(root, `${url("A")} > 128`), "A has platform generalized child of another type");
  equal(getFrameNodePath(root, `${url("A")} > 128`).calls.length, 0, "second generalized type is a leaf.");

  ok(getFrameNodePath(root, `${url("A")} > ${url("E")} > ${url("F")} > 64`),
     "a second leaf of the first generalized type exists deep in the tree.");
  ok(getFrameNodePath(root, `${url("A")} > 128`), "A has platform generalized child of another type");

  equal(getFrameNodePath(root, "64").category,
     getFrameNodePath(root, `${url("A")} > ${url("E")} > ${url("F")} > 64`).category,
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
    { location: "contentY", category: CATEGORY_MASK("css") },
    { location: "http://content/D" }
  ]
}, {
  time: 5 + 6 + 7,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "contentY", category: CATEGORY_MASK("css") },
    { location: "http://content/E" },
    { location: "http://content/F" },
    { location: "contentY", category: CATEGORY_MASK("js") },
  ]
}, {
  time: 5 + 20,
  frames: [
    { location: "(root)" },
    { location: "contentX", category: CATEGORY_MASK("js") },
  ]
}, {
  time: 5 + 25,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "contentZ", category: CATEGORY_MASK("gc", 1) },
  ]
}]);
