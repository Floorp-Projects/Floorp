/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the DominatorTreeNode.attachShortestPaths function can correctly
// attach the deduplicated shortest retaining paths for each node it is given.

const startNodeId = 9999;
const maxNumPaths = 2;

// Mock data mapping node id to shortest paths to that node id.
const shortestPaths = new Map([
  [1000, [
    [pathEntry(1100, "a"), pathEntry(1200, "b")],
    [pathEntry(1100, "c"), pathEntry(1300, "d")],
  ]],
  [2000, [
    [pathEntry(2100, "e"), pathEntry(2200, "f"), pathEntry(2300, "g")]
  ]],
  [3000, [
    [pathEntry(3100, "h")],
    [pathEntry(3100, "i")],
    [pathEntry(3100, "j")],
    [pathEntry(3200, "k")],
    [pathEntry(3300, "l")],
    [pathEntry(3400, "m")],
  ]],
]);

const actual = [
  makeTestDominatorTreeNode({ nodeId: 1000 }),
  makeTestDominatorTreeNode({ nodeId: 2000 }),
  makeTestDominatorTreeNode({ nodeId: 3000 }),
];

const expected = [
  makeTestDominatorTreeNode({
    nodeId: 1000,
    shortestPaths: {
      nodes: [
        { id: 1000, label: ["SomeType-1000"] },
        { id: 1100, label: ["SomeType-1100"] },
        { id: 1200, label: ["SomeType-1200"] },
        { id: 1300, label: ["SomeType-1300"] },
      ],
      edges: [
        { from: 1100, to: 1200, name: "a" },
        { from: 1100, to: 1300, name: "c" },
        { from: 1200, to: 1000, name: "b" },
        { from: 1300, to: 1000, name: "d" },
      ]
    }
  }),

  makeTestDominatorTreeNode({
    nodeId: 2000,
    shortestPaths: {
      nodes: [
        { id: 2000, label: ["SomeType-2000"] },
        { id: 2100, label: ["SomeType-2100"] },
        { id: 2200, label: ["SomeType-2200"] },
        { id: 2300, label: ["SomeType-2300"] },
      ],
      edges: [
        { from: 2100, to: 2200, name: "e" },
        { from: 2200, to: 2300, name: "f" },
        { from: 2300, to: 2000, name: "g" },
      ]
    }
  }),

  makeTestDominatorTreeNode({ nodeId: 3000,
    shortestPaths: {
      nodes: [
        { id: 3000, label: ["SomeType-3000"] },
        { id: 3100, label: ["SomeType-3100"] },
        { id: 3200, label: ["SomeType-3200"] },
        { id: 3300, label: ["SomeType-3300"] },
        { id: 3400, label: ["SomeType-3400"] },
      ],
      edges: [
        { from: 3100, to: 3000, name: "h" },
        { from: 3100, to: 3000, name: "i" },
        { from: 3100, to: 3000, name: "j" },
        { from: 3200, to: 3000, name: "k" },
        { from: 3300, to: 3000, name: "l" },
        { from: 3400, to: 3000, name: "m" },
      ]
    }
  }),
];

const breakdown = {
  by: "internalType",
  then: { by: "count", count: true, bytes: true }
};

const mockSnapshot = {
  computeShortestPaths: (start, nodeIds, max) => {
    equal(start, startNodeId);
    equal(max, maxNumPaths);

    return new Map(nodeIds.map(nodeId => {
      const paths = shortestPaths.get(nodeId);
      ok(paths, "Expected computeShortestPaths call for node id = " + nodeId);
      return [nodeId, paths];
    }));
  },

  describeNode: (bd, nodeId) => {
    equal(bd, breakdown);
    return {
      ["SomeType-" + nodeId]: {
        count: 1,
        bytes: 10,
      }
    };
  },
};

function run_test() {
  DominatorTreeNode.attachShortestPaths(mockSnapshot,
                                        breakdown,
                                        startNodeId,
                                        actual,
                                        maxNumPaths);

  dumpn("Expected = " + JSON.stringify(expected, null, 2));
  dumpn("Actual = " + JSON.stringify(actual, null, 2));

  assertStructurallyEquivalent(expected, actual);
}
