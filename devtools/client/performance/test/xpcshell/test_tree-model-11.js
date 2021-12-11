/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the costs for recursive frames does not overcount the collapsed
 * samples.
 */

add_task(function() {
  const {
    ThreadNode,
  } = require("devtools/client/performance/modules/logic/tree-model");
  const thread = new ThreadNode(gThread, {
    startTime: 0,
    endTime: 50,
    flattenRecursion: true,
  });

  /**
   * Samples
   *
   * A->B->C
   * A->B->B->B->C
   * A->B
   * A->B->B->B
   */

  [
    // total, self, name
    [100, 0, "(root)", [[100, 0, "A", [[100, 50, "B", [[50, 50, "C"]]]]]]],
  ].forEach(compareFrameInfo(thread));
});

function compareFrameInfo(root, parent) {
  parent = parent || root;
  return function(def) {
    const [total, self, name, children] = def;
    const node = getFrameNodePath(parent, name);
    const data = node.getInfo({ root });
    equal(
      total,
      data.totalPercentage,
      `${name} has correct total percentage: ${data.totalPercentage}`
    );
    equal(
      self,
      data.selfPercentage,
      `${name} has correct self percentage: ${data.selfPercentage}`
    );
    if (children) {
      children.forEach(compareFrameInfo(root, node));
    }
  };
}

var gThread = synthesizeProfileForTest([
  {
    time: 5,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B" },
      { location: "B" },
      { location: "B" },
      { location: "C" },
    ],
  },
  {
    time: 10,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B" },
      { location: "C" },
    ],
  },
  {
    time: 15,
    frames: [
      { location: "(root)" },
      { location: "A" },
      { location: "B" },
      { location: "B" },
      { location: "B" },
    ],
  },
  {
    time: 20,
    frames: [{ location: "(root)" }, { location: "A" }, { location: "B" }],
  },
]);
