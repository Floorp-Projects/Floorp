/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Like test_tree-model-12, but inverted.

function run_test() {
  run_next_test();
}

add_task(function () {
  let { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  let root = new ThreadNode(gThread, { invertTree: true, startTime: 0, endTime: 50 });

  /**
   * Samples
   *
   * A->B
   * C->B
   * B
   * A
   * Z->Y->X
   * W->Y->X
   * Y->X
   */

  equal(getFrameNodePath(root, "B").youngestFrameSamples, 3,
        "B has the correct self count");
  equal(getFrameNodePath(root, "A").youngestFrameSamples, 1,
        "A has the correct self count");
  equal(getFrameNodePath(root, "X").youngestFrameSamples, 3,
        "X has the correct self count");
  equal(getFrameNodePath(root, "X > Y").samples, 3,
        "X > Y has the correct total count");
});

var gThread = synthesizeProfileForTest([{
  time: 5,
  frames: [
    { location: "(root)" },
    { location: "A" },
    { location: "B" },
  ]
}, {
  time: 10,
  frames: [
    { location: "(root)" },
    { location: "C" },
    { location: "B" },
  ]
}, {
  time: 15,
  frames: [
    { location: "(root)" },
    { location: "B" },
  ]
}, {
  time: 20,
  frames: [
    { location: "(root)" },
    { location: "A" },
  ]
}, {
  time: 21,
  frames: [
    { location: "(root)" },
    { location: "Z" },
    { location: "Y" },
    { location: "X" },
  ]
}, {
  time: 22,
  frames: [
    { location: "(root)" },
    { location: "W" },
    { location: "Y" },
    { location: "X" },
  ]
}, {
  time: 23,
  frames: [
    { location: "(root)" },
    { location: "Y" },
    { location: "X" },
  ]
}]);
