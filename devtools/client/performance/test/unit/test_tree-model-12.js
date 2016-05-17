/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that uninverting the call tree works correctly when there are stacks
// in the profile that prefixes of other stacks.

function run_test() {
  run_next_test();
}

add_task(function () {
  let { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");
  let thread = new ThreadNode(gThread, { startTime: 0, endTime: 50 });
  let root = getFrameNodePath(thread, "(root)");

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

  equal(getFrameNodePath(root, "A > B").youngestFrameSamples, 1,
        "A > B has the correct self count");
  equal(getFrameNodePath(root, "C > B").youngestFrameSamples, 1,
        "C > B has the correct self count");
  equal(getFrameNodePath(root, "B").youngestFrameSamples, 1,
        "B has the correct self count");
  equal(getFrameNodePath(root, "A").youngestFrameSamples, 1,
        "A has the correct self count");
  equal(getFrameNodePath(root, "Z > Y > X").youngestFrameSamples, 1,
        "Z > Y > X has the correct self count");
  equal(getFrameNodePath(root, "W > Y > X").youngestFrameSamples, 1,
        "W > Y > X has the correct self count");
  equal(getFrameNodePath(root, "Y > X").youngestFrameSamples, 1,
        "Y > X has the correct self count");
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
