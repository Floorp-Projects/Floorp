/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests if an inverted call tree model can be correctly computed from a samples
 * array.
 */

var time = 1;

var gThread = synthesizeProfileForTest([{
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "C" }
  ]
}, {
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "D" },
    { location: "C" }
  ]
}, {
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "E" },
    { location: "C" }
  ],
}, {
  time: time++,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
    { location: "F" }
  ]
}]);

function run_test() {
  run_next_test();
}

add_task(function test() {
  let { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");

  let root = new ThreadNode(gThread, { invertTree: true, startTime: 0, endTime: 4 });

  equal(root.calls.length, 2,
     "Should get the 2 youngest frames, not the 1 oldest frame");

  let C = getFrameNodePath(root, "C");
  ok(C, "Should have C as a child of the root.");

  equal(C.calls.length, 3,
     "Should have 3 frames that called C.");
  ok(getFrameNodePath(C, "B"), "B called C.");
  ok(getFrameNodePath(C, "D"), "D called C.");
  ok(getFrameNodePath(C, "E"), "E called C.");

  equal(getFrameNodePath(C, "B").calls.length, 1);
  ok(getFrameNodePath(C, "B > A"), "A called B called C");
  equal(getFrameNodePath(C, "D").calls.length, 1);
  ok(getFrameNodePath(C, "D > A"), "A called D called C");
  equal(getFrameNodePath(C, "E").calls.length, 1);
  ok(getFrameNodePath(C, "E > A"), "A called E called C");

  let F = getFrameNodePath(root, "F");
  ok(F, "Should have F as a child of the root.");

  equal(F.calls.length, 1);
  ok(getFrameNodePath(F, "B"), "B called F");

  equal(getFrameNodePath(F, "B").calls.length, 1);
  ok(getFrameNodePath(F, "B > A"), "A called B called F");
});
