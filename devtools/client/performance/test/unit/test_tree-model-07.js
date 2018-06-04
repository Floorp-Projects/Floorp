/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that when displaying only content nodes, platform nodes are generalized.
 */

var { CATEGORY_INDEX } = require("devtools/client/performance/modules/categories");

add_task(function test() {
  const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  const url = (n) => `http://content/${n}`;

  // Create a root node from a given samples array.

  const root = getFrameNodePath(new ThreadNode(gThread, { startTime: 5, endTime: 30,
                                                          contentOnly: true }), "(root)");

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
  ok(getFrameNodePath(root, `${CATEGORY_INDEX("js")}`),
     "root has platform generalized child");
  equal(getFrameNodePath(root, `${CATEGORY_INDEX("js")}`).calls.length, 0,
        "platform generalized child is a leaf.");

  ok(getFrameNodePath(root, `${url("A")} > ${CATEGORY_INDEX("gc")}`),
     "A has platform generalized child of another type");
  equal(getFrameNodePath(root, `${url("A")} > ${CATEGORY_INDEX("gc")}`).calls.length, 0,
        "second generalized type is a leaf.");

  ok(getFrameNodePath(
       root,
       `${url("A")} > ${url("E")} > ${url("F")} > ${CATEGORY_INDEX("js")}`
     ),
     "a second leaf of the first generalized type exists deep in the tree.");
  ok(getFrameNodePath(root, `${url("A")} > ${CATEGORY_INDEX("gc")}`),
     "A has platform generalized child of another type");

  equal(getFrameNodePath(root, `${CATEGORY_INDEX("js")}`).category,
     getFrameNodePath(
       root,
       `${url("A")} > ${url("E")} > ${url("F")} > ${CATEGORY_INDEX("js")}`
     ).category,
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
    { location: "contentY", category: CATEGORY_INDEX("layout") },
    { location: "http://content/D" }
  ]
}, {
  time: 5 + 6 + 7,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "contentY", category: CATEGORY_INDEX("layout") },
    { location: "http://content/E" },
    { location: "http://content/F" },
    { location: "contentY", category: CATEGORY_INDEX("js") },
  ]
}, {
  time: 5 + 20,
  frames: [
    { location: "(root)" },
    { location: "contentX", category: CATEGORY_INDEX("js") },
  ]
}, {
  time: 5 + 25,
  frames: [
    { location: "(root)" },
    { location: "http://content/A" },
    { location: "contentZ", category: CATEGORY_INDEX("gc") },
  ]
}]);
